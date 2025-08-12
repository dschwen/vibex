#include <cassert>
#include <vector>
#include <string>

#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/normalize.hpp"
#include "et/rewrite.hpp"
#include "et/rules_default.hpp"

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
  auto [x,y,z,p,q,r,s,w] = Vars<double,8>();

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

  // Compile and normalize
  RGraph g0 = compile_to_runtime(e);
  g0 = normalize(g0);
  std::string before = r_to_string(g0);

  // Apply default rules to fixed point and normalize
  auto rules = default_rules();
  RGraph gr = rewrite_fixed_point(g0, rules, 12);
  gr = normalize(gr);
  std::string after = r_to_string(gr);

  // Structure should change
  assert(before != after);

  // Check key transformation evidence in the final canonical string:
  // - Pythagorean removed both sin(u)^2 and cos(u)^2 -> contributes a single constant 1
  // - log(exp(u)) vanished
  // - like-term merging produced a 5*... multiplier somewhere
  // - square completion introduced Pow(Add(...), C(2)) and Pow(Sub(...), C(2)) forms
  assert(count_substr(after, "Log(") == 0);
  assert(count_substr(after, "Cos(") == 0);
  assert(count_substr(after, "Pow(") >= 1); // at least one pow from square completion
  assert(after.find("Mul(C(5)") != std::string::npos);

  // Numeric equivalence check across a few points
  auto eval_et = [&](double X,double Y,double Z,double P,double Q,double R,double S,double W){
    // Evaluate original ET directly
    return e(X,Y,Z,P,Q,R,S,W);
  };

  auto eval_rg = [&](double X,double Y,double Z,double P,double Q,double R,double S,double W){
    return eval(gr, std::vector<double>{X,Y,Z,P,Q,R,S,W});
  };

  // A couple of test points (avoid domains that cause issues like log of nonpositive)
  struct Pt { double X,Y,Z,P,Q,R,S,W; };
  std::vector<Pt> pts = {
    {0.7, 0.9, -0.3, 1.1, -0.4, 0.2, 0.5, 0.8},
    {1.3, 0.2,  0.4, -0.7, 0.6, 1.5, -0.9, -0.2},
    {0.0, 2.0,  1.0, 0.3, 0.3, -1.2, 0.4, 1.2}
  };
  for (const auto& pt : pts) {
    double v0 = eval_et(pt.X,pt.Y,pt.Z,pt.P,pt.Q,pt.R,pt.S,pt.W);
    double v1 = eval_rg(pt.X,pt.Y,pt.Z,pt.P,pt.Q,pt.R,pt.S,pt.W);
    assert(approx(v0, v1));
  }

  return 0;
}

