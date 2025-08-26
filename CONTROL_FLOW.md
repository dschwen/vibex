# Control Flow Design: Conditional and Loop Operators

Audience: contributors extending the ET with non‑algebraic control flow while keeping header‑only, constexpr‑friendly code, clean AD semantics, and existing backends (runtime, CSE, Tape, Torch JIT) intact.

Goals
- Add first‑class conditional and looping constructs to the runtime AST and ET surface.
- Keep expressions pure and deterministic; no side effects across branches/iterations.
- Integrate with normalization/simplify (constant folding, light algebraic pushing).
- Support compile‑time (symbolic) differentiation where sound; robust reverse‑mode (tape) differentiation at runtime.
- Lower to TorchScript prim::If and prim::Loop when Torch is enabled.

Non‑Goals (initial phase)
- General higher‑order lambdas/closures inside the ET.
- Effectful control flow or data‑dependent reordering through AC nodes.
- Full symbolic differentiation through arbitrary data‑dependent loops.

## 1. Proposed Operators

We introduce two flow operators and one auxiliary elementwise operator that unblocks many use‑cases without full loops.

1) IfThenElse (If)
- Tag: `IfOp` (PascalCase tag, consistent with core ops)
- Signature: `If(cond: Bool, then: T, else_: T) -> T` (T must match in both branches)
- Semantics: evaluate `cond`; if true, evaluate `then`, else evaluate `else_`.
- Purity: only the chosen branch evaluates (short‑circuit); branches must be pure expressions.

2) Loop (While/For)
- Tag: `LoopOp`
- Minimal core form: `Loop(max_iter: Int, init_state: S, cond: State -> Bool, body: State -> State) -> S`.
- In our first phase (no lambdas), we provide a realizable subset via a state placeholder and explicit subgraphs in the runtime AST (details below). We also provide a simpler counted form:
  - `ForN(n: Int, init_state: S, body: State -> State) -> S` as a specialization where the loop runs exactly `n` times.

3) Select (Elementwise conditional)
- Tag: `SelectOp`
- Signature: `Select(mask: BoolLike, on_true: T, on_false: T) -> T` with broadcasting rules where defined.
- Semantics: elementwise conditional (like NumPy/torch `where`). Unlike `IfOp`, both branches are conceptually evaluated but masked; useful for tensor programs and always differentiable w.r.t. data (not the mask).

Why both `IfOp` and `SelectOp`?
- `IfOp` models control flow with mutually exclusive branch execution (maps to prim::If, structured loops, and enables CSE isolation per branch). `SelectOp` is algebraic and fits existing normalization and AD better for elementwise cases.

## 2. AST Representation

Runtime AST (`include/et/runtime_ast.hpp`)
- Extend `enum class RKind` with: `If`, `Loop`, `Select`.
- `RNode` payloads:
  - `If`: fields `{cond: id, then_: id, else_: id}`.
  - `Select`: `{mask: id, on_true: id, on_false: id}`.
  - `Loop`: `{max_iter: id /*const or var*/, init_state: id, cond_subgraph_root: id, body_subgraph_root: id}`.
    - The `cond_subgraph_root` and `body_subgraph_root` are roots of subgraphs that may reference a reserved loop‑state variable node `RKind::LoopState` (new node kind) to read the current state.
    - We avoid general lambdas by introducing a single `LoopState` read node (no write; the body result becomes the next state). This fits our existing index‑based var emission model and keeps loops first‑order.

ET surface (`include/et/expr.hpp`)
- Add tags `IfOp`, `LoopOp`, `SelectOp`, and `LoopState`.
- Provide helpers:
  - `If(cond, then_e, else_e)`
  - `Select(mask, a, b)`
  - `ForN(n, init_state, body_expr_with_state)` and `While(init_state, cond_expr_with_state, body_expr_with_state)` using `LoopState()` inside the sub‑expressions to read the carried state. Example:
    - `auto s0 = Var<double,0>{};
       auto body = Add(LoopState<double>(), Const(1.0));
       auto out = ForN(Const(10), s0, body); // s_{k+1} = s_k + 1`

Compile to runtime
- Extend `compile_to_runtime` to lower the new ET nodes to the `RGraph` forms above.
- For `While`, lower to `Loop` with both `cond` and `body` subgraphs.

