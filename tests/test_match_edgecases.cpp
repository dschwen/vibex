#include <cassert>
#include <vector>

#include "et/ast.hpp"
#include "et/normalize.hpp"
#include "et/rewrite_ast.hpp"
#include <string>
#include <unordered_map>
#include <functional>

// Minimal AST pattern/matcher for Add/Mul with spreads, for test parity
namespace et { namespace pat {
  struct Pattern {
    enum class Kind { Placeholder, Node };
    Kind kind{Kind::Node};
    std::string name; // "Add","Mul","Neg",...
    int placeholder_id{-1}; bool is_spread{false};
    std::vector<Pattern> ch;
    static Pattern placeholder(int id){ Pattern p; p.kind=Kind::Placeholder; p.placeholder_id=id; return p; }
    static Pattern node(const char* n, std::vector<Pattern> c={}){ Pattern p; p.kind=Kind::Node; p.name=n; p.ch=std::move(c); return p; }
  };
  inline Pattern P(int id){ return Pattern::placeholder(id); }
  inline Pattern S(int id){ Pattern p=Pattern::placeholder(id); p.is_spread=true; return p; }
  inline Pattern add(const Pattern& a, const Pattern& b){ return Pattern::node("Add", {a,b}); }
  inline Pattern mul(const Pattern& a, const Pattern& b){ return Pattern::node("Mul", {a,b}); }
  inline Pattern neg(const Pattern& a){ return Pattern::node("Neg", {a}); }
  inline int specificity(const Pattern& p){ if (p.kind==Pattern::Kind::Placeholder) return 0; int s=1; for (auto& c:p.ch) s+=specificity(c); return s; }
} }

namespace et {
  // Helpers to introspect AST
  static inline bool is_named(const Expr& e, const char* n) {
    auto p = e.n;
    if (std::string(n)=="Add") return (bool)std::dynamic_pointer_cast<AddNode>(p);
    if (std::string(n)=="Mul") return (bool)std::dynamic_pointer_cast<MulNode>(p);
    if (std::string(n)=="Neg") return (bool)std::dynamic_pointer_cast<NegNode>(p);
    return false;
  }
  static inline void gather_named(const Expr& e, const char* n, std::vector<Expr>& out) {
    if (std::string(n)=="Add") {
      if (auto ad = std::dynamic_pointer_cast<AddNode>(e.n)) { gather_named(Expr{ad->a}, n, out); gather_named(Expr{ad->b}, n, out); }
      else out.push_back(e);
    } else if (std::string(n)=="Mul") {
      if (auto md = std::dynamic_pointer_cast<MulNode>(e.n)) { gather_named(Expr{md->a}, n, out); gather_named(Expr{md->b}, n, out); }
      else out.push_back(e);
    } else {
      out.push_back(e);
    }
  }
}

namespace et {
  using Bindings = std::unordered_map<int, Expr>;
  using MultiBindings = std::unordered_map<int, std::vector<Expr>>;
  inline bool equal_ast(const Expr& a, const Expr& b) { return equal(a,b); }

  static bool match_node(const Expr& e, const pat::Pattern& p, Bindings& b, MultiBindings& mb);

  static bool match_ac(const Expr& e, const pat::Pattern& p, Bindings& b, MultiBindings& mb) {
    // Flatten children of e under the AC op
    std::vector<Expr> xs; gather_named(e, p.name.c_str(), xs);
    // Pattern children excluding spread, ordered by specificity
    std::vector<std::size_t> pidx; pidx.reserve(p.ch.size());
    for (std::size_t i=0;i<p.ch.size();++i) if (!(p.ch[i].kind==pat::Pattern::Kind::Placeholder && p.ch[i].is_spread)) pidx.push_back(i);
    std::sort(pidx.begin(), pidx.end(), [&](std::size_t i, std::size_t j){ return pat::specificity(p.ch[i]) > pat::specificity(p.ch[j]); });
    // Spread position if any
    std::size_t spreads = 0, spread_idx = ~std::size_t(0);
    for (std::size_t i=0;i<p.ch.size();++i) if (p.ch[i].kind==pat::Pattern::Kind::Placeholder && p.ch[i].is_spread) { spreads++; spread_idx=i; }
    if (spreads>1) return false;
    if (spreads==0 && p.ch.size()!=xs.size()) return false;
    if (spreads==1 && p.ch.size()-1>xs.size()) return false;
    // Backtracking assign
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
        for (std::size_t i=0;i<remaining.size();++i) if (!equal_ast(it->second[i], remaining[i])) return false;
      }
    } else if (!remaining.empty()) return false;
    return true;
  }

  static bool match_node(const Expr& e, const pat::Pattern& p, Bindings& b, MultiBindings& mb) {
    if (p.kind==pat::Pattern::Kind::Placeholder) {
      if (p.is_spread) {
        auto it=mb.find(p.placeholder_id);
        if (it==mb.end()) { mb.emplace(p.placeholder_id, std::vector<Expr>{e}); return true; }
        const auto& vec=it->second; return vec.size()==1 && equal_ast(vec[0], e);
      } else {
        auto it=b.find(p.placeholder_id);
        if (it==b.end()) { b.emplace(p.placeholder_id, e); return true; }
        return equal_ast(it->second, e);
      }
    }
    // Node: check name and handle AC
    if (!is_named(e, p.name.c_str())) return false;
    if (p.name=="Add" || p.name=="Mul") return match_ac(e, p, b, mb);
    // Non-AC: arity must match exactly
    if (p.name=="Neg") {
      auto n = std::dynamic_pointer_cast<NegNode>(e.n);
      return match_node(Expr{n->a}, p.ch[0], b, mb);
    }
    return false;
  }

  inline bool match(const Expr& e, const pat::Pattern& p, Bindings& b, MultiBindings& mb) {
    b.clear(); mb.clear();
    return match_node(normalize(e), p, b, mb);
  }
}

using namespace et;

int main() {
  using namespace et::pat;
  auto x = var(0), y = var(1);

  // 1) Spread outside AC node used twice in same pattern: add(neg(S(1)), neg(S(1)))
  //    This forces the spread to bind to the same single child on both occurrences.
  //    Using different children should fail.
  {
    Expr e = (-sin(x)) + (-cos(x));
    et::Bindings b; et::MultiBindings mb;
    bool ok = et::match(e, add(neg(S(1)), neg(S(1))), b, mb);
    assert(!ok);
  }

  // 2) Conflicting placeholder bindings under AC: Add(P1,P1) vs Add(x,y) (x!=y) -> fail
  {
    Expr e = x + y;
    et::Bindings b; et::MultiBindings mb;
    bool ok = et::match(e, add(P(1), P(1)), b, mb);
    assert(!ok);
  }

  // 3) AC mismatch without spread: Add(P1,P2) vs Add(a,b,c) -> fail
  {
    Expr e = x + y + lit(1.0);
    et::Bindings b; et::MultiBindings mb;
    bool ok = et::match(e, add(P(1), P(2)), b, mb);
    assert(!ok);
  }

  // 4) AC with one spread: Add(P1, S(2)) vs Add(a,b,c) -> succeed, spread captures remainder
  {
    Expr e = x + y + lit(1.0);
    et::Bindings b; et::MultiBindings mb;
    bool ok = et::match(e, add(P(1), S(2)), b, mb);
    assert(ok);
    auto it = mb.find(2); assert(it != mb.end());
    // remainder should be size 2
    assert(it->second.size() == 2);
  }

  return 0;
}
