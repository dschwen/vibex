#pragma once
#include <vector>
#include <functional>
#include <array>
#include <cmath>
#include <cassert>
#include <cstdint>
#include <algorithm>
#include <type_traits>
#include "et/expr.hpp"

namespace et {

struct Tape {
  enum Kind : uint8_t { KVar, KConst, KAdd, KSub, KMul, KDiv, KPow, KNeg, KSin, KExp, KLog, KSqrt, KTanh, KCos,
#ifdef ET_ENABLE_CONTROL_FLOW
                        KLt, KLe, KGt, KGe, KEq, KNe, KNot, KIf, KSelect,
                        KIter, KStateRead, KLoopFor, KLoopOut,
#endif
  };

  struct Node {
    Kind kind;
    int  a = -1;
    int  b = -1;
    double c = 0;
    std::size_t var_index = ~std::size_t(0);
    std::vector<int> ch; // for LoopFor/LoopOut
  };

  std::vector<Node> nodes;
  int output_id = -1;

  double forward(const std::vector<double>& inputs) const {
    std::vector<double> val(nodes.size());
#ifdef ET_ENABLE_CONTROL_FLOW
    struct LoopCtx { std::size_t iter = 0; const std::vector<double>* state = nullptr; };
    std::function<double(int,const LoopCtx&)> rec_nm = [&](int id, const LoopCtx& ctx) -> double {
      const auto& n = nodes[id];
      switch (n.kind) {
        case KVar:  return inputs[n.var_index];
        case KConst:return n.c;
        case KAdd:  return rec_nm(n.a, ctx) + rec_nm(n.b, ctx);
        case KSub:  return rec_nm(n.a, ctx) - rec_nm(n.b, ctx);
        case KMul:  return rec_nm(n.a, ctx) * rec_nm(n.b, ctx);
        case KDiv:  return rec_nm(n.a, ctx) / rec_nm(n.b, ctx);
        case KPow:  return std::pow(rec_nm(n.a, ctx), rec_nm(n.b, ctx));
        case KNeg:  return -rec_nm(n.a, ctx);
        case KSin:  return std::sin(rec_nm(n.a, ctx));
        case KExp:  return std::exp(rec_nm(n.a, ctx));
        case KLog:  return std::log(rec_nm(n.a, ctx));
        case KSqrt: return std::sqrt(rec_nm(n.a, ctx));
        case KTanh: return std::tanh(rec_nm(n.a, ctx));
        case KCos:  return std::cos(rec_nm(n.a, ctx));
        case KLt:   return rec_nm(n.a, ctx) <  rec_nm(n.b, ctx) ? 1.0 : 0.0;
        case KLe:   return rec_nm(n.a, ctx) <= rec_nm(n.b, ctx) ? 1.0 : 0.0;
        case KGt:   return rec_nm(n.a, ctx) >  rec_nm(n.b, ctx) ? 1.0 : 0.0;
        case KGe:   return rec_nm(n.a, ctx) >= rec_nm(n.b, ctx) ? 1.0 : 0.0;
        case KEq:   return rec_nm(n.a, ctx) == rec_nm(n.b, ctx) ? 1.0 : 0.0;
        case KNe:   return rec_nm(n.a, ctx) != rec_nm(n.b, ctx) ? 1.0 : 0.0;
        case KNot:  return (rec_nm(n.a, ctx) == 0.0) ? 1.0 : 0.0;
        case KIf:   return (rec_nm(n.a, ctx) != 0.0) ? rec_nm(n.b, ctx) : rec_nm(n.c, ctx);
        case KSelect:return (rec_nm(n.a, ctx) != 0.0) ? rec_nm(n.b, ctx) : rec_nm(n.c, ctx);
        case KIter: return static_cast<double>(ctx.iter);
        case KStateRead: return ctx.state ? (*ctx.state)[n.var_index] : 0.0;
        case KLoopFor: return 0.0; // evaluated by KLoopOut
        case KLoopOut: {
          int loop_id = n.ch[0];
          const auto& lf = nodes[loop_id];
          std::size_t K = lf.var_index;
          // Evaluate N and initial states in current context
          std::size_t N = static_cast<std::size_t>(std::max(0.0, rec_nm(lf.ch[0], ctx)));
          std::vector<double> st(K);
          for (std::size_t k = 0; k < K; ++k) st[k] = rec_nm(lf.ch[1 + k], ctx);
          for (std::size_t it = 0; it < N; ++it) {
            LoopCtx inner{it, &st};
            std::vector<double> next(K);
            for (std::size_t k = 0; k < K; ++k) next[k] = rec_nm(lf.ch[1 + K + k], inner);
            st.swap(next);
          }
          std::size_t J = n.var_index;
          return (J < st.size()) ? st[J] : 0.0;
        }
      }
      return 0.0;
    };
#endif
    for (int i = 0; i < (int)nodes.size(); ++i) {
      const auto& n = nodes[i];
      switch (n.kind) {
        case KVar:  val[i] = inputs[n.var_index]; break;
        case KConst:val[i] = n.c; break;
        case KAdd:  val[i] = val[n.a] + val[n.b]; break;
        case KSub:  val[i] = val[n.a] - val[n.b]; break;
        case KMul:  val[i] = val[n.a] * val[n.b]; break;
        case KDiv:  val[i] = val[n.a] / val[n.b]; break;
        case KPow:  val[i] = std::pow(val[n.a], val[n.b]); break;
        case KNeg:  val[i] = -val[n.a]; break;
        case KSin:  val[i] = std::sin(val[n.a]); break;
        case KExp:  val[i] = std::exp(val[n.a]); break;
        case KLog:  val[i] = std::log(val[n.a]); break;
        case KSqrt: val[i] = std::sqrt(val[n.a]); break;
        case KTanh: val[i] = std::tanh(val[n.a]); break;
        case KCos:  val[i] = std::cos(val[n.a]); break;
#ifdef ET_ENABLE_CONTROL_FLOW
        case KLt:     val[i] = val[n.a] <  val[n.b] ? 1.0 : 0.0; break;
        case KLe:     val[i] = val[n.a] <= val[n.b] ? 1.0 : 0.0; break;
        case KGt:     val[i] = val[n.a] >  val[n.b] ? 1.0 : 0.0; break;
        case KGe:     val[i] = val[n.a] >= val[n.b] ? 1.0 : 0.0; break;
        case KEq:     val[i] = val[n.a] == val[n.b] ? 1.0 : 0.0; break;
        case KNe:     val[i] = val[n.a] != val[n.b] ? 1.0 : 0.0; break;
        case KNot:    val[i] = (val[n.a] == 0.0) ? 1.0 : 0.0; break;
        case KIf:     val[i] = (val[n.a] != 0.0) ? val[n.b] : val[n.c]; break;
        case KSelect: val[i] = (val[n.a] != 0.0) ? val[n.b] : val[n.c]; break;
        case KIter:   val[i] = 0.0; break; // meaningful only inside loop eval
        case KStateRead: val[i] = 0.0; break;
        case KLoopFor: val[i] = 0.0; break; // evaluated by KLoopOut
        case KLoopOut: val[i] = rec_nm(i, LoopCtx{}); break;
#endif
      }
    }
    return val[output_id];
  }

