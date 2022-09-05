#ifndef COBALT_TOKENIZER_HPP
#define COBALT_TOKENIZER_HPP
#include "cobalt/support/location.hpp"
#include "cobalt/support/functions.hpp"
#include "cobalt/support/token.hpp"
#include "flags.hpp"
#include <vector>
namespace cobalt {
  using preprocessor_directive = rc_function<std::string(std::string_view, bound_handler)>;
  using pp_map = std::unordered_map<sstring::string, preprocessor_directive>;
  extern pp_map default_directives;
  std::vector<token> tokenize(std::string_view code, location loc, flags_t flags = default_flags, pp_map const& directives = default_directives);
  inline std::vector<token> tokenize(std::string_view code, sstring::string file, flags_t flags = default_flags, pp_map const& directives = default_directives) {return tokenize(code, {file, 1, 1}, flags, directives);}
}
#endif