#pragma once

#ifdef ET_WITH_TORCH
#  include <torch/script.h>
#  include <torch/version.h>
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
  // Prefer CompilationUnit-based construction; support both jit and jit::script headers
  #if defined(__has_include)
    #if __has_include(<torch/csrc/jit/api/compilation_unit.h>)
      #include <torch/csrc/jit/api/compilation_unit.h>
      #define ET_TORCH_HAS_CU 1
      namespace et_detail { using CU = torch::jit::CompilationUnit; }
    #elif __has_include(<torch/csrc/jit/script/compilation_unit.h>)
      #include <torch/csrc/jit/script/compilation_unit.h>
      #define ET_TORCH_HAS_CU 1
      namespace et_detail { using CU = torch::jit::script::CompilationUnit; }
    #endif
  #endif

  // Only enable the Module wrapper when the build system defines
  // ET_TORCH_MODULE_WRAPPER_AVAILABLE explicitly (APIs vary across libtorch builds).
  // We always prefer the GraphExecutor fallback otherwise.
  #if defined(ET_TORCH_HAS_CU) && defined(ET_TORCH_MODULE_WRAPPER_AVAILABLE)

// SFINAE check for Module::set_method(name, Function*)
template <typename T>
class has_set_method {
  template <typename U>
  static auto test(int) -> decltype(std::declval<U>().set_method(std::declval<std::string>(), (torch::jit::Function*)nullptr), std::true_type{});
  template <typename>
  static std::false_type test(...);
public:
  static constexpr bool value = decltype(test<T>(0))::value;
};

