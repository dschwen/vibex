#pragma once

#ifdef ET_WITH_TORCH
#  include <torch/script.h>
#  include "et/expr.hpp"
#  include "et/torch_jit_backend.hpp"

namespace et {

struct TorchCompiled {
  TorchJITBackend backend;
  torch::jit::Value* output;
  std::size_t arity;

  explicit TorchCompiled(std::size_t a)
  : backend(static_cast<std::size_t>(a)), output(nullptr), arity(a) {}

  template <class Expr>
  static TorchCompiled compile_expr(const Expr& e, std::size_t arity) {
    TorchCompiled tc(arity);
    tc.output = compile(e, tc.backend);
    tc.backend.g.registerOutput(tc.output);
    return tc;
  }

  torch::jit::Graph& graph() { return backend.g; }
  const torch::jit::Graph& graph() const { return backend.g; }

  void print(std::ostream& os = std::cout) const { backend.g.print(os); }
};

template <class Expr>
inline TorchCompiled compile_to_torch(const Expr& e, std::size_t arity) {
  return TorchCompiled::compile_expr(e, arity);
}

} // namespace et

#endif // ET_WITH_TORCH

