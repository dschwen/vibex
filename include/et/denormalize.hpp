#pragma once
#include <vector>
#include <memory>
#include "et/ast.hpp"
#include "et/normalize.hpp"

namespace et {

inline Expr denormalize_sub(const Expr& e) {
  if (!e.n) return e;
  if (std::dynamic_pointer_cast<ConstNode>(e.n)) return e;
  if (std::dynamic_pointer_cast<VarNode>(e.n))   return e;
  if (auto n = std::dynamic_pointer_cast<NegNode>(e.n))   return -denormalize_sub(Expr{n->a});
  if (auto n = std::dynamic_pointer_cast<SinNode>(e.n))   return sin(denormalize_sub(Expr{n->a}));
  if (auto n = std::dynamic_pointer_cast<CosNode>(e.n))   return cos(denormalize_sub(Expr{n->a}));
  if (auto n = std::dynamic_pointer_cast<ExpNode>(e.n))   return exp(denormalize_sub(Expr{n->a}));
  if (auto n = std::dynamic_pointer_cast<LogNode>(e.n))   return log(denormalize_sub(Expr{n->a}));
  if (auto n = std::dynamic_pointer_cast<SqrtNode>(e.n))  return sqrt(denormalize_sub(Expr{n->a}));
  if (auto n = std::dynamic_pointer_cast<TanhNode>(e.n))  return tanh(denormalize_sub(Expr{n->a}));
  if (auto n = std::dynamic_pointer_cast<MulNode>(e.n))   return denormalize_sub(Expr{n->a}) * denormalize_sub(Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<DivNode>(e.n))   return denormalize_sub(Expr{n->a}) / denormalize_sub(Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<PowNode>(e.n))   return pow(denormalize_sub(Expr{n->a}), denormalize_sub(Expr{n->b}));

  if (auto an = std::dynamic_pointer_cast<AddNode>(e.n)) {
    std::vector<Expr> terms; terms.reserve(4);
    gather_add(e, terms);
    std::vector<Expr> pos, neg;
    for (auto& t : terms) {
      auto tn = t.n;
      if (auto n = std::dynamic_pointer_cast<NegNode>(tn)) {
        neg.push_back(denormalize_sub(Expr{n->a}));
      } else if (auto c = std::dynamic_pointer_cast<ConstNode>(tn)) {
        if (c->value < 0.0) neg.push_back(lit(-c->value)); else pos.push_back(t);
      } else {
        pos.push_back(denormalize_sub(t));
      }
    }
    auto make_add_local = [&](const std::vector<Expr>& xs){ return normalize(make_add(xs)); };
    if (pos.empty() && !neg.empty()) { Expr inner = make_add_local(neg); return -inner; }
    if (terms.size() == 2) {
      auto a = denormalize_sub(Expr{an->a});
      auto b = denormalize_sub(Expr{an->b});
      if (auto nb = std::dynamic_pointer_cast<NegNode>(b.n)) return a - Expr{nb->a};
      if (auto na = std::dynamic_pointer_cast<NegNode>(a.n)) return b - Expr{na->a};
      if (auto cb = std::dynamic_pointer_cast<ConstNode>(b.n); cb && cb->value < 0.0) return a - lit(-cb->value);
      if (auto ca = std::dynamic_pointer_cast<ConstNode>(a.n); ca && ca->value < 0.0) return b - lit(-ca->value);
      return a + b;
    }
    if (pos.size() == 1 && pos.size() + neg.size() == terms.size() && !neg.empty()) {
      Expr rhs = make_add_local(neg);
      return pos[0] - rhs;
    }
    std::vector<Expr> rebuilt; rebuilt.reserve(terms.size());
    for (auto& t : terms) rebuilt.push_back(denormalize_sub(t));
    return normalize(make_add(rebuilt));
  }

  if (auto n = std::dynamic_pointer_cast<SubNode>(e.n)) return denormalize_sub(Expr{n->a}) - denormalize_sub(Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<LtNode>(e.n))   return denormalize_sub(Expr{n->a}) <  denormalize_sub(Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<LeNode>(e.n))   return denormalize_sub(Expr{n->a}) <= denormalize_sub(Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<GtNode>(e.n))   return denormalize_sub(Expr{n->a}) >  denormalize_sub(Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<GeNode>(e.n))   return denormalize_sub(Expr{n->a}) >= denormalize_sub(Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<EqNode>(e.n))   return denormalize_sub(Expr{n->a}) == denormalize_sub(Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<NeNode>(e.n))   return denormalize_sub(Expr{n->a}) != denormalize_sub(Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<NotNode>(e.n))  return Not(denormalize_sub(Expr{n->a}));
  if (auto n = std::dynamic_pointer_cast<IfNode>(e.n))   return If(denormalize_sub(Expr{n->c}), denormalize_sub(Expr{n->t}), denormalize_sub(Expr{n->e}));
  if (auto n = std::dynamic_pointer_cast<SelectNode>(e.n))return Select(denormalize_sub(Expr{n->m}), denormalize_sub(Expr{n->t}), denormalize_sub(Expr{n->e}));
  if (auto n = std::dynamic_pointer_cast<IterNode>(e.n)) return e;
  if (auto n = std::dynamic_pointer_cast<StateReadNode>(e.n)) return e;
  if (auto n = std::dynamic_pointer_cast<LoopForNode>(e.n)) {
    std::vector<Expr> ch; ch.reserve(n->ch.size()); for (auto& c : n->ch) ch.push_back(denormalize_sub(Expr{c}));
    std::vector<std::shared_ptr<Node>> chp; chp.reserve(ch.size()); for (auto& ce : ch) chp.push_back(ce.n);
    return Expr{ std::make_shared<LoopForNode>(n->K, std::move(chp)) };
  }
  if (auto n = std::dynamic_pointer_cast<LoopOutNode>(e.n)) return loop_out(n->J, denormalize_sub(Expr{n->loop}));
  return e;
}

} // namespace et

