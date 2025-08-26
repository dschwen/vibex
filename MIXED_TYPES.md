# Mixed-Type Operations: Algebraic Properties and Rewrite Implications

This document outlines design considerations for supporting mixed-type operations (e.g., scalars, vectors, matrices, tensors, booleans, modular arithmetic) while maintaining sound rewrites, normalization, CSE, AD, and backend codegen. The core theme: algebraic properties are type- and operator-dependent; the engine must make property-aware decisions.

## Goals

- Correctness: never apply a rewrite that is invalid under the concrete algebra of the operands.
- Composability: enable scalar–matrix/tensor mixes, broadcasting, and semiring-like extensions.
- Canonicalization: retain a consistent normal form per algebra to maximize CSE and matcher power.
- Performance: avoid over-constraining rules; keep normalization inexpensive and predictable.

## Terminology and Scope

- Algebraic properties considered: associativity, commutativity, distributivity, identity/annihilator, invertibility, scalar action, transpose/conjugation behavior, zero divisors.
- Operators are explicit tags: `Add`, `Mul` (generic), domain-specific ops like `MatMul`, `Hadamard`, `Convolution`, `Compose`, `And`, `Or`.
- Types carry shape and domain: `Scalar<R>`, `Vector<R,n>`, `Matrix<R,m,n>`, `Tensor`, `Bool`, `Mod<p>`, possibly runtime-sized variants.

## Core Design Principles

1. Property queries are type- and op-aware
   - Provide trait-style queries: `is_commutative(op, types...)`, `is_associative(op, types...)`, `has_left_distrib(mul, add, types...)`, `scalars_commute_with(type)`.
   - Properties are not global to an `op`: e.g., `Add` is commutative for matrices and scalars, but `Mul` is not for matrices (`MatMul`) while it is for elementwise multiplication (`Hadamard`).

2. Distinguish operations, avoid overloading semantics
   - Keep `MatMul` distinct from `Mul`/`Hadamard`; keep `And`/`Or` distinct from `Add` in semiring contexts.
   - Elementwise operations use their own tags to avoid accidental reuse of rules intended for different algebras.

3. Guard rewrites with property preconditions
   - Every rewrite rule declares required properties. Example: commutative flatten/sort for `Add` must assert `is_commutative(Add, T...)`.
   - Factorization rules must state orientation and commutativity needs (left-factor vs right-factor vs both).

4. Normalize with the weakest safe assumptions
   - Only apply AC (flatten + sort) when both A and C hold for the operand types; otherwise apply only A (flatten) or neither.
   - Convert `Sub(a, b)` → `Add(a, Neg(b))` only if `Neg` is defined for the type; valid for rings, matrices over rings; not for `Bool`.

5. Typed pattern variables and constraints
   - Patterns include type predicates: `x:Scalar`, `M:Matrix`, `A:any` with property guards (e.g., `invertible(M)` when needed).
   - Mixed patterns permit scalar extraction/factoring only when the target type supports scalar action.

6. Type inference precedes aggressive rewrites
   - Run a light inference to determine concrete domains/shapes (or conservative upper bounds) so property guards can be evaluated.
   - When types are unknown, prefer delaying property-sensitive rewrites; allow safe local normalizations.

## Examples: Valid vs Invalid Rewrites

- Commutativity
  - `Add(a, b) → Add(b, a)` valid for scalars, vectors, matrices; also for booleans with `Or/And` individually.
  - `MatMul(A, B) → MatMul(B, A)` invalid in general.
  - `Hadamard(A, B) → Hadamard(B, A)` valid.

- Factorization
  - `Add(Mul(a, x), Mul(a, y)) → Mul(a, Add(x, y))` requires `Mul` left-distributes over `Add` and associativity; valid for scalars/matrices where `a` is a scalar and `x, y` any compatible type with scalar action.
  - `Add(MatMul(A, X), MatMul(B, X)) → MatMul(Add(A, B), X)` valid via right-factorization (associativity of `MatMul` and distributivity of `MatMul` over `Add`).
  - `Add(MatMul(X, A), MatMul(X, B)) → MatMul(X, Add(A, B))` valid via left-factorization.
  - `Add(MatMul(A, X), MatMul(X, B)) → ?` cannot factor generically; mixing left/right factors without commutativity is unsafe.

- Like-term merging
  - `Add(x, x) → Mul(2, x)` requires a scalar semiring acting on `type(x)` and embedding of scalars that commute with `x` (modules/semimodules). Valid for matrices/tensors over reals/complex.
  - For `Bool`, `Add` is not boolean OR; do not map `x OR x` to `2*x`.

