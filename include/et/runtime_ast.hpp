#pragma once
#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <type_traits>
#include <algorithm>

#include "et/expr.hpp"

namespace et {

// Runtime AST kinds mirror ET Op tags
enum class NodeKind : uint8_t {
  Var, Const,
  Add, Sub, Mul, Div,
  Neg, Sin, Cos, Exp, Log, Sqrt, Tanh
};

struct RNode {
  NodeKind kind{};
  std::vector<int> ch;     // children node ids
  double cval = 0.0;       // for Const
  std::size_t var_index{}; // for Var
};

struct RGraph {
  std::vector<RNode> nodes;
  int root = -1;

  int add(RNode n) {
    nodes.push_back(std::move(n));
    return (int)nodes.size() - 1;
  }
};

// Map ET op tag to NodeKind
template <class Op> struct nodekind_of;
template <> struct nodekind_of<AddOp>  { static constexpr NodeKind value = NodeKind::Add; };
template <> struct nodekind_of<SubOp>  { static constexpr NodeKind value = NodeKind::Sub; };
template <> struct nodekind_of<MulOp>  { static constexpr NodeKind value = NodeKind::Mul; };
template <> struct nodekind_of<DivOp>  { static constexpr NodeKind value = NodeKind::Div; };
template <> struct nodekind_of<NegOp>  { static constexpr NodeKind value = NodeKind::Neg; };
template <> struct nodekind_of<SinOp>  { static constexpr NodeKind value = NodeKind::Sin; };
template <> struct nodekind_of<CosOp>  { static constexpr NodeKind value = NodeKind::Cos; };
template <> struct nodekind_of<ExpOp>  { static constexpr NodeKind value = NodeKind::Exp; };
template <> struct nodekind_of<LogOp>  { static constexpr NodeKind value = NodeKind::Log; };
template <> struct nodekind_of<SqrtOp> { static constexpr NodeKind value = NodeKind::Sqrt; };
template <> struct nodekind_of<TanhOp> { static constexpr NodeKind value = NodeKind::Tanh; };

// Compile ET expression to runtime graph (returns node id)
template <class T, std::size_t I>
inline int compile_to_runtime(const Var<T,I>&, RGraph& g) {
  RNode n; n.kind = NodeKind::Var; n.var_index = I; return g.add(std::move(n));
}
template <class T>
inline int compile_to_runtime(const Const<T>& c, RGraph& g) {
  RNode n; n.kind = NodeKind::Const; n.cval = static_cast<double>(c.value); return g.add(std::move(n));
}
template <class Op, class... Ch>
inline int compile_to_runtime(const Apply<Op,Ch...>& a, RGraph& g) {
  RNode n; n.kind = nodekind_of<Op>::value;
  // Recurse over children
  constexpr std::size_t N = sizeof...(Ch);
  n.ch.reserve(N);
  std::apply([&](const auto&... c){ (n.ch.push_back(compile_to_runtime(c, g)), ...); }, a.ch);
  return g.add(std::move(n));
}

// Structural equality on subtrees
inline bool r_equal(const RGraph& g, int a, int b) {
  if (a == b) return true;
  const RNode& na = g.nodes[a];
  const RNode& nb = g.nodes[b];
  if (na.kind != nb.kind) return false;
  if (na.kind == NodeKind::Const) return na.cval == nb.cval;
  if (na.kind == NodeKind::Var)   return na.var_index == nb.var_index;
  if (na.ch.size() != nb.ch.size()) return false;
  for (std::size_t i = 0; i < na.ch.size(); ++i)
    if (!r_equal(g, na.ch[i], nb.ch[i])) return false;
  return true;
}

// Convert runtime graph back to ET expression (templated on scalar type)
template <class T>
struct ReifyET {
  using Expr = Const<T>; // placeholder type alias; actual type is variant of ET nodes

