# Refactor Plan: Remove Compile-Time Derivatives and Adopt a Runtime AST

This document proposes a staged refactor to migrate from template-heavy
compile‑time Expression Templates (ET) and symbolic `diff()` to a simpler,
explicit runtime Abstract Syntax Tree (AST) with operator overloading that
constructs AST nodes directly. The runtime AST will be the single source of
truth for evaluation, compilation, and AD (via Tape). No more template magic.

The goals are to:
- Replace ET node types (e.g., `Apply<Op,...>`, `Var<T,I>`) with a class
  hierarchy rooted at a common `Node` base class.
- Replace the current `diff()` symbolic system: gradients are obtained via the
  Tape backend (VJP) compiled from the AST.
- Keep user ergonomics: overloaded operators and math helpers build ASTs.
- Ensure AST node lifetimes are safe (no temporaries getting destroyed) and
  assignable as class members (e.g., via `std::shared_ptr` and a light `Expr`
  handle type).
- Minimize disruption by providing a bridging layer so existing backends can
  still operate (e.g., `compile_runtime` from AST → RGraph or directly from AST).

## Current Status (Aug 2025)

- Runtime AST is implemented (`include/et/ast.hpp`) with `Expr` and nodes for arithmetic, math, comparisons, control flow (`If`/`Select`), and loops (`Iter`, `StateRead`, `LoopFor`, `LoopOut`).
- Direct AST→Tape lowering exists (`include/et/compile_ast.hpp`), including control flow and loops. Tape reverse‑mode through loops is implemented.
- Normalization over AST (`include/et/normalize_ast.hpp`).
- AST rewrite engine (`include/et/rewrite_ast.hpp`) with core algebraic rules and additional ones (exp product combine; integer‑exponent power merging; common‑denominator combining).
- AST CSE:
  - Structural‑hash CSE (`include/et/compile_cse_ast.hpp`) with collision‑checked keys; supports control flow and loops.
  - String‑key CSE (`include/et/compile_hash_cse_ast.hpp`) with control flow and loops.
- Torch JIT backend gained dynamic loop emitters so AST compilers can lower loops to prim::Loop (`emitIter`, `emitStateRead`, `emitLoopFor`, `emitLoopOut`).
- Examples migrated to AST CSE: `examples/06_ops_and_cse.cpp`, `examples/07_hash_cse.cpp`.
- Tests migrated to AST, including loop CSE; CI is green.
- Legacy CSE headers removed: `include/et/compile_cse.hpp`, `include/et/compile_hash_cse.hpp`.

Next steps
- Expand rewrite rules and migrate any remaining RGraph‑based passes/tests to AST.
- Improve hashing and canonicalization (e.g., optional hash‑consing, stronger keys).
- Documentation sweep to ensure USAGE/CONTROL_FLOW reflect AST compilers and loop builders (`loop_for`, `loop_out`, `state`, `iter`).

## Scope & Non‑Goals

- In scope:
  - New runtime AST class hierarchy with `Node` base and derived node types.
  - `Expr` handle type with overloaded operators to build ASTs.
  - Bridges from new AST to existing backends (runtime eval, Tape, CSE, etc.).
  - Removal of template‑based `diff()` and compile‑time loop AD.
  - Test conversion away from compile‑time derivatives.

- Out of scope (short term):
  - Global hash‑consing or structural sharing beyond `shared_ptr` reuse.
  - E-graph integration (can be layered later).
  - Large algorithmic changes to optimizers/rewrite engines beyond adapting to
    the new AST.

## High‑Level Architecture

- `Expr`: small, copyable handle (value type) that owns a `std::shared_ptr<Node>`.
- `Node` (abstract): common base class with virtuals for core behaviors.
  - Derived: `ConstNode`, `VarNode`, `AddNode`, `SubNode`, `MulNode`, `DivNode`,
    `NegNode`, `PowNode`, `SinNode`, `CosNode`, `ExpNode`, `LogNode`, `SqrtNode`,
    `TanhNode`.
  - Optional control flow: `IfNode`, `SelectNode`.
  - Optional loop IR: `LoopForNode`, `LoopOutNode`, `IterNode`, `StateReadNode`.

- Operator overloads and function helpers construct `Expr` by creating the
  appropriate `Node` subtype and wiring children.

- Backends consume AST:
  - Evaluation: recursively evaluate AST (memoized) given an input vector.
  - Compilation: lower AST to Tape backend (or retain the current `RGraph` and
    add AST→RGraph conversion).

- Memory/Lifetime: use `std::shared_ptr<Node>`; nodes are immutable; children
  are `shared_ptr<Node>`. `Expr` can be assigned to class members safely.

## API Sketch

