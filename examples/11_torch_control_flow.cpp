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

#if defined(ET_TORCH_ENABLE_MODULE_WRAPPER) && defined(ET_TORCH_MODULE_WRAPPER_AVAILABLE)
  // Build a ScriptModule and run forward with TorchScript
  auto runner = make_torch_method_runner(expr, /*arity=*/1);
  auto out_iv = runner({x_t});
  std::cout << "TorchScript forward result: " << out_iv.toTensor() << "\n";
#elif defined(ET_TORCH_ENABLE_DEFINE_WRAPPER)
  // Define()-based Module wrapper (portable TorchScript path)
  auto mod = make_script_module_define(expr, /*arity=*/1);
  auto out_iv = mod.get_method("forward")({x_t});
  std::cout << "TorchScript (define) result: " << out_iv.toTensor() << "\n";
#elif defined(ET_TORCH_HAS_GRAPH_EXECUTOR) && defined(ET_TORCH_USE_GRAPH_EXECUTOR)
  // Fallback: run the compiled Graph via GraphExecutor (no Module)
  auto ge_runner = make_torch_graph_runner(expr, /*arity=*/1);
  auto out_iv2 = ge_runner({x_t});
  std::cout << "Torch GraphExecutor result: " << out_iv2.toTensor() << "\n";
#endif
#else
  std::cout << "Built without Torch.\n";
#endif
  return 0;
}
