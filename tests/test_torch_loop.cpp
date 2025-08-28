#include <cassert>
#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"

#ifdef ET_WITH_TORCH
#  include "et/torch_wrapper.hpp"
#endif

using namespace et;

int main() {
#ifdef ET_WITH_TORCH
#  if defined(ET_TORCH_HAS_GRAPH_EXECUTOR)
  // Fibonacci aN via GraphExecutor
  auto n = Var<double,0>{};
  auto aN = Out<0>(LoopFor<2>(n, lit(0.0), lit(1.0), State<1>(), State<0>() + State<1>()));
  auto runner = make_torch_graph_runner(aN, /*arity=*/1);
  auto out = runner({torch::tensor(7.0)});
  // F(7) = 13
  assert(std::abs(out.toTensor().item<double>() - 13.0) < 1e-9);
#  endif
#endif
  return 0;
}

