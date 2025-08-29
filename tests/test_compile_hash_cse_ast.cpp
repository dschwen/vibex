#include <cassert>
#include <vector>
#include <cmath>

#include "et/ast.hpp"
#include "et/compile.hpp"
#include "et/compile_hash_cse.hpp"
#include "et/tape_backend.hpp"

using namespace et;

int main() {
  auto x = var(0), y = var(1);
  Expr t = sin(x) + cos(y);
  Expr e = t*t + t*t + t*t; // heavy reuse of same structure

  // Naive compile
  TapeBackend tb_naive(2);
  int root_naive = compile_runtime(e, tb_naive);
  tb_naive.tape.output_id = root_naive;
  auto nodes_naive = tb_naive.tape.nodes.size();

  // CSE compile
  TapeBackend tb_cse(2);
  int root_cse = compile_hash_cse(e, tb_cse);
  tb_cse.tape.output_id = root_cse;
  auto nodes_cse = tb_cse.tape.nodes.size();

  // Expect fewer nodes with CSE, and identical forward value
  assert(nodes_cse < nodes_naive);
  std::vector<double> in = {1.2, -0.7};
  double v1 = tb_naive.tape.forward(in);
  double v2 = tb_cse.tape.forward(in);
  assert(std::fabs(v1 - v2) < 1e-12);
  return 0;
}
