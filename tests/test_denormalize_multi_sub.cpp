#include <cassert>
#include <string>

#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/normalize.hpp"

using namespace et;

int main() {
  auto [a,b,c] = Vars<double,3>();

  // Case: Add(a, Neg(b), Neg(c)) -> Sub(a, Add(b,c))
  {
    auto e = a - b - c; // builds as Sub(Sub(a,b),c); normalize -> Add(a,Neg(b),Neg(c))
    RGraph g = normalize(compile_to_runtime(e));
    RGraph gd = denormalize_sub(g);
    std::string s = r_to_string(gd);
    // Expect outer Sub present and inner Add
    assert(s.find("Sub(") != std::string::npos);
    assert(s.find("Add(") != std::string::npos);
  }

  // Case: all neg terms Add(Neg(a), Neg(b)) -> Neg(Add(a,b))
  {
    auto e = -(a) - b; // normalize -> Add(Neg(a), Neg(b))
    RGraph g = normalize(compile_to_runtime(e));
    RGraph gd = denormalize_sub(g);
    std::string s = r_to_string(gd);
    // Expect Neg(Add(...))
    assert(s.find("Neg(Add(") != std::string::npos);
  }

  // Case: Include negative constant: a - 3 - b -> Sub(a, Add(C(3), b))
  {
    auto e = a - lit(3.0) - b;
    RGraph g = normalize(compile_to_runtime(e));
    RGraph gd = denormalize_sub(g);
    std::string s = r_to_string(gd);
    assert(s.find("Sub(") != std::string::npos);
    assert(s.find("C(3)") != std::string::npos);
  }

  return 0;
}
