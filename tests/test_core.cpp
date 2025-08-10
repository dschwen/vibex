#include <cassert>
#include <cmath>
#include <vector>

#include "et/expr.hpp"
#include "et/simplify.hpp"
#include "et/tape_backend.hpp"
#include "et/compile_hash_cse.hpp"

using namespace et;

static bool approx(double a, double b, double eps = 1e-9) {
  return std::fabs(a - b) <= eps * (1.0 + std::max(std::fabs(a), std::fabs(b)));
}

template <class Expr>
static double fd_partial3(const Expr& e, std::vector<double> in, std::size_t i, double h = 1e-6) {
  in[i] += h; double f1 = e(in[0], in[1], in[2]);
  in[i] -= 2*h; double f2 = e(in[0], in[1], in[2]);
  return (f1 - f2) / (2*h);
}

int main() {
  // Variables and a canonical test expression
  auto [x, y, z] = Vars<double, 3>();
  auto f = sin(x) * y + z * z;

  // 1) Basic evaluation
  {
    double xv = 2.4, yv = 6.0, zv = 1.1;
    double expected = std::sin(xv) * yv + zv * zv;
    double got = f(xv, yv, zv);
    assert(approx(got, expected));
  }

  // 2) Symbolic diff (d/dx) evaluation
  {
    auto dfx = diff(f, x); // cos(x) * y
    double xv = 0.5, yv = 3.0, zv = 0.0;
    double expected = std::cos(xv) * yv;
    double got = dfx(xv, yv, zv);
    assert(approx(got, expected));

    // Finite-difference cross-check
    std::vector<double> in = {xv, yv, zv};
    double fd = fd_partial3(f, in, 0);
    assert(approx(got, fd, 1e-6));
  }

  // 3) Simplification: unary const folding (sin(Const))
  {
    auto g = simplify(sin(lit(0.5)));
    double expected = std::sin(0.5);
    double got = g(0.0); // args ignored for const
    assert(approx(got, expected));
  }

  // 4) Tape backend: forward + VJP vs analytic gradient
  {
    TapeBackend tb(3);
    int root = compile_hash_cse(f, tb);
    tb.tape.output_id = root;

    std::vector<double> in = {1.2, 2.0, 0.3};
    double forward_ref = std::sin(in[0]) * in[1] + in[2] * in[2];
    double forward_val = tb.tape.forward(in);
    assert(approx(forward_val, forward_ref));

    auto grad = tb.tape.vjp(in);
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

  // 5) CSE sanity: sin(x) + sin(x) compiles shared subexprs
  {
    auto h = sin(x) + sin(x);
    TapeBackend tb(1);
    int root = compile_hash_cse(h, tb);
    tb.tape.output_id = root;
    // Expect: Var, Sin, Add => 3 nodes total
    assert(tb.tape.nodes.size() == 3u);
  }

  return 0;
}
