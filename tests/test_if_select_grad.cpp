#include <cassert>
#include <cmath>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/tape_backend.hpp"

using namespace et;

template <class Expr>
static std::vector<double> tape_grad_1var(const Expr& expr, double xval) {
  TapeBackend tb(1);
  auto root = compile(expr, tb);
  tb.tape.output_id = root;
  return tb.tape.backward({xval});
}

int main() {
  auto [x] = Vars<double,1>();

  // y = If(x>0, x*x, x) => dy/dx = 2x if x>0 else 1
  auto y_if = If(x > lit(0.0), x * x, x);
  {
    auto g = tape_grad_1var(y_if, 3.0);
    assert(std::abs(g[0] - 6.0) < 1e-12);
  }
  {
    auto g = tape_grad_1var(y_if, -2.0);
    assert(std::abs(g[0] - 1.0) < 1e-12);
  }

  // y = Select(x>0, x, -x) => dy/dx = 1 if x>0 else -1
  auto y_sel = Select(x > lit(0.0), x, -x);
  {
    auto g = tape_grad_1var(y_sel, 3.0);
    assert(std::abs(g[0] - 1.0) < 1e-12);
  }
  {
    auto g = tape_grad_1var(y_sel, -4.0);
    assert(std::abs(g[0] + 1.0) < 1e-12);
  }

  // Predicate has no gradient: build p = (x>0) and check grad is zero
  auto p = x > lit(0.0);
  auto g = tape_grad_1var(p, 1.0);
  assert(g[0] == 0.0);

  return 0;
}
