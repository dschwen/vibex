#pragma once
#include <tuple>
#include <type_traits>
#include <utility>
#include <cstddef>
#include <cmath>

namespace et {

//===========================
// Core IR
//===========================
template <class T, std::size_t I>
struct Var {
  using value_type = T;
  static constexpr std::size_t index = I;

  template <class First, class... Rest>
  static constexpr decltype(auto) get(std::integral_constant<std::size_t,0>, First&& f, Rest&&...) {
    return std::forward<First>(f);
  }
  template <std::size_t K, class First, class... Rest>
  static constexpr decltype(auto) get(std::integral_constant<std::size_t,K>, First&&, Rest&&... r) {
    return get(std::integral_constant<std::size_t,K-1>{}, std::forward<Rest>(r)...);
  }

  template <class... Args>
  constexpr value_type operator()(Args&&... args) const {
    auto&& a = get(std::integral_constant<std::size_t, I>{}, std::forward<Args>(args)...);
    return static_cast<T>(a);
  }
};

template <class T>
struct Const {
  using value_type = T;
  T value;
  constexpr explicit Const(const T& v) : value(v) {}
  template <class... Args>
  constexpr value_type operator()(Args&&...) const { return value; }
};

template <class T> constexpr auto lit(T v) { return Const<T>{v}; }

template <class T, class = void>
struct value_type_of { using type = T; };
template <class T>
struct value_type_of<T, std::void_t<typename T::value_type>> { using type = typename T::value_type; };
template <class T> using value_type_of_t = typename value_type_of<T>::type;

template <class Op, class... Children>
struct Apply {
  using value_type = decltype(Op::eval(std::declval<value_type_of_t<Children>>()...));
  std::tuple<Children...> ch;

  constexpr explicit Apply(Children... c) : ch(std::move(c)...) {}

  template <class... Args, std::size_t... Is>
  constexpr value_type call(std::index_sequence<Is...>, Args&&... args) const {
    return Op::eval(std::get<Is>(ch)(std::forward<Args>(args)...)...);
  }
  template <class... Args>
  constexpr value_type operator()(Args&&... args) const {
    return call(std::make_index_sequence<sizeof...(Children)>{}, std::forward<Args>(args)...);
  }

