#include <cassert>
#include <vector>
#include <cmath>

#include "et/ast.hpp"
#include "et/compile_ast.hpp"
#include "et/tape_backend.hpp"

using namespace et;

static bool approx(double a, double b, double eps = 1e-10) {
  return std::fabs(a - b) <= eps * (1.0 + std::max(std::fabs(a), std::fabs(b)));
}

int main() {
  auto x = var(0), y = var(1);

  // Build an AST with a variety of ops to exercise AST->Tape lowering
  Expr f = pow(sin(x) + cos(y), lit(2.0))
         + log(exp(x * y))
         + sqrt(x + lit(3.0))
         + tanh(-y)
         + (x / (y + lit(2.0)));

  // AST -> Tape via compile_runtime
  TapeBackend tb(2);
  int out = compile_runtime(f, tb);
  tb.tape.output_id = out;

  // Compare forward eval against AST eval
  std::vector<double> pt = {0.7, 1.3};
  double v_ast = eval(f, pt);
  double v_tp = tb.tape.forward(pt);
  assert(approx(v_ast, v_tp));

  // Compare gradient vs. finite differences (avoid domain issues)
  auto grad = tb.tape.backward(pt);
  assert(grad.size() >= 2);
  auto fd = [&](std::size_t idx){
    auto p1 = pt, p2 = pt; const double h = 1e-6;
    p1[idx]+=h; p2[idx]-=h;
    double f1 = eval(f, p1); double f2 = eval(f, p2);
    return (f1 - f2) / (2*h);
  };
  double gx = fd(0);
  double gy = fd(1);
  assert(approx(gx, grad[0], 1e-6));
  assert(approx(gy, grad[1], 1e-6));

  return 0;
}
