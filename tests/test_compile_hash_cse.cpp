#include <cassert>
#include <cstddef>

#include "et/expr.hpp"
#include "et/compile_hash_cse.hpp"

using namespace et;

struct CountingBackend {
  using result_type = int;
  std::size_t nVar = 0, nConst = 0, nUnary = 0, nBinary = 0;
  int next = 0;

  template <class T, std::size_t I>
  result_type emitVar(Var<T,I>) { ++nVar; return next++; }
  template <class T>
  result_type emitConst(Const<T>) { ++nConst; return next++; }
  template <class Op>
  result_type emitApply(Op, int) { ++nUnary; return next++; }
  template <class Op>
  result_type emitApply(Op, int, int) { ++nBinary; return next++; }
};

int main() {
  auto [x,y] = Vars<double,2>();
  auto t = sin(x) + cos(y);
  auto e = t*t + t*t + t*t; // heavy reuse of same structure

  CountingBackend b;
  auto res = compile_hash_cse(e, b);
  (void)res;

  // Expect only unique substructures compiled once:
  // Var x, Var y, Const 0? none, Unary sin(x), Unary cos(y), Binary add(t1,t2),
  // Binary mul(t,t) unique structure used multiple times but compiled once, and top-level adds
  // Unique nodes: x, y, sin(x), cos(y), t=add, m=mul(t,t), plus combinations for add(m,m) then add(add(m,m),m)
  // Count checks are relative; ensure total ops are far fewer than naive expansion (which would be 3*(sin+cos+add+mul))
  std::size_t total_ops = b.nVar + b.nConst + b.nUnary + b.nBinary;
  assert(total_ops < 12); // much less than naive 3*4 = 12

  // Ensure the binary count includes at least 3 (t=add, m=mul, top-level adds folded via tree)
  assert(b.nBinary >= 3);
  assert(b.nUnary >= 2);
  assert(b.nVar == 2);
  return 0;
}

