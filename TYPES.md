# TYPES.md — Generalized Mixed-Type Support (Scalars, Custom Vectors, Etc.)

Audience: C++ developers designing and extending Vibex to support multiple numeric “kinds” (scalars and user-defined vector-like types) with predictable operator return types, clean runtime execution, and correct reverse-mode AD.

Status: Design document (implementation guidelines). Not limited to matrices; focuses on general vector-like types where, for example, `Vec * Vec -> Scalar` (dot) and `Scalar * Vec -> Vec` (scale).

---

## Goals

- Keep the AST core simple and non-templated to avoid code-size and compile-time explosion.
- Let C++ deduce return types at operator sites (compile-time) via traits/concepts on the typed front-end.
- Support user-defined types (e.g., SmallVec2, SIMDVec, DualNumbers, etc.) with pluggable rules:
  - Which operations are valid (Add, Neg, Scale, Dot, Elementwise Mul, …)
  - What the return type is for each operation
  - How to evaluate kernels and how to differentiate them (VJP callbacks)
- Keep backends (Tape, Torch) straightforward and robust for mixed types.

Non-goals (initially)
- Full matrix/tensor algebra (broadcasting, shape-polymorphic ops, etc.). Basic vector/mixed rules are sufficient; the design leaves room to extend.

---

## High-Level Approach

1) Typed front-end, type-erased runtime AST
- Add a typed wrapper `ExprT<T>` that models expressions producing values of C++ type `T`.
- Overload operators on `ExprT<T>` with concepts/traits to constrain valid combos and deduce the result type:
  - Example: `ExprT<Vec> + ExprT<Vec> -> ExprT<Vec>` (elementwise add)
  - Example: `ExprT<Vec> * ExprT<Vec> -> ExprT<Scalar>` (dot)
  - Example: `ExprT<Scalar> * ExprT<Vec> -> ExprT<Vec>` (scale)
- Under the hood, `ExprT<T>` only carries a `std::shared_ptr<Node>` plus static type metadata; the Node itself is not templated.

2) Runtime type tags + minimal shape metadata
- Each Node stores `ValueKind` (e.g., Scalar, Vec, …) and any needed shape metadata (e.g., length for vectors).
- When constructing a node from typed operators, we compute the result kind (and shape) via compile-time traits and record them in the Node.
- Evaluation is single-dispatch on Node kind (and op tag), not a full dynamic multi-dispatch.

3) Type traits for return type deduction
- Provide traits that encode type rules per operation:
  - `result_of_add<L,R>` (usually same-kind elementwise; may allow scalar broadcast on either side)
  - `result_of_mul<L,R>` (e.g., `Vec*Vec->Scalar` for dot; `Scalar*Vec->Vec` for scale)
  - `supports_op<Op,L,R>` to enable/disable overloads
- End users customize behavior by specializing these traits for their types.

4) Evaluation backends configured via type info registry
- A small registry maps `typeid(T)` or an internal `TypeTag` to evaluation kernels and VJP callbacks for each op we support.
- The core AST evaluator (`eval`) operates on a type-erased `Value` (e.g., `std::variant<double, Vec, …>`) and invokes the correct kernel from the registry for `(op, kind, shape)`.
- Tape backend stores op kind + type kind (+ shape) and calls registered forward/VJP lambdas.

---

## Public API Sketch

```cpp
namespace et {

// 1) Typed variable and literal construction
template <class T>
ExprT<T> var(std::size_t index);
template <class T>
ExprT<T> lit(const T& value);

// 2) Operator overloads (examples)
template <class L, class R>
  requires supports_add_v<L,R>
auto operator+(const ExprT<L>& a, const ExprT<R>& b) -> ExprT< result_of_add_t<L,R> >;

template <class L, class R>
  requires supports_mul_v<L,R>
auto operator*(const ExprT<L>& a, const ExprT<R>& b) -> ExprT< result_of_mul_t<L,R> >;

// Also scalar on either side
template <class V>
  requires supports_scale_v<V,double>
auto operator*(double s, const ExprT<V>& v) -> ExprT<V>;

// 3) Evaluate and differentiate remain type-erased under the hood
double eval(const ExprT<double>& e, const std::vector<double>& inputs);
// For vector outputs, eval returns a concrete Vec via a typed overload
Vec eval(const ExprT<Vec>& e, const RuntimeInputs& in); // see below

} // namespace et
```

