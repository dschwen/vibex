#include <cassert>
#include <cmath>
#include "et/expr.hpp"
#include "et/simplify.hpp"

using namespace et;

static bool approx(double a, double b, double eps = 1e-9) {
  return std::fabs(a - b) <= eps * (1.0 + std::max(std::fabs(a), std::fabs(b)));
}

template <class Expr>
static double fd1(const Expr& e, double x, double h = 1e-6) {
  double f1 = e(x + h);
  double f2 = e(x - h);
  return (f1 - f2) / (2*h);
}

int main() {
  auto [x] = Vars<double, 1>();

  // exp: d/dx exp(x) = exp(x)
  {
    auto e = exp(x);
    auto de = diff(e, x);
    double xv = 0.3;
    assert(approx(e(xv), std::exp(xv)));
    assert(approx(de(xv), std::exp(xv)));
    // FD check
    auto ef = [&](double v){ return e(v); };
    assert(approx(de(xv), fd1(ef, xv), 1e-6));
  }

  // sin: d/dx sin(x) = cos(x); cos: d/dx cos(x) = -sin(x)
  {
    auto s = sin(x);
    auto ds = diff(s, x);
    auto c = cos(x);
    auto dc = diff(c, x);
    double xv = 0.8;
    assert(approx(s(xv), std::sin(xv)));
    assert(approx(c(xv), std::cos(xv)));
    assert(approx(ds(xv), std::cos(xv)));
    assert(approx(dc(xv), -std::sin(xv)));
    auto sf = [&](double v){ return s(v); };
    auto cf = [&](double v){ return c(v); };
    assert(approx(ds(xv), fd1(sf, xv), 1e-6));
    assert(approx(dc(xv), fd1(cf, xv), 1e-6));
  }

  // log: d/dx log(x) = 1/x (x>0)
  {
    auto l = log(x);
    auto dl = diff(l, x);
    double xv = 1.7;
    assert(approx(l(xv), std::log(xv)));
    assert(approx(dl(xv), 1.0 / xv));
    auto lf = [&](double v){ return l(v); };
    assert(approx(dl(xv), fd1(lf, xv), 1e-6));
  }

  // sqrt: d/dx sqrt(x) = 1/(2*sqrt(x)) (x>0)
  {
    auto s = sqrt(x);
    auto ds = diff(s, x);
    double xv = 2.5;
    assert(approx(s(xv), std::sqrt(xv)));
    assert(approx(ds(xv), 0.5 / std::sqrt(xv)));
    auto sf = [&](double v){ return s(v); };
    assert(approx(ds(xv), fd1(sf, xv), 1e-6));
    // simplify: sqrt(Const)
    auto sc = simplify(sqrt(lit(2.5)));
    assert(approx(sc(0.0), std::sqrt(2.5)));
  }

  // tanh: d/dx tanh(x) = 1 - tanh(x)^2
  {
    auto t = tanh(x);
    auto dt = diff(t, x);
    double xv = -0.9;
    double th = std::tanh(xv);
    assert(approx(t(xv), th));
    assert(approx(dt(xv), 1.0 - th * th));
    auto tf = [&](double v){ return t(v); };
    assert(approx(dt(xv), fd1(tf, xv), 1e-6));
    // simplify: tanh(Const)
    auto tc = simplify(tanh(lit(-0.9)));
    assert(approx(tc(0.0), std::tanh(-0.9)));
  }

  return 0;
}