```cpp
// include/et/ast.hpp
namespace et {

struct Node {
  virtual ~Node() = default;
  virtual double eval(const std::vector<double>& inputs) const = 0;
  // Optional virtuals for pretty-print and structural hashing
  virtual void collect_children(std::vector<std::shared_ptr<Node>>& out) const = 0;
};

struct Expr {
  std::shared_ptr<Node> n;
  Expr() = default;
  explicit Expr(std::shared_ptr<Node> p) : n(std::move(p)) {}
  // Convenience: implicit bool to check validity
  explicit operator bool() const { return static_cast<bool>(n); }
};

struct ConstNode : Node {
  double value;
  explicit ConstNode(double v) : value(v) {}
  double eval(const std::vector<double>&) const override { return value; }
  void collect_children(std::vector<std::shared_ptr<Node>>& ) const override {}
};

struct VarNode : Node {
  std::size_t index;
  explicit VarNode(std::size_t i) : index(i) {}
  double eval(const std::vector<double>& inputs) const override { return inputs[index]; }
  void collect_children(std::vector<std::shared_ptr<Node>>& ) const override {}
};

struct AddNode : Node {
  std::shared_ptr<Node> a, b;
  AddNode(std::shared_ptr<Node> x, std::shared_ptr<Node> y) : a(std::move(x)), b(std::move(y)) {}
  double eval(const std::vector<double>& in) const override { return a->eval(in) + b->eval(in); }
  void collect_children(std::vector<std::shared_ptr<Node>>& out) const override { out.push_back(a); out.push_back(b); }
};

// ... similarly: SubNode, MulNode, DivNode, NegNode, PowNode, SinNode, CosNode, ExpNode, LogNode, SqrtNode, TanhNode

// Syntactic sugar: literals and variables
inline Expr lit(double v) { return Expr{std::make_shared<ConstNode>(v)}; }
inline Expr var(std::size_t i) { return Expr{std::make_shared<VarNode>(i)}; }

// Operators: only on Expr
inline Expr operator+(const Expr& x, const Expr& y) { return Expr{std::make_shared<AddNode>(x.n, y.n)}; }
inline Expr operator-(const Expr& x, const Expr& y) { return Expr{std::make_shared<SubNode>(x.n, y.n)}; }
inline Expr operator*(const Expr& x, const Expr& y) { return Expr{std::make_shared<MulNode>(x.n, y.n)}; }
inline Expr operator/(const Expr& x, const Expr& y) { return Expr{std::make_shared<DivNode>(x.n, y.n)}; }
inline Expr operator-(const Expr& x) { return Expr{std::make_shared<NegNode>(x.n)}; }

// Math
inline Expr sin(const Expr& x) { return Expr{std::make_shared<SinNode>(x.n)}; }
// ... etc for cos/exp/log/sqrt/tanh/pow

// Optional: control flow and loops (retained but now purely runtime)
// struct IfNode, SelectNode, IterNode, StateReadNode, LoopForNode, LoopOutNode

} // namespace et
```

### Evaluation

- Implement a non-virtual helper to eval with memoization:
  - Topologically compute values with a node-id assignment or per-call cache
    keyed by node pointer address.
  - Alternatively, keep the `RGraph` evaluator and consume the AST via a
    conversion pass.

### Compilation (Tape Backend)

Two options:
- O1 (bridging): Implement AST→RGraph conversion; reuse existing
  `compile_runtime(RGraph, TapeBackend)` unchanged.
- O2 (direct): Implement `compile_runtime(Expr, TapeBackend)` that walks the AST
  and calls `TapeBackend::emit*` methods. Prefer O2 (fewer layers), but O1 can
  accelerate the migration.

## Migration Steps (Milestones)

1) Introduce the runtime AST types
- Add `include/et/ast.hpp` with `Node` base, derived nodes for arithmetic and
  basic math, `Expr` handle, `lit()` and `var()`.
- Implement basic operators and math helpers (no templates).
- Add small unit tests for construction and `eval()`.

2) Bridge to existing infrastructure
- Add `ast_to_rgraph(const Expr&) -> RGraph` pass in a new header
  `include/et/ast_to_runtime.hpp` mirroring old `compile_to_runtime` logic.
- Implement `eval(const Expr&, inputs)` either directly or via
  `eval(ast_to_rgraph(expr), inputs)`.
- Implement `compile_runtime(const Expr&, TapeBackend&)` by either:
  - lowering AST→RGraph then calling existing `compile_runtime`, or
  - emitting directly to Tape (preferred once stable).

3) Replace front-end usage
- Create a new `Vars` helper API that returns runtime `Expr` variables:
  - `auto x = var(0); auto y = var(1);`
- Update examples and tests to use `Expr` instead of the template ET API.
- Keep the old ET headers available under a CMake option (default OFF) to ease
  migration, but do not use them in tests.

4) Remove compile‑time derivatives and ETs
- Delete (or gate off) template `diff()` and compile‑time loop AD in
  `include/et/expr.hpp`.
- Remove `Apply<Op,...>`, `Var<T,I>`, `Const<T>`, and the related template SFINAE
  operator overloads.
- Update CMake defaults to disable ET‑`diff()` headers completely.