Runtime inputs for mixed types
```cpp
struct RuntimeInputs {
  std::vector<double> scalars;   // index by var index for Scalar vars
  std::vector<Vec>    vecs;      // index by var index for Vec vars
  // Extend with other custom types as needed
};
```

---

## Type Traits (Return Types and Constraints)

Provide a small traits surface that end users can extend:

```cpp
// 0) Canonical kind classification
enum class Kind { Scalar, Vec /*, Mat, ...*/ };
template <class T> struct kind_of; // specializations: kind_of<double>=Scalar, kind_of<Vec>=Vec, ...

// 1) Addition
template <class L, class R> struct result_of_add; // defaults to invalid
template <> struct result_of_add<Vec, Vec> { using type = Vec; };
template <> struct result_of_add<double, double> { using type = double; };
// Optional: scalar broadcast for vector add, if desired
template <> struct result_of_add<Vec, double> { using type = Vec; };
template <> struct result_of_add<double, Vec> { using type = Vec; };

// 2) Multiplication
template <class L, class R> struct result_of_mul; // defaults to invalid
template <> struct result_of_mul<Vec, Vec> { using type = double; }; // dot
template <> struct result_of_mul<double, Vec> { using type = Vec; };  // scale
template <> struct result_of_mul<Vec, double> { using type = Vec; };  // scale

// Convenience
template <class L, class R>
using result_of_add_t = typename result_of_add<L,R>::type;
template <class L, class R>
using result_of_mul_t = typename result_of_mul<L,R>::type;

// Concepts (C++20) or SFINAE helpers
template <class L, class R>
concept supports_add_v = requires { typename result_of_add_t<L,R>; };
template <class L, class R>
concept supports_mul_v = requires { typename result_of_mul_t<L,R>; };
```

Users specializing these traits for their custom types automatically enable the right operators and return types on the front-end.

---

## AST Nodes and Type Metadata

The AST core remains non-templated:

```cpp
struct Node {
  virtual ~Node() = default;
  Kind kind;                    // Scalar, Vec, ...
  std::size_t len = 0;          // for Vec; rows/cols for future types
  // children and op-specific payloads
};

struct AddNode : Node { std::shared_ptr<Node> a, b; };
struct NegNode : Node { std::shared_ptr<Node> a; };
struct MulNode : Node { std::shared_ptr<Node> a, b; /* used for scale/elemwise */ };
struct DotNode : Node { std::shared_ptr<Node> a, b; }; // optional explicit op
struct ConstNode : Node { std::any payload; };          // or a small type-erased holder
struct VarNode   : Node { std::size_t index; };
```

Operator overloads on the typed front-end compute result kind/shape via traits and fill them in when constructing nodes. For a `Vec*Vec->double` dot, we either emit a `DotNode` or reuse `MulNode` with an op-tag `DotOp`.

---

## Evaluation and Dispatch

Evaluation uses a type-erased `Value` variant when working in C++ runtime:

```cpp
using Value = std::variant<double, Vec /*, other registered types */>;

struct TypeInfo {
  // Kernels for each op, given child values (type-erased) -> result value
  std::function<Value(const Value&, const Value&)> add;
  std::function<Value(const Value&)>               neg;
  std::function<Value(const Value&, const Value&)> mul;  // used for scale/elemwise
  std::function<Value(const Value&, const Value&)> dot;  // optional distinct op
  // Future: div, exp, etc., as needed by your algebra
};

// Registry maps (Kind) and op to the right kernel. Shape metadata is on the node.
```

This design provides single-dispatch based on the node’s result kind (and op tag). Where the op depends on both operand kinds, the overload set is already resolved at construction time (front-end traits), so the node carries the correct op tag (e.g., DotOp vs MulOp).

---

## Reverse-Mode AD (Tape) for General Bilinear Ops

Tape nodes mirror AST ops (e.g., KAdd, KNeg, KMul, KDot). For VJP rules:

- Dot (`Vec·Vec -> Scalar`):
  - Forward: `y = dot(u, v)`
  - Backward: given upstream seed `g` (scalar),
    - `bar_u += g * v`
    - `bar_v += g * u`

- Scale (`Scalar*Vec -> Vec`):
  - Forward: `y = a * x`
  - Backward: given upstream seed `g` (Vec),
    - `bar_a += dot(g, x)`
    - `bar_x += a * g`

