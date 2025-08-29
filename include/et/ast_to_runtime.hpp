#pragma once
#include <memory>
#include "et/ast.hpp"
#include "et/runtime_ast.hpp"

namespace et {

inline RGraph ast_to_rgraph(const Expr& e) {
  RGraph g;
  std::function<int(const std::shared_ptr<Node>&)> rec;
  rec = [&](const std::shared_ptr<Node>& p) -> int {
    if (!p) return -1;
    if (auto q = std::dynamic_pointer_cast<ConstNode>(p)) {
      RNode n; n.kind = NodeKind::Const; n.cval = q->value; return g.add(std::move(n));
    }
    if (auto q = std::dynamic_pointer_cast<VarNode>(p)) {
      RNode n; n.kind = NodeKind::Var; n.var_index = q->index; return g.add(std::move(n));
    }
    auto emit_un = [&](NodeKind k, const std::shared_ptr<Node>& a){ RNode n; n.kind=k; n.ch = { rec(a) }; return g.add(std::move(n)); };
    auto emit_bin= [&](NodeKind k, const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b){ RNode n; n.kind=k; n.ch = { rec(a), rec(b)}; return g.add(std::move(n)); };
    if (auto q = std::dynamic_pointer_cast<NegNode>(p))   return emit_un(NodeKind::Neg, q->a);
    if (auto q = std::dynamic_pointer_cast<SinNode>(p))   return emit_un(NodeKind::Sin, q->a);
    if (auto q = std::dynamic_pointer_cast<CosNode>(p))   return emit_un(NodeKind::Cos, q->a);
    if (auto q = std::dynamic_pointer_cast<ExpNode>(p))   return emit_un(NodeKind::Exp, q->a);
    if (auto q = std::dynamic_pointer_cast<LogNode>(p))   return emit_un(NodeKind::Log, q->a);
    if (auto q = std::dynamic_pointer_cast<SqrtNode>(p))  return emit_un(NodeKind::Sqrt,q->a);
    if (auto q = std::dynamic_pointer_cast<TanhNode>(p))  return emit_un(NodeKind::Tanh,q->a);
    if (auto q = std::dynamic_pointer_cast<AddNode>(p))   return emit_bin(NodeKind::Add, q->a, q->b);
    if (auto q = std::dynamic_pointer_cast<SubNode>(p))   return emit_bin(NodeKind::Sub, q->a, q->b);
    if (auto q = std::dynamic_pointer_cast<MulNode>(p))   return emit_bin(NodeKind::Mul, q->a, q->b);
    if (auto q = std::dynamic_pointer_cast<DivNode>(p))   return emit_bin(NodeKind::Div, q->a, q->b);
    if (auto q = std::dynamic_pointer_cast<PowNode>(p))   return emit_bin(NodeKind::Pow, q->a, q->b);
    if (auto q = std::dynamic_pointer_cast<LtNode>(p))    return emit_bin(NodeKind::Lt, q->a, q->b);
    if (auto q = std::dynamic_pointer_cast<LeNode>(p))    return emit_bin(NodeKind::Le, q->a, q->b);
    if (auto q = std::dynamic_pointer_cast<GtNode>(p))    return emit_bin(NodeKind::Gt, q->a, q->b);
    if (auto q = std::dynamic_pointer_cast<GeNode>(p))    return emit_bin(NodeKind::Ge, q->a, q->b);
    if (auto q = std::dynamic_pointer_cast<EqNode>(p))    return emit_bin(NodeKind::Eq, q->a, q->b);
    if (auto q = std::dynamic_pointer_cast<NeNode>(p))    return emit_bin(NodeKind::Ne, q->a, q->b);
    if (auto q = std::dynamic_pointer_cast<IfNode>(p))    { RNode n; n.kind = NodeKind::If;    n.ch = { rec(q->c), rec(q->t), rec(q->e)}; return g.add(std::move(n)); }
    if (auto q = std::dynamic_pointer_cast<SelectNode>(p)){ RNode n; n.kind = NodeKind::Select;n.ch = { rec(q->m), rec(q->t), rec(q->e)}; return g.add(std::move(n)); }
    // Fallback constant
    RNode n; n.kind = NodeKind::Const; n.cval = 0.0; return g.add(std::move(n));
  };
  g.root = rec(e.n);
  return g;
}

} // namespace et

