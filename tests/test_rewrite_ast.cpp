#include <cassert>
#include <vector>
#include <cmath>

#include "et/ast.hpp"
#include "et/normalize_ast.hpp"
#include "et/rewrite_ast.hpp"

using namespace et;

static bool approx(double a, double b, double eps = 1e-12) {
  return std::fabs(a - b) <= eps * (1.0 + std::max(std::fabs(a), std::fabs(b)));
}

int main() {
  // 1) Pythagorean identity: sin(u)^2 + cos(u)^2 -> 1
  {
    auto x = var(0);
    Expr e = sin(x)*sin(x) + cos(x)*cos(x);
    Expr r = rewrite_fixed_point(e);
    for (double t : { -2.3, -0.7, 0.0, 1.1, 2.5 }) {
      double v = eval(r, {t});
      assert(approx(v, 1.0));
    }
  }

  // 2) log(exp(u)) cancellation: u = x + y
  {
    auto x = var(0), y = var(1);
    Expr e = log(exp(x + y));
    Expr r = rewrite_fixed_point(e);
    for (std::pair<double,double> pt : { std::pair<double,double>{0.7, -1.3}, {1.2, 0.4}, {-0.5, 2.2} }) {
      double v = eval(r, {pt.first, pt.second});
      assert(approx(v, pt.first + pt.second));
    }
  }

  // 3) Rewrite inside larger context and repeated terms
  {
    auto x = var(0), y = var(1);
    Expr u = log(exp(x + y));
    Expr t = sin(x)*sin(x) + cos(x)*cos(x);
    Expr e = u + t + u; // 2*(x+y) + 1
    Expr r = rewrite_fixed_point(e);
    for (std::pair<double,double> pt : { std::pair<double,double>{-0.9, 0.3}, {0.0, 0.0}, {2.0, -1.0} }) {
      double v = eval(r, {pt.first, pt.second});
      assert(approx(v, 2.0*(pt.first + pt.second) + 1.0));
    }
  }

  // 4) Factoring: a*x + a*y -> a*(x+y)
  {
    auto a = var(0), x = var(1), y = var(2);
    Expr e = a * x + a * y;
    Expr r = rewrite_fixed_point(e);
    for (std::tuple<double,double,double> pt : { std::make_tuple(2.0, 3.0, 5.0), std::make_tuple(-1.5, 0.7, -0.2) }) {
      double av, xv, yv; std::tie(av,xv,yv) = pt;
      double v = eval(r, {av, xv, yv});
      assert(approx(v, av*(xv + yv)));
    }
  }


  // 5) Square completion: x^2 + 2xy + y^2 -> (x+y)^2 and x^2 - 2xy + y^2 -> (x-y)^2
  {
    auto x = var(0), y = var(1);
    Expr e1 = x*x + lit(2.0)*x*y + y*y;
    Expr r1 = rewrite_fixed_point(e1);
    for (std::pair<double,double> pt : { std::pair<double,double>{1.0,2.0}, {-0.5, 0.7} }) {
      double xv=pt.first, yv=pt.second;
      assert(approx(eval(r1, {xv, yv}), (xv + yv)*(xv + yv)));
    }
    Expr e2 = x*x - lit(2.0)*x*y + y*y;
    Expr r2 = rewrite_fixed_point(e2);
    for (std::pair<double,double> pt : { std::pair<double,double>{1.3,-0.4}, {0.0, 2.1} }) {
      double xv=pt.first, yv=pt.second;
      assert(approx(eval(r2, {xv, yv}), (xv - yv)*(xv - yv)));
    }
  }

  return 0;
}
