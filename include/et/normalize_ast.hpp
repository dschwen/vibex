#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "et/ast.hpp"

namespace et {

// Helpers to detect constant values
inline bool is_const(const Expr& e, double* out = nullptr) {
  if (!e.n) return false;
  if (auto c = std::dynamic_pointer_cast<ConstNode>(e.n)) {
    if (out) *out = c->value; return true;
  }
  return false;
}
inline bool is_zero(const Expr& e) { double v; return is_const(e, &v) && v == 0.0; }
inline bool is_one (const Expr& e) { double v; return is_const(e, &v) && v == 1.0; }

// Canonical ordering key: (kind, address) where kind groups similar nodes.
inline int kind_rank(const Expr& e) {
  auto p = e.n;
  if (std::dynamic_pointer_cast<ConstNode>(p)) return 0;
  if (std::dynamic_pointer_cast<VarNode>(p))   return 1;
  if (std::dynamic_pointer_cast<NegNode>(p))   return 2;
  if (std::dynamic_pointer_cast<SinNode>(p))   return 3;
  if (std::dynamic_pointer_cast<CosNode>(p))   return 4;
  if (std::dynamic_pointer_cast<ExpNode>(p))   return 5;
  if (std::dynamic_pointer_cast<LogNode>(p))   return 6;
  if (std::dynamic_pointer_cast<SqrtNode>(p))  return 7;
  if (std::dynamic_pointer_cast<TanhNode>(p))  return 8;
  if (std::dynamic_pointer_cast<AddNode>(p))   return 9;
  if (std::dynamic_pointer_cast<SubNode>(p))   return 10;
  if (std::dynamic_pointer_cast<MulNode>(p))   return 11;
  if (std::dynamic_pointer_cast<DivNode>(p))   return 12;
  if (std::dynamic_pointer_cast<PowNode>(p))   return 13;
  if (std::dynamic_pointer_cast<LtNode>(p))    return 14;
  if (std::dynamic_pointer_cast<LeNode>(p))    return 15;
  if (std::dynamic_pointer_cast<GtNode>(p))    return 16;
  if (std::dynamic_pointer_cast<GeNode>(p))    return 17;
  if (std::dynamic_pointer_cast<EqNode>(p))    return 18;
  if (std::dynamic_pointer_cast<NeNode>(p))    return 19;
  if (std::dynamic_pointer_cast<IfNode>(p))    return 20;
  if (std::dynamic_pointer_cast<SelectNode>(p))return 21;
  return 100;
}

inline auto order_key(const Expr& e) {
  return std::make_pair(kind_rank(e), reinterpret_cast<std::uintptr_t>(e.n.get()));
}

// Forward decl
Expr normalize(const Expr& e);

inline void gather_add(const Expr& e, std::vector<Expr>& out) {
  if (auto n = std::dynamic_pointer_cast<AddNode>(e.n)) {
    gather_add(Expr{n->a}, out);
    gather_add(Expr{n->b}, out);
  } else {
    out.push_back(e);
  }
}
inline void gather_mul(const Expr& e, std::vector<Expr>& out) {
  if (auto n = std::dynamic_pointer_cast<MulNode>(e.n)) {
    gather_mul(Expr{n->a}, out);
    gather_mul(Expr{n->b}, out);
  } else {
    out.push_back(e);
  }
}

inline Expr make_add(std::vector<Expr> terms) {
  if (terms.empty()) return lit(0.0);
  Expr acc = terms[0];
  for (std::size_t i = 1; i < terms.size(); ++i) acc = acc + terms[i];
  return acc;
}
inline Expr make_mul(std::vector<Expr> terms) {
  if (terms.empty()) return lit(1.0);
  Expr acc = terms[0];
  for (std::size_t i = 1; i < terms.size(); ++i) acc = acc * terms[i];
  return acc;
}

inline Expr normalize_add(const std::vector<Expr>& xs) {
  // Flatten, normalize children, fold constants, drop zeros, sort
  std::vector<Expr> terms; terms.reserve(xs.size());
  double csum = 0.0; bool has_csum = false;
  for (auto& t0 : xs) {
    Expr t = normalize(t0);
    if (is_const(t)) { double v; is_const(t, &v); csum += v; has_csum = true; continue; }
    // flatten nested adds
    if (std::dynamic_pointer_cast<AddNode>(t.n)) gather_add(t, terms);
    else terms.push_back(t);
  }
  // remove zeros and add constant if any
  std::vector<Expr> filtered; filtered.reserve(terms.size()+1);
  for (auto& t : terms) if (!is_zero(t)) filtered.push_back(t);
  if (has_csum && (csum != 0.0 || filtered.empty())) filtered.push_back(lit(csum));
  std::sort(filtered.begin(), filtered.end(), [&](const Expr& a, const Expr& b){ return order_key(a) < order_key(b); });
  return make_add(filtered);
}

inline Expr normalize_mul(const std::vector<Expr>& xs) {
  std::vector<Expr> terms; terms.reserve(xs.size());
  double cprod = 1.0; bool has_cprod = false;
  for (auto& t0 : xs) {
    Expr t = normalize(t0);
    if (is_const(t)) { double v; is_const(t, &v); cprod *= v; has_cprod = true; continue; }
    if (std::dynamic_pointer_cast<MulNode>(t.n)) gather_mul(t, terms);
    else terms.push_back(t);
  }
  if (has_cprod) {
    if (cprod == 0.0) return lit(0.0);
  }
  std::vector<Expr> filtered; filtered.reserve(terms.size()+1);
  for (auto& t : terms) if (!is_one(t)) filtered.push_back(t);
  if (has_cprod && (cprod != 1.0 || filtered.empty())) filtered.push_back(lit(cprod));
  std::sort(filtered.begin(), filtered.end(), [&](const Expr& a, const Expr& b){ return order_key(a) < order_key(b); });
  return make_mul(filtered);
}

inline Expr normalize(const Expr& e) {
  if (!e.n) return e;
  // Leaves
  if (std::dynamic_pointer_cast<ConstNode>(e.n)) return e;
  if (std::dynamic_pointer_cast<VarNode>(e.n))   return e;

  // Unary
  if (auto n = std::dynamic_pointer_cast<NegNode>(e.n)) {
    Expr a = normalize(Expr{n->a});
    double v; if (is_const(a, &v)) return lit(-v);
    return -a;
  }
  if (auto n = std::dynamic_pointer_cast<SinNode>(e.n)) { Expr a = normalize(Expr{n->a}); double v; if (is_const(a, &v)) return lit(std::sin(v)); return sin(a); }
  if (auto n = std::dynamic_pointer_cast<CosNode>(e.n)) { Expr a = normalize(Expr{n->a}); double v; if (is_const(a, &v)) return lit(std::cos(v)); return cos(a); }
  if (auto n = std::dynamic_pointer_cast<ExpNode>(e.n)) { Expr a = normalize(Expr{n->a}); double v; if (is_const(a, &v)) return lit(std::exp(v)); return exp(a); }
  if (auto n = std::dynamic_pointer_cast<LogNode>(e.n)) { Expr a = normalize(Expr{n->a}); double v; if (is_const(a, &v)) return lit(std::log(v)); return log(a); }
  if (auto n = std::dynamic_pointer_cast<SqrtNode>(e.n)){ Expr a = normalize(Expr{n->a}); double v; if (is_const(a, &v)) return lit(std::sqrt(v)); return sqrt(a); }
  if (auto n = std::dynamic_pointer_cast<TanhNode>(e.n)){ Expr a = normalize(Expr{n->a}); double v; if (is_const(a, &v)) return lit(std::tanh(v)); return tanh(a); }

  // Binary arithmetic
  if (auto n = std::dynamic_pointer_cast<AddNode>(e.n)) {
    std::vector<Expr> xs; xs.reserve(2); xs.push_back(Expr{n->a}); xs.push_back(Expr{n->b});
    return normalize_add(xs);
  }
  if (auto n = std::dynamic_pointer_cast<SubNode>(e.n)) {
    Expr a = normalize(Expr{n->a}); Expr b = normalize(Expr{n->b});
    // Fold constants
    if (is_const(a) && is_const(b)) { double va,vb; is_const(a,&va); is_const(b,&vb); return lit(va - vb); }
    if (is_zero(b)) return a;
    return a + (-b);
  }
  if (auto n = std::dynamic_pointer_cast<MulNode>(e.n)) {
    std::vector<Expr> xs; xs.reserve(2); xs.push_back(Expr{n->a}); xs.push_back(Expr{n->b});
    return normalize_mul(xs);
  }
  if (auto n = std::dynamic_pointer_cast<DivNode>(e.n)) {
    Expr a = normalize(Expr{n->a}); Expr b = normalize(Expr{n->b});
    if (is_const(a) && is_const(b)) { double va,vb; is_const(a,&va); is_const(b,&vb); return lit(va / vb); }
    if (is_zero(a)) return lit(0.0);
    if (is_one(b)) return a;
    return a / b;
  }
  if (auto n = std::dynamic_pointer_cast<PowNode>(e.n)) {
    Expr a = normalize(Expr{n->a}); Expr b = normalize(Expr{n->b});
    if (is_const(a) && is_const(b)) { double va,vb; is_const(a,&va); is_const(b,&vb); return lit(std::pow(va, vb)); }
    // identities: x^1 = x; x^0 = 1 (for x!=0)
    if (is_one(b)) return a;
    double bv; if (is_const(b, &bv) && bv == 0.0) return lit(1.0);
    return pow(a, b);
  }

  // Comparisons
  if (auto n = std::dynamic_pointer_cast<LtNode>(e.n)) { Expr a = normalize(Expr{n->a}); Expr b = normalize(Expr{n->b}); double va,vb; if (is_const(a,&va)&&is_const(b,&vb)) return lit(va<vb?1.0:0.0); return a < b; }
  if (auto n = std::dynamic_pointer_cast<LeNode>(e.n)) { Expr a = normalize(Expr{n->a}); Expr b = normalize(Expr{n->b}); double va,vb; if (is_const(a,&va)&&is_const(b,&vb)) return lit(va<=vb?1.0:0.0); return a <= b; }
  if (auto n = std::dynamic_pointer_cast<GtNode>(e.n)) { Expr a = normalize(Expr{n->a}); Expr b = normalize(Expr{n->b}); double va,vb; if (is_const(a,&va)&&is_const(b,&vb)) return lit(va>vb?1.0:0.0); return a > b; }
  if (auto n = std::dynamic_pointer_cast<GeNode>(e.n)) { Expr a = normalize(Expr{n->a}); Expr b = normalize(Expr{n->b}); double va,vb; if (is_const(a,&va)&&is_const(b,&vb)) return lit(va>=vb?1.0:0.0); return a >= b; }
  if (auto n = std::dynamic_pointer_cast<EqNode>(e.n)) { Expr a = normalize(Expr{n->a}); Expr b = normalize(Expr{n->b}); double va,vb; if (is_const(a,&va)&&is_const(b,&vb)) return lit(va==vb?1.0:0.0); return a == b; }
  if (auto n = std::dynamic_pointer_cast<NeNode>(e.n)) { Expr a = normalize(Expr{n->a}); Expr b = normalize(Expr{n->b}); double va,vb; if (is_const(a,&va)&&is_const(b,&vb)) return lit(va!=vb?1.0:0.0); return a != b; }

  // If/Select
  if (auto n = std::dynamic_pointer_cast<IfNode>(e.n)) {
    Expr c = normalize(Expr{n->c}); Expr t = normalize(Expr{n->t}); Expr el = normalize(Expr{n->e});
    double vc; if (is_const(c,&vc)) return (vc != 0.0) ? t : el;
    return If(c, t, el);
  }
  if (auto n = std::dynamic_pointer_cast<SelectNode>(e.n)) {
    Expr m = normalize(Expr{n->m}); Expr t = normalize(Expr{n->t}); Expr el = normalize(Expr{n->e});
    double vm; if (is_const(m,&vm)) return (vm != 0.0) ? t : el;
    return Select(m, t, el);
  }

  // Fallback
  return e;
}

} // namespace et

