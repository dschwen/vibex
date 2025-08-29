#pragma once
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <functional>
#include <string>

#include "et/ast.hpp"
#include "et/normalize.hpp"
#include "et/rewrite_ast.hpp"
#include "et/pattern.hpp"

namespace et {

using AstBindings = std::unordered_map<int, Expr>;
using AstMultiBindings = std::unordered_map<int, std::vector<Expr>>;

inline bool is_named(const Expr& e, const char* n) {
  auto p = e.n; std::string s(n);
  if (s == "Add") return (bool)std::dynamic_pointer_cast<AddNode>(p);
  if (s == "Mul") return (bool)std::dynamic_pointer_cast<MulNode>(p);
  if (s == "Neg") return (bool)std::dynamic_pointer_cast<NegNode>(p);
  if (s == "Sin") return (bool)std::dynamic_pointer_cast<SinNode>(p);
  if (s == "Cos") return (bool)std::dynamic_pointer_cast<CosNode>(p);
  return false;
}

inline void gather_named(const Expr& e, const char* n, std::vector<Expr>& out) {
  std::string s(n);
  if (s == "Add") {
    if (auto ad = std::dynamic_pointer_cast<AddNode>(e.n)) { gather_named(Expr{ad->a}, n, out); gather_named(Expr{ad->b}, n, out); }
    else out.push_back(e);
  } else if (s == "Mul") {
    if (auto md = std::dynamic_pointer_cast<MulNode>(e.n)) { gather_named(Expr{md->a}, n, out); gather_named(Expr{md->b}, n, out); }
    else out.push_back(e);
  } else {
    out.push_back(e);
  }
}

namespace detail_ast_match {
  inline bool match_node(const Expr& e, const astpat::Pattern& p, AstBindings& b, AstMultiBindings& mb);

  inline bool match_ac(const Expr& e, const astpat::Pattern& p, AstBindings& b, AstMultiBindings& mb) {
    std::vector<Expr> xs; gather_named(e, p.name.c_str(), xs);
    std::vector<std::size_t> pidx; pidx.reserve(p.ch.size());
    for (std::size_t i=0;i<p.ch.size();++i) if (!(p.ch[i].kind==astpat::Pattern::Kind::Placeholder && p.ch[i].is_spread)) pidx.push_back(i);
    std::sort(pidx.begin(), pidx.end(), [&](std::size_t i, std::size_t j){ return astpat::specificity(p.ch[i]) > astpat::specificity(p.ch[j]); });
    std::size_t spreads = 0, spread_idx = ~std::size_t(0);
    for (std::size_t i=0;i<p.ch.size();++i) if (p.ch[i].kind==astpat::Pattern::Kind::Placeholder && p.ch[i].is_spread) { spreads++; spread_idx=i; }
    if (spreads>1) return false;
    if (spreads==0 && p.ch.size()!=xs.size()) return false;
    if (spreads==1 && p.ch.size()-1>xs.size()) return false;
    std::vector<Expr> remaining = xs;
    std::function<bool(std::size_t)> dfs = [&](std::size_t i)->bool{
      if (i==pidx.size()) return true;
      const auto& pc = p.ch[pidx[i]];
      for (std::size_t r=0;r<remaining.size();++r) {
        Expr cand = remaining[r];
        auto b_snapshot=b; auto mb_snapshot=mb;
        if (match_node(cand, pc, b, mb)) {
          remaining.erase(remaining.begin()+r);
          if (dfs(i+1)) return true;
        }
        b = std::move(b_snapshot); mb = std::move(mb_snapshot);
      }
      return false;
    };
    bool ok = dfs(0);
    if (!ok) return false;
    if (spreads==1) {
      const auto& sp = p.ch[spread_idx];
      auto it = mb.find(sp.placeholder_id);
      if (it==mb.end()) mb.emplace(sp.placeholder_id, remaining);
      else {
        if (it->second.size()!=remaining.size()) return false;
        for (std::size_t i=0;i<remaining.size();++i) if (!equal(it->second[i], remaining[i])) return false;
      }
    } else if (!remaining.empty()) return false;
    return true;
  }

  inline bool match_node(const Expr& e, const astpat::Pattern& p, AstBindings& b, AstMultiBindings& mb) {
    if (p.kind==astpat::Pattern::Kind::Placeholder) {
      if (p.is_spread) {
        auto it=mb.find(p.placeholder_id);
        if (it==mb.end()) { mb.emplace(p.placeholder_id, std::vector<Expr>{e}); return true; }
        const auto& vec=it->second; return vec.size()==1 && equal(vec[0], e);
      } else {
        auto it=b.find(p.placeholder_id);
        if (it==b.end()) { b.emplace(p.placeholder_id, e); return true; }
        return equal(it->second, e);
      }
    }
    if (!is_named(e, p.name.c_str())) return false;
    if (p.name=="Add" || p.name=="Mul") return match_ac(e, p, b, mb);
    if (p.name=="Neg") {
      auto n = std::dynamic_pointer_cast<NegNode>(e.n);
      return match_node(Expr{n->a}, p.ch[0], b, mb);
    }
    if (p.name=="Sin") { auto n = std::dynamic_pointer_cast<SinNode>(e.n); return match_node(Expr{n->a}, p.ch[0], b, mb); }
    if (p.name=="Cos") { auto n = std::dynamic_pointer_cast<CosNode>(e.n); return match_node(Expr{n->a}, p.ch[0], b, mb); }
    return false;
  }
} // namespace detail_ast_match

inline bool match(const Expr& e, const astpat::Pattern& p, AstBindings& b, AstMultiBindings& mb) {
  b.clear(); mb.clear();
  Expr en = normalize(e);
  return detail_ast_match::match_node(en, p, b, mb);
}

} // namespace et