inline torch::jit::Module make_script_module(TorchCompiled& tc, const std::string& method_name = "forward") {
  auto gcopy = tc.graph().copy();
  auto cu = std::make_shared<et_detail::CU>();
  c10::QualifiedName qn(method_name);
  torch::jit::Function* fn = cu->create_function(std::move(qn), gcopy);
  (void)fn;
  torch::jit::Module m(cu, "ETModule");
  if constexpr (has_set_method<torch::jit::Module>::value) {
    m.set_method(method_name, fn);
  }
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
  #endif // ET_TORCH_HAS_CU && ET_TORCH_MODULE_WRAPPER_AVAILABLE
#endif // ET_TORCH_ENABLE_MODULE_WRAPPER

// GraphExecutor-based runner (no Module). Broader availability across libtorch builds.
#if defined(__has_include)
#  if __has_include(<torch/csrc/jit/graph_executor.h>)
#    include <torch/csrc/jit/graph_executor.h>
#    define ET_TORCH_HAS_GRAPH_EXECUTOR 1
#    if __has_include(<torch/csrc/jit/passes/eliminate_dead_code.h>)
#      include <torch/csrc/jit/passes/eliminate_dead_code.h>
#      define ET_TORCH_HAS_PASS_DCE 1
#    endif
#    if __has_include(<torch/csrc/jit/passes/constant_propagation.h>)
#      include <torch/csrc/jit/passes/constant_propagation.h>
#      define ET_TORCH_HAS_PASS_CONST 1
#    endif
#    if __has_include(<torch/csrc/jit/passes/common_subexpression_elimination.h>)
#      include <torch/csrc/jit/passes/common_subexpression_elimination.h>
#      define ET_TORCH_HAS_PASS_CSE 1
#    endif
#    if __has_include(<torch/csrc/jit/passes/graph_fuser.h>)
#      include <torch/csrc/jit/passes/graph_fuser.h>
#      define ET_TORCH_HAS_PASS_FUSE 1
#    endif
#  endif
#endif

#ifdef ET_TORCH_HAS_GRAPH_EXECUTOR
struct TorchGraphRunner {
  std::shared_ptr<torch::jit::Graph> graph;
  std::unique_ptr<torch::jit::GraphExecutor> exec;
  size_t expected_inputs = 0;

  explicit TorchGraphRunner(std::shared_ptr<torch::jit::Graph> g)
  : graph(std::move(g)) {
    if (graph) {
      graph->lint();
#if defined(ET_TORCH_HAS_PASS_DCE)
      torch::jit::EliminateDeadCode(graph);
#endif

// (define wrapper moved below, outside this block)
#if defined(ET_TORCH_HAS_PASS_CONST)
      torch::jit::ConstantPropagation(graph);
#endif
#if defined(ET_TORCH_HAS_PASS_CSE)
      torch::jit::EliminateCommonSubexpression(graph);
#endif
#if defined(ET_TORCH_HAS_PASS_FUSE)
      torch::jit::FuseGraph(graph, /*arg*/true);
#endif
      expected_inputs = graph->inputs().size();
    }
    exec = std::make_unique<torch::jit::GraphExecutor>(graph, /*name*/"et_forward");
  }

  c10::IValue operator()(const std::vector<c10::IValue>& inputs) const {
    if (expected_inputs && inputs.size() != expected_inputs) {
      // Mismatch; return an empty IValue to signal error in a header-only context.
      return c10::IValue();
    }
    torch::jit::Stack stack;
    stack.reserve(inputs.size());
    for (const auto& iv : inputs) stack.push_back(iv);
#if defined(__has_include)
#  if __has_include(<torch/autograd.h>)
#    include <torch/autograd.h>
    torch::NoGradGuard no_grad;
#  endif
#endif
    exec->run(stack);
    // Expect at least one output on stack
    if (!stack.empty()) return stack.back();
    return c10::IValue();
  }
};

template <class Expr>
inline TorchGraphRunner make_torch_graph_runner(const Expr& e, std::size_t arity) {
  auto tc = compile_to_torch(e, arity);
  auto gcopy = tc.graph().copy();
  return TorchGraphRunner(std::move(gcopy));
}
#endif

// TorchScript define() wrapper placed outside GraphExecutor block
#ifdef ET_TORCH_ENABLE_DEFINE_WRAPPER
namespace detail_ts {
  inline std::string num(double v){ std::ostringstream os; os.setf(std::ios::fixed); os.precision(6); os<<v; return os.str(); }
  template <class T, std::size_t I> std::string emit(const Var<T,I>&){ return std::string("x")+std::to_string(I); }
  template <class T> std::string emit(const Const<T>& c){ return num(static_cast<double>(c.value)); }
  template <class Op, class A> std::string emit(const Apply<Op,A>&);
  template <class Op, class A, class B> std::string emit(const Apply<Op,A,B>&);
  template <class Op, class A, class B, class C> std::string emit(const Apply<Op,A,B,C>&);
  template <class A> std::string emit(const Apply<NegOp,A>& a){ return "(-"+emit(a.template child<0>())+")"; }
  template <class A> std::string emit(const Apply<SinOp,A>& a){ return "torch.sin("+emit(a.template child<0>())+")"; }
  template <class A> std::string emit(const Apply<CosOp,A>& a){ return "torch.cos("+emit(a.template child<0>())+")"; }
  template <class A> std::string emit(const Apply<ExpOp,A>& a){ return "torch.exp("+emit(a.template child<0>())+")"; }
  template <class A> std::string emit(const Apply<LogOp,A>& a){ return "torch.log("+emit(a.template child<0>())+")"; }
  template <class A> std::string emit(const Apply<SqrtOp,A>& a){ return "torch.sqrt("+emit(a.template child<0>())+")"; }
  template <class A> std::string emit(const Apply<TanhOp,A>& a){ return "torch.tanh("+emit(a.template child<0>())+")"; }
  template <class A> std::string emit(const Apply<NotOp,A>& a){ return "torch.logical_not("+emit(a.template child<0>())+")"; }
  template <class A, class B> std::string emit(const Apply<AddOp,A,B>& a){ return "("+emit(a.template child<0>())+"+"+emit(a.template child<1>())+")"; }
  template <class A, class B> std::string emit(const Apply<SubOp,A,B>& a){ return "("+emit(a.template child<0>())+"-"+emit(a.template child<1>())+")"; }
  template <class A, class B> std::string emit(const Apply<MulOp,A,B>& a){ return "("+emit(a.template child<0>())+"*"+emit(a.template child<1>())+")"; }
  template <class A, class B> std::string emit(const Apply<DivOp,A,B>& a){ return "("+emit(a.template child<0>())+"/"+emit(a.template child<1>())+")"; }
  template <class A, class B> std::string emit(const Apply<PowOp,A,B>& a){ return "("+emit(a.template child<0>())+"**"+emit(a.template child<1>())+")"; }
  template <class A, class B> std::string emit(const Apply<LtOp,A,B>& a){ return "("+emit(a.template child<0>())+"<"+emit(a.template child<1>())+")"; }
  template <class A, class B> std::string emit(const Apply<LeOp,A,B>& a){ return "("+emit(a.template child<0>())+"<="+emit(a.template child<1>())+")"; }
  template <class A, class B> std::string emit(const Apply<GtOp,A,B>& a){ return "("+emit(a.template child<0>())+">"+emit(a.template child<1>())+")"; }
  template <class A, class B> std::string emit(const Apply<GeOp,A,B>& a){ return "("+emit(a.template child<0>())+">="+emit(a.template child<1>())+")"; }
  template <class A, class B> std::string emit(const Apply<EqOp,A,B>& a){ return "("+emit(a.template child<0>())+"=="+emit(a.template child<1>())+")"; }
  template <class A, class B> std::string emit(const Apply<NeOp,A,B>& a){ return "("+emit(a.template child<0>())+"!="+emit(a.template child<1>())+")"; }
  template <class A, class B, class C> std::string emit(const Apply<IfOp,A,B,C>& a){ return "torch.where("+emit(a.template child<0>())+","+emit(a.template child<1>())+","+emit(a.template child<2>())+")"; }
  template <class A, class B, class C> std::string emit(const Apply<SelectOp,A,B,C>& a){ return "torch.where("+emit(a.template child<0>())+","+emit(a.template child<1>())+","+emit(a.template child<2>())+")"; }
  template <class Expr> std::string emit_any(const Expr& e){ return emit(e); }
  template <class Expr> std::string gen_source(const Expr& e, std::size_t arity){ std::ostringstream os; os<<"def forward(self"; for(std::size_t i=0;i<arity;++i) os<<", x"<<i; os<<"):\n    return "<<emit_any(e)<<"\n"; return os.str(); }
}

template <class Expr>
inline torch::jit::Module make_script_module_define(const Expr& e, std::size_t arity){ torch::jit::Module m("ETModule"); auto src = detail_ts::gen_source(e, arity); m.define(src); return m; }
#endif // ET_TORCH_ENABLE_DEFINE_WRAPPER

} // namespace et

#endif // ET_WITH_TORCH
