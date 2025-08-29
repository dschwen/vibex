#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#include "et/ast.hpp"
#include "et/compile.hpp"
#include "et/tape_backend.hpp"

using namespace et;

static bool approx(double a, double b, double eps = 1e-10) {
  return std::fabs(a - b) <= eps * (1.0 + std::max(std::fabs(a), std::fabs(b)));
}

int main() {
  // Build an AST with a variety of ops to exercise AST->Tape lowering
  auto x = var(0), y = var(1);
  Expr f = pow(sin(x) + cos(y), lit(2.0))
         + log(exp(x * y))
         + sqrt(x + lit(3.0))
         + tanh(-y)
         + (x / (y + lit(2.0)));

  std::vector<double> pt = {0.7, 1.3};

  // 1) Compare AST eval vs. Tape forward
  double v_ast = eval(f, pt);
  TapeBackend tb(2);
  int out = compile_runtime(f, tb); // AST -> Tape
  tb.tape.output_id = out;
  double v_tape = tb.tape.forward(pt);
  assert(approx(v_ast, v_tape));

  // 2) Compare Tape gradient vs finite differences
  auto grad = tb.tape.backward(pt);
  assert(grad.size() >= 2);
  // Finite differences (central) for x and y
  auto fd = [&](std::size_t idx) {
    std::vector<double> p1 = pt, p2 = pt;
    const double h = 1e-6;
    p1[idx] += h; p2[idx] -= h;
    double f1 = eval(f, p1);
    double f2 = eval(f, p2);
    return (f1 - f2) / (2*h);
  };
  double gx_fd = fd(0);
  double gy_fd = fd(1);
  assert(approx(grad[0], gx_fd, 1e-6));
  assert(approx(grad[1], gy_fd, 1e-6));

  std::cout << "ok ast_vs_tape forward+grad sanity\n";
  return 0;
}
