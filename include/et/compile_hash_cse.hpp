#pragma once
#include "et/ast.hpp"
#include "et/normalize.hpp"
#include "et/tape_backend.hpp"
#include "et/compile_hash_cse_ast.hpp"

namespace et {

inline int compile_hash_cse(const Expr& e, TapeBackend& b) {
  return compile_hash_cse_ast(e, b);
}

} // namespace et