- Distributivity orientations
  - `MatMul(A, Add(X, Y)) → Add(MatMul(A, X), MatMul(A, Y))` valid.
  - `MatMul(Add(X, Y), A) → Add(MatMul(X, A), MatMul(Y, A))` valid.
  - `Hadamard(A, Add(X, Y))` distributes elementwise if shapes align.

- Scalar movement
  - `MatMul(a, X) ↔ MatMul(X, a)` only if `a` is a scalar that commutes with matrices and the backend interprets scalar-matrix multiplication symmetrically. In practice, represent scalar action as `Scale(a, X)` to avoid ambiguity.

## Normalization Strategy per Operator

- Add
  - Associative for all numeric types; commutative for scalars/vectors/matrices/tensors; boolean `Or/And` are separate ops and individually AC.
  - Normalize: flatten nested `Add`, sort children if commutative for the operand types; constant fold where safe.

- Mul (generic)
  - Treat as a family: `MatMul` (associative, non-commutative), `Hadamard` (AC), `ScalarMul`/`Scale` (scalar action), `Compose` (associative, non-commutative), `Convolution` (often associative, not commutative unless circular with identical kernels).
  - Normalize: flatten only where associative; never sort unless commutative holds for that variant.

- Neg/Sub
  - Valid for additive groups: scalars, vectors, matrices, tensors. Convert `Sub` → `Add`+`Neg`. Fold `Neg(Const)` and `Neg(Neg(x))` universally where `Neg` is defined.

- Transpose/Conjugate
  - Record algebra-aware identities: `(AB)^T = B^T A^T`, `(aX)^T = a X^T` for real scalars; conjugation interacts with complex scalars. Rewrites must check domain and orientation.

## Rule Authoring Guidelines

- Always specify property preconditions
  - Example: `Add(x, x) -> Scale(2, x)` requires `has_scalar_action(type(x)) && scalars_commute_with(type(x))`.
  - Example: AC sort for `Add` requires `is_commutative(Add, types...)`.

- Use oriented factoring rules
  - Left-factor and right-factor are distinct rules for non-commutative ops; avoid a symmetric "factor common" unless commutativity is proven.

- Keep op tags specific
  - Avoid writing a rewrite over a generic `Mul` unless its semantics are clearly elementwise; prefer `MatMul`, `Hadamard`, `Scale`.

- Prefer local, semantics-preserving simplifications
  - Cancel `Neg(Neg(x))`, fold `Add(x, 0)`, `MatMul(I, X)`, `MatMul(X, I)` where identity existence is guaranteed for the types/shapes.

## Pattern Matching and Guards

- Typed variables: `x:Scalar`, `v:Vector`, `M:Matrix`, `T:Tensor`, `b:Bool`.
- Property guards: `invertible(M)`, `symmetric(M)`, `diagonal(M)`, `orthonormal(M)`, `broadcastable(x, M)`.
- Shape guards: ensure `MatMul(M[m,k], N[k,n])` matches; guide safe distribution/factorization.
- Domain guards: `complex(x)` to enable conjugation-specific rewrites; `mod_p(x)` for modulo arithmetic identities.

## Type Inference and Unknowns

- Inference pass resolves as much as possible from literals, variables with annotations, and operator constraints.
- When types remain unknown:
  - Avoid AC sorting for `Mul`-like ops; safe to flatten associative structures only.
  - Permit `Sub` → `Add`+`Neg` only if both operands share an additive group.
  - Defer scalar extraction merges until scalar action is confirmed.

## CSE and Hashing

- Include type and operator tag in the structural hash.
- For commutative ops, canonicalize child order in the hash only when commutativity holds for the resolved types; otherwise preserve order.
- Mixed-type `Add` still commutative (matrices, vectors), enabling hash CSE across reorderings.
- Keep normalization consistent with hashing to avoid churn between passes.

## AD (Automatic Differentiation) Considerations

- Non-commutative products require order-aware adjoint rules:
  - d/dX `MatMul(A, X)` → left-multiplication by `A^T` in reverse mode; d/dX `MatMul(X, B)` → right-multiplication by `B^T`.
  - Chain rules for `Compose` follow function composition order.
- Scalar extraction during simplification must maintain AD semantics (e.g., `Scale(c, X)` derivatives pass through `c`).
- Transpose/conjugate interaction: ensure correct adjoints in complex domains.

## Backends and Codegen

- Backends must receive concrete type/shape to choose kernels (BLAS for `MatMul`, elementwise loops for `Hadamard`).
- Emitting variables by runtime index must be accompanied by type descriptors to drive correct emission.
- Scalar action should map to a dedicated kernel or fused scaling to avoid ambiguous `MatMul` with a 1×1 matrix.
- Torch backend: differentiate `torch.matmul` vs `*` elementwise, respect broadcasting rules, and avoid reordering `matmul` chains.

