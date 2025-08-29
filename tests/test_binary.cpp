// AST binary ops tests: Tape gradients vs finite differences
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

static double fd2(const Expr& e, std::vector<double> in, std::size_t i, double h = 1e-6) {
  in[i] += h; double f1 = eval(e, in);
  in[i] -= 2*h; double f2 = eval(e, in);
  return (f1 - f2) / (2*h);
}

int main() {
  auto x = var(0), y = var(1);

  // f(x,y) = x*y + x/y - y*x + x + 3 - x*x
  Expr f = x*y + x/y - y*x + x + lit(3.0) - x*x;

  // Tape gradient vs closed form and FD
  std::vector<double> in = {1.3, 2.2};
  double xv = in[0], yv = in[1];
  double dfx_expected = 1.0 / yv + 1.0 - 2.0 * xv;
  double dfy_expected = -xv / (yv * yv);

  TapeBackend tb(2);
  int root = compile_runtime(f, tb);
  tb.tape.output_id = root;
  auto grad = tb.tape.backward(in);
  double dfx_fd = fd2(f, in, 0);
  double dfy_fd = fd2(f, in, 1);
  assert(grad.size() >= 2);
  assert(approx(grad[0], dfx_expected, 1e-6));
  assert(approx(grad[1], dfy_expected, 1e-6));
  assert(approx(grad[0], dfx_fd, 1e-6));
  assert(approx(grad[1], dfy_fd, 1e-6));

  // Unary neg interaction: g(x,y) = -(x*y) + y
  Expr g = -(x*y) + y;
  TapeBackend tb2(2);
  int r2 = compile_runtime(g, tb2);
  tb2.tape.output_id = r2;
  std::vector<double> in2 = {0.7, -1.1};
  auto gg = tb2.tape.backward(in2);
  double gx_fd = fd2(g, in2, 0);
  double gy_fd = fd2(g, in2, 1);
  assert(approx(gg[0], gx_fd, 1e-6));
  assert(approx(gg[1], gy_fd, 1e-6));

  return 0;
}
