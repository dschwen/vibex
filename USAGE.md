# Vibex User Guide

This is a quick, practical tour of the “user-facing” surface of Vibex: building expressions, evaluating, differentiating, simplifying, compiling to backends (tape), running CSE, and exporting to TorchScript.

---

## 1) Core Concepts (AST)

### Variables and constants
Build expressions with the runtime AST API in `et/ast.hpp`:

```cpp
using namespace et;
auto x = var(0), y = var(1), z = var(2);
Expr c = lit(2.5);
Expr f = sin(x) * y + z * z + c / (x + y);
```

### Evaluation
Pass inputs as `std::vector<double>` indexed by `var(i)`:

```cpp
double v = eval(f, {2.4, 6.0, 1.1});
```

---

## 2) Automatic Differentiation

Prefer reverse‑mode via the Tape backend at runtime. Symbolic `diff()` remains available via `et/expr.hpp` for compile‑time expressions when needed, but the default path is AST→Tape.

---

## 3) Simplification (AST)

Use the AST normalization and rewrite passes:
- `normalize(e)`: recursively normalizes and folds constants (`normalize_ast.hpp`).
- `rewrite_fixed_point(e)`: applies AST-native algebraic rules to a fixed point (`rewrite_ast.hpp`).

Example:

```cpp
#include "et/ast.hpp"
#include "et/normalize.hpp"
#include "et/rewrite_ast.hpp"

auto x = et::var(0), y = et::var(1);
et::Expr f = et::sin(x)*y + et::lit(0.0);
et::Expr fs = et::rewrite_fixed_point(f); // folds +0 and normalizes
```

---

## 4) Backends & Compilation (AST)

Vibex is backend-agnostic. A backend provides a few methods and Vibex calls them via `compile_runtime(…, backend)` or the AST CSE variants. A minimal backend API looks like:

```cpp
struct MyBackend {
  using result_type = /* handle/id/type you use to refer to compiled nodes */;

  // Emit a variable by runtime index (maps to input slot `idx`)
  result_type emitVar(std::size_t idx);

  // Emit a constant literal
  result_type emitConst(double);

  template <class Op, class... Hs> // Hs: result_type produced for children
  result_type emitApply(Op, Hs...);
};
```

### Generic compilation

```cpp
MyBackend b;
auto h = compile_runtime(f, b); // returns MyBackend::result_type
```

### CSE variants (AST)
- **Hashed structural CSE**: `compile_cse(f, backend)`
- **String-key CSE** (Tape backend): `compile_hash_cse(f, tape_backend)`

```cpp
auto h1 = compile_cse(f, b);
auto h2 = compile_hash_cse(f, tape_backend);
```

---

## 5) Tape backend (forward eval + reverse VJP)

The provided `TapeBackend` builds a compact instruction tape you can execute. It’s useful for:
- **Runtime evaluation** of expressions (fast, vector-free)
- **Reverse-mode gradients** via VJP (vector–Jacobian product)

### Building a tape

```cpp
TapeBackend tb(arity);
int root = compile_runtime(f, tb);   // or compile_cse(), compile_hash_cse()
tb.tape.output_id = root;
```

Under the hood:
- `emitVar<T>(idx)` registers a “slot” that will read from input vector position `idx`, cast to `T`.
- `emitConst<T>` registers a constant slot.
- `emitApply(Op{}, ...)` emits an opcode + child indices.

### Evaluate forward

```cpp
std::vector<double> inputs = {2.4, 6.0, 1.1}; // x,y,z
double out = tb.tape.forward(inputs);
```

### Reverse-mode (Backward)

`backward(inputs)` back-propagates to produce partials wrt inputs:

```cpp
std::vector<double> grad = tb.tape.backward(inputs);
// grad[0] = d f / d x  at inputs
// grad[1] = d f / d y
// grad[2] = d f / d z
```

> Why both symbolic `diff` and tape VJP?  
> - Symbolic `diff` gives you a new **expression** (great for further algebra, codegen).  
> - Tape VJP gives you a **number** quickly at runtime (great for optimization loops).

---

## 6) Torch JIT (TorchScript) export

You can build a backend that emits **TorchScript IR** (or a scripted `Module`) instead of a tape. The shape mirrors `TapeBackend`, but the `result_type` is a Torch handle (e.g. `torch::jit::Value*` / node handles depending on API version).

### Sketch: Torch backend

