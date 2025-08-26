#pragma once

#ifdef ET_WITH_TORCH
#  include <torch/script.h>
#  include <memory>
#  include "et/expr.hpp"
#  include "et/torch_jit_backend.hpp"

namespace et {

struct TorchCompiled {
  std::unique_ptr<TorchJITBackend> backend;
  torch::jit::Value* output;
  std::size_t arity;

  explicit TorchCompiled(std::size_t a)
  : backend(std::make_unique<TorchJITBackend>(static_cast<std::size_t>(a))), output(nullptr), arity(a) {}

  template <class Expr>
  static TorchCompiled compile_expr(const Expr& e, std::size_t arity) {
    TorchCompiled tc(arity);
    tc.output = compile(e, *tc.backend);
    tc.backend->g.registerOutput(tc.output);
    return tc; // NRVO/move
  }

  torch::jit::Graph& graph() { return backend->g; }
  const torch::jit::Graph& graph() const { return backend->g; }

  void print(std::ostream& os = std::cout) const { backend->g.print(os); }
};

template <class Expr>
inline TorchCompiled compile_to_torch(const Expr& e, std::size_t arity) {
  return TorchCompiled::compile_expr(e, arity);
}

// Experimental: build a ScriptModule with a single forward method that runs the compiled graph.
// This uses a Torch C++ API that may vary across versions. Tested on Torch 2.3.x.
// Enable by defining ET_TORCH_ENABLE_MODULE_WRAPPER at compile time for the target.
#ifdef ET_TORCH_ENABLE_MODULE_WRAPPER
inline torch::jit::Module make_script_module(const TorchCompiled& tc, const std::string& method_name = "forward") {
  torch::jit::Module m("ETModule");
  // Copy the graph so the Module owns its own instance
  auto gcopy = tc.graph().copy(); // std::shared_ptr<torch::jit::Graph>
  // Create a method from the graph on this module. API name may vary; _create_method exists in 2.3.x
  m._create_method(method_name, gcopy);
  return m;
}

struct TorchMethodRunner {
  torch::jit::Module module;
  std::string method_name;

  explicit TorchMethodRunner(torch::jit::Module m, std::string name = "forward")
  : module(std::move(m)), method_name(std::move(name)) {}

  c10::IValue operator()(const std::vector<c10::IValue>& inputs) const {
    auto method = module.get_method(method_name);
    return method(inputs);
  }
};

template <class Expr>
inline TorchMethodRunner make_torch_method_runner(const Expr& e, std::size_t arity, const std::string& method_name = "forward") {
  auto tc = compile_to_torch(e, arity);
  auto mod = make_script_module(tc, method_name);
  return TorchMethodRunner(std::move(mod), method_name);
}
#endif

} // namespace et

#endif // ET_WITH_TORCH
