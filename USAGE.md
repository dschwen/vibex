# Vibex User Guide

This is a quick, practical tour of the “user-facing” surface of Vibex: building expressions, evaluating, differentiating, simplifying, compiling to backends (tape), running CSE, and exporting to TorchScript.

---

## 1) Core Concepts & Types

### Variables
Variables are **typed placeholders** addressed by index in the call operator.

```cpp
using namespace et;

auto x = Var<double, 0>{};
auto y = Var<double, 1>{};
auto z = Var<double, 2>{};

// convenience: pack variables
auto xyz = Vars<double, 3>(); // std::tuple<Var<double,0>, Var<double,1>, Var<double,2>>
```

### Constants
Use `lit(value)` to inject numeric constants:

```cpp
auto c = lit(2.5); // Const<double>
```

### Building expressions
Use constrained operator overloads and function shorthands (only kick in when at least one operand is an ET node, so they don’t pollute normal code):

```cpp
auto f = sin(x) * y + z * z + c / (x + y);
```

### Evaluation
Call the expression like a function with **heterogeneous** arguments (C++ converts to the declared `value_type`s of the variables):

```cpp
double v = f(2.4, 6, 1.1); // x=2.4 (double), y=6 (int), z=1.1 (double)
```

There’s also a helper:

```cpp
auto v2 = evaluate(f, 2.4, 6, 1.1);
```

---

## 2) Automatic Differentiation (symbolic)

### Single partial
You can differentiate by index or, more pleasantly, by variable:

```cpp
// by variable (index deduced)
auto dfx = diff(f, x);
auto dfy = diff(f, y);
auto dfz = diff(f, z);

// by index
auto dfx2 = diff(f, std::integral_constant<std::size_t, 0>{});
```

> Note: the “diff recursion” bug was fixed by removing a recursive trailing return type on the sugar overload. You shouldn’t need to tweak template depth flags.

### Gradient
Works with either separate vars or a tuple:

```cpp
auto g1 = grad(f, x, y, z);  // tuple of 3 ET expressions
auto g2 = grad(f, xyz);      // same
```

You can evaluate gradients like expressions:

```cpp
auto [gx, gy, gz] = g1;
auto gx_val = gx(2.4, 6, 1.1);
```

---

## 3) Simplification

The `simplify` pass performs:
- recursive simplification of children
- **pure constant folding** (operations where both sides are `Const<T>` or unary const)

We intentionally **don’t** do value-dependent single-sided rewrites (like `x * 1 → x`) because that introduces mixed return types with `auto` and breaks deduction. You get predictable, type-stable simplifications:

```cpp
auto g = simplify(dfx);      // returns an ET node (possibly with Const folded)
auto v = g(2.4, 6, 1.1);
```

For aggressive canonicalization and algebraic identities, use the rewrite engine:

```cpp
auto r = rewrite(f, default_rules());
```

It normalizes associative/commutative ops and applies pattern-based rules
like neutral elements and trig identities (see `DESIGN_REWRITE.md` and
`examples/08_rewrite_rules.cpp`).

---

## 4) Backends & Visitors

Vibex is backend-agnostic. A backend provides a few methods and Vibex calls them via `compile(…, backend)` or the CSE variants. A minimal backend API looks like:

```cpp
struct MyBackend {
  using result_type = /* handle/id/type you use to refer to compiled nodes */;

  // Emit a variable by runtime index (maps to input slot `idx`)
  template <class T>
  result_type emitVar(std::size_t idx);

  template <class T>
  result_type emitConst(const Const<T>&);

  template <class Op, class... Hs> // Hs: result_type produced for children
  result_type emitApply(Op, Hs...);
};
```

### Generic compilation

```cpp
MyBackend b;
auto h = compile(f, b); // returns MyBackend::result_type
```

### CSE variants
- **Structural CSE**: memoizes by structural string key
- **Hashed CSE**: memoizes by a structural hash with lazy key materialization for collisions

```cpp
auto h1 = compile_cse(f, b);
auto h2 = compile_hash_cse(f, b);
```

---

## 5) Tape backend (forward eval + reverse VJP)

The provided `TapeBackend` builds a compact instruction tape you can execute. It’s useful for:
- **Runtime evaluation** of expressions (fast, vector-free)
- **Reverse-mode gradients** via VJP (vector–Jacobian product)

### Building a tape

```cpp
TapeBackend tape;
auto root = compile_hash_cse(f, tape);   // or compile(), compile_cse()
```

