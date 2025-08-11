#pragma once
#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <type_traits>
#include <algorithm>
#include <functional>
#include <limits>
#include <cmath>
#include <sstream>
#include <string>

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

// Evaluate runtime graph numerically given input vector (by var_index)
inline double eval(const RGraph& g, const std::vector<double>& inputs) {
  std::vector<double> memo(g.nodes.size(), std::numeric_limits<double>::quiet_NaN());
  std::function<double(int)> rec = [&](int id) -> double {
    double& slot = memo[id];
    if (slot == slot) return slot; // not NaN => already computed
    const RNode& n = g.nodes[id];
    switch (n.kind) {
      case NodeKind::Const: slot = n.cval; break;
      case NodeKind::Var:   slot = inputs[n.var_index]; break;
      case NodeKind::Add:   slot = rec(n.ch[0]); for (std::size_t i=1;i<n.ch.size();++i) slot += rec(n.ch[i]); break;
      case NodeKind::Mul:   slot = rec(n.ch[0]); for (std::size_t i=1;i<n.ch.size();++i) slot *= rec(n.ch[i]); break;
      case NodeKind::Sub:   slot = rec(n.ch[0]) - rec(n.ch[1]); break;
      case NodeKind::Div:   slot = rec(n.ch[0]) / rec(n.ch[1]); break;
      case NodeKind::Neg:   slot = -rec(n.ch[0]); break;
      case NodeKind::Sin:   slot = std::sin(rec(n.ch[0])); break;
      case NodeKind::Cos:   slot = std::cos(rec(n.ch[0])); break;
      case NodeKind::Exp:   slot = std::exp(rec(n.ch[0])); break;
      case NodeKind::Log:   slot = std::log(rec(n.ch[0])); break;
      case NodeKind::Sqrt:  slot = std::sqrt(rec(n.ch[0])); break;
      case NodeKind::Tanh:  slot = std::tanh(rec(n.ch[0])); break;
    }
    return slot;
  };
  return rec(g.root);
}

// Deterministic structural string for testing/canonical comparison
inline std::string r_to_string(const RGraph& g) {
  std::function<std::string(int)> rec = [&](int id) -> std::string {
    const RNode& n = g.nodes[id];
    auto fmt_c = [&](double v){
      std::ostringstream os;
      double r = std::round(v);
      if (std::fabs(v - r) < 1e-12) os << static_cast<long long>(r);
      else { os.setf(std::ios::fixed); os.precision(12); os << v; }
      return std::string("C(") + os.str() + ")";
    };
    auto join = [&](const char* name){
      std::string s; s += name; s += '(';
      for (std::size_t i=0;i<n.ch.size();++i) {
        if (i) s += ',';
        s += rec(n.ch[i]);
      }
      s += ')'; return s;
    };
    switch (n.kind) {
      case NodeKind::Const: return fmt_c(n.cval);
      case NodeKind::Var:   return std::string("V(") + std::to_string(n.var_index) + ")";
      case NodeKind::Add:   return join("Add");
      case NodeKind::Mul:   return join("Mul");
      case NodeKind::Sub:   return join("Sub");
      case NodeKind::Div:   return join("Div");
      case NodeKind::Neg:   return std::string("Neg(") + rec(n.ch[0]) + ")";
      case NodeKind::Sin:   return std::string("Sin(") + rec(n.ch[0]) + ")";
      case NodeKind::Cos:   return std::string("Cos(") + rec(n.ch[0]) + ")";
      case NodeKind::Exp:   return std::string("Exp(") + rec(n.ch[0]) + ")";
      case NodeKind::Log:   return std::string("Log(") + rec(n.ch[0]) + ")";
      case NodeKind::Sqrt:  return std::string("Sqrt(") + rec(n.ch[0]) + ")";
      case NodeKind::Tanh:  return std::string("Tanh(") + rec(n.ch[0]) + ")";
    }
    return "";
  };
  return rec(g.root);
}

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

// Forward declaration for recursive builder
template <class T>
inline auto build_et(const RGraph& g, int id);

template <class T, class Op>
inline auto make_bin_build(const RGraph& g, const RNode& n) {
  auto a = build_et<T>(g, n.ch[0]);
  auto b = build_et<T>(g, n.ch[1]);
  return Apply<Op, decltype(a), decltype(b)>(std::move(a), std::move(b));
}

template <class T, class Op>
inline auto fold_nary_build(const RGraph& g, const RNode& n) {
  auto acc = build_et<T>(g, n.ch[0]);
  for (std::size_t i = 1; i < n.ch.size(); ++i) {
    auto rhs = build_et<T>(g, n.ch[i]);
    acc = Apply<Op, decltype(acc), decltype(rhs)>(std::move(acc), std::move(rhs));
  }
  return acc;
}

template <class T>
inline auto build_et(const RGraph& g, int id) {
  const RNode& n = g.nodes[id];
  switch (n.kind) {
    case NodeKind::Const: return lit(static_cast<T>(n.cval));
    case NodeKind::Var:   return reify_var<T>(n.var_index);
    case NodeKind::Neg: { auto a = build_et<T>(g, n.ch[0]); return Apply<NegOp, decltype(a)>(std::move(a)); }
    case NodeKind::Sin: { auto a = build_et<T>(g, n.ch[0]); return Apply<SinOp, decltype(a)>(std::move(a)); }
    case NodeKind::Cos: { auto a = build_et<T>(g, n.ch[0]); return Apply<CosOp, decltype(a)>(std::move(a)); }
    case NodeKind::Exp: { auto a = build_et<T>(g, n.ch[0]); return Apply<ExpOp, decltype(a)>(std::move(a)); }
    case NodeKind::Log: { auto a = build_et<T>(g, n.ch[0]); return Apply<LogOp, decltype(a)>(std::move(a)); }
    case NodeKind::Sqrt:{ auto a = build_et<T>(g, n.ch[0]); return Apply<SqrtOp, decltype(a)>(std::move(a)); }
    case NodeKind::Tanh:{ auto a = build_et<T>(g, n.ch[0]); return Apply<TanhOp, decltype(a)>(std::move(a)); }
    case NodeKind::Add:  return fold_nary_build<T,AddOp>(g, n);
    case NodeKind::Sub:  return make_bin_build<T,SubOp>(g, n);
    case NodeKind::Mul:  return fold_nary_build<T,MulOp>(g, n);
    case NodeKind::Div:  return make_bin_build<T,DivOp>(g, n);
  }
  return lit(static_cast<T>(0));
}

template <class T>
inline auto to_et(const RGraph& g) {
  return build_et<T>(g, g.root);
}

// Entry: end-to-end ET -> runtime graph
template <class Expr>
inline RGraph compile_to_runtime(const Expr& e) {
  RGraph g;
  g.root = compile_to_runtime(e, g);
  return g;
}

} // namespace et
