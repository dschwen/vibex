#pragma once
#include <vector>
#include <cmath>
#include <cassert>
#include <cstdint>
#include <algorithm>
#include <type_traits>

namespace et {

struct Tape {
  enum Kind : uint8_t { KVar, KConst, KAdd, KSub, KMul, KDiv, KNeg, KSin, KExp, KLog, KSqrt, KTanh, KCos };

  struct Node {
    Kind kind;
    int  a = -1;
    int  b = -1;
    double c = 0;
    std::size_t var_index = ~std::size_t(0);
  };

  std::vector<Node> nodes;
  int output_id = -1;

  double forward(const std::vector<double>& inputs) const {
    std::vector<double> val(nodes.size());
    for (int i = 0; i < (int)nodes.size(); ++i) {
      const auto& n = nodes[i];
      switch (n.kind) {
        case KVar:  val[i] = inputs[n.var_index]; break;
        case KConst:val[i] = n.c; break;
        case KAdd:  val[i] = val[n.a] + val[n.b]; break;
        case KSub:  val[i] = val[n.a] - val[n.b]; break;
        case KMul:  val[i] = val[n.a] * val[n.b]; break;
        case KDiv:  val[i] = val[n.a] / val[n.b]; break;
        case KNeg:  val[i] = -val[n.a]; break;
        case KSin:  val[i] = std::sin(val[n.a]); break;
        case KExp:  val[i] = std::exp(val[n.a]); break;
        case KLog:  val[i] = std::log(val[n.a]); break;
        case KSqrt: val[i] = std::sqrt(val[n.a]); break;
        case KTanh: val[i] = std::tanh(val[n.a]); break;
        case KCos:  val[i] = std::cos(val[n.a]); break;
      }
    }
    return val[output_id];
  }

  std::vector<double> vjp(const std::vector<double>& inputs) const {
    const int N = (int)nodes.size();
    std::vector<double> val(N), bar(N, 0.0);
    for (int i = 0; i < N; ++i) {
      const auto& n = nodes[i];
      switch (n.kind) {
        case KVar:  val[i] = inputs[n.var_index]; break;
        case KConst:val[i] = n.c; break;
        case KAdd:  val[i] = val[n.a] + val[n.b]; break;
        case KSub:  val[i] = val[n.a] - val[n.b]; break;
        case KMul:  val[i] = val[n.a] * val[n.b]; break;
        case KDiv:  val[i] = val[n.a] / val[n.b]; break;
        case KNeg:  val[i] = -val[n.a]; break;
        case KSin:  val[i] = std::sin(val[n.a]); break;
        case KExp:  val[i] = std::exp(val[n.a]); break;
        case KLog:  val[i] = std::log(val[n.a]); break;
        case KSqrt: val[i] = std::sqrt(val[n.a]); break;
        case KTanh: val[i] = std::tanh(val[n.a]); break;
        case KCos:  val[i] = std::cos(val[n.a]); break;
      }
    }
    bar[output_id] = 1.0;
    for (int i = N - 1; i >= 0; --i) {
      const auto& n = nodes[i];
      switch (n.kind) {
        case KVar:   break;
        case KConst: break;
        case KAdd:
          bar[n.a] += bar[i];
          bar[n.b] += bar[i];
          break;
        case KSub:
          bar[n.a] += bar[i];
          bar[n.b] -= bar[i];
          break;
        case KMul:
          bar[n.a] += bar[i] * val[n.b];
          bar[n.b] += bar[i] * val[n.a];
          break;
        case KDiv:
          bar[n.a] += bar[i] / val[n.b];
          bar[n.b] -= bar[i] * val[n.a] / (val[n.b] * val[n.b]);
          break;
        case KNeg:
          bar[n.a] -= bar[i];
          break;
        case KSin:
          bar[n.a] += bar[i] * std::cos(val[n.a]);
          break;
        case KExp:
          bar[n.a] += bar[i] * std::exp(val[n.a]);
          break;
        case KLog:
          bar[n.a] += bar[i] / val[n.a];
          break;
        case KSqrt:
          bar[n.a] += bar[i] * (0.5 / std::sqrt(val[n.a]));
          break;
        case KTanh: {
          double t = std::tanh(val[n.a]);
          bar[n.a] += bar[i] * (1.0 - t * t);
          break; }
        case KCos:
          bar[n.a] -= bar[i] * std::sin(val[n.a]);
          break;
      }
    }
    std::size_t arity = 0;
    for (auto& n : nodes) if (n.kind == KVar) arity = std::max(arity, n.var_index+1);
    std::vector<double> grad(arity, 0.0);
    for (int i = 0; i < N; ++i)
      if (nodes[i].kind == KVar) grad[nodes[i].var_index] = bar[i];
    return grad;
  }
};

struct TapeBackend {
  using result_type = int;
  Tape tape;

  explicit TapeBackend(std::size_t /*arity*/) { tape.nodes.reserve(64); }

  template <class T, std::size_t I>
  result_type emitVar(Var<T,I>) {
    Tape::Node n; n.kind = Tape::KVar; n.var_index = I;
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }

  template <class T>
  result_type emitConst(Const<T> c) {
    Tape::Node n; n.kind = Tape::KConst; n.c = static_cast<double>(c.value);
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }

  template <class Op>
  result_type emitApply(Op, int a) {
    Tape::Node n;
    if constexpr (std::is_same<Op, NegOp>::value) n.kind = Tape::KNeg;
    else if constexpr (std::is_same<Op, SinOp>::value) n.kind = Tape::KSin;
    else if constexpr (std::is_same<Op, ExpOp>::value) n.kind = Tape::KExp;
    else if constexpr (std::is_same<Op, LogOp>::value) n.kind = Tape::KLog;
    else if constexpr (std::is_same<Op, SqrtOp>::value) n.kind = Tape::KSqrt;
    else if constexpr (std::is_same<Op, TanhOp>::value) n.kind = Tape::KTanh;
    else if constexpr (std::is_same<Op, CosOp>::value) n.kind = Tape::KCos;
    else static_assert(!std::is_same<Op,Op>::value, "Unary op not mapped to Tape");
    n.a = a;
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }

  template <class Op>
  result_type emitApply(Op, int a, int b) {
    Tape::Node n;
    if constexpr      (std::is_same<Op, AddOp>::value) n.kind = Tape::KAdd;
    else if constexpr (std::is_same<Op, SubOp>::value) n.kind = Tape::KSub;
    else if constexpr (std::is_same<Op, MulOp>::value) n.kind = Tape::KMul;
    else if constexpr (std::is_same<Op, DivOp>::value) n.kind = Tape::KDiv;
    else static_assert(!std::is_same<Op,Op>::value, "Binary op not mapped to Tape");
    n.a = a; n.b = b;
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }
};

} // namespace et
