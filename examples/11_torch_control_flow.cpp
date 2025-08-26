#include <iostream>

#define ET_ENABLE_CONTROL_FLOW 1
#include "et/expr.hpp"
#include "et/torch_jit_backend.hpp"
#include "et/torch_wrapper.hpp"

int main() {
#ifdef ET_WITH_TORCH
  using namespace et;
  auto [x] = Vars<double,1>();

  // Build Select(x > 0, x + 1, x - 1) and lower to TorchScript
  auto mask = x > lit(0.0);
  auto expr = Select(mask, x + lit(1.0), x - lit(1.0));

  auto tc = compile_to_torch(expr, /*arity=*/1);
  std::cout << "Torch JIT graph for Select(x>0, x+1, x-1):\n";
  std::cout << tc.graph().toString() << "\n";

  // Evaluate the same expression eagerly with ATen for a demo
  auto x_t = torch::tensor(std::vector<double>{-2.0, 0.0, 3.0});
  auto y_t = torch::where(x_t.gt(0), x_t + 1.0, x_t - 1.0);
  std::cout << "ATen result for x=[-2,0,3]: " << y_t << "\n";

#ifdef ET_TORCH_ENABLE_MODULE_WRAPPER
  // Build a ScriptModule and run forward with TorchScript
  auto runner = make_torch_method_runner(expr, /*arity=*/1);
  auto out_iv = runner({x_t});
  std::cout << "TorchScript forward result: " << out_iv.toTensor() << "\n";
#endif
#else
  std::cout << "Built without Torch.\n";
#endif
  return 0;
}