  std::vector<double> backward(const std::vector<double>& inputs) const {
    const int N = (int)nodes.size();
    std::vector<double> val(N), bar(N, 0.0);
    #ifdef ET_ENABLE_CONTROL_FLOW
    struct LoopCtx { std::size_t iter = 0; const std::vector<double>* state = nullptr; };
    std::function<double(int,const LoopCtx&)> rec_nm = [&](int id, const LoopCtx& ctx) -> double {
      const auto& n = nodes[id];
      switch (n.kind) {
        case KVar:  return inputs[n.var_index];
        case KConst:return n.c;
        case KAdd:  return rec_nm(n.a, ctx) + rec_nm(n.b, ctx);
        case KSub:  return rec_nm(n.a, ctx) - rec_nm(n.b, ctx);
        case KMul:  return rec_nm(n.a, ctx) * rec_nm(n.b, ctx);
        case KDiv:  return rec_nm(n.a, ctx) / rec_nm(n.b, ctx);
        case KPow:  return std::pow(rec_nm(n.a, ctx), rec_nm(n.b, ctx));
        case KNeg:  return -rec_nm(n.a, ctx);
        case KSin:  return std::sin(rec_nm(n.a, ctx));
        case KExp:  return std::exp(rec_nm(n.a, ctx));
        case KLog:  return std::log(rec_nm(n.a, ctx));
        case KSqrt: return std::sqrt(rec_nm(n.a, ctx));
        case KTanh: return std::tanh(rec_nm(n.a, ctx));
        case KCos:  return std::cos(rec_nm(n.a, ctx));
        case KLt:   return rec_nm(n.a, ctx) <  rec_nm(n.b, ctx) ? 1.0 : 0.0;
        case KLe:   return rec_nm(n.a, ctx) <= rec_nm(n.b, ctx) ? 1.0 : 0.0;
        case KGt:   return rec_nm(n.a, ctx) >  rec_nm(n.b, ctx) ? 1.0 : 0.0;
        case KGe:   return rec_nm(n.a, ctx) >= rec_nm(n.b, ctx) ? 1.0 : 0.0;
        case KEq:   return rec_nm(n.a, ctx) == rec_nm(n.b, ctx) ? 1.0 : 0.0;
        case KNe:   return rec_nm(n.a, ctx) != rec_nm(n.b, ctx) ? 1.0 : 0.0;
        case KNot:  return (rec_nm(n.a, ctx) == 0.0) ? 1.0 : 0.0;
        case KIf:   return (rec_nm(n.a, ctx) != 0.0) ? rec_nm(n.b, ctx) : rec_nm(n.c, ctx);
        case KSelect:return (rec_nm(n.a, ctx) != 0.0) ? rec_nm(n.b, ctx) : rec_nm(n.c, ctx);
        case KIter: return static_cast<double>(ctx.iter);
        case KStateRead: return ctx.state ? (*ctx.state)[n.var_index] : 0.0;
        case KLoopFor: return 0.0;
        case KLoopOut: {
          int loop_id = n.ch[0];
          const auto& lf = nodes[loop_id];
          std::size_t K = lf.var_index;
          std::size_t Niter = static_cast<std::size_t>(std::max(0.0, rec_nm(lf.ch[0], ctx)));
          std::vector<double> st(K);
          for (std::size_t k = 0; k < K; ++k) st[k] = rec_nm(lf.ch[1 + k], ctx);
          for (std::size_t it = 0; it < Niter; ++it) {
            LoopCtx inner{it, &st};
            std::vector<double> next(K);
            for (std::size_t k = 0; k < K; ++k) next[k] = rec_nm(lf.ch[1 + K + k], inner);
            st.swap(next);
          }
          std::size_t J = n.var_index;
          return (J < st.size()) ? st[J] : 0.0;
        }
      }
      return 0.0;
    };
    #endif
    for (int i = 0; i < N; ++i) {
      const auto& n = nodes[i];
      switch (n.kind) {
        case KVar:  val[i] = inputs[n.var_index]; break;
        case KConst:val[i] = n.c; break;
        case KAdd:  val[i] = val[n.a] + val[n.b]; break;
        case KSub:  val[i] = val[n.a] - val[n.b]; break;
        case KMul:  val[i] = val[n.a] * val[n.b]; break;
        case KDiv:  val[i] = val[n.a] / val[n.b]; break;
        case KPow:  val[i] = std::pow(val[n.a], val[n.b]); break;
        case KNeg:  val[i] = -val[n.a]; break;
        case KSin:  val[i] = std::sin(val[n.a]); break;
        case KExp:  val[i] = std::exp(val[n.a]); break;
        case KLog:  val[i] = std::log(val[n.a]); break;
        case KSqrt: val[i] = std::sqrt(val[n.a]); break;
        case KTanh: val[i] = std::tanh(val[n.a]); break;
        case KCos:  val[i] = std::cos(val[n.a]); break;
#ifdef ET_ENABLE_CONTROL_FLOW
        case KLt:     val[i] = val[n.a] <  val[n.b] ? 1.0 : 0.0; break;
        case KLe:     val[i] = val[n.a] <= val[n.b] ? 1.0 : 0.0; break;
        case KGt:     val[i] = val[n.a] >  val[n.b] ? 1.0 : 0.0; break;
        case KGe:     val[i] = val[n.a] >= val[n.b] ? 1.0 : 0.0; break;
        case KEq:     val[i] = val[n.a] == val[n.b] ? 1.0 : 0.0; break;
        case KNe:     val[i] = val[n.a] != val[n.b] ? 1.0 : 0.0; break;
        case KNot:    val[i] = (val[n.a] == 0.0) ? 1.0 : 0.0; break;
        case KIf:     val[i] = (val[n.a] != 0.0) ? val[n.b] : val[n.c]; break;
        case KSelect: val[i] = (val[n.a] != 0.0) ? val[n.b] : val[n.c]; break;
        case KIter:   val[i] = 0.0; break;
        case KStateRead: val[i] = 0.0; break;
        case KLoopFor: val[i] = 0.0; break;
        case KLoopOut: val[i] = rec_nm(i, LoopCtx{}); break;
#endif
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
        case KPow: {
          double f = std::pow(val[n.a], val[n.b]);
          bar[n.a] += bar[i] * f * (val[n.b] / val[n.a]);
          bar[n.b] += bar[i] * f * std::log(val[n.a]);
          break; }
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
#ifdef ET_ENABLE_CONTROL_FLOW
        case KLt: case KLe: case KGt: case KGe: case KEq: case KNe: case KNot:
          // No gradient into predicate inputs
          break;
        case KIf: {
          bool cond = (val[n.a] != 0.0);
          if (cond) bar[n.b] += bar[i]; else bar[n.c] += bar[i];
          break;
        }
        case KSelect: {
          bool cond = (val[n.a] != 0.0);
          if (cond) bar[n.b] += bar[i]; else bar[n.c] += bar[i];
          break;
        }
        case KIter: case KStateRead: case KLoopFor: {
          // No direct adjoints; handled via KLoopOut
          break;
        }
        case KLoopOut: {
          // Propagate gradient into loop initial states via reverse iteration of Jacobian^T
          int loop_id = n.ch[0];
          const auto& lf = nodes[loop_id];
          std::size_t K = lf.var_index;
          // Build initial state and iterate forward to compute per-iter states
          std::size_t Niter = static_cast<std::size_t>(std::max(0.0, val[lf.ch[0]]));
          std::vector<std::vector<double>> states; states.reserve(Niter+1);
          std::vector<double> st(K);
          for (std::size_t k = 0; k < K; ++k) st[k] = val[lf.ch[1 + k]];
          states.push_back(st); // s0
          for (std::size_t it = 0; it < Niter; ++it) {
            LoopCtx inner{it, &st};
            std::vector<double> next(K);
            for (std::size_t k = 0; k < K; ++k) next[k] = rec_nm(lf.ch[1 + K + k], inner);
            st.swap(next);
            states.push_back(st);
          }
          // Build representative Var node per input index
          std::size_t arity = 0;
          for (const auto& nd : nodes) if (nd.kind == KVar) arity = std::max(arity, nd.var_index+1);
          std::vector<int> rep_var_node(arity, -1);
          for (int nid = 0; nid < (int)nodes.size(); ++nid) if (nodes[nid].kind == KVar) {
            auto vi = nodes[nid].var_index; if (rep_var_node[vi] == -1) rep_var_node[vi] = nid;
          }
          // Helper: value + grad wrt state and inputs for a given node in given ctx
          struct VG { double v; std::vector<double> gs; std::vector<double> gi; };
          std::function<VG(int,const LoopCtx&,std::size_t,std::size_t)> vg = [&](int id, const LoopCtx& ctx, std::size_t Kloc, std::size_t Iarity) -> VG {
            const auto& nn = nodes[id];
            switch (nn.kind) {
              case KVar:   { VG out{ inputs[nn.var_index], std::vector<double>(Kloc, 0.0), std::vector<double>(Iarity, 0.0)}; out.gi[nn.var_index] = 1.0; return out; }
              case KConst: return VG{ nn.c, std::vector<double>(Kloc, 0.0), std::vector<double>(Iarity, 0.0)};
              case KIter:  return VG{ static_cast<double>(ctx.iter), std::vector<double>(Kloc, 0.0), std::vector<double>(Iarity, 0.0)};
              case KStateRead: {
                VG out{ ctx.state ? (*ctx.state)[nn.var_index] : 0.0, std::vector<double>(Kloc, 0.0), std::vector<double>(Iarity, 0.0)};
                if (nn.var_index < Kloc) out.gs[nn.var_index] = 1.0;
                return out;
              }
              case KAdd: {
                auto A = vg(nn.a, ctx, Kloc, Iarity); auto B = vg(nn.b, ctx, Kloc, Iarity);
                VG out{ A.v + B.v, std::move(A.gs), std::move(A.gi) };
                for (std::size_t k = 0; k < Kloc; ++k) out.gs[k] += B.gs[k];
                for (std::size_t i = 0; i < Iarity; ++i) out.gi[i] += B.gi[i];
                return out;
              }
              case KSub: {
                auto A = vg(nn.a, ctx, Kloc, Iarity); auto B = vg(nn.b, ctx, Kloc, Iarity);
                VG out{ A.v - B.v, std::move(A.gs), std::move(A.gi) };
                for (std::size_t k = 0; k < Kloc; ++k) out.gs[k] -= B.gs[k];
                for (std::size_t i = 0; i < Iarity; ++i) out.gi[i] -= B.gi[i];
                return out;
              }
              case KMul: {
                auto A = vg(nn.a, ctx, Kloc, Iarity); auto B = vg(nn.b, ctx, Kloc, Iarity);
                VG out{ A.v * B.v, std::vector<double>(Kloc, 0.0), std::vector<double>(Iarity, 0.0)};
                for (std::size_t k = 0; k < Kloc; ++k) out.gs[k] = A.gs[k]*B.v + B.gs[k]*A.v;
                for (std::size_t i = 0; i < Iarity; ++i) out.gi[i] = A.gi[i]*B.v + B.gi[i]*A.v;
                return out;
              }
              case KDiv: {
                auto A = vg(nn.a, ctx, Kloc, Iarity); auto B = vg(nn.b, ctx, Kloc, Iarity);
                VG out{ A.v / B.v, std::vector<double>(Kloc, 0.0), std::vector<double>(Iarity, 0.0)};
                double invb2 = 1.0 / (B.v * B.v);
                for (std::size_t k = 0; k < Kloc; ++k) out.gs[k] = (A.gs[k]*B.v - B.gs[k]*A.v) * invb2;
                for (std::size_t i = 0; i < Iarity; ++i) out.gi[i] = (A.gi[i]*B.v - B.gi[i]*A.v) * invb2;
                return out;
              }
              case KPow: {
                auto A = vg(nn.a, ctx, Kloc, Iarity); auto B = vg(nn.b, ctx, Kloc, Iarity);
                double f = std::pow(A.v, B.v);
                VG out{ f, std::vector<double>(Kloc, 0.0), std::vector<double>(Iarity, 0.0)};
                for (std::size_t k = 0; k < Kloc; ++k) out.gs[k] = f * ( B.gs[k]*std::log(std::max(A.v, 1e-12)) + (B.v / std::max(A.v, 1e-12)) * A.gs[k] );
                for (std::size_t i = 0; i < Iarity; ++i) out.gi[i] = f * ( B.gi[i]*std::log(std::max(A.v, 1e-12)) + (B.v / std::max(A.v, 1e-12)) * A.gi[i] );
                return out;
              }
              case KNeg: { auto A = vg(nn.a, ctx, Kloc, Iarity); for (double& x : A.gs) x = -x; for (double& y : A.gi) y = -y; return VG{ -A.v, std::move(A.gs), std::move(A.gi) }; }
              case KSin: { auto A = vg(nn.a, ctx, Kloc, Iarity); double cv = std::cos(A.v); for (double& x : A.gs) x *= cv; for (double& y : A.gi) y *= cv; return VG{ std::sin(A.v), std::move(A.gs), std::move(A.gi) }; }
              case KCos: { auto A = vg(nn.a, ctx, Kloc, Iarity); double sv = std::sin(A.v); for (double& x : A.gs) x *= -sv; for (double& y : A.gi) y *= -sv; return VG{ std::cos(A.v), std::move(A.gs), std::move(A.gi) }; }
              case KExp: { auto A = vg(nn.a, ctx, Kloc, Iarity); double ev = std::exp(A.v); for (double& x : A.gs) x *= ev; for (double& y : A.gi) y *= ev; return VG{ ev, std::move(A.gs), std::move(A.gi) }; }
              case KLog: { auto A = vg(nn.a, ctx, Kloc, Iarity); double inv = 1.0 / std::max(A.v, 1e-12); for (double& x : A.gs) x *= inv; for (double& y : A.gi) y *= inv; return VG{ std::log(A.v), std::move(A.gs), std::move(A.gi) }; }
              case KSqrt:{ auto A = vg(nn.a, ctx, Kloc, Iarity); double coef = 0.5 / std::sqrt(std::max(A.v, 1e-12)); for (double& x : A.gs) x *= coef; for (double& y : A.gi) y *= coef; return VG{ std::sqrt(A.v), std::move(A.gs), std::move(A.gi) }; }
              case KTanh:{ auto A = vg(nn.a, ctx, Kloc, Iarity); double t = std::tanh(A.v); double fac = 1.0 - t*t; for (double& x : A.gs) x *= fac; for (double& y : A.gi) y *= fac; return VG{ t, std::move(A.gs), std::move(A.gi) }; }
              case KLt: case KLe: case KGt: case KGe: case KEq: case KNe: case KNot: {
                // Predicates: zero derivative wrt state
                return VG{ rec_nm(id, ctx), std::vector<double>(Kloc, 0.0), std::vector<double>(Iarity, 0.0)};
              }
              case KIf: {
                auto c = rec_nm(nn.a, ctx);
                if (c != 0.0) return vg(nn.b, ctx, Kloc, Iarity);
                return vg(nn.c, ctx, Kloc, Iarity);
              }
              case KSelect: {
                auto c = rec_nm(nn.a, ctx);
                if (c != 0.0) return vg(nn.b, ctx, Kloc, Iarity);
                return vg(nn.c, ctx, Kloc, Iarity);
              }
              default: return VG{ rec_nm(id, ctx), std::vector<double>(Kloc, 0.0), std::vector<double>(Iarity, 0.0)};
            }
          };
          // Reverse propagate g through iterations
          std::vector<double> gvec(K, 0.0);
          std::size_t J = n.var_index;
          if (J < K) gvec[J] = bar[i];
          // Accumulator for input variable grads (by var index)
          std::vector<double> g_inputs(arity, 0.0);
          for (std::size_t it = Niter; it-- > 0; ) {
            // Evaluate Jacobian rows at s_it
            LoopCtx inner{it, &states[it]};
            std::vector<std::vector<double>> Jrows(K, std::vector<double>(K, 0.0));
            std::vector<std::vector<double>> GIrows(K, std::vector<double>(arity, 0.0));
            for (std::size_t k = 0; k < K; ++k) {
              auto vgk = vg(lf.ch[1 + K + k], inner, K, arity);
              Jrows[k] = vgk.gs;
              GIrows[k] = vgk.gi;
            }
            // g_prev = J^T * g_next
            std::vector<double> gprev(K, 0.0);
            for (std::size_t r = 0; r < K; ++r) {
              for (std::size_t c = 0; c < K; ++c) gprev[c] += Jrows[r][c] * gvec[r];
              for (std::size_t vi = 0; vi < arity; ++vi) g_inputs[vi] += GIrows[r][vi] * gvec[r];
            }
            gvec.swap(gprev);
          }
          // Accumulate into init nodes
          for (std::size_t k = 0; k < K; ++k) bar[lf.ch[1 + k]] += gvec[k];
          // Accumulate into representative Var nodes (one per input index)
          for (std::size_t vi = 0; vi < arity; ++vi) if (rep_var_node[vi] != -1) bar[rep_var_node[vi]] += g_inputs[vi];
          break;
        }
#endif
      }
    }
    std::size_t arity = 0;
    for (auto& n : nodes) if (n.kind == KVar) arity = std::max(arity, n.var_index+1);
    std::vector<double> grad(arity, 0.0);
    for (int i = 0; i < N; ++i)
      if (nodes[i].kind == KVar) grad[nodes[i].var_index] += bar[i];
    return grad;
  }

};

struct TapeBackend {
  using result_type = int;
  Tape tape;