## Interop: Broadcasting and Promotion

- Explicitly model broadcasting as an op or as a type-level constraint to prevent accidental rewrites across broadcasted dimensions.
- Promotion lattice: `Bool → Int → Real → Complex`; scalars may act on matrices/tensors only when defined by the target domain's module structure.
- Rewrites must not change broadcast semantics (e.g., factorization must preserve broadcasted shapes and alignment).

## Testing Strategy

- Add unit tests that differentiate `MatMul` vs `Hadamard` behavior under the same syntax.
- Property-specific tests:
  - Verify AC normalization on `Add` for matrices.
  - Verify no AC sorting on `MatMul` but allow associative flatten.
  - Check left/right factorization rules independently.
  - Like-term merging with matrices produces `Scale(2, M)` and evaluates correctly.
- AD tests for matrix products confirming correct adjoints with orientation.
- Torch tests gated with `-DET_BUILD_TORCH_TESTS=ON` verifying op selection and broadcasting.

## Migration Path from Scalar-Only Rules

- Audit existing rules and annotate with property guards.
- Split generic `Mul` rules into `Scale`, `Hadamard`, `MatMul` as appropriate.
- Update normalization to consult property queries dynamically based on inferred types.
- Extend CSE hashing to conditional commutativity canonicalization.

## Open Questions

- How to represent and propagate algebraic domains at runtime without RTTI/virtuals? Options: type tags + variant + constexpr traits tables.
- Do we need e-graph integration with per-analysis properties to handle mixed algebras robustly?
- Should scalar action be a first-class op (`Scale`) everywhere to make factoring and AD unambiguous?
- What is the minimal property set needed to unlock most rewrites without risking unsoundness?

## Takeaways

- Algebra is contextual: correctness requires property-aware rewrites.
- Keep operations explicit and typed; avoid overloading that conflates semantics.
- Normalize conservatively by default; escalate to AC only when proven safe.
- Encode and test orientation-sensitive rules for non-commutative products.
- Invest in typed patterns, property guards, and a lightweight inference pass to unlock mixed-type optimization safely.

## Minimal Properties/Traits Tables

The following compact tables summarize default, conservative properties and policies for common ops. Use these as guard inputs for rewrites, normalization, hashing, AD, and backend selection.

### Algebraic Properties

| Op Tag     | Types                             | Assoc (A) | Comm (C) | Left Distrib over Add | Right Distrib over Add | Identity | Annihilator | Canonicalization |
|------------|-----------------------------------|-----------|----------|-----------------------|------------------------|----------|-------------|------------------|
| Add        | Scalar/Vector/Matrix/Tensor       | Yes       | Yes      | N/A                   | N/A                    | 0        | —           | AC (flatten+sort) |
| Or         | Bool                              | Yes       | Yes      | N/A                   | N/A                    | False    | True        | AC (flatten+sort) |
| And        | Bool                              | Yes       | Yes      | N/A                   | N/A                    | True     | False       | AC (flatten+sort) |
| MatMul     | Matrix (shape-compatible)         | Yes       | No       | Yes                   | Yes                    | I        | —           | A-only (ordered) |
| Hadamard   | Elementwise tensor multiply       | Yes       | Yes      | Yes                   | Yes                    | Ones     | Zeros       | AC (flatten+sort) |
| Scale      | Scalar × (Vector/Matrix/Tensor)   | —         | —        | Yes                   | N/A                    | 1        | 0           | Pull/combine scales |
| Compose    | Function/Linear map composition   | Yes       | No       | No (general)          | No (general)           | Id       | —           | A-only (ordered) |

Notes:
- Identity/annihilator depend on domain and shape; ensure they exist for matched types (e.g., `I` must be appropriately sized).
- For `Scale`, treat scalar action as its own op to avoid ambiguity with `MatMul` by 1×1 matrices.

### Hashing and Normalization Policy

| Op Tag   | Child Order in Hash | Normalization Policy             | Like-Terms/Combines |
|----------|----------------------|----------------------------------|---------------------|
| Add      | Sorted               | Flatten + sort (AC)              | Merge constants; collect scales |
| MatMul   | As-written           | Flatten only (preserve order)    | Associate; no reordering |
| Hadamard | Sorted               | Flatten + sort (AC)              | Merge constants/scales |
| Scale    | Scalar first         | Combine adjacent scales; hoist   | Multiply scalars |
| Or/And   | Sorted               | Flatten + sort (AC)              | Idempotent elimination (x∨x=x, x∧x=x) |

### Backend Mapping (indicative)

