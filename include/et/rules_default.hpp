#pragma once
#include <vector>
#include "et/pattern.hpp"
#include "et/rewrite.hpp"

namespace et {

inline std::vector<Rule> default_rules() {
  using namespace et::pat;
  std::vector<Rule> rs;

  // sin(p)^2 + cos(p)^2 -> 1
  rs.push_back(Rule{
    /*lhs=*/ (sin(P(1))*sin(P(1))) + (cos(P(1))*cos(P(1))),
    /*rhs=*/ C(1.0),
    /*guard=*/ {},
    /*name=*/ "pythagorean", /*priority=*/ 10
  });

  // log(exp(p)) -> p
  rs.push_back(Rule{
    /*lhs=*/ log(exp(P(1))),
    /*rhs=*/ P(1),
    /*guard=*/ {},
    /*name=*/ "log_exp", /*priority=*/ 5
  });

  // exp(log(p)) -> p (no guard here; users may prefer to guard by domain)
  rs.push_back(Rule{
    /*lhs=*/ exp(log(P(1))),
    /*rhs=*/ P(1),
    /*guard=*/ {},
    /*name=*/ "exp_log", /*priority=*/ 5
  });

  return rs;
}

} // namespace et