Structural hashing/CSE
- Include kind and child ids for `If`, `Select` as usual.
- For `Loop`, include the triples `(max_iter, init_state, cond_root, body_root)`.
- For `LoopState`, hash only by kind+dtype (there’s only one logical read in scope; references become equivalent within the same subgraph during lowering).

## 3. Normalization & Simplify

General rules
- Do not reorder across `If`/`Loop` boundaries.
- Propagate and fold constants aggressively where safe.

IfOp
- Constant fold: `If(Const(true), a, b) -> a`, `If(Const(false), a, b) -> b`.
- Common subexpr: if `a == b` structurally, `If(c, a, a) -> a`.
- Push through pure unary/binary ops when both branches share an outer op and it’s total:
  - `Op(If(c, a, b)) -> If(c, Op(a), Op(b))` for ops known pure and shape‑preserving (guarded using traits).
- Dead branch simplify: if a guard can statically prove `c` is const, drop the other branch; if `c` depends only on constants/reified parameters, we can pre‑evaluate.

SelectOp
- Follows existing broadcast/elementwise rules (see MIXED_TYPES.md). const fold `Select(true, a, b)->a`, `Select(false, a, b)->b`.
- Algebraic pushing identical to `IfOp` but elementwise; `Select` commutes with Add/Mul under broadcasting when masks are shape compatible.

LoopOp
- Zero‑trip: `ForN(Const(0), s0, body) -> s0`.
- One‑trip: `ForN(Const(1), s0, body) -> substitute(LoopState:=s0) in body`.
- Unroll small constants: for small `n` (configurable threshold), unroll into nested substitutions; guard against code blowup.
- While: if `cond` is `false` at `init_state` and independent of `LoopState`, fold to `init_state`.

Rewrite engine integration
- Patterns may target `If`/`Select` for the constant/identical‑branch cases above.
- Do not flatten/sort across `If`/`Loop`.

## 4. Differentiation

Assumptions
- We never differentiate w.r.t. boolean conditions. `Bool` is non‑differentiable.
- All branches/bodies are pure; gradients flow only through executed paths (for control flow) or masked paths (for `Select`).

4.1 Symbolic/compile‑time AD (tests under `tests/test_ops.cpp`, `tests/test_binary.cpp`, `tests/test_rules_*.cpp` patterns)
- IfOp
  - If `cond` is constant: differentiate the surviving branch normally.
  - If `cond` is symbolic but independent of the differentiation variables: treat it as constant selector; derivative is `If(cond, d(then), d(else))` (rule mirrors primal).
  - If `cond` depends on differentiation variables: we cannot take the derivative of the Heaviside/indicator in our algebra without distributions. We choose conservative behavior:
    - Default: treat `cond` as non‑diff; derivative ignores ∂cond and becomes `If(cond, d(then), d(else))`. This yields correct gradients almost everywhere and matches Tape AD behavior.
    - Optional future: add guarded rules to surface subgradients for common cases (e.g., `If(x>0, x, 0)` → ReLU with known derivative), but do this via dedicated ops like `ReluOp` or `MaxOp` rather than generic `If`.
- SelectOp
  - Treat mask as non‑diff; derivative mirrors primal elementwise: `d Select(m,a,b) = Select(m, d a, d b)`.
- LoopOp
  - For `ForN` with small constant `n`: unroll and differentiate each expansion.
  - General `Loop`/`While`: do not symbolically differentiate across unknown iteration counts. Either leave `d(Loop)` as a `Loop` over differentiated body and state (same structure), or require user to opt‑in via a trait `is_symbolically_diffable<Loop>` with bounded `max_iter`. Initial phase: gate symbolic differentiation of loops behind an explicit `ET_ENABLE_SYM_DIFF_LOOP` macro and unroll only for constants within a small threshold.

4.2 Reverse‑mode Tape AD (`include/et/tape_backend.hpp`)
- IfOp
  - Forward: evaluate `cond`. Evaluate only the chosen branch; record a tape marker `IfTaken{branch=Then|Else}`. Optionally record any needed intermediates from the chosen branch.
  - Backward: route adjoint to the chosen branch only; accumulate gradients; no gradient flows into `cond` (bool).
- SelectOp
  - Forward: compute `mask`, `a`, `b`, then `out = mask ? a : b` elementwise.
  - Backward: route adjoints elementwise: `ga += mask * gout`, `gb += (1-mask) * gout`. No grad into `mask`.
