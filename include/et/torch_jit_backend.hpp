#pragma once

// Optional TorchScript backend.
// Compile this file only if ET_WITH_TORCH is defined and libtorch is available.

#ifdef ET_WITH_TORCH
  #include <torch/script.h>
#endif

namespace et {

#ifdef ET_WITH_TORCH
struct TorchJITBackend {
  using result_type = torch::jit::Value*;

  torch::jit::Graph g;
  std::vector<torch::jit::Value*> inputs;

  explicit TorchJITBackend(std::size_t arity) {
    for (std::size_t i = 0; i < arity; ++i)
      inputs.push_back(g.addInput());
  }

  template <class T, std::size_t I>
  result_type emitVar(Var<T,I>) {
    return inputs[I];
  }

  template <class T>
  result_type emitConst(Const<T> c) {
    auto n = g.create(torch::jit::prim::Constant);
    n->output()->setType(c10::TensorType::get());
    auto t = torch::tensor(static_cast<double>(c.value));
    n->t_(c10::Symbol::attr("value"), t);
    g.insertNode(n);
    return n->output();
  }

  template <class Op>
  result_type emitApply(Op, result_type a) {
    if constexpr (std::is_same<Op, NegOp>::value) {
      auto n = g.create(c10::Symbol::fromQualString("aten::neg"), {a});
      g.insertNode(n); return n->output();
    } else if constexpr (std::is_same<Op, SinOp>::value) {
      auto n = g.create(c10::Symbol::fromQualString("aten::sin"), {a});
      g.insertNode(n); return n->output();
    } else if constexpr (std::is_same<Op, ExpOp>::value) {
      auto n = g.create(c10::Symbol::fromQualString("aten::exp"), {a});
      g.insertNode(n); return n->output();
    } else if constexpr (std::is_same<Op, LogOp>::value) {
      auto n = g.create(c10::Symbol::fromQualString("aten::log"), {a});
      g.insertNode(n); return n->output();
    } else if constexpr (std::is_same<Op, SqrtOp>::value) {
      auto n = g.create(c10::Symbol::fromQualString("aten::sqrt"), {a});
      g.insertNode(n); return n->output();
    } else if constexpr (std::is_same<Op, TanhOp>::value) {
      auto n = g.create(c10::Symbol::fromQualString("aten::tanh"), {a});
      g.insertNode(n); return n->output();
      auto n = g.create(c10::Symbol::fromQualString("aten::sin"), {a});
      g.insertNode(n); return n->output();
    } else {
      static_assert(!std::is_same<Op,Op>::value, "Unary op not mapped to Torch JIT");
    }
  }

  template <class Op>
  result_type emitApply(Op, result_type a, result_type b) {
    c10::Symbol sym;
    if constexpr      (std::is_same<Op, AddOp>::value) sym = c10::Symbol::fromQualString("aten::add");
    else if constexpr (std::is_same<Op, SubOp>::value) sym = c10::Symbol::fromQualString("aten::sub");
    else if constexpr (std::is_same<Op, MulOp>::value) sym = c10::Symbol::fromQualString("aten::mul");
    else if constexpr (std::is_same<Op, DivOp>::value) sym = c10::Symbol::fromQualString("aten::div");
    else static_assert(!std::is_same<Op,Op>::value, "Binary op not mapped to Torch JIT");

    auto n = g.create(sym, {a, b});
    g.insertNode(n);
    return n->output();
  }
};
#else
// Stub to help users who include this header without ET_WITH_TORCH
struct TorchJITBackend; // no definition
#endif

} // namespace et
