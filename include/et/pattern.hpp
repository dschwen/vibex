#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>

#include "et/runtime_ast.hpp"

namespace et { namespace pat {

struct Pattern {
  enum class Kind : uint8_t { Placeholder, Node };
  Kind kind{Kind::Node};
  NodeKind node_kind{NodeKind::Const};
  int placeholder_id{-1};
  bool is_spread{false}; // matches zero or more args in AC nodes
  std::vector<Pattern> ch;
  // Optional payloads for concrete Const/Var in patterns (primarily for RHS construction)
  double cval{0.0};
  std::size_t var_index{0};

  static Pattern placeholder(int id) {
    Pattern p; p.kind = Kind::Placeholder; p.placeholder_id = id; return p;
  }
  static Pattern node(NodeKind k, std::vector<Pattern> c = {}) {
    Pattern p; p.kind = Kind::Node; p.node_kind = k; p.ch = std::move(c); return p;
  }
};

// Placeholder factory
inline Pattern P(int id) { return Pattern::placeholder(id); }
inline Pattern S(int id) { Pattern p = Pattern::placeholder(id); p.is_spread = true; return p; }
inline Pattern C(double v) { Pattern p = Pattern::node(NodeKind::Const, {}); p.cval = v; return p; }
inline Pattern V(std::size_t idx) { Pattern p = Pattern::node(NodeKind::Var, {}); p.var_index = idx; return p; }

// Unary builders
inline Pattern neg(const Pattern& a) { return Pattern::node(NodeKind::Neg, {a}); }
inline Pattern sin(const Pattern& a) { return Pattern::node(NodeKind::Sin, {a}); }
inline Pattern cos(const Pattern& a) { return Pattern::node(NodeKind::Cos, {a}); }
inline Pattern exp(const Pattern& a) { return Pattern::node(NodeKind::Exp, {a}); }
inline Pattern log(const Pattern& a) { return Pattern::node(NodeKind::Log, {a}); }
inline Pattern sqrt(const Pattern& a){ return Pattern::node(NodeKind::Sqrt,{a}); }
inline Pattern tanh(const Pattern& a){ return Pattern::node(NodeKind::Tanh,{a}); }

// Binary builders
inline Pattern add(const Pattern& a, const Pattern& b) { return Pattern::node(NodeKind::Add, {a,b}); }
inline Pattern sub(const Pattern& a, const Pattern& b) { return Pattern::node(NodeKind::Sub, {a,b}); }
inline Pattern mul(const Pattern& a, const Pattern& b) { return Pattern::node(NodeKind::Mul, {a,b}); }
inline Pattern div(const Pattern& a, const Pattern& b) { return Pattern::node(NodeKind::Div, {a,b}); }
inline Pattern pow(const Pattern& a, const Pattern& b) { return Pattern::node(NodeKind::Pow, {a,b}); }

// Operator sugar for patterns
inline Pattern operator+(const Pattern& a, const Pattern& b) { return add(a,b); }
inline Pattern operator-(const Pattern& a, const Pattern& b) { return sub(a,b); }
inline Pattern operator*(const Pattern& a, const Pattern& b) { return mul(a,b); }
inline Pattern operator/(const Pattern& a, const Pattern& b) { return div(a,b); }
inline Pattern operator-(const Pattern& a) { return neg(a); }

// Heuristic specificity score: higher means more specific
inline int specificity(const Pattern& p) {
  if (p.kind == Pattern::Kind::Placeholder) return 0;
  int s = 1;
  for (const auto& c : p.ch) s += specificity(c);
  return s;
}

} } // namespace et::pat
