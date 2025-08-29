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

static double fd_partial3(const Expr& e, std::vector<double> in, std::size_t i, double h = 1e-6) {
  in[i] += h; double f1 = eval(e, in);
  in[i] -= 2*h; double f2 = eval(e, in);
  return (f1 - f2) / (2*h);
}

int main() {
  // Variables and a canonical test expression
  auto x = var(0), y = var(1), z = var(2);
  Expr f = sin(x) * y + z * z;

  // 1) Basic evaluation
  {
    double xv = 2.4, yv = 6.0, zv = 1.1;
    double expected = std::sin(xv) * yv + zv * zv;
    double got = eval(f, {xv, yv, zv});
    assert(approx(got, expected));
  }

  // 2) Gradient d/dx via Tape vs analytic and FD
  {
    double xv = 0.5, yv = 3.0, zv = 0.0;
    std::vector<double> in = {xv, yv, zv};
    TapeBackend tb(3);
    int root = compile_runtime(f, tb);
    tb.tape.output_id = root;
    auto g = tb.tape.backward(in);
    double expected = std::cos(xv) * yv;
    assert(g.size() == 3);
    assert(approx(g[0], expected));
    // Finite-difference cross-check
    double fd = fd_partial3(f, in, 0);
    assert(approx(g[0], fd, 1e-6));
  }

  // 3) Constant expression eval sanity (sin(Const))
  {
    auto g = sin(lit(0.5));
    double expected = std::sin(0.5);
    double got = eval(g, {0.0});
    assert(approx(got, expected));
  }

  // 4) Tape backend: forward + backward vs analytic gradient
  {
    TapeBackend tb(3);
    int root = compile_runtime(f, tb);
    tb.tape.output_id = root;
    std::vector<double> in = {1.2, 2.0, 0.3};
    double forward_ref = std::sin(in[0]) * in[1] + in[2] * in[2];
    double forward_val = tb.tape.forward(in);
    assert(approx(forward_val, forward_ref));
    auto grad = tb.tape.backward(in);
    double gx = std::cos(in[0]) * in[1];
    double gy = std::sin(in[0]);
    double gz = 2.0 * in[2];
    assert(grad.size() == 3);
    assert(approx(grad[0], gx));
    assert(approx(grad[1], gy));
    assert(approx(grad[2], gz));

    // Finite-difference cross-check for all partials
    double fd_gx = fd_partial3(f, in, 0);
    double fd_gy = fd_partial3(f, in, 1);
    double fd_gz = fd_partial3(f, in, 2);
    assert(approx(grad[0], fd_gx, 1e-6));
    assert(approx(grad[1], fd_gy, 1e-6));
    assert(approx(grad[2], fd_gz, 1e-6));
  }

  // 5) AST structure sanity: sin(x) + sin(x)
  {
    Expr h = sin(x) + sin(x);
    auto add = std::dynamic_pointer_cast<AddNode>(h.n);
    assert(add);
  }

  return 0;
}