| Op Tag   | CPU Backend                 | Torch Backend     | Notes |
|----------|-----------------------------|-------------------|-------|
| Add      | Elementwise add             | `torch.add`       | Support broadcasting per type rules |
| MatMul   | BLAS GEMM/STRIDED BATCHED   | `torch.matmul`    | Preserve multiplication order |
| Hadamard | Elementwise multiply        | `torch.mul`       | Shape-aligned elementwise product |
| Scale    | Fused scale or mul-by-scalar| `torch.mul` (scalar) | Prefer fusion to avoid extra passes |
| Or/And   | Bit/Bool ops                | `torch.logical_or`/`logical_and` | Keep separate from numeric Add |
| Compose  | Inline function composition | n/a               | Use explicit kernels for linear maps |

### AD Orientation (reverse mode, key cases)

Let `Y = MatMul(A, X)` and upstream bar `ȳ` (same shape as `Y`). Then:
- ∂L/∂X = MatMul(Aᵀ, ȳ)
- ∂L/∂A = MatMul(ȳ, Xᵀ)

Let `Y = MatMul(X, B)`:
- ∂L/∂X = MatMul(ȳ, Bᵀ)
- ∂L/∂B = MatMul(Xᵀ, ȳ)

Let `Y = Scale(c, X)` with scalar `c`:
- ∂L/∂X = Scale(c, ȳ)
- ∂L/∂c = ⟨ȳ, X⟩ (reduce over domain as appropriate)

For `Add(X, Z)`:
- ∂L/∂X = ȳ, ∂L/∂Z = ȳ

## Traits API Sketch (no implementation)

Purpose: provide a small, constexpr-friendly query surface the optimizer, normalizer, CSE, and backends can use to decide which rewrites are sound for given ops and types. Results are tri-state to enable conservative fallbacks when types are unknown.

- Op tags: `AddOp`, `MatMulOp`, `HadamardOp`, `ScaleOp`, `OrOp`, `AndOp`, `ComposeOp`.
- Type tags: `Scalar<T>`, `Vector<T,n>`, `Matrix<T,m,n>`, `Tensor<T,Rank,...>`, `Bool`, `Mod<p>`, with `dyn` markers for runtime sizes.
- Result kind: `Tri` with values `Yes`, `No`, `Unknown`.

Query surface (signatures are illustrative, not code):

- `is_associative(Op, Types...) -> Tri`
- `is_commutative(Op, Types...) -> Tri`
- `distributes_left(MulOp, AddOp, Types...) -> Tri`
- `distributes_right(MulOp, AddOp, Types...) -> Tri`
- `has_identity(Op, Types...) -> Tri`
- `has_annihilator(Op, Types...) -> Tri`
- `has_neg(Type) -> Tri` (additive inverse exists)
- `has_scalar_action(ScalarType, TargetType) -> Tri`
- `scalars_commute_with(ScalarType, TargetType) -> Tri`
- `broadcastable(T0, T1, ...) -> Tri`
- `promote(T0, T1, ...) -> TypeTag?` (may return Unknown)
- `matmul_compatible(A[m,k], B[k,n]) -> Tri`
- `canon_policy(Op, Types...) -> { flatten: bool, sort: bool }` (derive from A/C)
- `hash_policy(Op, Types...) -> { order: ordered|sorted }`
- `backend_kernel(Op, Types...) -> KernelTag` (e.g., GEMM, EwiseMul)

Guard usage patterns (illustrative):

- AC normalization for `Add` only if `is_associative(AddOp, T...)==Yes && is_commutative(AddOp, T...)==Yes`.
- Right factoring for `MatMul`: require `is_associative(MatMulOp, T...)==Yes && distributes_right(MatMulOp, AddOp, T...)==Yes`.
- Like-term merge `Add(x, x) -> Scale(2, x)` requires `has_scalar_action(Scalar<Real>, type(x))==Yes && scalars_commute_with(Scalar<Real>, type(x))==Yes`.
- `Sub(a,b) -> Add(a, Neg(b))` guarded by `has_neg(type(a))==Yes && type(a)==type(b)`.

Fallback semantics for Unknown:

- If any required property query returns `Unknown`, the corresponding rewrite must not fire.
- Normalization defaults: apply associative flattening when `is_associative==Yes`; do not sort unless `is_commutative==Yes`.
- Hashing defaults: preserve child order unless commutativity is `Yes`.

Extensibility:

- New ops/types add specializations by declaring their properties; defaults return `Unknown`.
- Domains can refine behavior (e.g., complex numbers affect conjugation rules) by extending the trait tables for `Scalar<Complex>`.
- Backends select kernels via `backend_kernel`; if `Unknown`, fall back to a generic implementation without reordering.
