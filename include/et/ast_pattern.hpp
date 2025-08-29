#pragma once
#include <vector>
#include <string>

namespace et { namespace astpat {

struct Pattern {
  enum class Kind { Placeholder, Node };
  Kind kind{Kind::Node};
  std::string name; // e.g., "Add","Mul","Neg","Sin","Cos"
  int placeholder_id{-1};
  bool is_spread{false};
  std::vector<Pattern> ch;

  static Pattern placeholder(int id) {
    Pattern p; p.kind = Kind::Placeholder; p.placeholder_id = id; return p;
  }
  static Pattern node(const char* nm, std::vector<Pattern> c = {}) {
    Pattern p; p.kind = Kind::Node; p.name = nm; p.ch = std::move(c); return p;
  }
};

// Placeholder helpers
inline Pattern P(int id) { return Pattern::placeholder(id); }
inline Pattern S(int id) { Pattern p = Pattern::placeholder(id); p.is_spread = true; return p; }

// Node builders
inline Pattern add(const Pattern& a, const Pattern& b) { return Pattern::node("Add", {a,b}); }
inline Pattern mul(const Pattern& a, const Pattern& b) { return Pattern::node("Mul", {a,b}); }
inline Pattern neg(const Pattern& a) { return Pattern::node("Neg", {a}); }
inline Pattern sin(const Pattern& a) { return Pattern::node("Sin", {a}); }
inline Pattern cos(const Pattern& a) { return Pattern::node("Cos", {a}); }

// Specificity heuristic
inline int specificity(const Pattern& p) {
  if (p.kind == Pattern::Kind::Placeholder) return 0;
  int s = 1; for (auto& c : p.ch) s += specificity(c); return s;
}

// Operator sugar
inline Pattern operator+(const Pattern& a, const Pattern& b) { return add(a,b); }
inline Pattern operator*(const Pattern& a, const Pattern& b) { return mul(a,b); }


} } // namespace et::astpat
