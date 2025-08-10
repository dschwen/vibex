#include <cassert>
#include <cmath>
#include <vector>

#include "et/expr.hpp"

using namespace et;

static bool approx(double a, double b, double eps = 1e-9) {
  return std::fabs(a - b) <= eps * (1.0 + std::max(std::fabs(a), std::fabs(b)));
}

template <class Expr>
static double fd2(const Expr& e, std::vector<double> in, std::size_t i, double h = 1e-6) {
  in[i] += h; double f1 = e(in[0], in[1]);
  in[i] -= 2*h; double f2 = e(in[0], in[1]);
  return (f1 - f2) / (2*h);
}

int main() {
  auto [x, y] = Vars<double, 2>();

  // Exercise binary ops: +, -, *, /
  // f(x,y) = x*y + x/y - y*x + x + 3 - x*x
  // Simplifies to: f = x/y + x + 3 - x*x
  // df/dx = 1/y + 1 - 2x; df/dy = -x/(y^2)
  auto f = x*y + x/y - y*x + x + lit(3.0) - x*x;

  // Analytic via diff should match closed form and finite difference
  auto dfx = diff(f, x);
  auto dfy = diff(f, y);

  std::vector<double> in = {1.3, 2.2};
  double xv = in[0], yv = in[1];

  double dfx_expected = 1.0 / yv + 1.0 - 2.0 * xv;
  double dfy_expected = -xv / (yv * yv);

  assert(approx(dfx(xv, yv), dfx_expected));
  assert(approx(dfy(xv, yv), dfy_expected));

  double dfx_fd = fd2(f, in, 0);
  double dfy_fd = fd2(f, in, 1);
  assert(approx(dfx(xv, yv), dfx_fd, 1e-6));
  assert(approx(dfy(xv, yv), dfy_fd, 1e-6));

  // Unary neg interaction: g(x,y) = -(x*y) + y
  auto g = -(x*y) + y;
  auto dgx = diff(g, x);
  auto dgy = diff(g, y);
  std::vector<double> in2 = {0.7, -1.1};
  double gx_fd = fd2(g, in2, 0);
  double gy_fd = fd2(g, in2, 1);
  assert(approx(dgx(in2[0], in2[1]), gx_fd, 1e-6));
  assert(approx(dgy(in2[0], in2[1]), gy_fd, 1e-6));

  return 0;
}