  static auto build(const RGraph& g, int id) {
    const RNode& n = g.nodes[id];
    switch (n.kind) {
      case NodeKind::Const: return lit(static_cast<T>(n.cval));
      case NodeKind::Var:   return Var<T, 0 + 0>{}.template operator()<T>(T{}), lit(T{}); // dummy to silence warnings
      default: break; // handled below via helpers
    }
    // Non-leaf helpers
    auto child = [&](int cid){ return build(g, cid); };
    switch (n.kind) {
      case NodeKind::Neg: {
        auto a = child(n.ch[0]);
        return Apply<NegOp, decltype(a)>(std::move(a));
      }
      case NodeKind::Sin: { auto a = child(n.ch[0]); return Apply<SinOp, decltype(a)>(std::move(a)); }
      case NodeKind::Cos: { auto a = child(n.ch[0]); return Apply<CosOp, decltype(a)>(std::move(a)); }
      case NodeKind::Exp: { auto a = child(n.ch[0]); return Apply<ExpOp, decltype(a)>(std::move(a)); }
      case NodeKind::Log: { auto a = child(n.ch[0]); return Apply<LogOp, decltype(a)>(std::move(a)); }
      case NodeKind::Sqrt:{ auto a = child(n.ch[0]); return Apply<SqrtOp, decltype(a)>(std::move(a)); }
      case NodeKind::Tanh:{ auto a = child(n.ch[0]); return Apply<TanhOp, decltype(a)>(std::move(a)); }
      case NodeKind::Add:  return fold_nary<AddOp>(g, n);
      case NodeKind::Sub:  return make_bin<SubOp>(g, n);
      case NodeKind::Mul:  return fold_nary<MulOp>(g, n);
      case NodeKind::Div:  return make_bin<DivOp>(g, n);
      default: break;
    }
    // Unreachable
    return lit(static_cast<T>(0));
  }

private:
  template <class Op>
  static auto make_bin(const RGraph& g, const RNode& n) {
    auto a = build(g, n.ch[0]);
    auto b = build(g, n.ch[1]);
    return Apply<Op, decltype(a), decltype(b)>(std::move(a), std::move(b));
  }
  template <class Op>
  static auto fold_nary(const RGraph& g, const RNode& n) {
    // Left fold: (((c0 op c1) op c2) ...)
    auto acc = build(g, n.ch[0]);
    for (std::size_t i = 1; i < n.ch.size(); ++i) {
      auto rhs = build(g, n.ch[i]);
      acc = Apply<Op, decltype(acc), decltype(rhs)>(std::move(acc), std::move(rhs));
    }
    return acc;
  }
};

// Specialized creators for leaves to avoid needing runtime info about Var template index
// We reconstruct variables by scanning var_index and instantiating Var<T,I> via a factory.
// Helper: make Var<T,I> by index at compile-time using a small switch (bounded by reasonable arity).
template <class T, std::size_t I>
inline auto make_var_by_index(std::integral_constant<std::size_t, I>) { return Var<T, I>{}; }

template <class T>
inline auto reify_var(std::size_t idx) {
  // Support a reasonable upper bound (e.g., 64). For larger, fallback to runtime error in debug.
  switch (idx) {
    case 0: return Var<T,0>{}; case 1: return Var<T,1>{}; case 2: return Var<T,2>{}; case 3: return Var<T,3>{};
    case 4: return Var<T,4>{}; case 5: return Var<T,5>{}; case 6: return Var<T,6>{}; case 7: return Var<T,7>{};
    case 8: return Var<T,8>{}; case 9: return Var<T,9>{}; case 10: return Var<T,10>{}; case 11: return Var<T,11>{};
    case 12: return Var<T,12>{}; case 13: return Var<T,13>{}; case 14: return Var<T,14>{}; case 15: return Var<T,15>{};
    default: return Var<T,0>{};
  }
}

template <class T>
inline auto to_et(const RGraph& g) {
  // Local lambda to build recursively with proper Var construction
  struct Builder {
    static auto build(const RGraph& g, int id) {
      const RNode& n = g.nodes[id];
      switch (n.kind) {
        case NodeKind::Const: return lit(static_cast<T>(n.cval));
        case NodeKind::Var:   return reify_var<T>(n.var_index);
        case NodeKind::Neg: { auto a = build(g, n.ch[0]); return Apply<NegOp, decltype(a)>(std::move(a)); }
        case NodeKind::Sin: { auto a = build(g, n.ch[0]); return Apply<SinOp, decltype(a)>(std::move(a)); }
        case NodeKind::Cos: { auto a = build(g, n.ch[0]); return Apply<CosOp, decltype(a)>(std::move(a)); }
        case NodeKind::Exp: { auto a = build(g, n.ch[0]); return Apply<ExpOp, decltype(a)>(std::move(a)); }
        case NodeKind::Log: { auto a = build(g, n.ch[0]); return Apply<LogOp, decltype(a)>(std::move(a)); }
        case NodeKind::Sqrt:{ auto a = build(g, n.ch[0]); return Apply<SqrtOp, decltype(a)>(std::move(a)); }
        case NodeKind::Tanh:{ auto a = build(g, n.ch[0]); return Apply<TanhOp, decltype(a)>(std::move(a)); }
        case NodeKind::Add:  return fold_nary<AddOp>(g, n);
        case NodeKind::Sub:  return make_bin<SubOp>(g, n);
        case NodeKind::Mul:  return fold_nary<MulOp>(g, n);
        case NodeKind::Div:  return make_bin<DivOp>(g, n);
      }
      return lit(static_cast<T>(0));
    }
    template <class Op>
    static auto make_bin(const RGraph& g, const RNode& n) {
      auto a = build(g, n.ch[0]);
      auto b = build(g, n.ch[1]);
      return Apply<Op, decltype(a), decltype(b)>(std::move(a), std::move(b));
    }
    template <class Op>
    static auto fold_nary(const RGraph& g, const RNode& n) {
      auto acc = build(g, n.ch[0]);
      for (std::size_t i = 1; i < n.ch.size(); ++i) {
        auto rhs = build(g, n.ch[i]);
        acc = Apply<Op, decltype(acc), decltype(rhs)>(std::move(acc), std::move(rhs));
      }
      return acc;
    }
  };
  return Builder::build(g, g.root);
}

// Entry: end-to-end ET -> runtime graph
template <class Expr>
inline RGraph compile_to_runtime(const Expr& e) {
  RGraph g;
  g.root = compile_to_runtime(e, g);
  return g;
}

} // namespace et

