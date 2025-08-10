#pragma once
#include <string>
#include <unordered_map>
#include <sstream>
#include <tuple>     // for std::apply

namespace et {

// --- stringify keys for structural CSE ---------------------------------
inline void to_key_stream(std::ostream& os, const char*) {}

template <class T, std::size_t I>
inline void to_key_stream(std::ostream& os, const Var<T,I>&) { os << "Var<" << I << ">"; }

template <class T>
inline void to_key_stream(std::ostream& os, const Const<T>& c) { os << "Const(" << static_cast<long double>(c.value) << ")"; }

template <class Op> inline const char* op_name() { return "Op?"; }
template <> inline const char* op_name<AddOp>() { return "Add"; }
template <> inline const char* op_name<SubOp>() { return "Sub"; }
template <> inline const char* op_name<MulOp>() { return "Mul"; }
template <> inline const char* op_name<DivOp>() { return "Div"; }
template <> inline const char* op_name<NegOp>() { return "Neg"; }
template <> inline const char* op_name<SinOp>() { return "Sin"; }
template <> inline const char* op_name<CosOp>() { return "Cos"; }
template <> inline const char* op_name<ExpOp>() { return "Exp"; }
template <> inline const char* op_name<LogOp>() { return "Log"; }
template <> inline const char* op_name<SqrtOp>() { return "Sqrt"; }
template <> inline const char* op_name<TanhOp>() { return "Tanh"; }

template <class Op, class... Ch>
inline void to_key_stream(std::ostream& os, const Apply<Op,Ch...>& a) {
  os << op_name<Op>() << "(";
  bool first = true;
  std::apply([&](const auto&... c){
    ((os << (first ? (first=false, "") : ", "), to_key_stream(os, c)), ...);
  }, a.ch);
  os << ")";
}

template <class Expr>
inline std::string key_of(const Expr& e) {
  std::ostringstream oss; to_key_stream(oss, e); return oss.str();
}

// --- non-local helper to satisfy GCC: member templates in local classes are ill-formed in C++17
template <class Backend>
struct CSEHelper {
  Backend& b;
  std::unordered_map<std::string, typename Backend::result_type>& memo;

  template <class X>
  typename Backend::result_type operator()(const X& x) {
    auto k = key_of(x);
    auto it = memo.find(k);
    if (it != memo.end()) return it->second;
    auto out = compile_impl(x);
    memo.emplace(std::move(k), out);
    return out;
  }

  template <class T, std::size_t I>
  typename Backend::result_type compile_impl(const Var<T,I>& v) {
    return b.template emitVar<T,I>(v);
  }
  template <class T>
  typename Backend::result_type compile_impl(const Const<T>& c) {
    return b.template emitConst<T>(c);
  }
  template <class Op, class... Ch>
  typename Backend::result_type compile_impl(const Apply<Op,Ch...>& a) {
    return std::apply([&](const auto&... c){
      return b.emitApply(Op{}, (*this)(c)...);
    }, a.ch);
  }
};

template <class Backend, class Expr>
auto compile_cse(const Expr& e, Backend& b) -> typename Backend::result_type {
  std::unordered_map<std::string, typename Backend::result_type> memo;
  CSEHelper<Backend> H{b, memo};
  return H(e);
}

} // namespace et
