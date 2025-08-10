#include <cassert>
#include <vector>

#include "et/expr.hpp"
#include "et/tape_backend.hpp"
#include "et/compile_cse.hpp"
// Avoid including compile_hash_cse.hpp here to prevent to_key_stream ODR conflicts

using namespace et;

template <class Expr>
static int compile_no_cse_nodes(const Expr& e) {
  TapeBackend tb(4);
  auto root = compile(e, tb);
  tb.tape.output_id = root;
  return (int)tb.tape.nodes.size();
}

template <class Expr>
static int compile_cse_nodes(const Expr& e) {
  TapeBackend tb(4);
  auto root = compile_cse(e, tb);
  tb.tape.output_id = root;
  return (int)tb.tape.nodes.size();
}

int main() {
  // 1) Simple duplicate: sin(x) + sin(x)
  {
    auto [x] = Vars<double, 1>();
    auto h = sin(x) + sin(x);
    int n_no = compile_no_cse_nodes(h);
    int n_cse = compile_cse_nodes(h);
    // No CSE: Var, Sin, Var, Sin, Add => 5
    assert(n_no == 5);
    // With CSE: Var, Sin, Add => 3
    assert(n_cse == 3);
  }

  // 2) Larger shared subtree: (x*y + sin(x)) + (x*y + sin(x))
  {
    auto [x,y] = Vars<double, 2>();
    auto sub = x*y + sin(x);
    auto f = sub + sub;
    int n_no = compile_no_cse_nodes(f);
    int n_cse = compile_cse_nodes(f);
    // No CSE: Var0, Var1, Mul, Var0, Sin, Add, Var0, Var1, Mul, Var0, Sin, Add, Add = 13
    assert(n_no == 13);
    // With CSE: Var0, Var1, Mul, Sin, Add, Add = 6
    assert(n_cse == 6);
  }

  // 3) Mixed with constants: exp(x) + exp(x) + 2 + 2
  {
    auto [x] = Vars<double, 1>();
    auto two = lit(2.0);
    auto f = exp(x) + exp(x) + two + two;
    int n_no = compile_no_cse_nodes(f);
    int n_cse = compile_cse_nodes(f);
    // No CSE: Var, Exp, Var, Exp, Const, Const, Add, Add, Add = 9
    assert(n_no == 9);
    // With CSE: Var, Exp, Const, Add, Add, Add = 6
    assert(n_cse == 6);
  }

  return 0;
}
