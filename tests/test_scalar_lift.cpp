#include "et/expr.hpp"
#include <cassert>
#include <cmath>

using namespace et;

int main() {
  Var<double, 0> x;

  auto e1 = x + 1.0;                 // node + scalar
  assert(evaluate(e1, 2.5) == 3.5);

  auto e2 = 2.0 + x;                 // scalar + node
  assert(evaluate(e2, -0.5) == 1.5);

  auto e3 = x - 3.0;                 // node - scalar
  assert(evaluate(e3, 7.0) == 4.0);

  auto e4 = 5.0 - x;                 // scalar - node
  assert(evaluate(e4, 1.5) == 3.5);

  auto e5 = x * 2.0;                 // node * scalar
  assert(evaluate(e5, 1.5) == 3.0);

  auto e6 = 3.0 * x;                 // scalar * node
  assert(evaluate(e6, 2.0) == 6.0);

  auto e7 = x / 2.0;                 // node / scalar
  assert(evaluate(e7, 9.0) == 4.5);

  auto e8 = 9.0 / x;                 // scalar / node
  assert(evaluate(e8, 3.0) == 3.0);

  auto e9 = pow(x, 2.0);             // node ^ scalar
  assert(evaluate(e9, 3.0) == 9.0);

  auto e10 = pow(2.0, x);            // scalar ^ node
  assert(std::abs(evaluate(e10, 3.0) - 8.0) < 1e-12);

  return 0;
}