  explicit TapeBackend(std::size_t /*arity*/) { tape.nodes.reserve(64); }

  template <class T>
  result_type emitVar(std::size_t idx) {
    Tape::Node n; n.kind = Tape::KVar; n.var_index = idx;
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }
  // Non-templated convenience
  result_type emitVar(std::size_t idx) {
    Tape::Node n; n.kind = Tape::KVar; n.var_index = idx;
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }

  template <class T>
  result_type emitConst(Const<T> c) {
    Tape::Node n; n.kind = Tape::KConst; n.c = static_cast<double>(c.value);
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }
  // Non-templated convenience
  result_type emitConst(double v) {
    Tape::Node n; n.kind = Tape::KConst; n.c = v;
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }

  // Non-templated helpers to allow generic lowering from a runtime graph
  // These mirror the specialized emitApply variants below but avoid requiring
  // compile-time indices for control-flow constructs.
#ifdef ET_ENABLE_CONTROL_FLOW
  // Iteration index placeholder
  result_type emitIter() {
    Tape::Node n; n.kind = Tape::KIter; tape.nodes.push_back(n); return (int)tape.nodes.size() - 1;
  }
  // Loop state read by runtime index
  result_type emitStateRead(std::size_t idx) {
    Tape::Node n; n.kind = Tape::KStateRead; n.var_index = idx; tape.nodes.push_back(n); return (int)tape.nodes.size() - 1;
  }
  // LoopFor with K carried states; children layout: [N, inits..., nexts...]
  result_type emitLoopFor(std::size_t K, const std::vector<result_type>& ch) {
    Tape::Node n; n.kind = Tape::KLoopFor; n.var_index = K;
    n.ch.reserve(ch.size());
    for (auto id : ch) n.ch.push_back(id);
    tape.nodes.push_back(std::move(n));
    return (int)tape.nodes.size() - 1;
  }
  // Out<J>(loop)
  result_type emitLoopOut(std::size_t J, result_type loop_id) {
    Tape::Node n; n.kind = Tape::KLoopOut; n.var_index = J; n.ch = {loop_id};
    tape.nodes.push_back(std::move(n));
    return (int)tape.nodes.size() - 1;
  }
#endif

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
    #ifdef ET_ENABLE_CONTROL_FLOW
    else if constexpr (std::is_same<Op, NotOp>::value) n.kind = Tape::KNot;
    else if constexpr (std::is_same<Op, IterOp>::value) n.kind = Tape::KIter;
    #endif
    else static_assert(!std::is_same<Op,Op>::value, "Unary op not mapped to Tape");
    n.a = a;
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }

