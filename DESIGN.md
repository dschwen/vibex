# DESIGN.md — et: Expression Templates with Symbolic AD and Modular Backends

**Audience:** C++ developers new to this codebase who will add operations, improve automatic differentiation (AD), or add new backends (TorchScript, reverse-mode tape, codegen, printers, etc.).  
**Status:** Minimal but production-grade skeleton: header-only, zero RTTI, no heap allocations in the IR, works in C++14+.

---

## 1. Goals and Philosophy

- **Single, tiny IR** for math expressions built with normal C++ operators (expression templates).
- **Symbolic AD** that returns an expression tree, so the same infrastructure can compile the gradient to any backend.
- **Modularity by construction:** add an operation (`Op`) once; evaluation, AD, simplification, and all backends see it with minimal glue.
- **No runtime polymorphism** in the IR. Nodes are simple templates; composition is resolved at compile-time.
- **Backend plug-ins** via a single `compile(expr, backend)` visitor function. Backends implement `emitVar`, `emitConst`, `emitApply` overloads.
- **Extensible:** add new ops, extend simplifier, add backends (Torch JIT, tape, C codegen, printers) without changing core nodes.

Trade-off: a templated IR yields large types and deep instantiations. We mitigate through small nodes, minimal traits, and optional compile passes (e.g., simplifier). If code size becomes a concern, a future hash-consing layer can inter n-ary nodes.

---

## 2. Directory Overview

```
include/et/expr.hpp            # Core IR, ops, evaluation, AD, compile visitor, Vars, grad helpers
include/et/simplify.hpp        # Tiny algebraic simplifier (constant folding + neutral/annihilator rules)
include/et/tape_backend.hpp    # Reverse-mode tape backend (forward eval + VJP gradient)
include/et/torch_jit_backend.hpp  # TorchScript backend (optional; -DET_WITH_TORCH)
examples/                      # Small programs exercising the API and backends
README.md                      # Quick start
DESIGN.md                      # (this doc)
```

---

## 3. Core IR

### 3.1 Node Types

- `Var<T, I>` — Placeholder variable with **value type** `T` and **argument index** `I` (position in `operator()`).
- `Const<T>` — Literal constant node.
- `Apply<Op, Children...>` — Operation node applying operation tag `Op` to child expressions `Children...`.

Each node is **immutable** and **stateless** (other than `Const` holding its value). Children are stored in a `std::tuple`. No virtuals, no heap allocation.

```cpp
template <class Op, class... Children>
struct Apply {
  using value_type = decltype(Op::eval(std::declval<value_type_of_t<Children>>()...));
  std::tuple<Children...> ch;
  // operator() applies Op::eval to the evaluated children
};
```

`value_type_of_t<X>` resolves the “runtime value” type of a node (`Var<T,I>` exposes `value_type=T`; for `Apply`, we use `Op::eval(...)`).

### 3.2 Operations (`Op` tags)

An operation is a **zero-sized tag** with two static member functions:

- `eval(args...)` — Pure function for eager evaluation.
- `template <size_t I> d(children...)` — **Symbolic derivative rule** of the op, returning an **expression** in terms of children and their differentials `diff(child, I)`.

Example (product rule):
```cpp
struct MulOp {
  template<class A, class B>
  static auto eval(A&& a, B&& b) -> decltype(a*b) { return a*b; }

  template <size_t I, class ANode, class BNode>
  static auto d(const ANode& a, const BNode& b) {
    auto da = diff(a, std::integral_constant<size_t,I>{});
    auto db = diff(b, std::integral_constant<size_t,I>{});
    return (da * b) + (a * db);
  }
};
```

This keeps **AD local to each op** (no central registry). Adding an op means adding its rules **once**.

### 3.3 Building Expressions

Operators are overloaded to return `Apply<Op, ...>`:
```cpp
auto [x,y,z] = Vars<double,3>();           // Var<double,0/1/2>
auto f = sin(x)*y + z*z;                    // tree of Apply<...>
double v = evaluate(f, 2.4, 6.0, 1.5);      // eager compute
```

`Var<T,I>::operator()` picks the I-th argument, casts to `T`, and returns it. This allows **heterogeneous calls** like `f(2.4, 6, MyDual{})`, as long as each actual is convertible to the `Var`'s `T`.

---

## 4. Automatic Differentiation (Symbolic)

`diff(expr, I)` visits the tree and returns a **new expression** (IR) for the derivative with respect to **argument index** `I`. Base cases:

- `diff(Var<T,J>, I) = 1` if `I==J`, else `0`.
- `diff(Const<T>, I) = 0`.

For `Apply<Op, Children...>`, we call `Op::d<I>(children...)`. This **delegates calculus rules to each op** and composes them (chain, product, quotient, etc.).

### 4.1 `diff(expr, var)` sugar and gradient helpers

