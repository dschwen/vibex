#include <cassert>
#include <string>

#include "et/ast.hpp"
#include "et/ast_to_runtime.hpp"
#include "et/normalize.hpp"

using namespace et;

int main() {
  auto a = var(0), b = var(1), c = var(2);

  // Case: Add(a, Neg(b), Neg(c)) -> Sub(a, Add(b,c))
  {
    Expr e = a - b - c; // normalize -> Add(a,Neg(b),Neg(c)) in RGraph
    RGraph g = normalize(ast_to_rgraph(e));
    RGraph gd = denormalize_sub(g);
    std::string s = r_to_string(gd);
    // Expect outer Sub present and inner Add
    assert(s.find("Sub(") != std::string::npos);
    assert(s.find("Add(") != std::string::npos);
  }

  // Case: all neg terms Add(Neg(a), Neg(b)) -> Neg(Add(a,b))
  {
    Expr e = -(a) - b; // normalize -> Add(Neg(a), Neg(b))
    RGraph g = normalize(ast_to_rgraph(e));
    RGraph gd = denormalize_sub(g);
    std::string s = r_to_string(gd);
    // Expect Neg(Add(...))
    assert(s.find("Neg(Add(") != std::string::npos);
  }

  // Case: Include negative constant: a - 3 - b -> Sub(a, Add(C(3), b))
  {
    Expr e = a - lit(3.0) - b;
    RGraph g = normalize(ast_to_rgraph(e));
    RGraph gd = denormalize_sub(g);
    std::string s = r_to_string(gd);
    assert(s.find("Sub(") != std::string::npos);
    assert(s.find("C(3)") != std::string::npos);
  }

  return 0;
}