  // Non-templated unary emitters
  result_type emitNeg(int a) { Tape::Node n; n.kind = Tape::KNeg; n.a = a; tape.nodes.push_back(n); return (int)tape.nodes.size()-1; }
  result_type emitSin(int a) { Tape::Node n; n.kind = Tape::KSin; n.a = a; tape.nodes.push_back(n); return (int)tape.nodes.size()-1; }
  result_type emitCos(int a) { Tape::Node n; n.kind = Tape::KCos; n.a = a; tape.nodes.push_back(n); return (int)tape.nodes.size()-1; }
  result_type emitExp(int a) { Tape::Node n; n.kind = Tape::KExp; n.a = a; tape.nodes.push_back(n); return (int)tape.nodes.size()-1; }
  result_type emitLog(int a) { Tape::Node n; n.kind = Tape::KLog; n.a = a; tape.nodes.push_back(n); return (int)tape.nodes.size()-1; }
  result_type emitSqrt(int a){ Tape::Node n; n.kind = Tape::KSqrt;n.a = a; tape.nodes.push_back(n); return (int)tape.nodes.size()-1; }
  result_type emitTanh(int a){ Tape::Node n; n.kind = Tape::KTanh;n.a = a; tape.nodes.push_back(n); return (int)tape.nodes.size()-1; }

#ifdef ET_ENABLE_CONTROL_FLOW
  // Zero-arg emit: Iter/StateRead
  result_type emitApply(IterOp) {
    Tape::Node n; n.kind = Tape::KIter; tape.nodes.push_back(n); return (int)tape.nodes.size() - 1;
  }
  template <std::size_t I>
  result_type emitApply(StateOp<I>) {
    Tape::Node n; n.kind = Tape::KStateRead; n.var_index = I; tape.nodes.push_back(n); return (int)tape.nodes.size() - 1;
  }
#endif

