#pragma once
#include <type_traits>

namespace et {

template <class T> struct is_const_node : std::false_type {};
template <class T> struct is_const_node<Const<T>> : std::true_type {};
template <class X> inline constexpr bool is_const_node_v = is_const_node<X>::value;

template <class C> struct const_value;
template <class T> struct const_value<Const<T>> { static constexpr const T& get(const Const<T>& c){ return c.value; } };

template <class T, class U>
constexpr auto make_same_const(U v) { return Const<T>{ static_cast<T>(v) }; }

template <class E> constexpr auto simplify(const E& e);

// base cases
template <class T, std::size_t I>
constexpr auto simplify(const Var<T,I>& v) { return v; }
template <class T>
constexpr auto simplify(const Const<T>& c) { return c; }

// unary
template <class A>
constexpr auto simplify(const Apply<NegOp, A>& node) {
  auto a = simplify(node.template child<0>());
  if constexpr (is_const_node_v<decltype(a)>) {
    using T = typename decltype(a)::value_type;
    return Const<T>{ -const_value<decltype(a)>::get(a) };
  } else {
    return Apply<NegOp, decltype(a)>(std::move(a));
  }
}
template <class A>
constexpr auto simplify(const Apply<SinOp, A>& node) {
  auto a = simplify(node.template child<0>());
  if constexpr (is_const_node_v<decltype(a)>) {
    using T = typename decltype(a)::value_type;
    using std::sin;
    return Const<T>{ sin(const_value<decltype(a)>::get(a)) };
  } else {
    return Apply<SinOp, decltype(a)>(std::move(a));
  }
}
template <class A>
constexpr auto simplify(const Apply<CosOp, A>& node) {
  auto a = simplify(node.template child<0>());
  if constexpr (is_const_node_v<decltype(a)>) {
    using T = typename decltype(a)::value_type;
    using std::cos;
    return Const<T>{ cos(const_value<decltype(a)>::get(a)) };
  } else {
    return Apply<CosOp, decltype(a)>(std::move(a));
  }
}
template <class A>
constexpr auto simplify(const Apply<ExpOp, A>& node) {
  auto a = simplify(node.template child<0>());
  if constexpr (is_const_node_v<decltype(a)>) {
    using T = typename decltype(a)::value_type;
    using std::exp;
    return Const<T>{ exp(const_value<decltype(a)>::get(a)) };
  } else {
    return Apply<ExpOp, decltype(a)>(std::move(a));
  }
}
template <class A>
constexpr auto simplify(const Apply<LogOp, A>& node) {
  auto a = simplify(node.template child<0>());
  if constexpr (is_const_node_v<decltype(a)>) {
    using T = typename decltype(a)::value_type;
    using std::log;
    return Const<T>{ log(const_value<decltype(a)>::get(a)) };
  } else {
    return Apply<LogOp, decltype(a)>(std::move(a));
  }
}
template <class A>
constexpr auto simplify(const Apply<SqrtOp, A>& node) {
  auto a = simplify(node.template child<0>());
  if constexpr (is_const_node_v<decltype(a)>) {
    using T = typename decltype(a)::value_type;
    using std::sqrt;
    return Const<T>{ sqrt(const_value<decltype(a)>::get(a)) };
  } else {
    return Apply<SqrtOp, decltype(a)>(std::move(a));
  }
}
template <class A>
constexpr auto simplify(const Apply<TanhOp, A>& node) {
  auto a = simplify(node.template child<0>());
  if constexpr (is_const_node_v<decltype(a)>) {
    using T = typename decltype(a)::value_type;
    using std::tanh;
    return Const<T>{ tanh(const_value<decltype(a)>::get(a)) };
  } else {
    return Apply<TanhOp, decltype(a)>(std::move(a));
  }
}

// binary helpers
template <class L, class R>
constexpr auto simplify_add(L&& l, R&& r) {
  auto ls = simplify(l); auto rs = simplify(r);
  if constexpr (is_const_node_v<decltype(ls)> && is_const_node_v<decltype(rs)>) {
    using TL = typename decltype(ls)::value_type;
    return Const<TL>{ const_value<decltype(ls)>::get(ls) + const_value<decltype(rs)>::get(rs) };
  } else if constexpr (is_const_node_v<decltype(ls)>) {
    using TL = typename decltype(ls)::value_type;
    if (const_value<decltype(ls)>::get(ls) == TL(0)) return rs;
  } else if constexpr (is_const_node_v<decltype(rs)>) {
    using TR = typename decltype(rs)::value_type;
    if (const_value<decltype(rs)>::get(rs) == TR(0)) return ls;
  }
  return Apply<AddOp, decltype(ls), decltype(rs)>(std::move(ls), std::move(rs));
}
template <class L, class R>
constexpr auto simplify_sub(L&& l, R&& r) {
  auto ls = simplify(l); auto rs = simplify(r);
  if constexpr (is_const_node_v<decltype(ls)> && is_const_node_v<decltype(rs)>) {
    using TL = typename decltype(ls)::value_type;
    return Const<TL>{ const_value<decltype(ls)>::get(ls) - const_value<decltype(rs)>::get(rs) };
  } else if constexpr (is_const_node_v<decltype(rs)>) {
    using TR = typename decltype(rs)::value_type;
    if (const_value<decltype(rs)>::get(rs) == TR(0)) return ls;
  }
  return Apply<SubOp, decltype(ls), decltype(rs)>(std::move(ls), std::move(rs));
}
template <class L, class R>
constexpr auto simplify_mul(L&& l, R&& r) {
  auto ls = simplify(l); auto rs = simplify(r);
  if constexpr (is_const_node_v<decltype(ls)> && is_const_node_v<decltype(rs)>) {
    using TL = typename decltype(ls)::value_type;
    return Const<TL>{ const_value<decltype(ls)>::get(ls) * const_value<decltype(rs)>::get(rs) };
  }
  if constexpr (is_const_node_v<decltype(ls)>) {
    using TL = typename decltype(ls)::value_type;
    if (const_value<decltype(ls)>::get(ls) == TL(0))
      return make_same_const<typename decltype(rs)::value_type>(0);
    if (const_value<decltype(ls)>::get(ls) == TL(1)) return rs;
  }
  if constexpr (is_const_node_v<decltype(rs)>) {
    using TR = typename decltype(rs)::value_type;
    if (const_value<decltype(rs)>::get(rs) == TR(0))
      return make_same_const<typename decltype(ls)::value_type>(0);
    if (const_value<decltype(rs)>::get(rs) == TR(1)) return ls;
  }
  return Apply<MulOp, decltype(ls), decltype(rs)>(std::move(ls), std::move(rs));
}
template <class L, class R>
constexpr auto simplify_div(L&& l, R&& r) {
  auto ls = simplify(l); auto rs = simplify(r);
  if constexpr (is_const_node_v<decltype(ls)> && is_const_node_v<decltype(rs)>) {
    using TL = typename decltype(ls)::value_type;
    return Const<TL>{ const_value<decltype(ls)>::get(ls) / const_value<decltype(rs)>::get(rs) };
  }
  if constexpr (is_const_node_v<decltype(rs)>) {
    using TR = typename decltype(rs)::value_type;
    if (const_value<decltype(rs)>::get(rs) == TR(1)) return ls;
  }
  if constexpr (is_const_node_v<decltype(ls)>) {
    using TL = typename decltype(ls)::value_type;
    if (const_value<decltype(ls)>::get(ls) == TL(0))
      return make_same_const<typename decltype(rs)::value_type>(0);
  }
  return Apply<DivOp, decltype(ls), decltype(rs)>(std::move(ls), std::move(rs));
}

// Apply dispatcher
template <class Op, class L, class R>
constexpr auto simplify(const Apply<Op, L, R>& node) {
  if constexpr (std::is_same<Op, AddOp>::value) return simplify_add(node.template child<0>(), node.template child<1>());
  if constexpr (std::is_same<Op, SubOp>::value) return simplify_sub(node.template child<0>(), node.template child<1>());
  if constexpr (std::is_same<Op, MulOp>::value) return simplify_mul(node.template child<0>(), node.template child<1>());
  if constexpr (std::is_same<Op, DivOp>::value) return simplify_div(node.template child<0>(), node.template child<1>());
  auto l = simplify(node.template child<0>());
  auto r = simplify(node.template child<1>());
  return Apply<Op, decltype(l), decltype(r)>(std::move(l), std::move(r));
}
template <class Op, class A>
constexpr auto simplify(const Apply<Op, A>& node) {
  auto a = simplify(node.template child<0>());
  return Apply<Op, decltype(a)>(std::move(a));
}

} // namespace et
