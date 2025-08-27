#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/runtime_ast.hpp"

using namespace et;

int main() {
  // ForN: sum of i for i in [0, n)
  auto n = Var<double,0>{};
  auto init = lit(0.0);
  auto next = State<0>() + Iter();
  auto loop = LoopForN(n, init, next);

  auto g = compile_to_runtime(loop);

  // Quick structural check
  auto s = r_to_string(g);
  assert(s.find("LoopFor(") != std::string::npos);

  // Numeric checks
  for (int k = 0; k <= 12; ++k) {
    double got = eval(g, {static_cast<double>(k)});
    double expected = 0.5 * k * (k - 1);
    assert(std::abs(got - expected) < 1e-9);
  }

  return 0;
}