  template <class Op>
  result_type emitApply(Op, int a, int b) {
    Tape::Node n;
    if constexpr      (std::is_same<Op, AddOp>::value) n.kind = Tape::KAdd;
    else if constexpr (std::is_same<Op, SubOp>::value) n.kind = Tape::KSub;
    else if constexpr (std::is_same<Op, MulOp>::value) n.kind = Tape::KMul;
    else if constexpr (std::is_same<Op, DivOp>::value) n.kind = Tape::KDiv;
    else if constexpr (std::is_same<Op, PowOp>::value) n.kind = Tape::KPow;
    #ifdef ET_ENABLE_CONTROL_FLOW
    else if constexpr (std::is_same<Op, LtOp>::value) n.kind = Tape::KLt;
    else if constexpr (std::is_same<Op, LeOp>::value) n.kind = Tape::KLe;
    else if constexpr (std::is_same<Op, GtOp>::value) n.kind = Tape::KGt;
    else if constexpr (std::is_same<Op, GeOp>::value) n.kind = Tape::KGe;
    else if constexpr (std::is_same<Op, EqOp>::value) n.kind = Tape::KEq;
    else if constexpr (std::is_same<Op, NeOp>::value) n.kind = Tape::KNe;
    #endif
    else static_assert(!std::is_same<Op,Op>::value, "Binary op not mapped to Tape");
    n.a = a; n.b = b;
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }

