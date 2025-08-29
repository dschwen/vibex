#include <cassert>
#include <vector>
#include <string>

#include "et/ast.hpp"
#include "et/normalize.hpp"
#include "et/rewrite_ast.hpp"

using namespace et;

static bool approx(double a, double b, double eps = 1e-10) {
  return std::fabs(a - b) <= eps * (1.0 + std::max(std::fabs(a), std::fabs(b)));
}

static int count_substr(const std::string& s, const std::string& sub) {
  if (sub.empty()) return 0;
  int c = 0; std::size_t pos = 0;
  while (true) {
    pos = s.find(sub, pos);
    if (pos == std::string::npos) break;
    ++c; ++pos;
  }
  return c;
}

int main() {
  // Many variables to build complex nested subterms
  auto x = var(0), y = var(1), z = var(2), p = var(3), q = var(4), rr = var(5), s = var(6), w = var(7);

  // Complex subterm that will itself be simplified by rules: log(exp(x+y)) -> (x+y)
  auto a = log(exp(x + y));
  // Compose a placeholder-worthy subterm that appears in multiple places
  auto u = a + sin(w);

  // Build a large expression combining many identities:
  // - Pythagorean: sin(u)^2 + cos(u)^2
  // - log(exp(u)) cancellation
  // - like-term merging: 2*u + 3*u
  // - square completion (both plus and minus variants)
  // - a trailing constant
  auto e = sin(u)*sin(u) + cos(u)*cos(u)
         + log(exp(u))
         + (lit(2.0)*u + lit(3.0)*u)
         + (p*p + lit(2.0)*p*q + q*q)
         + ( (p*p) - (lit(2.0)*p*q) + (q*q) )
         + lit(5.0);

  // Rewrite AST to fixed point using AST-native rules
  Expr r = rewrite_fixed_point(e);

  // Numeric equivalence check across a few points
  auto eval_ast = [&](double X,double Y,double Z,double P,double Q,double R,double S,double W){ return eval(e, std::vector<double>{X,Y,Z,P,Q,R,S,W}); };

  // A couple of test points (avoid domains that cause issues like log of nonpositive)
  struct Pt { double X,Y,Z,P,Q,R,S,W; };
  std::vector<Pt> pts = {
    {0.7, 0.9, -0.3, 1.1, -0.4, 0.2, 0.5, 0.8},
    {1.3, 0.2,  0.4, -0.7, 0.6, 1.5, -0.9, -0.2},
    {0.0, 2.0,  1.0, 0.3, 0.3, -1.2, 0.4, 1.2}
  };
  for (const auto& pt : pts) {
    double v0 = eval_ast(pt.X,pt.Y,pt.Z,pt.P,pt.Q,pt.R,pt.S,pt.W);
    double v1 = eval(r, std::vector<double>{pt.X,pt.Y,pt.Z,pt.P,pt.Q,pt.R,pt.S,pt.W});
    assert(approx(v0, v1));
  }

  return 0;
}
