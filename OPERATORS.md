# Operators, Scalar Lifting, and Refactor Plan

This note documents the current operator design in the ET layer, the newly added scalar‑lifting behavior, and a planned refactor to reduce duplication without resorting to heavy macros.

## Context

- The ET core lives under `include/et/*.hpp` and exposes node types `Var<T,I>`, `Const<T>`, and `Apply<Op,...>`.
- Operators (`+`, `-`, `*`, `/`, and `pow`) are intentionally constrained so they only engage when at least one side is an ET node. This preserves normal scalar math and keeps overload resolution predictable.
- Historically, mixing scalars required explicit `lit(...)`, e.g., `x + lit(1.0)`.

## Goals

- Ergonomics: allow `x + 1.0`, `2.0 * x`, `pow(x, 2.0)`, etc.
- Safety: do not hijack scalar‑scalar math; only participate when one side is a node.
- Predictability: avoid surprising implicit promotions and keep value types consistent with node types.
- Maintainability: minimize repeated boilerplate in operator definitions.

## Scalar Lifting Semantics

When combining a node with a scalar that is convertible to the node’s `value_type`, the scalar is lifted to a `Const<value_type_of_t<Node>>` using `lit(static_cast<T>(scalar))`.

- Node + Node: `Apply<AddOp, L, R>`
- Node + Scalar: `Apply<AddOp, L, Const<TL>>` where `TL = value_type_of_t<L>`
- Scalar + Node: `Apply<AddOp, Const<TR>, R>` where `TR = value_type_of_t<R>`

The same pattern applies to `-`, `*`, `/`, and `pow`.

Key constraints:
- At least one side must be an ET node (`is_node_t<...>`); otherwise no overload participates and normal scalar math is used.
- The non‑node side must be convertible to the node’s value type: `std::is_convertible_v<Scalar, value_type_of_t<Node>>`.
- Lifting uses `static_cast` to the node’s value type before wrapping in `Const<T>` to keep the graph’s numeric type consistent.

## Current Implementation

To make the behavior explicit and unambiguous, we provide three overloads per binary operator:

- Node–Node (maps directly to `Apply<Op, L, R>`)
- Node–Scalar (lifts right to `Const<value_type_of_t<L>>`)
- Scalar–Node (lifts left to `Const<value_type_of_t<R>>`)

This tightens the previous “at least one node” template into a clearer trio of overloads and removes the possibility of forming `Apply<Op, Node, double>` with a raw, non‑callable scalar.

Backward compatibility:
- Existing code using `lit(...)` continues to work unchanged.
- New usage like `x + 1.0` and `2.0 * x` is now supported.
- Operators still do not engage for scalar–scalar: those continue to use the standard operators.

## Planned Refactor: Helper‑Based Consolidation

There is currently visible repetition across the operator bodies. Rather than macros, we prefer a single helper that centralizes the lifting logic and keeps the operator declarations minimal.

Proposed structure (names simplified for clarity):

```cpp
namespace et { namespace detail {

// Node–Node
template<class Op, class L, class R,
         std::enable_if_t<is_node_t<L>::value && is_node_t<R>::value, int> = 0>
constexpr auto apply_op(L l, R r) {
  return Apply<Op, std::decay_t<L>, std::decay_t<R>>(std::move(l), std::move(r));
}

// Node–Scalar
template<class Op, class L, class R,
         std::enable_if_t<is_node_t<L>::value && !is_node_t<R>::value &&
                          std::is_convertible_v<R, value_type_of_t<L>>, int> = 0>
constexpr auto apply_op(L l, R r) {
  using TL = value_type_of_t<L>;
  return Apply<Op, std::decay_t<L>, Const<TL>>(std::move(l), lit(static_cast<TL>(r)));
}

// Scalar–Node
template<class Op, class L, class R,
         std::enable_if_t<!is_node_t<L>::value && is_node_t<R>::value &&
                          std::is_convertible_v<L, value_type_of_t<R>>, int> = 0>
constexpr auto apply_op(L l, R r) {
  using TR = value_type_of_t<R>;
  return Apply<Op, Const<TR>, std::decay_t<R>>(lit(static_cast<TR>(l)), std::move(r));
}

}} // namespace et::detail

// Public operators (forwarding wrappers)
template<class L, class R, std::enable_if_t<is_node_t<L>::value || is_node_t<R>::value, int> = 0>
constexpr auto operator+(L l, R r) { return detail::apply_op<AddOp>(std::move(l), std::move(r)); }

// Repeat for -, *, /, and pow
```

Benefits:
- One place to adjust lifting rules across all operators.
- Operators become one‑liners, reducing duplication and potential for inconsistencies.
- Keeps SFINAE surface small and readable; error messages remain clean.

## Macro Wrapper (Optional)

If desired, thin macros can be used only to generate the forwarding wrappers, while leaving the core lifting logic in the templated helper above:

```cpp
#define ET_DEFINE_BINOP(OPSYM, OPTAG) \
  template<class L, class R, \
           std::enable_if_t<is_node_t<L>::value || is_node_t<R>::value, int> = 0> \
  constexpr auto operator OPSYM(L l, R r) { \
    return ::et::detail::apply_op<OPTAG>(std::move(l), std::move(r)); \
  }

ET_DEFINE_BINOP(+, AddOp)
ET_DEFINE_BINOP(-, SubOp)
ET_DEFINE_BINOP(*, MulOp)
ET_DEFINE_BINOP(/, DivOp)
// pow remains a named function: `constexpr auto pow(L, R)`
```

Pros:
- Minimizes boilerplate even further.

Cons:
- Macro surface in headers can leak and is harder to debug.
- Template helper alone typically offers a better balance of readability and ergonomics.

## Edge Cases and Notes

- Mixed node value types: Node–Node value type is still deduced via `Op::eval`; we do not attempt to coerce node value types.
- Convertibility checks: Lifting only participates if the scalar is convertible to the node’s value type. This avoids accidental narrowings that would be ill‑formed.
- `pow` specifics: `et::pow` remains a namespaced wrapper to avoid colliding with `std::pow` overloads. Lifting ensures both sides are ET nodes before reaching `PowOp::eval`, which uses `std::pow` internally.
- Math wrappers (`sin`, `cos`, `exp`, `log`, `sqrt`, `tanh`) remain node‑only and are unaffected by lifting.

## Testing

- `tests/test_scalar_lift.cpp` covers the following cases:
  - `x + 1.0`, `2.0 + x`
  - `x - 3.0`, `5.0 - x`
  - `x * 2.0`, `3.0 * x`
  - `x / 2.0`, `9.0 / x`
  - `pow(x, 2.0)`, `pow(2.0, x)`
- Existing tests for simplify/normalize/rewrite/CSE continue to reason over `Const<T>` nodes, which scalar lifting creates explicitly.

## Refactor Plan (Post‑Merge)

1) Introduce `detail::apply_op<Op>(l, r)` with Node–Node, Node–Scalar, Scalar–Node overloads.
2) Replace the body of each operator with a one‑line forwarder to `apply_op`.
3) Keep current SFINAE: “only when at least one side is an ET node.”
4) Optionally add the macro wrappers, guarded under an internal config toggle if we ever need them.
5) Ensure `ctest` passes and that compile times remain reasonable.

This plan preserves behavior, reduces duplication, and keeps the public operator surface straightforward and consistent with the project guidelines.

