#ifndef COBALT_PARSER_HPP
#define COBALT_PARSER_HPP
#include "cobalt/support/token.hpp"
#include "cobalt/support/span.hpp"
#include "cobalt/flags.hpp"
namespace cobalt {
  class AST;
  AST parse(span<token> toks, flags_t flags);
}
#endif