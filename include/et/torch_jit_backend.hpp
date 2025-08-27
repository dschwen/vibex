#pragma once

#ifdef ET_WITH_TORCH
  #include <torch/script.h>
  #include <vector>
  #include <array>
#endif

namespace et {

#ifdef ET_WITH_TORCH
struct TorchJITBackend {
  using result_type = torch::jit::Value*;

  torch::jit::Graph g;
  std::vector<torch::jit::Value*> inputs;
  struct LoopCtx { torch::jit::Value* iter_int = nullptr; std::vector<torch::jit::Value*> state; };
  std::vector<LoopCtx> loop_stack;

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

  // Helper: constant int
  result_type makeConstInt(std::int64_t v) {
    auto n = g.create(torch::jit::prim::Constant);
    n->output()->setType(c10::IntType::get());
    n->i_(c10::Symbol::attr("value"), v);
    g.insertNode(n);
    return n->output();
  }
  // Helper: constant bool
  result_type makeConstBool(bool v) {
    auto n = g.create(torch::jit::prim::Constant);
    n->output()->setType(c10::BoolType::get());
    n->i_(c10::Symbol::attr("value"), v ? 1 : 0);
    g.insertNode(n);
    return n->output();
  }

  // Zero-arg ops (Iter/State) resolved using current loop context
  template <class Op>
  result_type emitApply(Op) {
    if constexpr (std::is_same<Op, IterOp>::value) {
      if (!loop_stack.empty() && loop_stack.back().iter_int) {
        auto n = g.create(torch::jit::prim::NumToTensor, {loop_stack.back().iter_int});
        g.insertNode(n);
        return n->output();
      }
      // Fallback: 0.0 tensor
      return emitConst(Const<double>{0.0});
    } else {
      static_assert(!std::is_same<Op,Op>::value, "Zero-arg op not mapped to Torch JIT");
    }
  }
  template <std::size_t I>
  result_type emitApply(StateOp<I>) {
    if (!loop_stack.empty() && I < loop_stack.back().state.size()) return loop_stack.back().state[I];
    return emitConst(Const<double>{0.0});
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
    #ifdef ET_ENABLE_CONTROL_FLOW
    else if constexpr (std::is_same<Op, NotOp>::value) return mk("aten::logical_not", a);
    #endif
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
#ifdef ET_ENABLE_CONTROL_FLOW
    else if constexpr (std::is_same<Op, LtOp>::value)  return mk("aten::lt", a, b);
    else if constexpr (std::is_same<Op, LeOp>::value)  return mk("aten::le", a, b);
    else if constexpr (std::is_same<Op, GtOp>::value)  return mk("aten::gt", a, b);
    else if constexpr (std::is_same<Op, GeOp>::value)  return mk("aten::ge", a, b);
    else if constexpr (std::is_same<Op, EqOp>::value)  return mk("aten::eq", a, b);
    else if constexpr (std::is_same<Op, NeOp>::value)  return mk("aten::ne", a, b);
#endif
    else static_assert(!std::is_same<Op,Op>::value, "Binary op not mapped to Torch JIT");
  }

#ifdef ET_ENABLE_CONTROL_FLOW
  template <class Op>
  result_type emitApply(Op, result_type a, result_type b, result_type c) {
    if constexpr (std::is_same<Op, IfOp>::value) {
      // Lower to prim::If with single value output
      auto* cond = a;
      auto* if_node = g.create(torch::jit::prim::If, 1);
      if_node->addInput(cond);
      auto* then_block = if_node->addBlock();
      auto* else_block = if_node->addBlock();
      {
        torch::jit::WithInsertPoint guard(then_block);
        then_block->registerOutput(b);
      }
      {
        torch::jit::WithInsertPoint guard(else_block);
        else_block->registerOutput(c);
      }
      g.insertNode(if_node);
      return if_node->output();
    } else if constexpr (std::is_same<Op, SelectOp>::value) {
      // Prefer aten::where(mask, a, b)
      auto n = g.create(c10::Symbol::fromQualString("aten::where"), {a, b, c});
      g.insertNode(n);
      return n->output();
    } else {
      static_assert(!std::is_same<Op,Op>::value, "Ternary op not mapped to Torch JIT");
    }
  }

  // LoopFor (K carried) lowered to prim::Loop + list construct
  template <std::size_t K, class... Children>
  result_type emitApply(LoopForOp<K>, Children... children) {
    static_assert(K >= 1, "LoopForOp<K>: K must be >=1");
    // Children: [N, init0..initK-1, next0..nextK-1]
    std::array<result_type, 1 + 2*K> ch{children...};
    // Cast N (tensor) -> int
    auto n_int = g.create(c10::Symbol::fromQualString("aten::Int"), {ch[0]});
    g.insertNode(n_int);
    // prim::Loop with K outputs
    auto* loop = g.create(torch::jit::prim::Loop, K);
    loop->addInput(n_int->output());                // max_trip_count
    loop->addInput(makeConstBool(true));            // initial cond = true
    for (std::size_t k = 0; k < K; ++k) loop->addInput(ch[1 + k]); // carried inits
    g.insertNode(loop);
    auto* body = loop->addBlock();
    {
      torch::jit::WithInsertPoint guard(body);
      auto* iter = body->addInput(); iter->setType(c10::IntType::get());
      auto* cond = body->addInput(); cond->setType(c10::BoolType::get()); (void)cond;
      std::vector<result_type> carried_in(K);
      for (std::size_t k = 0; k < K; ++k) {
        auto* si = body->addInput(); si->setType(c10::TensorType::get());
        carried_in[k] = si;
      }
      loop_stack.push_back(LoopCtx{iter, carried_in});
      std::array<result_type, K> next_out{};
      for (std::size_t k = 0; k < K; ++k) next_out[k] = ch[1 + K + k];
      loop_stack.pop_back();
      auto* cond_true = makeConstBool(true);
      body->registerOutput(cond_true);
      for (std::size_t k = 0; k < K; ++k) body->registerOutput(next_out[k]);
    }
    // Collect loop outputs and pack into a list
    std::vector<result_type> outs; outs.reserve(K);
    for (std::size_t k = 0; k < K; ++k) outs.push_back(loop->output(k));
    auto* list = g.createList(c10::TensorType::get(), outs);
    g.insertNode(list);
    return list->output();
  }

  // Out<J>(list)
  template <std::size_t J>
  result_type emitApply(LoopOutOp<J>, result_type list_v) {
    auto* idx = makeConstInt(static_cast<std::int64_t>(J));
    auto* get = g.create(c10::Symbol::fromQualString("aten::__getitem__"), {list_v, idx});
    g.insertNode(get);
    return get->output();
  }
#endif
};
#else
struct TorchJITBackend; // stub
#endif

} // namespace et