```cpp
struct TorchBackend {
  using result_type = torch::jit::Value*; // example

  torch::jit::Graph graph;
  torch::jit::Block* block;

  template <class T>
  result_type emitVar(std::size_t idx) {
    // Map runtime variable index -> graph input idx; insert casts as needed
  }

  template <class T>
  result_type emitConst(const Const<T>& c) {
    // Insert constant node
  }

  template <class Op, class... Hs>
  result_type emitApply(Op, Hs... hs) {
    // Map Op to corresponding Torch op:
    // AddOp -> aten::add, MulOp -> aten::mul, SinOp -> aten::sin, etc.
  }
};
```

Usage:

```cpp
TorchJITBackend tb(arity);
auto out = compile_runtime(f, tb);
// Optionally traverse tb.g to inspect prims and aten ops
```

**Type notes:**  
- Our variables are generic numeric types. On export, pick a canonical dtype (e.g., `float`), and cast differing `value_type`s where needed.  
- Multi-argument call sites: map `Var<_,I>` to Torch input `I`.  

**Adding new ops:**  
- Add a new op tag `struct MyOp { static arity; eval; d<I>(children...); }`  
- Add a Torch mapping to `emitApply(MyOp{}, ...)`

---

## 7) Control Flow (gated)

Control-flow and predicates are available behind `ET_ENABLE_CONTROL_FLOW`. Define it at compile time for targets that use them (examples/tests do this via target_compile_definitions).

- Comparisons: `<`, `<=`, `>`, `>=`, `==`, `!=` produce a boolean-like node (internally represented as numeric 0/1 for runtime eval).
- Logical not: `!cond`.
- Conditionals:
  - `If(cond, then_expr, else_expr)`: control-flow branch. Only the chosen branch is evaluated. Intended for scalar conditions; do not commute/normalize across it.
  - `Select(mask, on_true, on_false)`: elementwise conditional (like NumPy/torch `where`). Both branches are conceptually present; applies broadcasting where defined.

AD semantics
- Conditions/masks are non-differentiable. No gradients flow into them.
- Symbolic diff mirrors the primal structure:
  - `d If(c, a, b) = If(c, d a, d b)`
  - `d Select(m, a, b) = Select(m, d a, d b)`

Torch lowering (when also compiled with `-DET_WITH_TORCH=ON`)
- Comparisons map to `aten::{lt,le,gt,ge,eq,ne}`.
- `!cond` maps to `aten::logical_not`.
- `If` maps to `prim::If` (use a scalar condition); `Select` maps to `aten::where` for elementwise masking.

See also: `CONTROL_FLOW.md` for a deeper design write-up and tape/Torch details.

### Counted Loops (AST)

Looping is available behind `ET_ENABLE_CONTROL_FLOW` and modeled as a structured, loop-carried form:

- Placeholders inside the body:
  - `iter()`: current iteration index (0-based)
  - `state(i)`: i-th carried value at the current iteration
- Builders:
  - `loop_for(K, n, {inits...}, {nexts...})`: K carried states; returns a loop node
  - `loop_out(J, loop)`: select the J-th final carried value from a loop

Example: running sum and Fibonacci

```cpp
using namespace et;
auto n = var(0);

// Running sum: s_{t+1} = s_t + t
Expr core = loop_for(1, n, { lit(0.0) }, { state(0) + iter() });
Expr sum  = loop_out(0, core); // value after n iterations

// Fibonacci via two carried states: (a,b) <- (b, a+b)
Expr fib = loop_for(2, n, { lit(0.0), lit(1.0) }, { state(1), state(0) + state(1) });
Expr aN  = loop_out(0, fib); // F(n)
Expr bN  = loop_out(1, fib); // F(n+1)
```

Semantics and AD
- The loop runs exactly `n` iterations (non-negative; fractional parts truncated during execution). No gradients flow through `n`.
- Tape VJP supports loops by reverse iterating the body VJP and propagating into init state and inputs.

Torch lowering (when enabled)
- `LoopFor` lowers to `prim::Loop` with carried dependencies; `Out<J>` lowers to indexing into the loop’s carried outputs.
- `Iter()` maps to the loop’s iteration index (cast to a Tensor); `State<I>()` maps to the I-th carried block input.

### Torch JIT (AST path)
Lower an AST directly to a TorchScript graph using `TorchJITBackend`:

