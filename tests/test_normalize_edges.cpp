#include <cassert>
#include <string>

#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/normalize.hpp"

using namespace et;

int main() {
  // Add of constants only: 2 + 3 -> C(5)
  {
    auto e = lit(2.0) + lit(3.0);
    RGraph g = normalize(compile_to_runtime(e));
    assert(r_to_string(g) == std::string("C(5)"));
  }

  // Add zeros only: 0 + 0 -> C(0)
  {
    auto e = lit(0.0) + lit(0.0);
    RGraph g = normalize(compile_to_runtime(e));
    assert(r_to_string(g) == std::string("C(0)"));
  }

  // x + 0 -> x (size==1 collapsing)
  {
    auto [x] = Vars<double,1>();
    auto e = x + lit(0.0);
    RGraph g = normalize(compile_to_runtime(e));
    assert(r_to_string(g) == std::string("V(0)"));
  }

  // Mul constants only: 1*1 -> C(1) (empty flat)
  {
    auto e = lit(1.0) * lit(1.0);
    RGraph g = normalize(compile_to_runtime(e));
    assert(r_to_string(g) == std::string("C(1)"));
  }

  // Mul constants fold: 2*x*3 -> Mul(Const(6), Var(0)) (order deterministic but assert structurally)
  {
    auto [x] = Vars<double,1>();
    auto e = lit(2.0) * x * lit(3.0);
    RGraph g = normalize(compile_to_runtime(e));
    double v = eval(g, std::vector<double>{1.75});
    assert(std::fabs(v - 6.0*1.75) < 1e-12);
  }

  // Mul constants only: 2*3 -> C(6)
  {
    auto e = lit(2.0) * lit(3.0);
    RGraph g = normalize(compile_to_runtime(e));
    assert(r_to_string(g) == std::string("C(6)"));
  }

  // Div equal -> 1: x/x -> C(1)
  {
    auto [x] = Vars<double,1>();
    auto e = x / x;
    RGraph g = normalize(compile_to_runtime(e));
    assert(r_to_string(g) == std::string("C(1)"));
  }

  return 0;
}
