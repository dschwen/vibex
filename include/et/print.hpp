#pragma once
#include <sstream>
#include <memory>
#include <functional>
#include "et/ast.hpp"

namespace et {

inline std::string to_string_pretty(const Expr& e) {
  std::function<std::string(const std::shared_ptr<Node>&)> rec = [&](const std::shared_ptr<Node>& p) -> std::string {
    if (!p) return "()";
    if (auto c = std::dynamic_pointer_cast<ConstNode>(p)) { std::ostringstream os; os<<"C("<<c->value<<")"; return os.str(); }
    if (auto v = std::dynamic_pointer_cast<VarNode>(p))   { return std::string("V(") + std::to_string(v->index) + ")"; }
    if (auto n = std::dynamic_pointer_cast<NegNode>(p))   return std::string("Neg(") + rec(n->a) + ")";
    if (auto n = std::dynamic_pointer_cast<SinNode>(p))   return std::string("Sin(") + rec(n->a) + ")";
    if (auto n = std::dynamic_pointer_cast<CosNode>(p))   return std::string("Cos(") + rec(n->a) + ")";
    if (auto n = std::dynamic_pointer_cast<ExpNode>(p))   return std::string("Exp(") + rec(n->a) + ")";
    if (auto n = std::dynamic_pointer_cast<LogNode>(p))   return std::string("Log(") + rec(n->a) + ")";
    if (auto n = std::dynamic_pointer_cast<SqrtNode>(p))  return std::string("Sqrt(") + rec(n->a) + ")";
    if (auto n = std::dynamic_pointer_cast<TanhNode>(p))  return std::string("Tanh(") + rec(n->a) + ")";
    if (auto n = std::dynamic_pointer_cast<AddNode>(p))   return std::string("Add(") + rec(n->a) + "," + rec(n->b) + ")";
    if (auto n = std::dynamic_pointer_cast<SubNode>(p))   return std::string("Sub(") + rec(n->a) + "," + rec(n->b) + ")";
    if (auto n = std::dynamic_pointer_cast<MulNode>(p))   return std::string("Mul(") + rec(n->a) + "," + rec(n->b) + ")";
    if (auto n = std::dynamic_pointer_cast<DivNode>(p))   return std::string("Div(") + rec(n->a) + "," + rec(n->b) + ")";
    if (auto n = std::dynamic_pointer_cast<PowNode>(p))   return std::string("Pow(") + rec(n->a) + "," + rec(n->b) + ")";
    if (auto n = std::dynamic_pointer_cast<LtNode>(p))    return std::string("Lt(")  + rec(n->a) + "," + rec(n->b) + ")";
    if (auto n = std::dynamic_pointer_cast<LeNode>(p))    return std::string("Le(")  + rec(n->a) + "," + rec(n->b) + ")";
    if (auto n = std::dynamic_pointer_cast<GtNode>(p))    return std::string("Gt(")  + rec(n->a) + "," + rec(n->b) + ")";
    if (auto n = std::dynamic_pointer_cast<GeNode>(p))    return std::string("Ge(")  + rec(n->a) + "," + rec(n->b) + ")";
    if (auto n = std::dynamic_pointer_cast<EqNode>(p))    return std::string("Eq(")  + rec(n->a) + "," + rec(n->b) + ")";
    if (auto n = std::dynamic_pointer_cast<NeNode>(p))    return std::string("Ne(")  + rec(n->a) + "," + rec(n->b) + ")";
    if (auto n = std::dynamic_pointer_cast<NotNode>(p))   return std::string("Not(") + rec(n->a) + ")";
    if (auto n = std::dynamic_pointer_cast<IfNode>(p))    return std::string("If(") + rec(n->c) + "," + rec(n->t) + "," + rec(n->e) + ")";
    if (auto n = std::dynamic_pointer_cast<SelectNode>(p))return std::string("Select(") + rec(n->m) + "," + rec(n->t) + "," + rec(n->e) + ")";
    if (auto n = std::dynamic_pointer_cast<IterNode>(p))  return std::string("Iter()");
    if (auto n = std::dynamic_pointer_cast<StateReadNode>(p)) return std::string("State(") + std::to_string(n->index) + ")";
    if (auto n = std::dynamic_pointer_cast<LoopForNode>(p)) {
      std::string s; s = "LoopFor(";
      for (std::size_t i = 0; i < n->ch.size(); ++i) { if (i) s+=","; s += rec(n->ch[i]); }
      s += ")"; return s;
    }
    if (auto n = std::dynamic_pointer_cast<LoopOutNode>(p)) return std::string("Out(") + std::to_string(n->J) + "," + rec(n->loop) + ")";
    return "?";
  };
  return rec(e.n);
}

} // namespace et