```cpp
#ifdef ET_WITH_TORCH
#  include "et/ast.hpp"
#  include "et/compile.hpp"
#  include "et/torch_jit_backend.hpp"
  auto x = et::var(0);
  et::Expr expr = et::Select(x > et::lit(0.0), x + et::lit(1.0), x - et::lit(1.0));
  et::TorchJITBackend JB(1);
  auto out = et::compile_runtime(expr, JB);
  JB.g.registerOutput(out);
  std::cout << JB.g.toString() << "\n";
#endif
```



## 7) Extending Vibex with new operations

Add a tag with:
- `arity` (static)
- `eval(...)` for runtime evaluation
- `template <std::size_t I> static auto d(children...)` for symbolic derivative

Example (softplus):

```cpp
struct SoftplusOp {
  static constexpr std::size_t arity = 1;

  template <class A>
  static constexpr auto eval(A&& a) {
    using std::log, std::exp;
    return log(1 + exp(std::forward<A>(a)));
  }

  template <std::size_t I, class X>
  static auto d(const X& x) {
    using std::exp;
    auto dx = diff(x, std::integral_constant<std::size_t, I>{});
    // d/dx softplus(x) = 1 / (1 + exp(-x)) * dx
    auto negx = Apply<NegOp, X>(x);
    auto sig  = Apply<DivOp, Const<double>, Apply<AddOp, Const<double>, Apply<ExpOp, decltype(negx)>>>(
                  lit(1.0), Apply<AddOp, Const<double>, Apply<ExpOp, decltype(negx)>>(
                              lit(1.0), Apply<ExpOp, decltype(negx)>(negx)));
    return Apply<MulOp, decltype(sig), decltype(dx)>(sig, dx);
  }
};

// sugar
template <class A, std::enable_if_t<is_node<std::decay_t<A>>::value, int> = 0>
constexpr auto softplus(A a) { return Apply<SoftplusOp, std::decay_t<A>>(std::move(a)); }
```

Update your backends’ `emitApply` to recognize `SoftplusOp`.

---

## 7) Rewrite and Optimize

The AST rewrite operates over a normalized AST (AC flattening/sorting for `Add`/`Mul`). For sum-like uniformity, `Sub(a,b)` is normalized to `Add(a, Neg(b))` internally.

- Fixed-point rewrite: `et::rewrite_fixed_point(expr)` (built-in rules for algebraic identities such as log∘exp, trig simplifications, like-term merging, factoring, etc.).
- Examples: `examples/08_rewrite_rules.cpp` and `examples/09_rewrite_nested.cpp`.

## 8) Practical tips & gotchas

- **Constrained operators**: Our `operator+/-/*///sin/cos/...` only participate when at least one operand is an ET node; this prevents hijacking standard operators (e.g., `iterator - int` inside `<vector>`).
- **Heterogeneous call args**: Each `Var<T,I>` casts its argument to `T`. If you pass unusual numeric types, make sure the cast is valid.
- **Simplify**: We only fold constants known on both sides. Neutral-element rewrites (like `x + 0`) are omitted to keep return types stable. If you want aggressive algebra, we can switch to NTTP constants later.
- **Template depth**: With the fixed `diff` sugar and careful `d<I>` implementations, default depth is fine. If you add extremely nested ops, `-ftemplate-depth=2000` is a safe global fallback.
- **CSE choice**:  
  - `compile_cse_ast` performs CSE on the AST using a structural hash with collision-checked keys (fast, general).  
  - `compile_hash_cse_ast` uses canonical string keys (simple and robust, a bit heavier).
- **Rewrite normalization**: Matching happens after AC normalization; subtraction is represented as `Add(..., Neg(...))` unless you denormalize back.
- **Torch op mapping**: Some ops may require broadcasting semantics; decide whether your graph should “scalarize” or broadcast to match tensor shapes.

---

## 9) Tiny end-to-end example (AST)

```cpp
#include "et/ast.hpp"
#include "et/compile_hash_cse.hpp"
#include "et/tape_backend.hpp"

using namespace et;

int main() {
  auto x = et::var(0), y = et::var(1), z = et::var(2);
  et::Expr f = et::sin(x) * y + z * z;

  et::TapeBackend tb(3);
  int root = et::compile_hash_cse(f, tb);
  tb.tape.output_id = root;

  std::vector<double> in = {2.4, 6.0, 1.1};
  double val = tb.tape.forward(in);        // f(x,y,z)
  auto grad = tb.tape.backward(in);        // ∂f/∂(x,y,z)
  (void)val; (void)grad;
}
```
