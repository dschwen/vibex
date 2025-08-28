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
  auto [x] = Vars<double,1>();
  auto y = Select(x > lit(0.0), x + lit(1.0), x - lit(1.0));
  auto runner = make_torch_graph_runner(y, /*arity=*/1);
  auto inp = torch::tensor(std::vector<double>{-2.0, 0.0, 3.0});
  auto out = runner({inp}).toTensor();
  auto v = out.to(torch::kCPU);
  auto a = v.index({0}).item<double>();
  auto b = v.index({1}).item<double>();
  auto c = v.index({2}).item<double>();
  assert(std::abs(a - (-3.0)) < 1e-12);
  assert(std::abs(b - (-1.0)) < 1e-12);
  assert(std::abs(c - (4.0)) < 1e-12);
#  endif
#endif
  return 0;
}

