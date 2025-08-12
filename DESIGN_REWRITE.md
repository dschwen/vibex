# Algebraic Refactorization & Rewrite System

This document proposes a flexible, header‑only rewrite engine for algebraic refactorization and simplification on top of the existing ET (expression template) IR. It adds a runtime AST, AC‑normalization (associative + commutative), a pattern language with named placeholders, and a deterministic rewrite loop.

## Goals

- Express common algebraic identities as rules with named placeholders and guards.
- Deterministic, canonical rewriting via normalization (flatten/sort) of `Add`/`Mul`.
- Reusable engine: match/replace on a runtime AST, then rebuild ET trees.
- Header‑only, minimal dependencies; fast enough for compile‑time and runtime flows.

## Non‑Goals (Initial Cut)

- Full E‑graph equality saturation (may be a future extension).
- Heavy SMT/constraint solving; we support simple guard predicates.

## High‑Level Architecture

- Runtime AST: compact node graph mirroring `Apply<Op,...>`/`Var`/`Const` with `NodeKind` and `children` vector. Enables dynamic matching and rewriters.
- Normalizer: flattens associative ops and sorts children for commutative ops; constant folding for simple cases; removes neutral elements.
- Pattern DSL: placeholders `P<ID>` (and typed variants), operators reused to build pattern trees that parallel the runtime AST shape.
- Matcher: structural match with placeholder bindings (unification). For AC ops, relies on normalized, sorted children; performs small backtracking for multi‑set matches.
- Rewriter: applies rules bottom‑up until a fixed point with budgets and rule priorities; supports rule guards.
- Reifier: converts rewritten runtime AST back to ET expression templates.

## Runtime AST

- `enum class NodeKind { Var, Const, Add, Sub, Mul, Div, Neg, Sin, Cos, Exp, Log, Sqrt, Tanh }`.
- `struct RNode { NodeKind kind; std::vector<int> ch; double cval; std::size_t var_index; };`
- `struct RGraph { std::vector<RNode> nodes; int root; }` with structural hashing (optional) to share nodes.
- Builders:
  - `compile_to_runtime(const Expr&) -> RGraph` (templated walker over ET, mirroring `compile`).
  - `to_et(const RGraph&) -> ET` rebuilds an ET expression (templated, recursive).

## Normalization

- Associative ops: `Add`, `Mul`.
- Commutative ops: `Add`, `Mul`.
- Steps (repeat bottom‑up once):
  - Flatten: `Add(Add(a,b),c)` → `Add(a,b,c)`; same for `Mul`.
  - Sort children: stable by `(NodeKind, arity, hash, textual key)` for determinism.
  - Constant combine: sum/multiply all const children; drop neutral elements (`+0`, `*1`); early exit on `*0`.
  - Sign normalization: `Sub(a,b)` → `Add(a, Neg(b))` for uniformity; `Div(a,b)` kept binary.
  - Optional: merge like terms (phase 2): `(k1*x) + (k2*x)` → `(k1+k2)*x` via factor extraction.

## Pattern Language (Header‑Only)

- Placeholders: `P<I>` match any subtree; same `I` must bind to equal subtrees.
  - Typed placeholders (optional): `PX<I>` (non‑const), `PC<I>` (const), `PV<I>` (var), or predicate‑constrained `PIf<I, Pred>`.
- Pattern nodes mirror runtime AST kinds and use the same operator sugar, but over pattern primitives.
- Example rule (Pythagorean): `sin(P<1>)*sin(P<1>) + cos(P<1>)*cos(P<1>)  ->  1`.
- Guards: `bool guard(const Bindings&)` to enforce domains (e.g., `x != 0`, `p > 0`).

### API Sketch

- `struct Rule { Pattern lhs; Pattern rhs; Guard guard; const char* name; int priority; };`
- `using Bindings = flat_map<int /*pid*/, int /*RGraph node id*/>;`
- `bool match(int expr_id, const Pattern&, Bindings&)` performs matching with unification.
- `int instantiate(const Pattern& rhs, const Bindings&, RGraph&)` builds replacement subtree.

## Matching Algorithm

