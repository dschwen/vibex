# DESIGN.md — et: Runtime AST with Modular Backends and Tape AD

Audience: C++ developers adding operations, improving reverse-mode AD (tape), or adding new backends (TorchScript JIT, printers, etc.).
Status: Header-only, C++17, runtime AST for expression building; reverse-mode Tape for gradients; optional Torch JIT integration.

---

## 1. Goals and Philosophy

- Single, simple runtime AST built via operator overloading (`et/ast.hpp`).
- Reverse-mode AD via a compact Tape backend (`et/tape_backend.hpp`).
- Algebraic rewrite over AST for canonicalization and simplification (`normalize_ast.hpp`, `rewrite_ast.hpp`).
- Modular backends: compile AST to Tape, TorchScript JIT, or other targets via small emitters.
- No template expression trees: evaluation and compilation happen at runtime.

---

## 2. Directory Overview (key headers)

```
include/et/ast.hpp                  # Runtime AST (Expr + Node hierarchy)
include/et/normalize_ast.hpp        # AST normalization (AC flatten/sort, constant folding)
include/et/rewrite_ast.hpp          # AST rewrite (fixed-point algebraic rules)
include/et/compile_ast.hpp          # AST→backend compilation (Tape/Torch)
include/et/compile_cse_ast.hpp      # AST CSE (structural hash)
include/et/compile_hash_cse_ast.hpp # AST CSE (string key)
include/et/tape_backend.hpp         # Reverse-mode Tape backend (forward eval + VJP)
include/et/torch_jit_backend.hpp    # TorchScript backend (optional; -DET_WITH_TORCH)

# Bridging (legacy RGraph for pattern matcher utilities)
include/et/ast_to_runtime.hpp       # AST→RGraph bridge
include/et/runtime_ast.hpp          # RGraph nodes (used by pattern matcher and utilities)
include/et/normalize.hpp            # RGraph normalize (for pattern matcher)
include/et/pattern.hpp              # RGraph pattern DSL
include/et/match.hpp                # RGraph structural matcher
```

Deprecation note: RGraph exists only for pattern matching and certain utilities during migration. New work should target the AST. RGraph and the bridge will be removed once the pattern/matcher layer is ported to AST.

---

## 3. Core IR (AST)

### 3.1 Node Types
- Arithmetic/math: `ConstNode`, `VarNode`, `AddNode`, `SubNode`, `MulNode`, `DivNode`, `NegNode`, `PowNode`, `SinNode`, `CosNode`, `ExpNode`, `LogNode`, `SqrtNode`, `TanhNode`.
- Control flow: `IfNode`, `SelectNode`, comparisons (`LtNode`, `LeNode`, `GtNode`, `GeNode`, `EqNode`, `NeNode`).
- Loops: `IterNode`, `StateReadNode`, `LoopForNode`, `LoopOutNode`.

`Expr` is a lightweight handle (`std::shared_ptr<Node>` under the hood). Operators and helpers (e.g., `sin`, `cos`, `pow`, `If`, `Select`) build AST nodes directly.

### 3.2 Building and Evaluating
```cpp
auto x = et::var(0), y = et::var(1), z = et::var(2);
et::Expr f = et::sin(x)*y + z*z;
double v = et::eval(f, {2.4, 6.0, 1.5});
```
Comparisons evaluate to 1.0/0.0, consistent with control-flow nodes.

---

## 4. Reverse-Mode AD (Tape)

AST compiles to a compact Tape (enum-kind nodes; indices to children). `forward(inputs)` computes the primal; `backward(inputs)` computes gradients via a single reverse sweep (VJPs). Loops are supported by reverse iteration of the body Jacobian^T, propagating into init state and inputs; no gradients flow into boolean predicates.

```cpp
et::TapeBackend tb(arity);
int root = et::compile_runtime(expr, tb);
tb.tape.output_id = root;
auto value = tb.tape.forward(inputs);
auto grad  = tb.tape.backward(inputs);
```

---

## 5. Normalization & Rewrite (AST)

- `normalize(e)` folds constants and canonicalizes Add/Mul (AC flatten/sort). `Sub(a,b)` is normalized to `Add(a, Neg(b))` internally.
- `rewrite_fixed_point(e)` applies AST-native algebraic rules (e.g., `log(exp(u))→u`, odd/even trigs, `sin^2+cos^2→1`, like-term merging, factoring, square completion, exp-product combination, integer-exponent power merging, and basic fraction combining).

---

## 6. CSE Compilers (AST)

- `compile_cse_ast(e, backend)`: structural-hash CSE with collision-checked keys; supports control-flow and loops for Tape/Torch.
- `compile_hash_cse_ast(e, TapeBackend)`: string-key CSE; simple and robust.

Both emit via `emitApply(OpTag{}, …)` for unary/binary/control-flow operations. Loops use dedicated emitters on backends that support them.

---

## 7. Backends and Compilation

Use `compile_runtime(const Expr&, Backend&)` for direct AST→backend lowering.

### 7.1 Tape Backend
Maps AST nodes to Tape kinds; forward computes values; backward computes gradients. Loops lower to `KLoopFor` + `KLoopOut` with reverse iteration VJP.

### 7.2 TorchScript JIT Backend
Maps arithmetic to `aten::` symbols, `If` to `prim::If`, `Select` to `aten::where`, and loops to `prim::Loop` with carried dependencies.

---

## 8. Modularity

- AST nodes are runtime objects; algorithms operate on `Expr` uniformly (`normalize`, `rewrite_fixed_point`, `compile_runtime`, CSE variants).
- Backends translate node kinds into target primitives.

---

## 9. Adding a New Operation (Checklist)

1. Add a node/helper in `ast.hpp` (e.g., `struct ExpNode` + `Expr exp(const Expr&)`).
2. Add constant folding to `normalize_ast.hpp` when safe.
3. Map to Tape (`TapeBackend`) and Torch (`TorchJITBackend`) in their emitters.
4. Optionally add AST rewrite identities.

Minimal viable support: (1)+(3). (2)+(4) improve quality (folding/simplifications).

---

## 10. Deprecated (RGraph) and Migration Plan

The legacy RGraph (`runtime_ast.hpp` + `normalize.hpp` + `pattern.hpp` + `match.hpp`) remains only for pattern-matching utilities. An `ast_to_rgraph` bridge exists to run those matchers on AST-built expressions.

Planned removal:
- Migrate pattern/matcher capabilities to an AST-native layer.
- Remove the RGraph bridge and RGraph headers after parity is reached.

Guidance:
- New features, rewrites, and backends should target AST (`ast.hpp`) exclusively.
- Use `ast_to_rgraph()` only in tests/utilities that still rely on the RGraph matcher.

