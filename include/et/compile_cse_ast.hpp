#pragma once
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <functional>

#include "et/ast.hpp"
#include "et/normalize_ast.hpp"
#include "et/expr.hpp"
#include "et/tape_backend.hpp"

namespace et {

// --- structural hash ids for AST node kinds
enum class AstOpId : std::uint64_t {
  Const = 0x01, Var   = 0x02,
  Neg   = 0x10, Sin   = 0x11, Cos = 0x12, Exp = 0x13, Log = 0x14, Sqrt = 0x15, Tanh = 0x16,
  Add   = 0x20, Sub   = 0x21, Mul = 0x22, Div  = 0x23, Pow  = 0x24,
  Lt    = 0x30, Le    = 0x31, Gt  = 0x32, Ge   = 0x33, Eq   = 0x34, Ne   = 0x35,
  If    = 0x40, Select= 0x41
};

static inline std::uint64_t mix64(std::uint64_t a, std::uint64_t b) {
  a ^= b + 0x9e3779b97f4a7c15ULL + (a<<6) + (a>>2);
  return a;
}

inline std::uint64_t shash_ast(const Expr& e) {
  if (!e.n) return 0xDEADBEEF;
  auto p = e.n;
  if (auto c = std::dynamic_pointer_cast<ConstNode>(p)) {
    std::uint64_t h = static_cast<std::uint64_t>(AstOpId::Const);
    std::hash<long double> H; h = mix64(h, H(static_cast<long double>(c->value)));
    return h;
  }
  if (auto v = std::dynamic_pointer_cast<VarNode>(p)) {
    std::uint64_t h = static_cast<std::uint64_t>(AstOpId::Var);
    return mix64(h, static_cast<std::uint64_t>(v->index * 0x9e37));
  }
  auto h1 = [&](AstOpId op, const Expr& a){ return mix64(static_cast<std::uint64_t>(op), shash_ast(a)); };
  auto h2 = [&](AstOpId op, const Expr& a, const Expr& b){ return mix64(mix64(static_cast<std::uint64_t>(op), shash_ast(a)), shash_ast(b)); };
  if (auto n = std::dynamic_pointer_cast<NegNode>(p))  return h1(AstOpId::Neg,  Expr{n->a});
  if (auto n = std::dynamic_pointer_cast<SinNode>(p))  return h1(AstOpId::Sin,  Expr{n->a});
  if (auto n = std::dynamic_pointer_cast<CosNode>(p))  return h1(AstOpId::Cos,  Expr{n->a});
  if (auto n = std::dynamic_pointer_cast<ExpNode>(p))  return h1(AstOpId::Exp,  Expr{n->a});
  if (auto n = std::dynamic_pointer_cast<LogNode>(p))  return h1(AstOpId::Log,  Expr{n->a});
  if (auto n = std::dynamic_pointer_cast<SqrtNode>(p)) return h1(AstOpId::Sqrt, Expr{n->a});
  if (auto n = std::dynamic_pointer_cast<TanhNode>(p)) return h1(AstOpId::Tanh, Expr{n->a});
  if (auto n = std::dynamic_pointer_cast<AddNode>(p))  return h2(AstOpId::Add,  Expr{n->a}, Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<SubNode>(p))  return h2(AstOpId::Sub,  Expr{n->a}, Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<MulNode>(p))  return h2(AstOpId::Mul,  Expr{n->a}, Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<DivNode>(p))  return h2(AstOpId::Div,  Expr{n->a}, Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<PowNode>(p))  return h2(AstOpId::Pow,  Expr{n->a}, Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<LtNode>(p))   return h2(AstOpId::Lt,   Expr{n->a}, Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<LeNode>(p))   return h2(AstOpId::Le,   Expr{n->a}, Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<GtNode>(p))   return h2(AstOpId::Gt,   Expr{n->a}, Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<GeNode>(p))   return h2(AstOpId::Ge,   Expr{n->a}, Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<EqNode>(p))   return h2(AstOpId::Eq,   Expr{n->a}, Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<NeNode>(p))   return h2(AstOpId::Ne,   Expr{n->a}, Expr{n->b});
  if (auto n = std::dynamic_pointer_cast<IfNode>(p))   return mix64(static_cast<std::uint64_t>(AstOpId::If), mix64(shash_ast(Expr{n->c}), mix64(shash_ast(Expr{n->t}), shash_ast(Expr{n->e}))));
  if (auto n = std::dynamic_pointer_cast<SelectNode>(p))return mix64(static_cast<std::uint64_t>(AstOpId::Select), mix64(shash_ast(Expr{n->m}), mix64(shash_ast(Expr{n->t}), shash_ast(Expr{n->e}))));
  return 0xFEEDBEEF;
}

inline std::string skey_ast(const Expr& e) {
  if (!e.n) return "()";
  std::ostringstream os;
  auto p = e.n;
  auto k1 = [&](const char* nm, const Expr& a){ os<<nm<<'('<<skey_ast(a)<<')'; };
  auto k2 = [&](const char* nm, const Expr& a, const Expr& b){ os<<nm<<'('<<skey_ast(a)<<','<<skey_ast(b)<<')'; };
  if (auto c = std::dynamic_pointer_cast<ConstNode>(p)) { os<<"C("<<c->value<<")"; return os.str(); }
  if (auto v = std::dynamic_pointer_cast<VarNode>(p))   { os<<"V("<<v->index<<")"; return os.str(); }
  if (auto n = std::dynamic_pointer_cast<NegNode>(p))   { k1("Neg", Expr{n->a}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<SinNode>(p))   { k1("Sin", Expr{n->a}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<CosNode>(p))   { k1("Cos", Expr{n->a}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<ExpNode>(p))   { k1("Exp", Expr{n->a}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<LogNode>(p))   { k1("Log", Expr{n->a}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<SqrtNode>(p))  { k1("Sqrt",Expr{n->a}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<TanhNode>(p))  { k1("Tanh",Expr{n->a}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<AddNode>(p))   { k2("Add", Expr{n->a}, Expr{n->b}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<SubNode>(p))   { k2("Sub", Expr{n->a}, Expr{n->b}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<MulNode>(p))   { k2("Mul", Expr{n->a}, Expr{n->b}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<DivNode>(p))   { k2("Div", Expr{n->a}, Expr{n->b}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<PowNode>(p))   { k2("Pow", Expr{n->a}, Expr{n->b}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<LtNode>(p))    { k2("Lt",  Expr{n->a}, Expr{n->b}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<LeNode>(p))    { k2("Le",  Expr{n->a}, Expr{n->b}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<GtNode>(p))    { k2("Gt",  Expr{n->a}, Expr{n->b}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<GeNode>(p))    { k2("Ge",  Expr{n->a}, Expr{n->b}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<EqNode>(p))    { k2("Eq",  Expr{n->a}, Expr{n->b}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<NeNode>(p))    { k2("Ne",  Expr{n->a}, Expr{n->b}); return os.str(); }
  if (auto n = std::dynamic_pointer_cast<IfNode>(p))    { auto q=n; os<<"If("<<skey_ast(Expr{q->c})<<","<<skey_ast(Expr{q->t})<<","<<skey_ast(Expr{q->e})<<")"; return os.str(); }
  if (auto n = std::dynamic_pointer_cast<SelectNode>(p)){ auto q=n; os<<"Sel("<<skey_ast(Expr{q->m})<<","<<skey_ast(Expr{q->t})<<","<<skey_ast(Expr{q->e})<<")"; return os.str(); }
  return "?";
}

template <class Backend>
struct AstHashMemo {
  struct Entry { std::string skey; typename Backend::result_type value; };
  std::unordered_map<std::uint64_t, std::vector<Entry>> map;

  bool find(const Expr& e, typename Backend::result_type& out) {
    auto h = shash_ast(e);
    auto it = map.find(h);
    if (it == map.end()) return false;
    if (it->second.size() == 1 && it->second[0].skey.empty()) { out = it->second[0].value; return true; }
    auto key = skey_ast(e);
    for (auto& ent : it->second) if (!ent.skey.empty() && ent.skey == key) { out = ent.value; return true; }
    return false;
  }
  void insert(const Expr& e, const typename Backend::result_type& v) {
    auto h = shash_ast(e);
    auto& vec = map[h];
    if (!vec.empty()) {
      auto key = skey_ast(e);
      for (auto& ent : vec) if (ent.skey.empty()) ent.skey = "<materialized>";
      vec.push_back(Entry{key, v});
    } else {
      vec.push_back(Entry{"", v});
    }
  }
};

template <class Backend>
auto compile_cse_ast(const Expr& e, Backend& b) -> typename Backend::result_type {
  Expr en = normalize(e);
  AstHashMemo<Backend> memo;
  std::function<typename Backend::result_type(const Expr&)> rec = [&](const Expr& x) -> typename Backend::result_type {
    typename Backend::result_type cached;
    if (memo.find(x, cached)) return cached;
    auto p = x.n;
    typename Backend::result_type id;
    if (auto c = std::dynamic_pointer_cast<ConstNode>(p)) id = b.emitConst(c->value);
    else if (auto v = std::dynamic_pointer_cast<VarNode>(p)) id = b.emitVar(v->index);
    else if (auto n = std::dynamic_pointer_cast<NegNode>(p))   id = b.template emitApply(NegOp{}, rec(Expr{n->a}));
    else if (auto n = std::dynamic_pointer_cast<SinNode>(p))   id = b.template emitApply(SinOp{}, rec(Expr{n->a}));
    else if (auto n = std::dynamic_pointer_cast<CosNode>(p))   id = b.template emitApply(CosOp{}, rec(Expr{n->a}));
    else if (auto n = std::dynamic_pointer_cast<ExpNode>(p))   id = b.template emitApply(ExpOp{}, rec(Expr{n->a}));
    else if (auto n = std::dynamic_pointer_cast<LogNode>(p))   id = b.template emitApply(LogOp{}, rec(Expr{n->a}));
    else if (auto n = std::dynamic_pointer_cast<SqrtNode>(p))  id = b.template emitApply(SqrtOp{}, rec(Expr{n->a}));
    else if (auto n = std::dynamic_pointer_cast<TanhNode>(p))  id = b.template emitApply(TanhOp{}, rec(Expr{n->a}));
    else if (auto n = std::dynamic_pointer_cast<AddNode>(p))   id = b.template emitApply(AddOp{}, rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<SubNode>(p))   id = b.template emitApply(SubOp{}, rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<MulNode>(p))   id = b.template emitApply(MulOp{}, rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<DivNode>(p))   id = b.template emitApply(DivOp{}, rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<PowNode>(p))   id = b.template emitApply(PowOp{}, rec(Expr{n->a}), rec(Expr{n->b}));
#ifdef ET_ENABLE_CONTROL_FLOW
    else if (auto n = std::dynamic_pointer_cast<LtNode>(p))    id = b.template emitApply(LtOp{},  rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<LeNode>(p))    id = b.template emitApply(LeOp{},  rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<GtNode>(p))    id = b.template emitApply(GtOp{},  rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<GeNode>(p))    id = b.template emitApply(GeOp{},  rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<EqNode>(p))    id = b.template emitApply(EqOp{},  rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<NeNode>(p))    id = b.template emitApply(NeOp{},  rec(Expr{n->a}), rec(Expr{n->b}));
    else if (auto n = std::dynamic_pointer_cast<IfNode>(p))    id = b.template emitApply(IfOp{}, rec(Expr{n->c}), rec(Expr{n->t}), rec(Expr{n->e}));
    else if (auto n = std::dynamic_pointer_cast<SelectNode>(p))id = b.template emitApply(SelectOp{}, rec(Expr{n->m}), rec(Expr{n->t}), rec(Expr{n->e}));
#endif
    else id = b.emitConst(0.0);
    memo.insert(x, id);
    return id;
  };
  return rec(en);
}

// Convenience overload for TapeBackend
inline int compile_cse_ast(const Expr& e, TapeBackend& b) {
  return compile_cse_ast<TapeBackend>(e, b);
}

} // namespace et