- Deterministic structural match for non‑AC nodes: kinds must equal; recursively match children.
- Placeholder binding: first occurrence records `pid -> node`; subsequent occurrences require structural equality with the previously bound node id.
- AC nodes (`Add`, `Mul`): both sides are normalized and sorted. We match a multiset of child patterns to a multiset of child nodes.
  - Strategy: order pattern children by specificity (fewest placeholders first), then greedy assign to candidate children with backtracking on conflicts.
  - Constraint propagation: when a placeholder appears in multiple child patterns (like `sin(P1)` and `cos(P1)`), binding in one reduces candidates for the other.

## Rewrite Strategy

- Bottom‑up pass: compute a postorder of nodes; at each node, try rules in descending `priority` and first‑match wins.
- Replace in place in `RGraph` (new nodes appended), and record changes.
- Iterate passes until no changes or a max iteration budget is reached (to avoid ping‑pong rules).
- Expose knobs: `max_passes`, `max_node_growth`, per‑rule enable/disable.

## Rule Sets (Initial)

- Neutral/annihilators:
  - `x + 0 -> x`, `0 + x -> x`; `x - 0 -> x`.
  - `x * 1 -> x`, `1 * x -> x`; `x * 0 -> 0`.
  - `x - x -> 0`; `x / x -> 1` with guard `x != 0`.
- Trig/exponential identities:
  - `sin(P1)*sin(P1) + cos(P1)*cos(P1) -> 1`.
  - `log(exp(P1)) -> P1`; `exp(log(P1)) -> P1` with guard `P1 > 0`.
- Algebraic factoring (phase 2):
  - `(k1*x) + (k2*x) -> (k1+k2)*x` where `k1,k2` are const.
  - `x*x -> x^2` (when/if `Pow` is introduced).

## Integration Points

- `rewrite(const Expr& e, const std::vector<Rule>& rules) -> Expr`:
  1) ET → runtime (`RGraph`), 2) normalize, 3) fixed‑point rewrite, 4) normalize again, 5) runtime → ET.
- `simplify()` can delegate to `rewrite()` with a default rule set, after existing const folding.
- CSE: run CSE either before runtime rebuild or on the ET rebuilt tree as today.

## Implementation Plan

1) Runtime AST and converters
   - Add `include/et/runtime_ast.hpp` with `RNode`, `RGraph`, and visitors.
   - Implement `compile_to_runtime(Expr)` and `to_et(RGraph)`.

2) Normalizer
   - Add `include/et/normalize.hpp` that flattens/sorts `Add`/`Mul`, folds consts, removes neutral elements.
   - Provide stable structural hash + ordering for deterministic sorting.

3) Pattern Primitives
   - Add `include/et/pattern.hpp` with `P<I>` and typed/guarded variants, plus pattern node types mirroring ops.
   - Overload operators to build `Pattern` trees using the same sugar as ET.

4) Matcher + Guards
   - Implement `match(expr_id, pattern, bindings)` with unification and AC multiset matching.
   - Add guard functors taking `Bindings` and `RGraph`.

5) Rewriter
   - Implement `apply_rules_once(RGraph&, Rules)` bottom‑up; then `rewrite_fixed_point(...)` with budgets.
   - Add basic metrics (nodes visited, replacements, time budget hooks).

6) Rule Library
   - Add `include/et/rules_default.hpp` containing the initial ruleset described above.
   - Expose an API to compose custom rule vectors.

7) Integration + Examples
   - Add `rewrite()` API and wire a `simplify_algebraic()` convenience wrapper.
   - Create an example: `examples/08_rewrite_rules.cpp` demonstrating Pythagorean identity and neutral element simplifications.

8) Tests
   - Unit tests for normalization (order/flatten), matcher (placeholder consistency), rewrite pass (fixed‑point), and rule guards.

## Future Extensions

- E‑graph backend (à la egg) for equality saturation with extraction by cost model.
- Rich predicates (monotonicity, nonnegativity) and domain tracking to justify guarded rewrites.
- Rule compilation to decision trees for faster matching on large rule sets.
- Coefficient extraction and rational simplification; polynomial factorization; gcd of monomials.

