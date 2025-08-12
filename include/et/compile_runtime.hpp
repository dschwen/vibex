#pragma once
#include <vector>
#include <type_traits>

#include "et/runtime_ast.hpp"
#include "et/expr.hpp"

namespace et {

// Compile a runtime AST (RGraph) into a Backend using Backend's emitVar/emitConst/emitApply API.
// Assumes Backend result_type is a handle and Backend supports the ET Op tag mapping.
template <class Backend>
inline auto compile_runtime(const RGraph& g, Backend& b) -> typename Backend::result_type {
  using R = typename Backend::result_type;
  std::vector<R> memo(g.nodes.size());

  std::function<R(int)> rec = [&](int id) -> R {
    const RNode& n = g.nodes[id];
    switch (n.kind) {
      case NodeKind::Const: {
        return b.template emitConst<double>(Const<double>{ static_cast<double>(n.cval) });
      }
      case NodeKind::Var: {
        return b.template emitVar<double>(n.var_index);
      }
      case NodeKind::Neg: {
        auto a = rec(n.ch[0]);
        return b.emitApply(NegOp{}, a);
      }
      case NodeKind::Sin: { auto a = rec(n.ch[0]); return b.emitApply(SinOp{}, a); }
      case NodeKind::Cos: { auto a = rec(n.ch[0]); return b.emitApply(CosOp{}, a); }
      case NodeKind::Exp: { auto a = rec(n.ch[0]); return b.emitApply(ExpOp{}, a); }
      case NodeKind::Log: { auto a = rec(n.ch[0]); return b.emitApply(LogOp{}, a); }
      case NodeKind::Sqrt:{ auto a = rec(n.ch[0]); return b.emitApply(SqrtOp{}, a); }
      case NodeKind::Tanh:{ auto a = rec(n.ch[0]); return b.emitApply(TanhOp{}, a); }
      case NodeKind::Sub: {
        auto a = rec(n.ch[0]); auto c = rec(n.ch[1]);
        return b.emitApply(SubOp{}, a, c);
      }
      case NodeKind::Div: {
        auto a = rec(n.ch[0]); auto c = rec(n.ch[1]);
        return b.emitApply(DivOp{}, a, c);
      }
      case NodeKind::Pow: {
        auto a = rec(n.ch[0]); auto c = rec(n.ch[1]);
        return b.emitApply(PowOp{}, a, c);
      }
      case NodeKind::Add: {
        auto acc = rec(n.ch[0]);
        for (std::size_t i = 1; i < n.ch.size(); ++i) {
          auto rhs = rec(n.ch[i]);
          acc = b.emitApply(AddOp{}, acc, rhs);
        }
        return acc;
      }
      case NodeKind::Mul: {
        auto acc = rec(n.ch[0]);
        for (std::size_t i = 1; i < n.ch.size(); ++i) {
          auto rhs = rec(n.ch[i]);
          acc = b.emitApply(MulOp{}, acc, rhs);
        }
        return acc;
      }
    }
    // Unreachable
    return b.template emitConst<double>(Const<double>{0.0});
  };

  return rec(g.root);
}

} // namespace et
