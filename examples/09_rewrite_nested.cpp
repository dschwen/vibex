#include <iostream>
#include <vector>

#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/normalize.hpp"
#include "et/rewrite.hpp"
#include "et/rules_default.hpp"

using namespace et;

int main() {
  auto [x,y,z,p,q,r,s,w] = Vars<double,8>();
  auto a = log(exp(x + y));
  auto u = a + sin(w);
  auto e = sin(u)*sin(u) + cos(u)*cos(u)
         + log(exp(u))
         + (lit(2.0)*u + lit(3.0)*u)
         + (p*p + lit(2.0)*p*q + q*q)
         + ( (p*p) - (lit(2.0)*p*q) + (q*q) )
         + lit(5.0);

  RGraph g0 = compile_to_runtime(e);
  g0 = normalize(g0);
  std::cout << "Before: " << r_to_string(g0) << "\n";

  auto rules = default_rules();

  // Show intermediate passes
  RGraph pass = g0;
  for (int i = 0; i < 6; ++i) {
    RGraph next = apply_rules_once(pass, rules);
    next = normalize(next);
    std::cout << "Pass " << i+1 << ": " << r_to_string(next) << "\n";
    if (r_equal(next, next.root, pass.root)) { pass = next; break; }
    pass = next;
  }
  RGraph gr = pass;
  std::cout << "After:  " << r_to_string(gr) << "\n";

  std::vector<double> in = {0.7,0.9,-0.3,1.1,-0.4,0.2,0.5,0.8};
  double v0 = e(in[0],in[1],in[2],in[3],in[4],in[5],in[6],in[7]);
  double v1 = eval(gr, in);
  std::cout << "Eval original: " << v0 << "\n";
  std::cout << "Eval rewritten: " << v1 << "\n";
  return 0;
}
