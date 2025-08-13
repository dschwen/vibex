#pragma once

#ifdef ET_WITH_TORCH
  #include <torch/script.h>
  #include <vector>
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

  template <class T>
  result_type emitVar(std::size_t idx) { return inputs[idx]; }

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
    auto mk = [&](const char* q, result_type x){
      auto n = g.create(c10::Symbol::fromQualString(q), {x});
      g.insertNode(n); return n->output();
    };
    if constexpr (std::is_same<Op, NegOp>::value)  return mk("aten::neg", a);
    else if constexpr (std::is_same<Op, SinOp>::value)  return mk("aten::sin", a);
    else if constexpr (std::is_same<Op, CosOp>::value)  return mk("aten::cos", a);
    else if constexpr (std::is_same<Op, ExpOp>::value)  return mk("aten::exp", a);
    else if constexpr (std::is_same<Op, LogOp>::value)  return mk("aten::log", a);
    else if constexpr (std::is_same<Op, SqrtOp>::value) return mk("aten::sqrt", a);
    else if constexpr (std::is_same<Op, TanhOp>::value) return mk("aten::tanh", a);
    else static_assert(!std::is_same<Op,Op>::value, "Unary op not mapped to Torch JIT");
  }

  template <class Op>
  result_type emitApply(Op, result_type a, result_type b) {
    auto mk = [&](const char* q, result_type x, result_type y){
      auto n = g.create(c10::Symbol::fromQualString(q), {x,y});
      g.insertNode(n); return n->output();
    };
    if constexpr      (std::is_same<Op, AddOp>::value) return mk("aten::add", a, b);
    else if constexpr (std::is_same<Op, SubOp>::value) return mk("aten::sub", a, b);
    else if constexpr (std::is_same<Op, MulOp>::value) return mk("aten::mul", a, b);
    else if constexpr (std::is_same<Op, DivOp>::value) return mk("aten::div", a, b);
    else if constexpr (std::is_same<Op, PowOp>::value) return mk("aten::pow", a, b);
    else static_assert(!std::is_same<Op,Op>::value, "Binary op not mapped to Torch JIT");
  }
};
#else
struct TorchJITBackend; // stub
#endif

} // namespace et
