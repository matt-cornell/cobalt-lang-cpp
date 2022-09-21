#ifndef COBALT_TOKENIZER_HPP
#define COBALT_TOKENIZER_HPP
#include "cobalt/support/location.hpp"
#include "cobalt/support/functions.hpp"
#include "cobalt/support/token.hpp"
#include "flags.hpp"
#include <vector>
namespace cobalt {
  using macro = rc_function<std::string(std::string_view, bound_handler)>;
  using macro_map = std::unordered_map<sstring, macro>;
  extern macro_map default_macros;
  std::vector<token> tokenize(std::string_view code, location loc, flags_t flags = default_flags, macro_map macros = default_macros);
  inline std::vector<token> tokenize(std::string_view code, sstring file, flags_t flags = default_flags, macro_map macros = default_macros) {return tokenize(code, {file, 1, 1}, flags, macros);}
}
#endif