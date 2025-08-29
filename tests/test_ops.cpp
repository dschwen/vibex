#include <cassert>
#include <cmath>
#include <vector>
#include "et/ast.hpp"
#include "et/compile_ast.hpp"
#include "et/tape_backend.hpp"

using namespace et;

static bool approx(double a, double b, double eps = 1e-9) {
  return std::fabs(a - b) <= eps * (1.0 + std::max(std::fabs(a), std::fabs(b)));
}

static double fd1(const Expr& e, double x, double h = 1e-6) {
  std::vector<double> p1 = {x}, p2 = {x};
  p1[0] += h; p2[0] -= h;
  return (eval(e, p1) - eval(e, p2)) / (2*h);
}

static void check_unary(const Expr& e, double xv, double ref, double ref_d) {
  // Forward
  assert(approx(eval(e, {xv}), ref));
  // Tape gradient
  TapeBackend tb(1);
  int out = compile_runtime(e, tb);
  tb.tape.output_id = out;
  auto g = tb.tape.backward({xv});
  assert(g.size() >= 1);
  // FD check
  double fd = fd1(e, xv);
  assert(approx(g[0], fd, 1e-6));
  // Compare to analytic ref (loose tolerance)
  assert(approx(g[0], ref_d, 1e-6));
}

int main() {
  auto x = var(0);

  // exp: d/dx exp(x) = exp(x)
  {
    auto e = exp(x);
    double xv = 0.3;
    check_unary(e, xv, std::exp(xv), std::exp(xv));
  }

  // sin: d/dx sin(x) = cos(x); cos: d/dx cos(x) = -sin(x)
  {
    auto s = sin(x);
    auto c = cos(x);
    double xv = 0.8;
    check_unary(s, xv, std::sin(xv), std::cos(xv));
    check_unary(c, xv, std::cos(xv), -std::sin(xv));
  }

  // log: d/dx log(x) = 1/x (x>0)
  {
    auto l = log(x);
    double xv = 1.7;
    check_unary(l, xv, std::log(xv), 1.0 / xv);
  }

  // sqrt: d/dx sqrt(x) = 1/(2*sqrt(x)) (x>0)
  {
    auto s = sqrt(x);
    double xv = 2.5;
    check_unary(s, xv, std::sqrt(xv), 0.5 / std::sqrt(xv));
    // constant expression eval
    auto sc = sqrt(lit(2.5));
    assert(approx(eval(sc, {0.0}), std::sqrt(2.5)));
  }

  // tanh: d/dx tanh(x) = 1 - tanh(x)^2
  {
    auto t = tanh(x);
    double xv = -0.9;
    double th = std::tanh(xv);
    check_unary(t, xv, th, 1.0 - th * th);
    // constant expression eval
    auto tc = tanh(lit(-0.9));
    assert(approx(eval(tc, {0.0}), std::tanh(-0.9)));
  }

  return 0;
}