  template <std::size_t I> constexpr const auto& child() const { return std::get<I>(ch); }
};

// Node detection
template <class T> struct is_node : std::false_type {};
template <class T, std::size_t I> struct is_node<Var<T,I>> : std::true_type {};
template <class T> struct is_node<Const<T>> : std::true_type {};
template <class Op, class... Ch> struct is_node<Apply<Op,Ch...>> : std::true_type {};

template <class T>
using is_node_t = is_node<std::decay_t<T>>;

//===========================
// Ops (tags) and sugar
//===========================
// Optional control-flow and selection operators
#ifdef ET_ENABLE_CONTROL_FLOW
// --- Comparisons and logical not (bool-valued) ---
struct LtOp {
  static constexpr std::size_t arity = 2;
  template <class A, class B> static constexpr bool eval(A&& a, B&& b) { return std::forward<A>(a) < std::forward<B>(b); }
  template <std::size_t I, class ANode, class BNode>
  static auto d(const ANode&, const BNode&) {
    using T = value_type_of_t<ANode>;
    return lit(static_cast<T>(0));
  }
};
struct LeOp {
  static constexpr std::size_t arity = 2;
  template <class A, class B> static constexpr bool eval(A&& a, B&& b) { return std::forward<A>(a) <= std::forward<B>(b); }
  template <std::size_t I, class ANode, class BNode>
  static auto d(const ANode&, const BNode&) {
    using T = value_type_of_t<ANode>;
    return lit(static_cast<T>(0));
  }
};
struct GtOp {
  static constexpr std::size_t arity = 2;
  template <class A, class B> static constexpr bool eval(A&& a, B&& b) { return std::forward<A>(a) > std::forward<B>(b); }
  template <std::size_t I, class ANode, class BNode>
  static auto d(const ANode&, const BNode&) {
    using T = value_type_of_t<ANode>;
    return lit(static_cast<T>(0));
  }
};
struct GeOp {
  static constexpr std::size_t arity = 2;
  template <class A, class B> static constexpr bool eval(A&& a, B&& b) { return std::forward<A>(a) >= std::forward<B>(b); }
  template <std::size_t I, class ANode, class BNode>
  static auto d(const ANode&, const BNode&) {
    using T = value_type_of_t<ANode>;
    return lit(static_cast<T>(0));
  }
};
struct EqOp {
  static constexpr std::size_t arity = 2;
  template <class A, class B> static constexpr bool eval(A&& a, B&& b) { return std::forward<A>(a) == std::forward<B>(b); }
  template <std::size_t I, class ANode, class BNode>
  static auto d(const ANode&, const BNode&) {
    using T = value_type_of_t<ANode>;
    return lit(static_cast<T>(0));
  }
};
struct NeOp {
  static constexpr std::size_t arity = 2;
  template <class A, class B> static constexpr bool eval(A&& a, B&& b) { return std::forward<A>(a) != std::forward<B>(b); }
  template <std::size_t I, class ANode, class BNode>
  static auto d(const ANode&, const BNode&) {
    using T = value_type_of_t<ANode>;
    return lit(static_cast<T>(0));
  }
};
struct NotOp {
  static constexpr std::size_t arity = 1;
  template <class A> static constexpr bool eval(A&& a) { return !static_cast<bool>(std::forward<A>(a)); }
  template <std::size_t I, class X>
  static auto d(const X&) {
    using T = value_type_of_t<X>;
    return lit(static_cast<T>(0));
  }
};

struct IfOp {
  static constexpr std::size_t arity = 3;
  template <class C, class A, class B>
  static constexpr auto eval(C&& c, A&& a, B&& b)
  -> decltype((c ? std::forward<A>(a) : std::forward<B>(b))) { return c ? std::forward<A>(a) : std::forward<B>(b); }
  template <std::size_t I, class CNode, class TNode, class ENode>
  static auto d(const CNode& c, const TNode& t, const ENode& e) {
    // Treat condition as non-differentiable selector; mirror structure
    return Apply<IfOp,
                 CNode,
                 decltype(diff(t, std::integral_constant<std::size_t,I>{})),
                 decltype(diff(e, std::integral_constant<std::size_t,I>{}))>
           ( c,
             diff(t, std::integral_constant<std::size_t,I>{}),
             diff(e, std::integral_constant<std::size_t,I>{}) );
  }
};
struct SelectOp {
  static constexpr std::size_t arity = 3;
  template <class M, class A, class B>
  static constexpr auto eval(M&& m, A&& a, B&& b)
  -> decltype((m ? std::forward<A>(a) : std::forward<B>(b))) { return m ? std::forward<A>(a) : std::forward<B>(b); }
  template <std::size_t I, class MNode, class TNode, class ENode>
  static auto d(const MNode& m, const TNode& t, const ENode& e) {
    // Mask is non-diff; elementwise select on derivatives
    return Apply<SelectOp,
                 MNode,
                 decltype(diff(t, std::integral_constant<std::size_t,I>{})),
                 decltype(diff(e, std::integral_constant<std::size_t,I>{}))>
           ( m,
             diff(t, std::integral_constant<std::size_t,I>{}),
             diff(e, std::integral_constant<std::size_t,I>{}) );
  }
};

// ----------------
// Loop placeholders
// ----------------
// Iteration index (0-based) – only meaningful inside loops; eval() returns 0 in scalar eval
struct IterOp {
  static constexpr std::size_t arity = 0;
  static constexpr std::size_t returns_index = 1;
  template <class... Dummy>
  static constexpr std::size_t eval(Dummy&&...) { return 0u; }
  template <std::size_t I>
  static auto d() { return lit(0.0); }
};

// Loop-carried state read by index – only meaningful inside loops; eval() returns 0 in scalar eval
template <std::size_t SIndex>
struct StateOp {
  static constexpr std::size_t arity = 0;
  static constexpr std::size_t state_index = SIndex;
  template <class... Dummy>
  static constexpr double eval(Dummy&&...) { return 0.0; }
  template <std::size_t I>
  static auto d() { return lit(0.0); }
};

// General ForN loop over K carried states. Children: [N, inits..., nexts...]
template <std::size_t K>
struct LoopForOp {
  static constexpr std::size_t arity = 1 + 2*K;
  template <class... Args>
  static constexpr double eval(Args&&...) { return 0.0; }
  template <std::size_t I, class... Args>
  static auto d(const Args&...) { return lit(0.0); }
};

// Extract J-th output of a loop
template <std::size_t J>
struct LoopOutOp {
  static constexpr std::size_t arity = 1;
  template <class A>
  static constexpr double eval(const A&) { return 0.0; }
  template <std::size_t I, class A>
  static auto d(const A&) { return lit(0.0); }
};
#endif
struct AddOp {
  static constexpr std::size_t arity = 2;
  template <class A, class B> static constexpr auto eval(A&& a, B&& b)
  -> decltype(std::forward<A>(a) + std::forward<B>(b)) { return std::forward<A>(a) + std::forward<B>(b); }
  template <std::size_t I, class ANode, class BNode>
  static auto d(const ANode& a, const BNode& b);
};
struct SubOp {
  static constexpr std::size_t arity = 2;
  template <class A, class B> static constexpr auto eval(A&& a, B&& b)
  -> decltype(std::forward<A>(a) - std::forward<B>(b)) { return std::forward<A>(a) - std::forward<B>(b); }
  template <std::size_t I, class ANode, class BNode>
  static auto d(const ANode& a, const BNode& b);
};
struct MulOp {
  static constexpr std::size_t arity = 2;
  template <class A, class B> static constexpr auto eval(A&& a, B&& b)
  -> decltype(std::forward<A>(a) * std::forward<B>(b)) { return std::forward<A>(a) * std::forward<B>(b); }
  template <std::size_t I, class ANode, class BNode>
  static auto d(const ANode& a, const BNode& b);
};
struct DivOp {
  static constexpr std::size_t arity = 2;
  template <class A, class B> static constexpr auto eval(A&& a, B&& b)
  -> decltype(std::forward<A>(a) / std::forward<B>(b)) { return std::forward<A>(a) / std::forward<B>(b); }
  template <std::size_t I, class ANode, class BNode>
  static auto d(const ANode& a, const BNode& b);
};
struct PowOp {
  static constexpr std::size_t arity = 2;
  template <class A, class B> static constexpr auto eval(A&& a, B&& b)
  -> decltype(std::pow(std::forward<A>(a), std::forward<B>(b))) { using std::pow; return pow(std::forward<A>(a), std::forward<B>(b)); }
  template <std::size_t I, class ANode, class BNode>
  static auto d(const ANode& a, const BNode& b);
};
struct NegOp {
  static constexpr std::size_t arity = 1;
  template <class A> static constexpr auto eval(A&& a)
  -> decltype(-std::forward<A>(a)) { return -std::forward<A>(a); }
  template <std::size_t I, class X>
  static auto d(const X& x);
};
struct SinOp {
  static constexpr std::size_t arity = 1;
  template <class A> static constexpr auto eval(A&& a)
  -> decltype(sin(std::forward<A>(a))) { using std::sin; return sin(std::forward<A>(a)); }
  template <std::size_t I, class X>
  static auto d(const X& x);
};
struct CosOp {
  static constexpr std::size_t arity = 1;
  template <class A> static constexpr auto eval(A&& a)
  -> decltype(cos(std::forward<A>(a))) { using std::cos; return cos(std::forward<A>(a)); }
  template <std::size_t I, class X>
  static auto d(const X& x);
};
struct ExpOp {
  static constexpr std::size_t arity = 1;
  template <class A> static constexpr auto eval(A&& a)
  -> decltype(exp(std::forward<A>(a))) { using std::exp; return exp(std::forward<A>(a)); }
  template <std::size_t I, class X>
  static auto d(const X& x);
};
struct LogOp {
  static constexpr std::size_t arity = 1;
  template <class A> static constexpr auto eval(A&& a)
  -> decltype(log(std::forward<A>(a))) { using std::log; return log(std::forward<A>(a)); }
  template <std::size_t I, class X>
  static auto d(const X& x);
};
struct SqrtOp {
  static constexpr std::size_t arity = 1;
  template <class A> static constexpr auto eval(A&& a)
  -> decltype(sqrt(std::forward<A>(a))) { using std::sqrt; return sqrt(std::forward<A>(a)); }
  template <std::size_t I, class X>
  static auto d(const X& x);
};
struct TanhOp {
  static constexpr std::size_t arity = 1;
  template <class A> static constexpr auto eval(A&& a)
  -> decltype(tanh(std::forward<A>(a))) { using std::tanh; return tanh(std::forward<A>(a)); }
  template <std::size_t I, class X>
  static auto d(const X& x);
};

// Operator sugar
// Binary operators (only when at least one side is an ET node)
template <class L, class R,
          std::enable_if_t<is_node_t<L>::value || is_node_t<R>::value, int> = 0>
constexpr auto operator+(L l, R r) { return Apply<AddOp, std::decay_t<L>, std::decay_t<R>>(std::move(l), std::move(r)); }

template <class L, class R,
          std::enable_if_t<is_node_t<L>::value || is_node_t<R>::value, int> = 0>
constexpr auto operator-(L l, R r) { return Apply<SubOp, std::decay_t<L>, std::decay_t<R>>(std::move(l), std::move(r)); }

template <class L, class R,
          std::enable_if_t<is_node_t<L>::value || is_node_t<R>::value, int> = 0>
constexpr auto operator*(L l, R r) { return Apply<MulOp, std::decay_t<L>, std::decay_t<R>>(std::move(l), std::move(r)); }

template <class L, class R,
          std::enable_if_t<is_node_t<L>::value || is_node_t<R>::value, int> = 0>
constexpr auto operator/(L l, R r) { return Apply<DivOp, std::decay_t<L>, std::decay_t<R>>(std::move(l), std::move(r)); }

// Unary minus (only for ET nodes)
template <class A,
          std::enable_if_t<is_node_t<A>::value, int> = 0>
constexpr auto operator-(A a) { return Apply<NegOp, std::decay_t<A>>(std::move(a)); }

// ET math wrappers (only for ET nodes) — avoids shadowing std::sin/cos for scalars
template <class A, std::enable_if_t<is_node_t<A>::value, int> = 0>
constexpr auto sin(A a) { return Apply<SinOp, std::decay_t<A>>(std::move(a)); }

template <class A, std::enable_if_t<is_node_t<A>::value, int> = 0>
constexpr auto cos(A a) { return Apply<CosOp, std::decay_t<A>>(std::move(a)); }

template <class A, std::enable_if_t<is_node_t<A>::value, int> = 0>
constexpr auto exp(A a) { return Apply<ExpOp, std::decay_t<A>>(std::move(a)); }

template <class A, std::enable_if_t<is_node_t<A>::value, int> = 0>
constexpr auto log(A a) { return Apply<LogOp, std::decay_t<A>>(std::move(a)); }

template <class A, std::enable_if_t<is_node_t<A>::value, int> = 0>
constexpr auto sqrt(A a) { return Apply<SqrtOp, std::decay_t<A>>(std::move(a)); }

template <class A, std::enable_if_t<is_node_t<A>::value, int> = 0>
constexpr auto tanh(A a) { return Apply<TanhOp, std::decay_t<A>>(std::move(a)); }

// Power wrapper (only when at least one side is an ET node)
template <class L, class R, std::enable_if_t<is_node_t<L>::value || is_node_t<R>::value, int> = 0>
constexpr auto pow(L l, R r) { return Apply<PowOp, std::decay_t<L>, std::decay_t<R>>(std::move(l), std::move(r)); }

#ifdef ET_ENABLE_CONTROL_FLOW
// Comparison operators (when at least one side is a node)
template <class L, class R, std::enable_if_t<is_node_t<L>::value || is_node_t<R>::value, int> = 0>
constexpr auto operator<(L l, R r) { return Apply<LtOp, std::decay_t<L>, std::decay_t<R>>(std::move(l), std::move(r)); }
template <class L, class R, std::enable_if_t<is_node_t<L>::value || is_node_t<R>::value, int> = 0>
constexpr auto operator<=(L l, R r) { return Apply<LeOp, std::decay_t<L>, std::decay_t<R>>(std::move(l), std::move(r)); }
template <class L, class R, std::enable_if_t<is_node_t<L>::value || is_node_t<R>::value, int> = 0>
constexpr auto operator>(L l, R r) { return Apply<GtOp, std::decay_t<L>, std::decay_t<R>>(std::move(l), std::move(r)); }
template <class L, class R, std::enable_if_t<is_node_t<L>::value || is_node_t<R>::value, int> = 0>
constexpr auto operator>=(L l, R r) { return Apply<GeOp, std::decay_t<L>, std::decay_t<R>>(std::move(l), std::move(r)); }
template <class L, class R, std::enable_if_t<is_node_t<L>::value || is_node_t<R>::value, int> = 0>
constexpr auto operator==(L l, R r) { return Apply<EqOp, std::decay_t<L>, std::decay_t<R>>(std::move(l), std::move(r)); }
template <class L, class R, std::enable_if_t<is_node_t<L>::value || is_node_t<R>::value, int> = 0>
constexpr auto operator!=(L l, R r) { return Apply<NeOp, std::decay_t<L>, std::decay_t<R>>(std::move(l), std::move(r)); }

// Logical not for node
template <class A, std::enable_if_t<is_node_t<A>::value, int> = 0>
constexpr auto operator!(A a) { return Apply<NotOp, std::decay_t<A>>(std::move(a)); }

// If/Select wrappers (only when at least one side is a node)
template <class C, class T, class E,
          std::enable_if_t<is_node_t<C>::value || is_node_t<T>::value || is_node_t<E>::value, int> = 0>
constexpr auto If(C c, T t, E e) { return Apply<IfOp, std::decay_t<C>, std::decay_t<T>, std::decay_t<E>>(std::move(c), std::move(t), std::move(e)); }

template <class M, class T, class E,
          std::enable_if_t<is_node_t<M>::value || is_node_t<T>::value || is_node_t<E>::value, int> = 0>
constexpr auto Select(M m, T t, E e) { return Apply<SelectOp, std::decay_t<M>, std::decay_t<T>, std::decay_t<E>>(std::move(m), std::move(t), std::move(e)); }

// Loop sugar
inline constexpr auto Iter() { return Apply<IterOp>{}; }
template <std::size_t I>
inline constexpr auto State() { return Apply<StateOp<I>>{}; }

// Loop core (K=1) and output selector convenience
template <class N, class... InitsAndNexts,
          std::enable_if_t<(sizeof...(InitsAndNexts) >= 2) && (is_node_t<N>::value || (std::disjunction<is_node<std::decay_t<InitsAndNexts>>...>::value)), int> = 0>
constexpr auto LoopForCore1(N n, InitsAndNexts... rest) {
  return Apply<LoopForOp<1>, std::decay_t<N>, std::decay_t<InitsAndNexts>...>(std::move(n), std::move(rest)...);
}

// ForN with one carried state: returns output 0 of core
template <class N, class Init, class Next,
          std::enable_if_t<is_node_t<N>::value || is_node_t<Init>::value || is_node_t<Next>::value, int> = 0>
constexpr auto LoopForN(N n, Init init, Next next) {
  auto core = LoopForCore1(std::move(n), std::move(init), std::move(next));
  return Apply<LoopOutOp<0>, decltype(core)>(std::move(core));
}
#endif

//===========================
// Automatic differentiation
//===========================

// Forward decl of diff for Apply
template <class Op, class... Ch, std::size_t I>
constexpr auto diff(const Apply<Op,Ch...>& node, std::integral_constant<std::size_t,I>);

// Base cases
template <class T, std::size_t J, std::size_t I>
constexpr auto diff(const Var<T,J>&, std::integral_constant<std::size_t,I>) {
  if constexpr (I == J) return lit(static_cast<T>(1));
  else                  return lit(static_cast<T>(0));
}
template <class T, std::size_t I>
constexpr auto diff(const Const<T>&, std::integral_constant<std::size_t,I>) {
  return lit(static_cast<T>(0));
}

// Generic Apply: call Op::d<I>(children...)
template <class Op, class... Ch, std::size_t I>
constexpr auto diff(const Apply<Op,Ch...>& node, std::integral_constant<std::size_t,I>) {
  return std::apply([&](const auto&... c){ return Op::template d<I>(c...); }, node.ch);
}

// Op derivative definitions (after diff exists)
template <std::size_t I, class ANode, class BNode>
inline auto AddOp::d(const ANode& a, const BNode& b) {
  return Apply<AddOp,
               decltype(diff(a, std::integral_constant<std::size_t,I>{})),
               decltype(diff(b, std::integral_constant<std::size_t,I>{}))>
         ( diff(a, std::integral_constant<std::size_t,I>{}),
           diff(b, std::integral_constant<std::size_t,I>{}) );
}
template <std::size_t I, class ANode, class BNode>
inline auto SubOp::d(const ANode& a, const BNode& b) {
  return Apply<SubOp,
               decltype(diff(a, std::integral_constant<std::size_t,I>{})),
               decltype(diff(b, std::integral_constant<std::size_t,I>{}))>
         ( diff(a, std::integral_constant<std::size_t,I>{}),
           diff(b, std::integral_constant<std::size_t,I>{}) );
}
template <std::size_t I, class ANode, class BNode>
inline auto MulOp::d(const ANode& a, const BNode& b) {
  auto da = diff(a, std::integral_constant<std::size_t,I>{});
  auto db = diff(b, std::integral_constant<std::size_t,I>{});
  return Apply<AddOp,
               Apply<MulOp, decltype(da), BNode>,
               Apply<MulOp, ANode, decltype(db)>>
         ( Apply<MulOp, decltype(da), BNode>(std::move(da), b),
           Apply<MulOp, ANode, decltype(db)>(a, std::move(db)) );
}
template <std::size_t I, class ANode, class BNode>
inline auto DivOp::d(const ANode& a, const BNode& b) {
  auto da = diff(a, std::integral_constant<std::size_t,I>{});
  auto db = diff(b, std::integral_constant<std::size_t,I>{});
  auto num = Apply<SubOp,
                   Apply<MulOp, decltype(da), BNode>,
                   Apply<MulOp, ANode, decltype(db)>>
             ( Apply<MulOp, decltype(da), BNode>(std::move(da), b),
               Apply<MulOp, ANode, decltype(db)>(a, std::move(db)) );
  auto den = Apply<MulOp, BNode, BNode>(b, b);
  return Apply<DivOp, decltype(num), decltype(den)>(std::move(num), std::move(den));
}
template <std::size_t I, class ANode, class BNode>
inline auto PowOp::d(const ANode& a, const BNode& b) {
  // d(a^b) = a^b * ( db*log(a) + b * da/a )
  auto da = diff(a, std::integral_constant<std::size_t,I>{});
  auto db = diff(b, std::integral_constant<std::size_t,I>{});
  auto apowb = Apply<PowOp, ANode, BNode>(a, b);
  auto term1 = Apply<MulOp, decltype(db), Apply<LogOp, ANode>>( std::move(db), Apply<LogOp, ANode>(a) );
  auto term2 = Apply<MulOp, BNode, Apply<DivOp, decltype(da), ANode>>( b, Apply<DivOp, decltype(da), ANode>( std::move(da), a ) );
  auto sum   = Apply<AddOp, decltype(term1), decltype(term2)>( std::move(term1), std::move(term2) );
  return Apply<MulOp, decltype(apowb), decltype(sum)>( std::move(apowb), std::move(sum) );
}
template <std::size_t I, class X>
inline auto NegOp::d(const X& x) {
  return Apply<NegOp, decltype(diff(x, std::integral_constant<std::size_t,I>{}))>
         ( diff(x, std::integral_constant<std::size_t,I>{}) );
}



template <std::size_t I, class X>
inline auto SinOp::d(const X& x) {
  // d/dx sin(x) = cos(x) * dx
  auto dx = diff(x, std::integral_constant<std::size_t,I>{});
  return Apply<MulOp, Apply<CosOp, X>, decltype(dx)>(
      Apply<CosOp, X>(x), std::move(dx));
}

template <std::size_t I, class X>
inline auto CosOp::d(const X& x) {
  // d/dx cos(x) = -sin(x) * dx
  auto dx = diff(x, std::integral_constant<std::size_t,I>{});
  return Apply<MulOp,
               Apply<NegOp, Apply<SinOp, X>>,
               decltype(dx)>(
      Apply<NegOp, Apply<SinOp, X>>( Apply<SinOp, X>(x) ),
      std::move(dx));
}

template <std::size_t I, class X>
inline auto ExpOp::d(const X& x) {
  // d/dx exp(x) = exp(x) * dx
  auto dx = diff(x, std::integral_constant<std::size_t,I>{});
  return Apply<MulOp, Apply<ExpOp, X>, decltype(dx)>(
      Apply<ExpOp, X>(x), std::move(dx));
}

template <std::size_t I, class X>
inline auto LogOp::d(const X& x) {
  // d/dx log(x) = dx / x
  auto dx = diff(x, std::integral_constant<std::size_t,I>{});
  return Apply<DivOp, decltype(dx), X>(
      std::move(dx), x);
}

template <std::size_t I, class X>
inline auto SqrtOp::d(const X& x) {
  // d/dx sqrt(x) = dx / (2 * sqrt(x))
  auto dx = diff(x, std::integral_constant<std::size_t,I>{});
  using TX = value_type_of_t<X>;
  auto two = lit(TX(2));
  return Apply<DivOp,
               decltype(dx),
               Apply<MulOp, decltype(two), Apply<SqrtOp, X>>>(
      std::move(dx),
      Apply<MulOp, decltype(two), Apply<SqrtOp, X>>(
          two, Apply<SqrtOp, X>(x)));
}

template <std::size_t I, class X>
inline auto TanhOp::d(const X& x) {
  // d/dx tanh(x) = (1 - tanh(x)^2) * dx
  auto dx = diff(x, std::integral_constant<std::size_t,I>{});
  using TX = value_type_of_t<X>;
  auto one = lit(TX(1));
  auto t = Apply<TanhOp, X>(x);
  auto t2 = Apply<MulOp, decltype(t), decltype(t)>(t, t);
  auto factor = Apply<SubOp, decltype(one), decltype(t2)>(one, t2);
  return Apply<MulOp, decltype(factor), decltype(dx)>(
      std::move(factor), std::move(dx));
}

//===========================
// Evaluation helper
//===========================
template <class Expr, class... Args>
constexpr auto evaluate(const Expr& e, Args&&... args) {
  return e(std::forward<Args>(args)...);
}

//===========================
// Backend visitor interface
//===========================
template <class Backend, class Expr>
auto compile(const Expr& e, Backend& b) -> typename Backend::result_type;

// Specializations:
template <class Backend, class T, std::size_t I>
auto compile(const Var<T,I>&, Backend& b) -> typename Backend::result_type {
  return b.template emitVar<T>(I);
}
template <class Backend, class T>
auto compile(const Const<T>& c, Backend& b) -> typename Backend::result_type {
  return b.template emitConst<T>(c);
}
template <class Backend, class Op, class... Ch>
auto compile(const Apply<Op,Ch...>& a, Backend& b) -> typename Backend::result_type {
  return std::apply([&](const auto&... c){
    return b.emitApply(Op{}, compile<Backend>(c, b)...);
  }, a.ch);
}

//===========================
// Vars factory helpers
//===========================
template <class T, std::size_t... Is>
constexpr auto Vars_impl(std::index_sequence<Is...>) {
  return std::make_tuple(Var<T, Is>{}...);
}
template <class T, std::size_t N>
constexpr auto Vars() {
  return Vars_impl<T>(std::make_index_sequence<N>{});
}

//===========================
// diff(expr, var) sugar + grad()
//===========================
template <class V> struct var_index_of;                    // primary
template <class T, std::size_t I>
struct var_index_of<Var<T,I>> : std::integral_constant<std::size_t, I> {};

template <class V>
inline constexpr std::size_t var_index_of_v = var_index_of<std::decay_t<V>>::value;

template <std::size_t I>
struct var_index_of<std::integral_constant<std::size_t, I>>
    : std::integral_constant<std::size_t, I> {};

template <class Expr, class V>
constexpr auto diff(const Expr& e, const V&) {
  return diff(e, std::integral_constant<std::size_t, var_index_of_v<V>>{});
}

template <class Expr, class... Vs>
constexpr auto grad(const Expr& e, const Vs&... vs) {
  return std::make_tuple(diff(e, vs)...);
}
template <class Expr, class Tuple, std::size_t... Is>
constexpr auto grad_tuple_impl(const Expr& e, const Tuple& t, std::index_sequence<Is...>) {
  return std::make_tuple(diff(e, std::get<Is>(t))...);
}
template <class Expr, class Tuple>
constexpr auto grad(const Expr& e, const Tuple& t) {
  constexpr std::size_t N = std::tuple_size<std::decay_t<Tuple>>::value;
  return grad_tuple_impl(e, t, std::make_index_sequence<N>{});
}

} // namespace et