  // Non-templated binary emitters
  result_type emitAdd(int a, int b) { Tape::Node n; n.kind = Tape::KAdd; n.a=a; n.b=b; tape.nodes.push_back(n); return (int)tape.nodes.size()-1; }
  result_type emitSub(int a, int b) { Tape::Node n; n.kind = Tape::KSub; n.a=a; n.b=b; tape.nodes.push_back(n); return (int)tape.nodes.size()-1; }
  result_type emitMul(int a, int b) { Tape::Node n; n.kind = Tape::KMul; n.a=a; n.b=b; tape.nodes.push_back(n); return (int)tape.nodes.size()-1; }
  result_type emitDiv(int a, int b) { Tape::Node n; n.kind = Tape::KDiv; n.a=a; n.b=b; tape.nodes.push_back(n); return (int)tape.nodes.size()-1; }
  result_type emitPow(int a, int b) { Tape::Node n; n.kind = Tape::KPow; n.a=a; n.b=b; tape.nodes.push_back(n); return (int)tape.nodes.size()-1; }

#ifdef ET_ENABLE_CONTROL_FLOW
  inline result_type emitApply(IfOp, int a, int b, int c) {
    Tape::Node n; n.kind = Tape::KIf; n.a = a; n.b = b; n.c = c;
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }
  inline result_type emitApply(SelectOp, int a, int b, int c) {
    Tape::Node n; n.kind = Tape::KSelect; n.a = a; n.b = b; n.c = c;
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }

  // LoopFor<K>(N, inits..., nexts...)
  template <std::size_t K, class... Children>
  result_type emitApply(LoopForOp<K>, Children... ch) {
    Tape::Node n; n.kind = Tape::KLoopFor; n.var_index = K;
    n.ch.reserve(1 + 2*K);
    std::array<int, 1 + 2*K> arr{ch...};
    for (auto id : arr) n.ch.push_back(id);
    tape.nodes.push_back(std::move(n));
    return (int)tape.nodes.size() - 1;
  }

  // Out<J>(loop_node)
  template <std::size_t J>
  result_type emitApply(LoopOutOp<J>, int loop_id) {
    Tape::Node n; n.kind = Tape::KLoopOut; n.var_index = J; n.ch = {loop_id};
    tape.nodes.push_back(std::move(n));
    return (int)tape.nodes.size() - 1;
  }
#endif
};

} // namespace et
