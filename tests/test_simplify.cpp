#include <cassert>
#include <cmath>
#include "et/expr.hpp"
#include "et/simplify.hpp"

using namespace et;

int main() {
  // Unary constant folding
  {
    auto e1 = simplify(exp(lit(0.2)));
    static_assert(et::is_const_node_v<decltype(e1)>, "exp(Const) should fold to Const");
    (void)e1;
    auto e2 = simplify(log(lit(1.7)));
    static_assert(et::is_const_node_v<decltype(e2)>, "log(Const) should fold to Const");
    auto e3 = simplify(sqrt(lit(2.5)));
    static_assert(et::is_const_node_v<decltype(e3)>, "sqrt(Const) should fold to Const");
    auto e4 = simplify(tanh(lit(-0.9)));
    static_assert(et::is_const_node_v<decltype(e4)>, "tanh(Const) should fold to Const");
    auto e5 = simplify(-lit(3.0));
    static_assert(et::is_const_node_v<decltype(e5)>, "neg(Const) should fold to Const");

    // Numeric checks (args ignored for Const)
    assert(std::fabs(e1(0.0) - std::exp(0.2)) < 1e-12);
    assert(std::fabs(e2(0.0) - std::log(1.7)) < 1e-12);
    assert(std::fabs(e3(0.0) - std::sqrt(2.5)) < 1e-12);
    assert(std::fabs(e4(0.0) - std::tanh(-0.9)) < 1e-12);
    assert(std::fabs(e5(0.0) - (-3.0)) < 1e-12);
  }

  // Non-constant unary should rebuild, not fold
  {
    auto [x] = Vars<double,1>();
    auto e = simplify(exp(x));
    // Can't static_assert structure easily here; just evaluate
    double xv = 0.3;
    assert(std::fabs(e(xv) - std::exp(xv)) < 1e-12);
  }

  // Binary constant folding when both sides are Const
  {
    auto a = simplify(lit(2.0) + lit(5.0));
    auto b = simplify(lit(6.0) - lit(1.5));
    auto c = simplify(lit(3.0) * lit(4.0));
    auto d = simplify(lit(9.0) / lit(2.0));
    static_assert(et::is_const_node_v<decltype(a)>, "+ fold");
    static_assert(et::is_const_node_v<decltype(b)>, "- fold");
    static_assert(et::is_const_node_v<decltype(c)>, "* fold");
    static_assert(et::is_const_node_v<decltype(d)>, "/ fold");
    assert(std::fabs(a(0.0) - (2.0 + 5.0)) < 1e-12);
    assert(std::fabs(b(0.0) - (6.0 - 1.5)) < 1e-12);
    assert(std::fabs(c(0.0) - (3.0 * 4.0)) < 1e-12);
    assert(std::fabs(d(0.0) - (9.0 / 2.0)) < 1e-12);
  }

  // Binary non-constant: no fold, evaluation preserved
  {
    auto [x,y] = Vars<double,2>();
    auto e = simplify(x + lit(3.0));
    double xv = 1.2, yv = -0.7;
    assert(std::fabs(e(xv, yv) - (xv + 3.0)) < 1e-12);
  }

  return 0;
}
