#pragma once
#include <unordered_map>
#include <string>
#include "et/ast.hpp"
#include "et/normalize.hpp"
#include "et/tape_backend.hpp"
#include "et/compile_cse_ast.hpp"

namespace et {

// Provide non-ast-suffixed aliases
template <class Backend>
auto compile_cse(const Expr& e, Backend& b) -> typename Backend::result_type {
  return compile_cse_ast(e, b);
}

inline int compile_cse(const Expr& e, TapeBackend& b) {
  return compile_cse_ast(e, b);
}

} // namespace et

