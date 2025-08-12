#include <cassert>
#include <vector>
#include <cmath>

#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/compile_runtime.hpp"
#include "et/tape_backend.hpp"

using namespace et;

static bool approx(double a, double b, double eps = 1e-10) {
  return std::fabs(a - b) <= eps * (1.0 + std::max(std::fabs(a), std::fabs(b)));
}

int main() {
  auto [x,y] = Vars<double,2>();

  // Build an ET with a variety of ops to exercise compile_runtime and tape backend
  auto f = pow(sin(x) + cos(y), lit(2.0))
         + log(exp(x*y))
         + sqrt(x + lit(3.0))
         + tanh(-y)
         + (x / (y + lit(2.0)));

  // ET -> runtime
  RGraph g = compile_to_runtime(f);

  // runtime -> Tape via compile_runtime
  TapeBackend tb(2);
  int out = compile_runtime(g, tb);
  tb.tape.output_id = out;

  // Compare forward eval against runtime eval
  std::vector<double> pt = {0.7, 1.3};
  double v_rt = eval(g, pt);
  double v_tp = tb.tape.forward(pt);
  assert(approx(v_rt, v_tp));

  // Compare gradient vs. ET differentiation at a safe point (avoid domain issues)
  // Compute ET grads
  auto dfx = diff(f, x);
  auto dfy = diff(f, y);
  double gx = dfx(pt[0], pt[1]);
  double gy = dfy(pt[0], pt[1]);

  auto grad = tb.tape.backward(pt);
  assert(grad.size() >= 2);
  assert(approx(gx, grad[0]));
  assert(approx(gy, grad[1]));

  return 0;
}