We provide a trait `var_index_of<Var<T,I>>` so you can write:
```cpp
auto dfdx = diff(f, x);                     // instead of integral_constant<0>
auto g    = grad(f, x, y, z);               // tuple of partials
```

### 4.2 Why symbolic?

- You can feed the derivative expression to **any backend** (`compile(df, backend)`).
- You can **simplify** the derivative (do algebraic cleanup).
- You can differentiate **again** (higher-order derivatives).
- For large graphs where runtime is critical, pair it with the **reverse-mode tape** (Section 7) which is operational AD.

---

## 5. Evaluation

`evaluate(expr, args...)` is a thin wrapper calling `expr.operator()(args...)`. Value types and mixing rules are delegated to the underlying numeric types (built-ins or your custom duals/bigints/etc.).

---

## 6. Simplification

`include/et/simplify.hpp` provides a conservative algebraic simplifier:

- **Constant folding** for unary (`-`, `sin`) and binary (`+,-,*,/`) ops.
- **Neutral elements:** `a+0→a`, `a-0→a`, `a*1→a`, `a/1→a`.
- **Annihilators:** `a*0→0`, `0*a→0`, `0/b→0`.

Design choices:
- Purely functional: returns a new (possibly smaller) tree.
- Local, safe rewrites only; no re-association or division-by-zero traps.
- Easy to extend (add cases for new ops; add identities like `x-x→0` when safe).

---

## 7. Backends and the `compile(...)` Visitor

A **backend** is any type `B` implementing three methods:
```cpp
using result_type = ...;
template<class T, size_t I> result_type emitVar(Var<T,I>);
template<class T>            result_type emitConst(Const<T>);
template<class Op, class... R>
result_type emitApply(Op, const R&... compiled_children);
```

The generic visitor:
```cpp
template <class Backend, class Expr>
auto compile(const Expr& e, Backend& b) -> typename Backend::result_type;
```
is overloaded for `Var`, `Const`, and `Apply<Op,Children...>`. For `Apply`, it recursively compiles children, then calls `b.emitApply(Op{}, compiled_child_results...)`.

**Why this design?**  
- Backends only care about the **shape** and **ops**; they don’t deal with the templated child types directly.  
- You write the minimal code to “lower” each op (`Add`, `Mul`, etc.) to your target (JIT node, tape instruction, C line…).  
- Adding an OP is trivially reflected in all backends: add a new `emitApply` branch (or a traits-based map if preferred).

### 7.1 TorchScript/JIT Backend

- Optional; compile with `-DET_WITH_TORCH` and link libtorch.
- `emitVar` maps to `g.addInput()` values created in the constructor (by index).
- `emitConst` makes a scalar (0-d) `prim::Constant` **tensor** for uniformity with `aten::` ops.
- `emitApply` maps IR ops to `aten::` symbols (e.g., `aten::add`, `aten::mul`, `aten::sin`, `aten::neg`).

**Extending:** add branches for new ops (`exp→aten::exp`, `log→aten::log`, etc.). If you want first-class scalars, split to scalar overloads (`aten::add.Scalar`); otherwise keep constants as 0-d Tensors.

### 7.2 Reverse-Mode Tape Backend

- Compiles IR to a **topologically sorted tape** with a tiny `enum Kind` and child indices.
- `forward(inputs)` computes the primal value.
- `vjp(inputs)` computes **dOutput/dInputs** using one backward sweep with locally coded VJPs (vector-Jacobian products).

**Why a tape when we already have symbolic AD?**
- For runtime-critical repeated eval/grad of the **same expression shape**, tape has lower constant factors than compiling and evaluating a symbolic derivative tree.
- Tape is a bridge to multi-output and batching; you can easily extend it to store intermediate values, checkpointing, etc.

---

## 8. Modularity: How All the Pieces Fit

- **IR is operation-agnostic**: nodes don’t know calculus or backends.
- **Ops own their math**: `Op::d<I>(children)` defines differentiation locally; `Op::eval` defines eager numeric behavior.
- **Algorithms are generic**: `diff`, `simplify`, `compile`, `evaluate` are templated on the IR and `Op`, not on concrete operations.
- **Backends are isolated**: each backend is a small emitter translating the structural traversal into target primitives.

This means you can:
- Add a new `Op` in one header and wire it into backends incrementally.
- Add a new backend without touching **any** ops or algorithms (except optional specialized rewrites).

---

## 9. Adding a New Operation (Checklist)

1. **Define the op tag** in `expr.hpp`:
   - `struct ExpOp { static auto eval(A&&) -> decltype(exp(a)); template<size_t I> static auto d(const X&); };`
2. **Operator sugar** (optional): `template<class A> auto exp(A a) { return Apply<ExpOp, A>(std::move(a)); }`
3. **Simplifier** (optional): add constant folding and safe identities.
4. **Torch backend** (optional): map to `aten::exp` in `emitApply`.
5. **Tape backend** (optional): add `Kind` enum and forward/VJP rules.