- LoopOp
  - Forward: push `LoopBegin` marker with loop‑carried state id; for each iteration, push a `LoopIter` marker capturing whatever the body needs in backward (body‑specific intermediates, or just the primal state if body recomputation is cheap); on exit push `LoopEnd{iters_executed}`.
  - Backward: pop `LoopEnd`, iterate backward over `iters_executed`, restoring per‑iter intermediates; apply body’s VJP to propagate adjoints from `state_{k+1}` to `state_k`. This is standard reverse‑mode through while‑loops.
  - Memory: allow a “checkpointing” policy: store full intermediates per iter or recompute body in backward based on a knob (macro or trait).

## 5. Backends

5.1 Runtime evaluator (`include/et/compile_runtime.hpp`)
- Add execution for `If`: evaluate `cond` → bool; branch accordingly.
- Add `Select`: elementwise dispatch with broadcasting when types support it.
- Add `Loop`: implement counted loops; for `While`, re‑evaluate `cond(state)` each iteration. The `LoopState` read pulls the current carried state from the evaluator’s loop frame.

5.2 CSE/Hash CSE compilers (`include/et/compile_cse.hpp`, `include/et/compile_hash_cse.hpp`)
- Treat `If`/`Select`/`Loop` as new op kinds in the visitors. For `If`/`Select`, dedupe identical subtrees as usual.
- For `Loop`, dedupe identical `cond`/`body` subgraphs reused in multiple callers; loop frames remain distinct per use site at runtime.

5.3 Tape backend (`include/et/tape_backend.hpp`)
- Extend `enum class Kind` with `If`, `Select`, `LoopBegin`, `LoopIter`, `LoopEnd`.
- Implement forward emission and reverse VJP routing per §4.2.
- Ensure variables are still emitted by runtime index (`emitVar<T>(std::size_t idx)`) and that loop state reads `LoopState` use a separate tape slot managed by the backend (not a user input index).

5.4 Torch JIT backend (`include/et/torch_jit_backend.hpp`)
- IfOp → `prim::If`
  - Build condition `Value* c`.
  - Create `then_block` and `else_block`; emit branch subgraphs; merge outputs into a single `Value* out`.
- SelectOp → `aten::where` (preferred) when mask and branches are tensor‑like; fallback to `prim::If` otherwise.
- LoopOp → `prim::Loop`
  - Torch’s loop takes trip count and condition as inputs and models loop‑carried dependencies as block inputs/outputs. Lower `ForN` by supplying trip count `n` and a constant `true` condition; propagate state as carried deps; update the condition each iter for `While` using the condensed subgraph rooted at `cond_root`.
- Variable emission remains by runtime index (`emitVar<T>(idx)`); loop state is a carried dependency in the prim::Loop and not a Var.

Gating
- All Torch code stays under `#if ET_WITH_TORCH` and examples/tests under `ET_BUILD_TORCH_*`.

## 6. Typing & Traits

- Add a `Bool` concept/type trait for condition nodes. `Bool` is non‑differentiable.
- Branch type unification: `If(cond, a, b)` requires `type(a)==type(b)`; `Select` uses broadcast rules (see MIXED_TYPES.md) and requires compatible dtypes.
- Add traits to query purity/totality for pushing ops through `If/Select` (e.g., `is_total_unary<Op,T>`).
- AD traits: `is_differentiable<Bool> = false`. Masks are non‑diff; gradients into masks are zero and not constructed.

## 7. Testing Plan

- Evaluation
  - `If` constant folding and branch selection.
  - `Select` elementwise with broadcasting.
  - `ForN` with simple affine body; `While` with early exit.
- Simplify/Normalize
  - `If(Const, a, b)` folding; `If(c, x, x) -> x`.
  - Unroll `ForN` for small `n`; zero/one‑trip.
- AD (symbolic)
  - `d/dx If(c, f(x), g(x))` where `c` is const vs depends on x (ensure no derivative of `c`).
  - `d/dx Select(m, f(x), g(x))` mirrors primal.
  - Unrolled `ForN` gradient matches numerical finite differences.
- Tape
  - Branching test where only chosen branch accumulates gradients.
  - Loop with recorded `iters_executed`; backward iterates in reverse.
