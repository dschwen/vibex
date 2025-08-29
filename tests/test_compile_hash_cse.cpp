#include <cassert>
#include <cstddef>

#include "et/ast.hpp"
#include "et/compile_cse_ast.hpp"

using namespace et;

struct CountingBackendAst {
  using result_type = int;
  std::size_t nVar = 0, nConst = 0, nUnary = 0, nBinary = 0, nTernary = 0;
  int next = 0;

  result_type emitVar(std::size_t) { ++nVar; return next++; }
  result_type emitConst(double)    { ++nConst; return next++; }
  template <class Op>
  result_type emitApply(Op, int) { ++nUnary; return next++; }
  template <class Op>
  result_type emitApply(Op, int, int) { ++nBinary; return next++; }
  template <class Op>
  result_type emitApply(Op, int, int, int) { ++nTernary; return next++; }
};

int main() {
  auto x = var(0), y = var(1);
  Expr t = sin(x) + cos(y);
  Expr e = t*t + t*t + t*t; // heavy reuse of same structure

  CountingBackendAst b;
  auto res = compile_cse_ast(e, b);
  (void)res;

  // Expect only unique substructures compiled once:
  // Var x, Var y, Unary sin(x), Unary cos(y), Binary add(t1,t2),
  // Binary mul(t,t) unique structure used multiple times but compiled once, and top-level adds
  std::size_t total_ops = b.nVar + b.nConst + b.nUnary + b.nBinary + b.nTernary;
  assert(total_ops < 12); // much less than naive 3*4 = 12

  // Ensure the binary count includes at least 3 (t=add, m=mul, top-level adds folded via tree)
  assert(b.nBinary >= 3);
  assert(b.nUnary >= 2);
  assert(b.nVar == 2);
  return 0;
}