Minimal work: (1)+(2). Everything else compiles but won’t simplify or lower to certain backends until you add support.

---

## 10. Extending Backends

### 10.1 Torch JIT
- Map new ops to `aten::` symbols.
- If you switch from scalars to tensors with shapes, annotate input/output types, add broadcasting semantics, and consider shape inference.

### 10.2 Tape
- Expand `enum Kind`, extend forward and VJP switch arms.
- Consider storing `double` today; templatize later if you need other scalar types.
- For performance, pre-allocate `val`/`bar`, support multi-output by seeding multiple adjoints.

### 10.3 Printers / Codegen
- Backend that emits strings or C code lines. The same visitor pattern applies; constants and variables have obvious textual forms.

---

## 11. Type System Notes

- `Var<T,I>` enforces that the I-th runtime argument is convertible to `T` (via `static_cast<T>`). This supports **heterogeneous calls**: `f(2.4, 6, Dual{})`.
- Result type of each `Apply` is deduced via `decltype(Op::eval(...))`. Promotion/mixing rules are delegated to your numeric types and the operators they implement.
- AD returns expressions with value_type inferred from the rules (e.g., `da*b + a*db`), following the same compile-time deduction.

If you need cross-type unification (e.g., `int` + `double` → `double`), prefer to encode it in the operator overloads rather than in the IR.

---

## 12. Performance Considerations

- Nodes are small; IR is static; calls inline well under `-O2`/`-O3`.
- **Symbolic AD** may grow trees; the **simplifier** keeps them in check (fold constants, drop neutral ops).
- For hot loops with fixed expressions, use the **tape backend** to cut constant factors.
- If compile-times grow, consider:
  - Reducing template instantiation depth by introducing helper aliases,
  - Hash-consing/CSE in `compile(...)` (memoize compiled children by address or structural hash) to avoid emitting the same sub-expression multiple times in backends,
  - Pre-compiling derivative expressions once and reusing them.

---

## 13. Examples Summary

- **01_basic_eval.cpp** — Build and evaluate `f = sin(x)*y + z*z`.
- **02_symbolic_diff.cpp** — Compute `diff(f, x)`, `diff(f, y)`, `diff(f, z)` and evaluate at a point.
- **03_simplify.cpp** — Simplify the derivative and evaluate.
- **04_tape_ad.cpp** — Compile to a reverse-mode tape and compute value + gradient via VJP.
- **05_torchjit.cpp** — (Optional) Lower the expression to a TorchScript graph and print it.

---

## 14. Future Work

- **Op set**: add `exp`, `log`, `sqrt`, `tanh`, `pow`, `abs`, comparisons, conditionals (`select`).
- **Common Subexpression Elimination (CSE)** in `compile(...)` to reduce backend node count (especially for JIT/tape).
- **Broadcasting & shapes** for tensor semantics; add shape/type inference traits for backends.
- **Reverse-mode symbolic** (generate symbolic adjoint expressions) if desired; today we pair symbolic forward with tape reverse.
- **Higher-order AD**: repeated `diff(...)` already works symbolically; for efficiency, consider forward-mode dual numbers as an alternate evaluation backend.
- **Error handling**: domain checks (`log`, `sqrt`) are currently delegated to underlying numeric types.

---

## 15. Contribution Guidelines

- New ops: add tag + `eval` + `d` in `expr.hpp`; add sugar function; extend simplifier/backends gradually.
- Backends: implement `emitVar`, `emitConst`, and `emitApply`; prefer exhaustive `if constexpr` mapping or a traits map.
- Tests: extend `examples/` or add unit tests comparing `evaluate(diff(f,x), point)` against finite differences where applicable.
- Style: keep headers self-contained and minimal; prefer `constexpr` and avoid runtime allocations in the IR.

---

## 16. FAQ

**Q: Why not store expressions as members in classes?**  
A: Expression types are long and hard to spell pre-C++17. Prefer rebuilding on demand (policy factory or CRTP provider). The IR is stateless so rebuilding is cheap and inlinable.

**Q: Why not virtual nodes?**  
A: We want zero-overhead templates and compile-time composition. Virtual dispatch and heap allocation hinder optimization and make AD harder to keep generic.

**Q: Can I use my own number types (dual, big rational, intervals)?**  
A: Yes. Implement `+,-,*,/, sin, cos, ...` (ADL-friendly) and they plug in seamlessly. Result type deduction follows your operator return types.

**Q: How do I target new runtimes?**  
A: Write a backend: implement the three `emit*` methods and call `compile(expr, backend)`. See `tape_backend.hpp` and `torch_jit_backend.hpp` for patterns.

---

Happy hacking!
