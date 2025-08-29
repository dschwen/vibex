#include <cassert>
#include <string>

#include "et/ast.hpp"
#include "et/normalize.hpp"
#include "et/denormalize.hpp"
#include "et/print.hpp"

using namespace et;

int main() {
  auto a = var(0), b = var(1), c = var(2);

  // Case: Add(a, Neg(b), Neg(c)) -> Sub(a, Add(b,c))
  {
    Expr e = a - b - c; // normalize -> Add(a,Neg(b),Neg(c))
    Expr n = normalize(e);
    Expr d = denormalize_sub(n);
    std::string s = to_string_pretty(d);
    // Expect outer Sub present and inner Add
    assert(s.find("Sub(") != std::string::npos);
    assert(s.find("Add(") != std::string::npos);
  }

  // Case: all neg terms Add(Neg(a), Neg(b)) -> Neg(Add(a,b))
  {
    Expr e = -(a) - b; // normalize -> Add(Neg(a), Neg(b))
    Expr n = normalize(e);
    Expr d = denormalize_sub(n);
    std::string s = to_string_pretty(d);
    // Expect Neg(Add(...))
    assert(s.find("Neg(Add(") != std::string::npos);
  }

  // Case: Include negative constant: a - 3 - b -> Sub(a, Add(C(3), b))
  {
    Expr e = a - lit(3.0) - b;
    Expr n = normalize(e);
    Expr d = denormalize_sub(n);
    std::string s = to_string_pretty(d);
    assert(s.find("Sub(") != std::string::npos);
    assert(s.find("C(3)") != std::string::npos);
  }

  return 0;
}
