#pragma once
#include <unordered_map>
#include <string>
#include <sstream>
#include "et/ast.hpp"
#include "et/normalize.hpp"
#include "et/expr.hpp"
#include "et/tape_backend.hpp"

namespace et {

inline std::string ast_key(const Expr& e) {
  if (!e.n) return "()";
  std::ostringstream os;
  auto p = e.n;
  auto key1 = [&](const char* name, const Expr& a){ os<<name<<'('<<ast_key(a)<<')'; };
  auto key2 = [&](const char* name, const Expr& a, const Expr& b){ os<<name<<'('<<ast_key(a)<<','<<ast_key(b)<<')'; };
  if (auto c = std::dynamic_pointer_cast<ConstNode>(p)) { os<<"C("<<c->value<<")"; return os.str(); }
  if (auto v = std::dynamic_pointer_cast<VarNode>(p))   { os<<"V("<<v->index<<")"; return os.str(); }
  if (std::dynamic_pointer_cast<IterNode>(p))           { os<<"Iter()"; return os.str(); }
  if (auto s = std::dynamic_pointer_cast<StateReadNode>(p)) { os<<"State("<<s->index<<")"; return os.str(); }
  if (auto n = std::dynamic_pointer_cast<NotNode>(p))   { key1("Not", Expr{n->a}); return os.str(); }
  if (std::dynamic_pointer_cast<NegNode>(p))  { key1("Neg", Expr{std::dynamic_pointer_cast<NegNode>(p)->a}); return os.str(); }
  if (std::dynamic_pointer_cast<SinNode>(p))  { key1("Sin", Expr{std::dynamic_pointer_cast<SinNode>(p)->a}); return os.str(); }
  if (std::dynamic_pointer_cast<CosNode>(p))  { key1("Cos", Expr{std::dynamic_pointer_cast<CosNode>(p)->a}); return os.str(); }
  if (std::dynamic_pointer_cast<ExpNode>(p))  { key1("Exp", Expr{std::dynamic_pointer_cast<ExpNode>(p)->a}); return os.str(); }
  if (std::dynamic_pointer_cast<LogNode>(p))  { key1("Log", Expr{std::dynamic_pointer_cast<LogNode>(p)->a}); return os.str(); }
  if (std::dynamic_pointer_cast<SqrtNode>(p)) { key1("Sqrt",Expr{std::dynamic_pointer_cast<SqrtNode>(p)->a}); return os.str(); }
  if (std::dynamic_pointer_cast<TanhNode>(p)) { key1("Tanh",Expr{std::dynamic_pointer_cast<TanhNode>(p)->a}); return os.str(); }
  if (std::dynamic_pointer_cast<AddNode>(p))  { key2("Add", Expr{std::dynamic_pointer_cast<AddNode>(p)->a}, Expr{std::dynamic_pointer_cast<AddNode>(p)->b}); return os.str(); }
  if (std::dynamic_pointer_cast<SubNode>(p))  { key2("Sub", Expr{std::dynamic_pointer_cast<SubNode>(p)->a}, Expr{std::dynamic_pointer_cast<SubNode>(p)->b}); return os.str(); }
  if (std::dynamic_pointer_cast<MulNode>(p))  { key2("Mul", Expr{std::dynamic_pointer_cast<MulNode>(p)->a}, Expr{std::dynamic_pointer_cast<MulNode>(p)->b}); return os.str(); }
  if (std::dynamic_pointer_cast<DivNode>(p))  { key2("Div", Expr{std::dynamic_pointer_cast<DivNode>(p)->a}, Expr{std::dynamic_pointer_cast<DivNode>(p)->b}); return os.str(); }
  if (std::dynamic_pointer_cast<PowNode>(p))  { key2("Pow", Expr{std::dynamic_pointer_cast<PowNode>(p)->a}, Expr{std::dynamic_pointer_cast<PowNode>(p)->b}); return os.str(); }
  if (std::dynamic_pointer_cast<LtNode>(p))   { key2("Lt",  Expr{std::dynamic_pointer_cast<LtNode>(p)->a},  Expr{std::dynamic_pointer_cast<LtNode>(p)->b}); return os.str(); }
  if (std::dynamic_pointer_cast<LeNode>(p))   { key2("Le",  Expr{std::dynamic_pointer_cast<LeNode>(p)->a},  Expr{std::dynamic_pointer_cast<LeNode>(p)->b}); return os.str(); }
  if (std::dynamic_pointer_cast<GtNode>(p))   { key2("Gt",  Expr{std::dynamic_pointer_cast<GtNode>(p)->a},  Expr{std::dynamic_pointer_cast<GtNode>(p)->b}); return os.str(); }
  if (std::dynamic_pointer_cast<GeNode>(p))   { key2("Ge",  Expr{std::dynamic_pointer_cast<GeNode>(p)->a},  Expr{std::dynamic_pointer_cast<GeNode>(p)->b}); return os.str(); }
  if (std::dynamic_pointer_cast<EqNode>(p))   { key2("Eq",  Expr{std::dynamic_pointer_cast<EqNode>(p)->a},  Expr{std::dynamic_pointer_cast<EqNode>(p)->b}); return os.str(); }
  if (std::dynamic_pointer_cast<NeNode>(p))   { key2("Ne",  Expr{std::dynamic_pointer_cast<NeNode>(p)->a},  Expr{std::dynamic_pointer_cast<NeNode>(p)->b}); return os.str(); }
  if (std::dynamic_pointer_cast<IfNode>(p))   { auto n=std::dynamic_pointer_cast<IfNode>(p); os<<"If("<<ast_key(Expr{n->c})<<","<<ast_key(Expr{n->t})<<","<<ast_key(Expr{n->e})<<")"; return os.str(); }
  if (std::dynamic_pointer_cast<SelectNode>(p)){ auto n=std::dynamic_pointer_cast<SelectNode>(p); os<<"Sel("<<ast_key(Expr{n->m})<<","<<ast_key(Expr{n->t})<<","<<ast_key(Expr{n->e})<<")"; return os.str(); }
  if (auto lf = std::dynamic_pointer_cast<LoopForNode>(p)) {
    os<<"LFor(K="<<lf->K<<";";
    for (std::size_t i = 0; i < lf->ch.size(); ++i) { if (i) os<<","; os<<ast_key(Expr{lf->ch[i]}); }
    os<<")"; return os.str();
  }
  if (auto lo = std::dynamic_pointer_cast<LoopOutNode>(p)) { os<<"LOut(J="<<lo->J<<","<<ast_key(Expr{lo->loop})<<")"; return os.str(); }
  os<<"?"; return os.str();
}

inline int compile_hash_cse_ast(const Expr& e, TapeBackend& b) {
  // Normalize first for consistent keys
  Expr en = normalize(e);
  std::unordered_map<std::string, int> memo;
  std::function<int(const Expr&)> rec = [&](const Expr& x) -> int {
    std::string k = ast_key(x);
    auto it = memo.find(k);
    if (it != memo.end()) return it->second;
    // Emit based on kind
    auto p = x.n;
    int id = -1;
    if (auto c = std::dynamic_pointer_cast<ConstNode>(p)) id = b.emitConst(c->value);
    else if (auto v = std::dynamic_pointer_cast<VarNode>(p)) id = b.emitVar(v->index);
    else if (auto n = std::dynamic_pointer_cast<NegNode>(p))   id = b.emitNeg(rec(Expr{n->a}));
    else if (auto n = std::dynamic_pointer_cast<SinNode>(p))   id = b.emitSin(rec(Expr{n->a}));
    else if (auto n = std::dynamic_pointer_cast<CosNode>(p))   id = b.emitCos(rec(Expr{n->a}));
    else if (auto n = std::dynamic_pointer_cast<ExpNode>(p))   id = b.emitExp(rec(Expr{n->a}));
    else if (auto n = std::dynamic_pointer_cast<LogNode>(p))   id = b.emitLog(rec(Expr{n->a}));
    else if (auto n = std::dynamic_pointer_cast<SqrtNode>(p))  id = b.emitSqrt(rec(Expr{n->a}));
    else if (auto n = std::dynamic_pointer_cast<TanhNode>(p))  id = b.emitTanh(rec(Expr{n->a}));
    else if (auto n = std::dynamic_pointer_cast<AddNode>(p))   id = b.emitAdd(rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<SubNode>(p))   id = b.emitSub(rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<MulNode>(p))   id = b.emitMul(rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<DivNode>(p))   id = b.emitDiv(rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<PowNode>(p))   id = b.emitPow(rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<LtNode>(p))    id = b.emitApply(LtOp{},  rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<LeNode>(p))    id = b.emitApply(LeOp{},  rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<GtNode>(p))    id = b.emitApply(GtOp{},  rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<GeNode>(p))    id = b.emitApply(GeOp{},  rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<EqNode>(p))    id = b.emitApply(EqOp{},  rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<NeNode>(p))    id = b.emitApply(NeOp{},  rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<IfNode>(p))    id = b.emitApply(IfOp{}, rec(Expr{n->c}), rec(Expr{n->t}), rec(Expr{n->e}));
    else if (auto n = std::dynamic_pointer_cast<SelectNode>(p))id = b.emitApply(SelectOp{}, rec(Expr{n->m}), rec(Expr{n->t}), rec(Expr{n->e}));
    else if (auto n = std::dynamic_pointer_cast<NotNode>(p))   id = b.emitApply(NotOp{}, rec(Expr{n->a}));
    else if (std::dynamic_pointer_cast<IterNode>(p))           id = b.emitIter();
    else if (auto n = std::dynamic_pointer_cast<StateReadNode>(p)) id = b.emitStateRead(n->index);
    else if (auto n = std::dynamic_pointer_cast<LoopForNode>(p)) {
      std::vector<int> chids; chids.reserve(n->ch.size());
      for (auto& c : n->ch) chids.push_back(rec(Expr{c}));
      id = b.emitLoopFor(n->K, chids);
    }
    else if (auto n = std::dynamic_pointer_cast<LoopOutNode>(p)) {
      int loop_id = rec(Expr{n->loop});
      id = b.emitLoopOut(n->J, loop_id);
    }
    else id = b.emitConst(0.0);
    memo.emplace(std::move(k), id);
    return id;
  };
  return rec(en);
}

} // namespace et