Under the hood:
- `emitVar<T>(idx)` registers a “slot” that will read from input vector position `idx`, cast to `T`.
- `emitConst<T>` registers a constant slot.
- `emitApply(Op{}, ...)` emits an opcode + child indices.

### Evaluate forward

```cpp
std::vector<double> inputs = {2.4, 6.0, 1.1}; // x,y,z
double out = tape.forward(root, inputs);
```

### Reverse-mode (Backward)

`backward(inputs)` back-propagates to produce partials wrt inputs:

```cpp
std::vector<double> grad = tape.backward(inputs);
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
TorchBackend tb;
auto out = compile_hash_cse(f, tb);

// Wrap graph as a scripted function/module as you prefer,
// then pass Tensors at runtime.
```

**Type notes:**  
- Our variables are generic numeric types. On export, pick a canonical dtype (e.g., `float`), and cast differing `value_type`s where needed.  
- Multi-argument call sites: map `Var<_,I>` to Torch input `I`.  

**Adding new ops:**  
- Add a new op tag `struct MyOp { static arity; eval; d<I>(children...); }`  
- Add a Torch mapping to `emitApply(MyOp{}, ...)`

---

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

The rewrite engine works over a normalized runtime AST with associative/commutative (AC) flattening and sorting for `Add`/`Mul`. For sum-like uniformity, `Sub(a,b)` is normalized to `Add(a, Neg(b))`. After rewriting to a fixed point, you can denormalize back to `Sub` in the two-term case for prettier output.

- Rules: see `include/et/rules_default.hpp`.
- Fixed-point rewrite: `rewrite_fixed_point(graph, rules)` or `rewrite_expr(expr, rules)`.
- Convenience: `optimize(expr, rules)` or `optimize(expr)` (uses default rules).
  - Flow: normalize → rewrite* → normalize → denormalize_sub.
  - Examples: `examples/08_rewrite_rules.cpp` (optimize), `examples/09_rewrite_nested.cpp` (per-pass + Pretty).

Quick example:

```cpp
#include "et/optimize.hpp"

auto [x] = et::Vars<double,1>();
auto e = et::sin(x)*et::sin(x) + et::cos(x)*et::cos(x) + (et::lit(2.0)*x + et::lit(3.0)*x);

// optimize() runs: normalize → rewrite* → normalize → denormalize_sub
et::RGraph g = et::optimize(e);
std::cout << r_to_string(g) << "\n"; // Already pretty (Sub restored where applicable)
```

## 8) Practical tips & gotchas

- **Constrained operators**: Our `operator+/-/*///sin/cos/...` only participate when at least one operand is an ET node; this prevents hijacking standard operators (e.g., `iterator - int` inside `<vector>`).
- **Heterogeneous call args**: Each `Var<T,I>` casts its argument to `T`. If you pass unusual numeric types, make sure the cast is valid.
- **Simplify**: We only fold constants known on both sides. Neutral-element rewrites (like `x + 0`) are omitted to keep return types stable. If you want aggressive algebra, we can switch to NTTP constants later.
- **Template depth**: With the fixed `diff` sugar and careful `d<I>` implementations, default depth is fine. If you add extremely nested ops, `-ftemplate-depth=2000` is a safe global fallback.
- **CSE choice**:  
  - `compile_cse` compares structural **strings** (robust, a bit heavier).  
  - `compile_hash_cse` is faster but uses hashing plus lazy structural keys only on collisions.
- **Rewrite normalization**: Matching happens after AC normalization; subtraction is represented as `Add(..., Neg(...))` unless you denormalize back.
- **Torch op mapping**: Some ops may require broadcasting semantics; decide whether your graph should “scalarize” or broadcast to match tensor shapes.

---

## 9) Tiny end-to-end example

```cpp
#include "et/expr.hpp"
#include "et/simplify.hpp"
#include "et/tape_backend.hpp"
#include "et/compile_hash_cse.hpp"

using namespace et;

int main() {
  auto [x, y, z] = Vars<double, 3>();

  auto f = sin(x) * y + z * z;
  auto dfx = simplify(diff(f, x)); // cos(x)*y

  TapeBackend tape;
  auto h = compile_hash_cse(dfx, tape);

  std::vector<double> in = {2.4, 6.0, 1.1};
  double val = tape.forward(h, in);        // numeric value of d f / d x at inputs
  auto grad = tape.backward(in);           // gradient wrt (x,y,z) of d f / d x (here mostly 0 except x chain)

  (void)val; (void)grad;
}
```
