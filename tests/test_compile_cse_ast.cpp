#include <cassert>
#include <vector>
#include <cmath>

#include "et/ast.hpp"
#include "et/compile.hpp"
#include "et/compile_cse.hpp"
#include "et/tape_backend.hpp"

using namespace et;

int main() {
  auto x = var(0), y = var(1);
  Expr sub = x*y + sin(x);
  Expr e = sub + sub; // duplicated subtree

  TapeBackend tb_naive(2);
  int root_naive = compile_runtime(e, tb_naive);
  tb_naive.tape.output_id = root_naive;
  auto nodes_naive = tb_naive.tape.nodes.size();

  TapeBackend tb_cse(2);
  int root_cse = compile_cse(e, tb_cse);
  tb_cse.tape.output_id = root_cse;
  auto nodes_cse = tb_cse.tape.nodes.size();

  assert(nodes_cse < nodes_naive);
  std::vector<double> in = {0.7, -1.3};
  double v1 = tb_naive.tape.forward(in);
  double v2 = tb_cse.tape.forward(in);
  assert(std::fabs(v1 - v2) < 1e-12);
  return 0;
}
