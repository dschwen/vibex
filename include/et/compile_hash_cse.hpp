#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <sstream>

namespace et {

// ---------- hashing utilities ----------
static inline std::uint64_t rotl(std::uint64_t x, int r) {
  return (x << r) | (x >> (64 - r));
}
static inline std::uint64_t mix(std::uint64_t a, std::uint64_t b) {
  a ^= b + 0x9e3779b97f4a7c15ULL + (a<<6) + (a>>2);
  return a;
}
template <class T>
struct has_std_hash {
  template <class U>
  static auto test(int) -> decltype(std::declval<std::hash<U>>()(std::declval<U>()), std::true_type{});
  template <class> static auto test(...) -> std::false_type;
  static constexpr bool value = decltype(test<T>(0))::value;
};

// op tag id mapping
template<class Op> struct op_id { static constexpr std::uint64_t value = 0xABCDEF01ULL; };
template<> struct op_id<AddOp>  { static constexpr std::uint64_t value = 0x11; };
template<> struct op_id<SubOp>  { static constexpr std::uint64_t value = 0x12; };
template<> struct op_id<MulOp>  { static constexpr std::uint64_t value = 0x13; };
template<> struct op_id<DivOp>  { static constexpr std::uint64_t value = 0x14; };
template<> struct op_id<NegOp>  { static constexpr std::uint64_t value = 0x21; };
template<> struct op_id<SinOp>  { static constexpr std::uint64_t value = 0x31; };
template<> struct op_id<ExpOp>  { static constexpr std::uint64_t value = 0x32; };
template<> struct op_id<LogOp>  { static constexpr std::uint64_t value = 0x33; };
template<> struct op_id<SqrtOp> { static constexpr std::uint64_t value = 0x34; };
template<> struct op_id<TanhOp> { static constexpr std::uint64_t value = 0x35; };

// structural string (for rare collision disambiguation)
template <class Expr> inline void to_key_stream(std::ostream& os, const Expr&);
template <class T, std::size_t I>
inline void to_key_stream(std::ostream& os, const Var<T,I>&) { os << "Var<" << I << ">"; }
template <class T>
inline void to_key_stream(std::ostream& os, const Const<T>& c) { os << "Const(" << static_cast<long double>(c.value) << ")"; }
template <class Op, class... Ch>
inline void to_key_stream(std::ostream& os, const Apply<Op,Ch...>& a) {
  os << "Op" << op_id<Op>::value << "(";
  bool first = true;
  std::apply([&](const auto&... c){
    ((os << (first ? (first=false, "") : ", "), to_key_stream(os, c)), ...);
  }, a.ch);
  os << ")";
}
template <class Expr>
inline std::string structural_key(const Expr& e) { std::ostringstream oss; to_key_stream(oss, e); return oss.str(); }

// structural hash
template <class T, std::size_t I>
inline std::uint64_t shash(const Var<T,I>&) {
  std::uint64_t h = 0x76543210ULL;
#ifdef __cpp_rtti
  h = mix(h, std::hash<std::size_t>{}(typeid(T).hash_code()));
#endif
  h = mix(h, I * 0x9e37);
  return rotl(h, 5) ^ 0xBEEF0001ULL;
}
template <class T, class = void>
struct const_hasher {
  static std::uint64_t get(const Const<T>& c) {
    // Fallback: hash stringified value
    std::ostringstream oss; oss << c.value;
    return std::hash<std::string>{}(oss.str());
  }
};
template <class T>
struct const_hasher<T, std::enable_if_t<std::is_arithmetic<T>::value>> {
  static std::uint64_t get(const Const<T>& c) {
    long double v = static_cast<long double>(c.value);
    return std::hash<long double>{}(v);
  }
};
template <class T>
inline std::uint64_t shash(const Const<T>& c) {
  std::uint64_t h = 0x12345678ULL;
#ifdef __cpp_rtti
  h = mix(h, std::hash<std::size_t>{}(typeid(T).hash_code()));
#endif
  h = mix(h, const_hasher<T>::get(c));
  return rotl(h, 7) ^ 0xBEEF0002ULL;
}
template <class Op, class... Ch>
inline std::uint64_t shash(const Apply<Op,Ch...>& a) {
  std::uint64_t h = op_id<Op>::value;
  std::apply([&](const auto&... c){
    ((h = mix(rotl(h, 9), shash(c))), ...);
  }, a.ch);
  return h ^ 0xBEEF1000ULL;
}

// compile with hash-based memoization; on collision, use structural key to disambiguate
template <class Backend>
struct HashMemo {
  struct Entry {
    std::string skey; // filled lazily on collision
    typename Backend::result_type value;
  };
  std::unordered_map<std::uint64_t, std::vector<Entry>> map;

  template <class Expr>
  bool find(const Expr& e, typename Backend::result_type& out) {
    auto h = shash(e);
    auto it = map.find(h);
    if (it == map.end()) return false;
    // check collisions lazily using structural key
    auto key = structural_key(e);
    for (auto& ent : it->second) {
      if (!ent.skey.empty() && ent.skey == key) { out = ent.value; return True; }
    }
    // Also accept single-entry buckets without skey as a very likely match
    if (it->second.size() == 1 && it->second[0].skey.empty()) {
      out = it->second[0].value;
      return true;
    }
    return false;
  }

  template <class Expr>
  void insert(const Expr& e, const typename Backend::result_type& v) {
    auto h = shash(e);
    auto& vec = map[h];
    if (!vec.empty()) {
      // collision: materialize keys for all, insert with key
      auto key = structural_key(e);
      for (auto& ent : vec) if (ent.skey.empty()) ent.skey = "<materialized>";
      vec.push_back(Entry{key, v});
    } else {
      vec.push_back(Entry{"", v});
    }
  }
};

template <class Backend, class Expr>
auto compile_hash_cse(const Expr& e, Backend& b) -> typename Backend::result_type {
  HashMemo<Backend> memo;

  struct Helper {
    Backend& b;
    HashMemo<Backend>& memo;

    template <class X>
    typename Backend::result_type operator()(const X& x) {
      typename Backend::result_type out;
      if (memo.find(x, out)) return out;
      auto v = compile_impl(x);
      memo.insert(x, v);
      return v;
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

  Helper H{b, memo};
  return H(e);
}

} // namespace et