Implementing these requires the Tape backend to know how to:
  - Construct zeros of a given kind/shape
  - Multiply or dot values of certain kinds
  - Add into adjoints of a given kind/shape

These capabilities come from the same `TypeInfo` registry used for forward eval (you provide VJP helpers per op and kind combo).

---

## CSE, Normalization, and Rewrites with Types

- CSE keys should include: `(op_kind, result_kind, child_ids, shape metadata)`. For constants, include a stable key for the payload.
- Normalization/rewrites remain conservative across different kinds.
  - `Add` is AC only when both children have the same kind.
  - `Mul` is not commutative in general; elementwise/scale cases can be simplified via guarded rules.
  - Avoid touching bilinear ops (like `Dot`) unless the rule is obviously type-preserving.

---

## Backends (Torch, Etc.)

Torch lowering: map ops based on kind and arity:
- `Add(Vec,Vec)` → `aten::add`
- `Neg(Vec)` → `aten::neg`
- `Dot(Vec,Vec)` → `aten::dot`
- `Scale(Scalar,Vec)` → `aten::mul` (scalar broadcast)

For custom vector types backed by Torch tensors, emit the corresponding graph nodes. For non-tensor custom types, keep Tape-only execution.

---

## Extending with a New Vector Type (Example)

Suppose `struct MyVec { /* user storage */ };`.

1) Classify its kind:
```cpp
template <> struct kind_of<MyVec> { static constexpr Kind value = Kind::Vec; };
```

2) Add result-type traits:
```cpp
template <> struct result_of_add<MyVec, MyVec> { using type = MyVec; };
template <> struct result_of_mul<MyVec, MyVec> { using type = double; }; // dot
template <> struct result_of_mul<double, MyVec> { using type = MyVec; }; // scale
template <> struct result_of_mul<MyVec, double> { using type = MyVec; }; // scale
```

3) Register kernels (forward and VJP) in the type registry:
```cpp
TypeInfo info;
info.add = [](const Value& a, const Value& b) -> Value { return add_myvec(std::get<MyVec>(a), std::get<MyVec>(b)); };
info.neg = [](const Value& a) -> Value { return neg_myvec(std::get<MyVec>(a)); };
info.mul = /* scale or elementwise if you support it */;
info.dot = [](const Value& a, const Value& b) -> Value { return dot_myvec(std::get<MyVec>(a), std::get<MyVec>(b)); };
// VJP helpers used by Tape to backprop
register_type<MyVec>(info);
```

4) Use it from the front-end:
```cpp
ExprT<MyVec> u = var<MyVec>(0), v = var<MyVec>(1);
auto a = var<double>(2);
auto y = u * v;     // dot → ExprT<double>
auto z = a * u;     // scale → ExprT<MyVec>
```

---

## Migration Plan

Phase 1 (minimal viable):
- Add `ExprT<T>` and typed front-end overloads for Scalar + one Vec type.
- Node gets `kind` + shape; add DotNode (or Mul+DotOp tag).
- Eval for {add,neg,scale,dot} with a simple registry.
- Tape forward + basic VJP for dot/scale.

Phase 2 (hardening):
- Extend to multiple vector-like types and broadcasting rules as needed.
- Torch lowering for supported ops.
- Add CSE hashing by kind/shape and typed normalization guards.

Phase 3 (optional):
- Add more ops (elementwise nonlinearities), richer shape APIs, and richer backends.

---

## Notes & Trade-offs

- Why not template Node on `T`?  
  That approach leads to exponential instantiation and larger binaries; a type-erased core with a typed front-end provides return-type deduction and keeps runtime simple.

- How do we “fall back” to native operators?  
  The front-end only intercepts when at least one operand is an `ExprT<T>`. Native–native operations behave as usual; mixing native and `ExprT<T>` uses `lit(...)` conversions.

- Where do AD rules live?  
  For scalar ops, AD is as today. For vector-like ops, register VJP callbacks with the type registry; the Tape backend calls them during backward.

- Shapes vs. sizes:  
  For small fixed-size vectors, shapes may be compile-time. You can still carry runtime `len` for verification and error messages. The design works either way.

---

## Takeaway

This design keeps the AST runtime small and generic while letting C++ determine return types for mixed operations at the call site. It plays well with user-defined vector types (e.g., dot products, scaling) and extends naturally to other kinds later, with well-defined places to plug in evaluation and AD rules.

