#include <cassert>
#include <string>

#include "et/expr.hpp"
#include "et/runtime_ast.hpp"
#include "et/normalize.hpp"

using namespace et;

int main() {
  auto [a,b,c] = Vars<double,3>();

  // 1) Basic: a - b normalizes to Add(a, Neg(b)) and denormalizes back to Sub(a,b)
  {
    auto e = a - b;
    RGraph g = normalize(compile_to_runtime(e));
    std::string s = r_to_string(g);
    assert(s.find("Add(") == 0);
    assert(s.find("Neg(") != std::string::npos);

    RGraph gd = denormalize_sub(g);
    std::string sd = r_to_string(gd);
    assert(sd.find("Sub(") == 0);
  }

  // 2) Mixed: a - (b - c) normalizes to Add(a, Neg(Add(b, Neg(c)))) which denormalizes inner Add to Sub
  {
    auto e = a - (b - c);
    RGraph g = normalize(compile_to_runtime(e));
    std::string s = r_to_string(g);
    assert(s.find("Add(") == 0);
    assert(s.find("Neg(") != std::string::npos);
    RGraph gd = denormalize_sub(g);
    std::string sd = r_to_string(gd);
    // Expect outer is still Add (three terms after normalization), but inner Sub exists
    assert(sd.find("Sub(") != std::string::npos);
  }

  // 3) Negation folds: -(-a) -> a; -Const -> Const(-)
  {
    auto e = -( -a ) + lit(3.0) + ( -lit(2.0) );
    RGraph g = normalize(compile_to_runtime(e));
    std::string s = r_to_string(g);
    // Should contain V(0) directly (no nested Neg) and constants folded
    assert(s.find("Neg(Neg(") == std::string::npos);
  }

  return 0;
}
