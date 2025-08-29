#pragma once
#include <memory>
#include <vector>
#include <algorithm>

#include "et/ast.hpp"
#include "et/normalize.hpp"

namespace et {

// Small helpers
inline bool is_integer(double v, long long* out = nullptr) {
  long long r = llround(v);
  if (std::fabs(v - static_cast<double>(r)) < 1e-12) { if (out) *out = r; return true; }
  return false;
}
inline bool is_integer_const(const Expr& e, long long* out = nullptr) {
  double v; if (!is_const(e, &v)) return false; return is_integer(v, out);
}

// Structural equality on AST
inline bool equal(const Expr& a, const Expr& b) {
  if (a.n.get() == b.n.get()) return true;
  // Kind checks
  auto ak = kind_rank(a), bk = kind_rank(b);
  if (ak != bk) return false;
  if (auto ca = std::dynamic_pointer_cast<ConstNode>(a.n)) {
    auto cb = std::dynamic_pointer_cast<ConstNode>(b.n); return cb && ca->value == cb->value;
  }
  if (auto va = std::dynamic_pointer_cast<VarNode>(a.n)) {
    auto vb = std::dynamic_pointer_cast<VarNode>(b.n); return vb && va->index == vb->index;
  }
  auto eq1 = [&](const std::shared_ptr<Node>& pa, const std::shared_ptr<Node>& pb){ return equal(Expr{pa}, Expr{pb}); };
  // Unary
  if (auto ua = std::dynamic_pointer_cast<NegNode>(a.n))  { auto ub = std::dynamic_pointer_cast<NegNode>(b.n);  return ub && eq1(ua->a, ub->a); }
  if (auto ua = std::dynamic_pointer_cast<SinNode>(a.n))  { auto ub = std::dynamic_pointer_cast<SinNode>(b.n);  return ub && eq1(ua->a, ub->a); }
  if (auto ua = std::dynamic_pointer_cast<CosNode>(a.n))  { auto ub = std::dynamic_pointer_cast<CosNode>(b.n);  return ub && eq1(ua->a, ub->a); }
  if (auto ua = std::dynamic_pointer_cast<ExpNode>(a.n))  { auto ub = std::dynamic_pointer_cast<ExpNode>(b.n);  return ub && eq1(ua->a, ub->a); }
  if (auto ua = std::dynamic_pointer_cast<LogNode>(a.n))  { auto ub = std::dynamic_pointer_cast<LogNode>(b.n);  return ub && eq1(ua->a, ub->a); }
  if (auto ua = std::dynamic_pointer_cast<SqrtNode>(a.n)) { auto ub = std::dynamic_pointer_cast<SqrtNode>(b.n); return ub && eq1(ua->a, ub->a); }
  if (auto ua = std::dynamic_pointer_cast<TanhNode>(a.n)) { auto ub = std::dynamic_pointer_cast<TanhNode>(b.n); return ub && eq1(ua->a, ub->a); }
  // Binary
  auto eq2 = [&](const std::shared_ptr<Node>& al, const std::shared_ptr<Node>& ar,
                 const std::shared_ptr<Node>& bl, const std::shared_ptr<Node>& br){
    return equal(Expr{al}, Expr{bl}) && equal(Expr{ar}, Expr{br});
  };
  if (auto na = std::dynamic_pointer_cast<AddNode>(a.n))  { auto nb = std::dynamic_pointer_cast<AddNode>(b.n);  return nb && eq2(na->a, na->b, nb->a, nb->b); }
  if (auto na = std::dynamic_pointer_cast<SubNode>(a.n))  { auto nb = std::dynamic_pointer_cast<SubNode>(b.n);  return nb && eq2(na->a, na->b, nb->a, nb->b); }
  if (auto na = std::dynamic_pointer_cast<MulNode>(a.n))  { auto nb = std::dynamic_pointer_cast<MulNode>(b.n);  return nb && eq2(na->a, na->b, nb->a, nb->b); }
  if (auto na = std::dynamic_pointer_cast<DivNode>(a.n))  { auto nb = std::dynamic_pointer_cast<DivNode>(b.n);  return nb && eq2(na->a, na->b, nb->a, nb->b); }
  if (auto na = std::dynamic_pointer_cast<PowNode>(a.n))  { auto nb = std::dynamic_pointer_cast<PowNode>(b.n);  return nb && eq2(na->a, na->b, nb->a, nb->b); }
  if (auto na = std::dynamic_pointer_cast<LtNode>(a.n))   { auto nb = std::dynamic_pointer_cast<LtNode>(b.n);   return nb && eq2(na->a, na->b, nb->a, nb->b); }
  if (auto na = std::dynamic_pointer_cast<LeNode>(a.n))   { auto nb = std::dynamic_pointer_cast<LeNode>(b.n);   return nb && eq2(na->a, na->b, nb->a, nb->b); }
  if (auto na = std::dynamic_pointer_cast<GtNode>(a.n))   { auto nb = std::dynamic_pointer_cast<GtNode>(b.n);   return nb && eq2(na->a, na->b, nb->a, nb->b); }
  if (auto na = std::dynamic_pointer_cast<GeNode>(a.n))   { auto nb = std::dynamic_pointer_cast<GeNode>(b.n);   return nb && eq2(na->a, na->b, nb->a, nb->b); }
  if (auto na = std::dynamic_pointer_cast<EqNode>(a.n))   { auto nb = std::dynamic_pointer_cast<EqNode>(b.n);   return nb && eq2(na->a, na->b, nb->a, nb->b); }
  if (auto na = std::dynamic_pointer_cast<NeNode>(a.n))   { auto nb = std::dynamic_pointer_cast<NeNode>(b.n);   return nb && eq2(na->a, na->b, nb->a, nb->b); }
  if (auto ia = std::dynamic_pointer_cast<IfNode>(a.n))   { auto ib = std::dynamic_pointer_cast<IfNode>(b.n);   return ib && equal(Expr{ia->c}, Expr{ib->c}) && equal(Expr{ia->t}, Expr{ib->t}) && equal(Expr{ia->e}, Expr{ib->e}); }
  if (auto sa = std::dynamic_pointer_cast<SelectNode>(a.n)){ auto sb = std::dynamic_pointer_cast<SelectNode>(b.n);return sb && equal(Expr{sa->m}, Expr{sb->m}) && equal(Expr{sa->t}, Expr{sb->t}) && equal(Expr{sa->e}, Expr{sb->e}); }
  return false;
}

inline void decompose_add(const Expr& e, std::vector<Expr>& out) {
  if (auto n = std::dynamic_pointer_cast<AddNode>(e.n)) { decompose_add(Expr{n->a}, out); decompose_add(Expr{n->b}, out); }
  else out.push_back(e);
}
inline bool is_mul_of(const Expr& e, const Expr& a, const Expr& b) {
  if (auto n = std::dynamic_pointer_cast<MulNode>(e.n)) {
    return (equal(Expr{n->a}, a) && equal(Expr{n->b}, b)) || (equal(Expr{n->a}, b) && equal(Expr{n->b}, a));
  }
  return false;
}

inline bool is_sin_sq(const Expr& e, Expr& u) {
  if (auto s = std::dynamic_pointer_cast<SinNode>(e.n)) {
    // e == sin(u)? then need sin(u)*sin(u) elsewhere; handled in caller
    return false;
  }
  if (auto m = std::dynamic_pointer_cast<MulNode>(e.n)) {
    auto s1 = std::dynamic_pointer_cast<SinNode>(m->a);
    auto s2 = std::dynamic_pointer_cast<SinNode>(m->b);
    if (s1 && s2 && equal(Expr{s1->a}, Expr{s2->a})) { u = Expr{s1->a}; return true; }
  }
  if (auto p = std::dynamic_pointer_cast<PowNode>(e.n)) {
    auto s = std::dynamic_pointer_cast<SinNode>(p->a);
    double cv; if (s && is_const(Expr{p->b}, &cv) && cv == 2.0) { u = Expr{s->a}; return true; }
  }
  return false;
}
inline bool is_cos_sq(const Expr& e, Expr& u) {
  if (auto m = std::dynamic_pointer_cast<MulNode>(e.n)) {
    auto c1 = std::dynamic_pointer_cast<CosNode>(m->a);
    auto c2 = std::dynamic_pointer_cast<CosNode>(m->b);
    if (c1 && c2 && equal(Expr{c1->a}, Expr{c2->a})) { u = Expr{c1->a}; return true; }
  }
  if (auto p = std::dynamic_pointer_cast<PowNode>(e.n)) {
    auto c = std::dynamic_pointer_cast<CosNode>(p->a);
    double cv; if (c && is_const(Expr{p->b}, &cv) && cv == 2.0) { u = Expr{c->a}; return true; }
  }
  return false;
}

inline Expr rewrite_once(const Expr& e, bool& changed);

inline Expr rewrite_children(const Expr& e, bool& changed) {
  // Rebuild with potentially rewritten children
  if (auto c = std::dynamic_pointer_cast<ConstNode>(e.n)) return e;
  if (auto v = std::dynamic_pointer_cast<VarNode>(e.n))   return e;
  auto rw1 = [&](const std::shared_ptr<Node>& a){ return rewrite_once(Expr{a}, changed); };
  auto rw2 = [&](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b){ return std::make_pair(rw1(a), rw1(b)); };
  if (auto n = std::dynamic_pointer_cast<NegNode>(e.n))  { auto a = rw1(n->a); return (equal(a, Expr{n->a}) ? e : -a); }
  if (auto n = std::dynamic_pointer_cast<SinNode>(e.n))  { auto a = rw1(n->a); return (equal(a, Expr{n->a}) ? e : sin(a)); }
  if (auto n = std::dynamic_pointer_cast<CosNode>(e.n))  { auto a = rw1(n->a); return (equal(a, Expr{n->a}) ? e : cos(a)); }
  if (auto n = std::dynamic_pointer_cast<ExpNode>(e.n))  { auto a = rw1(n->a); return (equal(a, Expr{n->a}) ? e : exp(a)); }
  if (auto n = std::dynamic_pointer_cast<LogNode>(e.n))  { auto a = rw1(n->a); return (equal(a, Expr{n->a}) ? e : log(a)); }
  if (auto n = std::dynamic_pointer_cast<SqrtNode>(e.n)) { auto a = rw1(n->a); return (equal(a, Expr{n->a}) ? e : sqrt(a)); }
  if (auto n = std::dynamic_pointer_cast<TanhNode>(e.n)) { auto a = rw1(n->a); return (equal(a, Expr{n->a}) ? e : tanh(a)); }
  if (auto n = std::dynamic_pointer_cast<AddNode>(e.n))  { auto [a,b] = rw2(n->a, n->b); return (equal(a, Expr{n->a}) && equal(b, Expr{n->b}) ? e : a + b); }
  if (auto n = std::dynamic_pointer_cast<SubNode>(e.n))  { auto [a,b] = rw2(n->a, n->b); return (equal(a, Expr{n->a}) && equal(b, Expr{n->b}) ? e : a - b); }
  if (auto n = std::dynamic_pointer_cast<MulNode>(e.n))  { auto [a,b] = rw2(n->a, n->b); return (equal(a, Expr{n->a}) && equal(b, Expr{n->b}) ? e : a * b); }
  if (auto n = std::dynamic_pointer_cast<DivNode>(e.n))  { auto [a,b] = rw2(n->a, n->b); return (equal(a, Expr{n->a}) && equal(b, Expr{n->b}) ? e : a / b); }
  if (auto n = std::dynamic_pointer_cast<PowNode>(e.n))  { auto [a,b] = rw2(n->a, n->b); return (equal(a, Expr{n->a}) && equal(b, Expr{n->b}) ? e : pow(a, b)); }
  if (auto n = std::dynamic_pointer_cast<LtNode>(e.n))   { auto [a,b] = rw2(n->a, n->b); return (equal(a, Expr{n->a}) && equal(b, Expr{n->b}) ? e : (a < b)); }
  if (auto n = std::dynamic_pointer_cast<LeNode>(e.n))   { auto [a,b] = rw2(n->a, n->b); return (equal(a, Expr{n->a}) && equal(b, Expr{n->b}) ? e : (a <= b)); }
  if (auto n = std::dynamic_pointer_cast<GtNode>(e.n))   { auto [a,b] = rw2(n->a, n->b); return (equal(a, Expr{n->a}) && equal(b, Expr{n->b}) ? e : (a > b)); }
  if (auto n = std::dynamic_pointer_cast<GeNode>(e.n))   { auto [a,b] = rw2(n->a, n->b); return (equal(a, Expr{n->a}) && equal(b, Expr{n->b}) ? e : (a >= b)); }
  if (auto n = std::dynamic_pointer_cast<EqNode>(e.n))   { auto [a,b] = rw2(n->a, n->b); return (equal(a, Expr{n->a}) && equal(b, Expr{n->b}) ? e : (a == b)); }
  if (auto n = std::dynamic_pointer_cast<NeNode>(e.n))   { auto [a,b] = rw2(n->a, n->b); return (equal(a, Expr{n->a}) && equal(b, Expr{n->b}) ? e : (a != b)); }
  if (auto n = std::dynamic_pointer_cast<IfNode>(e.n))   { auto c = rewrite_once(Expr{n->c}, changed); auto t = rewrite_once(Expr{n->t}, changed); auto el = rewrite_once(Expr{n->e}, changed); if (!equal(c, Expr{n->c}) || !equal(t, Expr{n->t}) || !equal(el, Expr{n->e})) return If(c,t,el); return e; }
  if (auto n = std::dynamic_pointer_cast<SelectNode>(e.n)){ auto m = rewrite_once(Expr{n->m}, changed); auto t = rewrite_once(Expr{n->t}, changed); auto el = rewrite_once(Expr{n->e}, changed); if (!equal(m, Expr{n->m}) || !equal(t, Expr{n->t}) || !equal(el, Expr{n->e})) return Select(m,t,el); return e; }
  return e;
}

inline Expr rewrite_once(const Expr& e, bool& changed) {
  // First rewrite children
  Expr cur = rewrite_children(e, changed);
  // Try local rules
  // log(exp(a)) -> a
  if (auto ln = std::dynamic_pointer_cast<LogNode>(cur.n))
    if (auto ex = std::dynamic_pointer_cast<ExpNode>(ln->a)) { changed = true; return normalize(Expr{ex->a}); }
  // exp(log(a)) -> a (unguarded)
  if (auto ex = std::dynamic_pointer_cast<ExpNode>(cur.n))
    if (auto ln = std::dynamic_pointer_cast<LogNode>(ex->a)) { changed = true; return normalize(Expr{ln->a}); }

  // Odd/even simplifications: sin(-x)=-sin(x); cos(-x)=cos(x); tanh(-x)=-tanh(x)
  if (auto sn = std::dynamic_pointer_cast<SinNode>(cur.n))
    if (auto neg = std::dynamic_pointer_cast<NegNode>(sn->a)) { changed = true; return normalize(-Expr{neg->a}); }
  if (auto cs = std::dynamic_pointer_cast<CosNode>(cur.n))
    if (auto neg = std::dynamic_pointer_cast<NegNode>(cs->a)) { changed = true; return normalize(cos(Expr{neg->a})); }
  if (auto th = std::dynamic_pointer_cast<TanhNode>(cur.n))
    if (auto neg = std::dynamic_pointer_cast<NegNode>(th->a)) { changed = true; return normalize(-Expr{neg->a}); }

  // Product of exponentials: exp(a) * exp(b) -> exp(a + b)
  if (auto mul_exp = std::dynamic_pointer_cast<MulNode>(cur.n)) {
    auto Lexp = std::dynamic_pointer_cast<ExpNode>(mul_exp->a);
    auto Rexp = std::dynamic_pointer_cast<ExpNode>(mul_exp->b);
    if (Lexp && Rexp) { changed = true; return normalize(exp(Expr{Lexp->a} + Expr{Rexp->a})); }
  }

  // Powers with same base and integer exponents: x^m * x^n -> x^(m+n)
  if (auto mul_pow = std::dynamic_pointer_cast<MulNode>(cur.n)) {
    auto LP = std::dynamic_pointer_cast<PowNode>(mul_pow->a);
    auto RP = std::dynamic_pointer_cast<PowNode>(mul_pow->b);
    // Case 1: pow(x,m) * pow(x,n)
    if (LP && RP) {
      if (equal(Expr{LP->a}, Expr{RP->a})) {
        long long mi, ni;
        if (is_integer_const(Expr{LP->b}, &mi) && is_integer_const(Expr{RP->b}, &ni)) {
          changed = true;
          return normalize(pow(Expr{LP->a}, lit(static_cast<double>(mi + ni))));
        }
      }
    }
    // Case 2: x * pow(x,n) or pow(x,n) * x
    if (LP && !RP) {
      if (equal(Expr{LP->a}, Expr{mul_pow->b})) {
        long long ni; if (is_integer_const(Expr{LP->b}, &ni)) { changed = true; return normalize(pow(Expr{LP->a}, lit(static_cast<double>(ni + 1)))); }
      }
    } else if (RP && !LP) {
      if (equal(Expr{RP->a}, Expr{mul_pow->a})) {
        long long ni; if (is_integer_const(Expr{RP->b}, &ni)) { changed = true; return normalize(pow(Expr{RP->a}, lit(static_cast<double>(ni + 1)))); }
      }
    }
  }

  // Pythagorean: sin(u)^2 + cos(u)^2 -> 1
  if (auto add = std::dynamic_pointer_cast<AddNode>(cur.n)) {
    std::vector<Expr> terms; decompose_add(cur, terms);
    // Find sin^2(u) and cos^2(u)
    for (std::size_t i = 0; i < terms.size(); ++i) {
      Expr ui;
      if (!is_sin_sq(terms[i], ui)) continue;
      for (std::size_t j = 0; j < terms.size(); ++j) if (j != i) {
        Expr uj;
        if (is_cos_sq(terms[j], uj) && equal(ui, uj)) {
          // Remove i and j, add 1.0
          std::vector<Expr> rest;
          rest.reserve(terms.size()-1);
          for (std::size_t k = 0; k < terms.size(); ++k) if (k != i && k != j) rest.push_back(terms[k]);
          rest.push_back(lit(1.0));
          changed = true;
          return normalize(make_add(rest));
        }
      }
    }
  }

  // Combine fractions with common denominator
  // a/b + c/b -> (a+c)/b
  if (auto add_div = std::dynamic_pointer_cast<AddNode>(cur.n)) {
    auto DL = std::dynamic_pointer_cast<DivNode>(add_div->a);
    auto DR = std::dynamic_pointer_cast<DivNode>(add_div->b);
    if (DL && DR && equal(Expr{DL->b}, Expr{DR->b})) {
      changed = true;
      return normalize((Expr{DL->a} + Expr{DR->a}) / Expr{DL->b});
    }
  }
  // a/b - c/b -> (a-c)/b
  if (auto sub_div = std::dynamic_pointer_cast<SubNode>(cur.n)) {
    auto DL = std::dynamic_pointer_cast<DivNode>(sub_div->a);
    auto DR = std::dynamic_pointer_cast<DivNode>(sub_div->b);
    if (DL && DR && equal(Expr{DL->b}, Expr{DR->b})) {
      changed = true;
      return normalize((Expr{DL->a} - Expr{DR->a}) / Expr{DL->b});
    }
  }

  // Like-term merging under Add: sum_i c_i * b_i with equal(b_i) -> (sum c_i) * b
  if (auto add2 = std::dynamic_pointer_cast<AddNode>(cur.n)) {
    std::vector<Expr> terms; decompose_add(cur, terms);
    struct Group { Expr base; double coeff; };
    std::vector<Group> groups;
    double csum = 0.0;
    auto add_group = [&](const Expr& base, double c){
      for (auto& g : groups) if (equal(g.base, base)) { g.coeff += c; return; }
      groups.push_back(Group{ base, c });
    };
    for (auto& t : terms) {
      // decompose t into coeff * base where base has no leading constant factor
      Expr tn = normalize(t);
      double v;
      if (is_const(tn, &v)) { csum += v; continue; }
      // Flatten mul and extract constant factor
      double coeff = 1.0;
      std::vector<Expr> facts;
      if (auto mn = std::dynamic_pointer_cast<MulNode>(tn.n)) {
        gather_mul(tn, facts);
      } else {
        facts.push_back(tn);
      }
      std::vector<Expr> nonc;
      for (auto& f : facts) {
        double fv; if (is_const(f, &fv)) coeff *= fv; else nonc.push_back(f);
      }
      if (nonc.empty()) { csum += coeff; continue; }
      Expr base = make_mul(nonc);
      add_group(base, coeff);
    }
    // Rebuild
    std::vector<Expr> out_terms;
    out_terms.reserve(groups.size() + 1);
    for (auto& g : groups) {
      if (std::fabs(g.coeff) < 1e-18) continue; // drop near-zero
      if (g.coeff == 1.0) out_terms.push_back(g.base);
      else out_terms.push_back(lit(g.coeff) * g.base);
    }
    if (!out_terms.empty() || csum != 0.0) {
      if (csum != 0.0) out_terms.push_back(lit(csum));
      Expr rebuilt = normalize(make_add(out_terms));
      if (!equal(rebuilt, cur)) { changed = true; return rebuilt; }
    }
  }

  // Common factor factoring: if all add terms share a factor f, factor to f * sum(rest)
  if (auto add3 = std::dynamic_pointer_cast<AddNode>(cur.n)) {
    std::vector<Expr> terms; decompose_add(cur, terms);
    if (terms.size() >= 2) {
      // Decompose each term into factors (flatten mul). Keep one list per term.
      std::vector<std::vector<Expr>> facts_list; facts_list.reserve(terms.size());
      for (auto& t0 : terms) {
        Expr t = normalize(t0);
        std::vector<Expr> fl;
        if (std::dynamic_pointer_cast<MulNode>(t.n)) gather_mul(t, fl);
        else fl.push_back(t);
        facts_list.push_back(std::move(fl));
      }
      // Candidate factors: all non-constant factors from first term
      std::vector<Expr> cands;
      for (auto& f : facts_list[0]) { double v; if (!is_const(f,&v)) cands.push_back(f); }
      for (auto& cand : cands) {
        bool ok = true;
        // For each other term, check occurrence of an equal factor and mark to remove
        std::vector<int> pos(facts_list.size(), -1);
        for (std::size_t i = 0; i < facts_list.size(); ++i) {
          auto& fl = facts_list[i];
          bool found = false;
          for (std::size_t j = 0; j < fl.size(); ++j) {
            if (equal(fl[j], cand)) { pos[i] = (int)j; found = true; break; }
          }
          if (!found) { ok = false; break; }
        }
        if (ok) {
          // Build rest terms by removing one occurrence of cand from each list
          std::vector<Expr> rest_terms; rest_terms.reserve(terms.size());
          for (std::size_t i = 0; i < facts_list.size(); ++i) {
            auto fl = facts_list[i];
            if (pos[i] >= 0) fl.erase(fl.begin() + pos[i]);
            Expr rest = make_mul(fl);
            rest_terms.push_back(rest);
          }
          Expr inner = normalize(make_add(rest_terms));
          Expr rebuilt = normalize(cand * inner);
          if (!equal(rebuilt, cur)) { changed = true; return rebuilt; }
        }
      }
      // Pairwise factoring for exactly two terms: factor any shared non-const
      if (terms.size() == 2) {
        auto f1 = facts_list[0], f2 = facts_list[1];
        for (std::size_t i = 0; i < f1.size(); ++i) {
          double v; if (is_const(f1[i], &v)) continue;
          for (std::size_t j = 0; j < f2.size(); ++j) {
            if (equal(f1[i], f2[j])) {
              auto fl1 = f1; auto fl2 = f2; fl1.erase(fl1.begin()+i); fl2.erase(fl2.begin()+j);
              Expr inner = normalize(make_add({ make_mul(fl1), make_mul(fl2) }));
              Expr rebuilt = normalize(f1[i] * inner);
              if (!equal(rebuilt, cur)) { changed = true; return rebuilt; }
            }
          }
        }
      }
    }
  }

  // Square completion: u*u ± 2*u*v + v*v -> (u ± v)^2
  if (auto add4 = std::dynamic_pointer_cast<AddNode>(cur.n)) {
    std::vector<Expr> terms; decompose_add(cur, terms);
    // Try all pairs for u^2 and v^2
    for (std::size_t i = 0; i < terms.size(); ++i) {
      for (std::size_t j = i+1; j < terms.size(); ++j) {
        auto pi = std::dynamic_pointer_cast<PowNode>(terms[i].n);
        auto pj = std::dynamic_pointer_cast<PowNode>(terms[j].n);
        double ei, ej;
        if (!(pi && pj && is_const(Expr{pi->b}, &ei) && is_const(Expr{pj->b}, &ej) && ei==2.0 && ej==2.0)) continue;
        Expr u = normalize(Expr{pi->a});
        Expr v = normalize(Expr{pj->a});
        // Find middle term ± 2*u*v
        for (std::size_t k = 0; k < terms.size(); ++k) if (k != i && k != j) {
          // Extract coefficient and base from terms[k]
          std::vector<Expr> fl; if (auto mk = std::dynamic_pointer_cast<MulNode>(terms[k].n)) gather_mul(terms[k], fl); else fl.push_back(terms[k]);
          double coeff = 1.0; std::vector<Expr> nonc;
          for (auto& f : fl) { double fv; if (is_const(f,&fv)) coeff *= fv; else nonc.push_back(f); }
          if (nonc.size() != 2) continue;
          Expr a = normalize(nonc[0]); Expr b = normalize(nonc[1]);
          bool match_uv = (equal(a,u) && equal(b,v)) || (equal(a,v) && equal(b,u));
          if (!match_uv) continue;
          if (std::fabs(std::fabs(coeff) - 2.0) < 1e-12) {
            // Build (u ± v)^2 based on sign
            Expr inner = (coeff > 0) ? normalize(u + v) : normalize(u - v);
            Expr rebuilt = normalize(pow(inner, lit(2.0)));
            // Build rest of terms
            std::vector<Expr> rest;
            for (std::size_t t = 0; t < terms.size(); ++t) if (t != (int)i && t != (int)j && t != (int)k) rest.push_back(terms[t]);
            rest.push_back(rebuilt);
            Expr sum = normalize(make_add(rest));
            if (!equal(sum, cur)) { changed = true; return sum; }
          }
        }
      }
    }
  }

  return cur;
}

inline Expr rewrite_fixed_point(const Expr& e, int max_iters = 12) {
  Expr cur = normalize(e);
  for (int it = 0; it < max_iters; ++it) {
    bool changed = false;
    Expr next = rewrite_once(cur, changed);
    next = normalize(next);
    if (!changed || equal(next, cur)) return next;
    cur = std::move(next);
  }
  return cur;
}

} // namespace et