5) Adapt rewrites/normalization/simplify (optional in phase 1)
- Refactor normalization and rewrite passes to operate on the AST. Start with
  simple algebraic folds (constant folding, `x+0`, `x*1`, etc.).
- If keeping `RGraph` temporarily, you can continue running those passes there
  and only refactor later.

6) Update tests incrementally
- Convert all tests to build expressions with `Expr` and assert results from
  `eval()` and from Tape gradients.
- Remove or gate tests that checked compile‑time templates or compile‑time loop
  AD. Prefer Tape for gradients.

7) Cleanup and deprecation
- Remove the ET headers entirely after all users and tests are migrated.
- Simplify CMake flags (drop ET‑related options).
- Update README/USAGE to document the new API.

## Node Set (Phase 1)

- Arithmetic: Const, Var, Add, Sub, Mul, Div, Pow, Neg
- Math: Sin, Cos, Exp, Log, Sqrt, Tanh
- Control Flow (if you still want): If, Select, Lt/Le/Gt/Ge/Eq/Ne, Not
- Loops (if you still want runtime loops): Iter, StateRead, LoopFor, LoopOut

You can defer control flow and loops until after the arithmetic subset is
stable; Tape gradients for loops will continue to work once their AST nodes are
introduced and lowered.

## Tape Integration Details

- Prefer a direct `compile_runtime(const Expr&, TapeBackend&)` visitor that
  switches on dynamic type (via `dynamic_cast`, `typeid`, or a virtual visitor
  method on `Node`).
- Map nodes to Tape kinds one-to-one (similar to current RGraph mapping).
- For n-ary Add/Mul, either build binary pairs or introduce `NaryAddNode` and
  emit a reduce.

## Memory & Lifetime

- All nodes are owned by `std::shared_ptr<Node>`; children are shared_ptrs.
- Nodes are immutable (their child pointers never change after construction).
- `Expr` is a lightweight handle; can be copied or stored in class members.
- Avoid cycles: AST is a DAG.
- Optional: introduce a `make_node<T>(args...)` factory if you later want to
  swap ownership semantics (e.g., to `intrusive_ptr`).

## Performance Considerations

- Construction overhead is acceptable for a first pass. Later, consider:
  - N-ary nodes for Add/Mul to reduce depth.
  - Simple CSE during construction (hash-consing) gated behind an option.
  - Cheap `eval()` memoization cache keyed by node address.

## Backward Compatibility & Removal

- Keep the current ET entrypoints behind a CMake option during the transition.
- Provide a migration cheatsheet in README:
  - Before: `auto [x,y] = Vars<double,2>(); auto f = sin(x)+y*y;` etc.
  - After: `auto x = var(0), y = var(1); auto f = sin(x) + y*y;`
- Remove ET templates and compile‑time diff after all tests/examples migrate.

## Milestone Breakdown

1. AST Core (Expr + Node hierarchy + operators) — 1–2 days
2. AST→Tape compilation (direct) — 1 day
3. AST eval (direct or via RGraph bridge) — 0.5–1 day
4. Update examples 01/02/03 and Tape example 04 — 0.5 day
5. Convert tests to Expr (arithmetic, ops, simplify, runtime) — 1–2 days
6. Gate or remove ET/diff tests (loop symbolic) — same PR as step 5
7. Remove ET headers and clean CMake — 0.5 day
8. Optional: port normalize/rewrite to AST or keep bridging temporarily — 1–2 days

## Open Questions

- Do we keep `RGraph` long-term, or migrate all backends to consume the AST
  directly? Recommendation: converge on AST→Tape direct (fewer layers), keep a
  small RGraph bridge to ease the transition and for coverage reports already
  wired to RGraph.
- Do we keep control-flow and loops in phase 1? If yes, add node types and
  direct Tape lowering; skip symbolic aspects entirely.
- Do we want minimal CSE on construction? Can be added later as a layer.

## Example: End-State User Code

```cpp
using namespace et;

int main() {
  auto x = var(0), y = var(1);
  Expr f = pow(sin(x) + cos(y), lit(2.0))
         + log(exp(x*y))
         + sqrt(x + lit(3.0))
         + tanh(-y)
         + (x / (y + lit(2.0)));

  // Evaluate
  double v = eval(f, {0.7, 1.3});

  // Compile to Tape and run forward/backward
  TapeBackend tb(2);
  int root = compile_runtime(f, tb); // direct AST→Tape
  tb.tape.output_id = root;
  auto grad = tb.tape.backward({0.7, 1.3});

  // Store Expr in members safely (shared_ptr-backed)
  struct M { Expr g; } m{f};
}
```

## Summary

This refactor replaces template-heavy ETs and compile-time `diff()` with a
clean, explicit, runtime AST plus operator overloading that creates nodes
directly. It simplifies the mental model, removes fragile template machinery,
and unifies execution paths through the Tape backend for AD. The staged plan
minimizes disruption and keeps CI green throughout.