- Torch (gated)
  - Graph contains `prim::If`/`prim::Loop`/`aten::where` as appropriate; run a few example executions if Torch is available.

## 8. Migration & Backward Compatibility

- Existing code unaffected until new ops are used.
- `SelectOp` may subsume some hand‑rolled max/min/ReLU patterns; we will not auto‑rewrite into `Select` unless rules are guarded and types support it.
- No changes to `emitVar<T>(idx)`; backends receive additional `emitApply` cases.

## 9. Implementation Sketch

Headers to touch
- `include/et/expr.hpp`: add op tags, `LoopState` node, helpers.
- `include/et/runtime_ast.hpp`: add `RKind::{If, Select, Loop, LoopState}` and node payloads.
- `include/et/compile_runtime.hpp`: evaluator for new nodes; loop frame with current state binding for `LoopState` reads.
- `include/et/simplify.hpp` and `include/et/normalize.hpp`: constant folding and small unrolls.
- `include/et/compile_cse.hpp`, `include/et/compile_hash_cse.hpp`: include new kinds in visitors and hashing.
- `include/et/tape_backend.hpp`: new tape kinds and VJP logic.
- `include/et/torch_jit_backend.hpp`: emit prim::If/Loop/aten::where.
- `include/et/rules_default.hpp`: add guarded rules for the simple folds/pushes.
- Examples: `examples/10_control_flow.cpp` (demo If/Select/ForN). Tests under `tests/` accordingly.

Key data structure additions (pseudocode)
```cpp
// runtime_ast.hpp
enum class RKind { /*...,*/ If, Select, Loop, LoopState };
struct RIf { int cond, then_, else_; };
struct RSelect { int mask, on_true, on_false; };
struct RLoop { int max_iter, init_state, cond_root, body_root; };
struct RNode {
  RKind kind;
  // union-like payload
};

// tape_backend.hpp (forward emission outline)
case If: {
  bool c = eval(cond);
  push(IfTaken{c});
  out = c ? eval(then_) : eval(else_);
}
case Loop: {
  push(LoopBegin{/*...*/});
  int it = 0;
  S state = eval(init_state);
  for (; it < max_iter && cond(state); ++it) {
    push(LoopIter{/*snapshot*/});
    state = body(state);
  }
  push(LoopEnd{it});
  return state;
}
```

## 10. Future Work

- Richer higher‑order bodies with multiple loop‑carried states (tuples) and multiple outputs; add lightweight tuple nodes to ET and RGraph.
- Peephole rewrites between `If` and algebraic ops (distributivity under guards) with soundness traits.
- Domain/guard tracking so we can justify turning `If(x>0, x, 0)` into `Relu(x)` and leverage known derivatives.
- Gradient checkpointing policies for loops; custom VJP hooks for user‑defined bodies.
- Pretty‑printer: add `to_string_pretty` support for If/Loop/Select nodes.

Scope Check
- This plan keeps core invariant: pure, value‑semantics nodes; header‑only; no RTTI/virtuals. Control flow lives as explicit nodes with minimal new machinery (a dedicated `LoopState` read node replacing general lambdas). AD integrates with clear, conservative rules; Torch and Tape map naturally to their structured control flow.

Build Flags & Defaults
- `ET_ENABLE_CONTROL_FLOW` (default ON via `et` interface target): enables If/Select and comparison operators throughout the project. Downstreams can opt out by configuring CMake with `-DET_ENABLE_CONTROL_FLOW=OFF`.
- `ET_WITH_TORCH`: required to include and build the Torch backend. Keep it OFF if libtorch is not available; turn it ON to enable Torch examples/tests.
- `ET_TORCH_ENABLE_MODULE_WRAPPER`: enables a convenience wrapper that constructs a TorchScript `Module` (`make_script_module`) and a direct callable (`make_torch_method_runner`).
  - Version guard: the wrapper is compiled only when Torch >= 2.3 (`ET_TORCH_MODULE_WRAPPER_AVAILABLE` is defined). Example 11 enables this by default when Torch is present.
- `ET_BUILD_CONTROL_FLOW_EXAMPLE` (default ON): builds example 10 (If/Select on runtime evaluator).
- `ET_BUILD_TORCH_EXAMPLES` / `ET_BUILD_TORCH_TESTS`: control Torch example/test targets; as a convenience, example 11 is also built when Torch tests are enabled.
