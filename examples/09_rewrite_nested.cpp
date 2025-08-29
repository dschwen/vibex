#include <iostream>
#include <vector>

#include "et/ast.hpp"
#include "et/normalize.hpp"
#include "et/rewrite_ast.hpp"

using namespace et;

int main() {
  auto x = var(0), y = var(1), z = var(2), p = var(3), q = var(4), rr = var(5), s = var(6), w = var(7);
  Expr a = log(exp(x + y));
  Expr u = a + sin(w);
  Expr e = sin(u)*sin(u) + cos(u)*cos(u)
         + log(exp(u))
         + (lit(2.0)*u + lit(3.0)*u)
         + (p*p + lit(2.0)*p*q + q*q)
         + ( (p*p) - (lit(2.0)*p*q) + (q*q) )
         + lit(5.0);

  Expr before = normalize(e);
  Expr after  = rewrite_fixed_point(e);

  std::vector<double> in = {0.7,0.9,-0.3,1.1,-0.4,0.2,0.5,0.8};
  auto eval_e = [&](const std::vector<double>& v){ return eval(e, v); };
  double v0 = eval_e(in);
  double v1 = eval(after, in);
  std::cout << "Eval original:  " << v0 << "\n";
  std::cout << "Eval rewritten: " << v1 << "\n";
  return 0;
}
