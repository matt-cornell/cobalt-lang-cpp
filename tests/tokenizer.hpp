#ifndef COBALT_TESTS_TOKENIZER_HPP
#define COBALT_TESTS_TOKENIZER_HPP
#include "cobalt/tokenizer.hpp"
namespace tests::tokenizer {
  using namespace cobalt;
  using namespace std::literals;
  flags_t flags = default_flags;
  #define DEF_TOK(LINE, COL, STR) {sstring::get("<test>"), LINE, COL, STR}
  bool identifiers() {
    const std::vector<token> expected = {
      DEF_TOK(1, 1, "This"),
      DEF_TOK(1, 6, "is"),
      DEF_TOK(1, 9, "a"),
      DEF_TOK(1, 11, "set"),
      DEF_TOK(1, 15, "of"),
      DEF_TOK(1, 18, "identifiers"),
      DEF_TOK(2, 1, "Here"),
      DEF_TOK(2, 6, "is"),
      DEF_TOK(2, 9, "a"),
      DEF_TOK(2, 11, "second"),
      DEF_TOK(2, 18, "line")
    };
    quiet_handler_t h;
    flags.onerror = h;
    auto toks = tokenize("This is a set of identifiers\nHere is a second line", sstring::get("<test>"), flags);
    if (h.errors || h.warnings) return false;
    return toks == expected;
  }
  bool strings() {
    const static std::vector<token> expected = {
      DEF_TOK(1, 1, "Here"),
      DEF_TOK(1, 6, "is"),
      DEF_TOK(1, 9, "a"),
      DEF_TOK(1, 11, "character"),
      DEF_TOK(1, 21, "'c\0\0\0"s),
      DEF_TOK(2, 1, "Here"),
      DEF_TOK(2, 6, "is"),
      DEF_TOK(2, 9, "a"),
      DEF_TOK(2, 11, "\"string")
    };
    quiet_handler_t h;
    flags.onerror = h;
    auto toks = tokenize(R"(Here is a character 'c'
Here is a "string")", sstring::get("<test>"), flags);
    if (h.errors || h.warnings) return false;
    return toks == expected;
  }
  bool macros() {
    const std::vector<token> expected = {
      DEF_TOK(1, 1, "\"test")
    };
    quiet_handler_t h;
    flags.onerror = h;
    auto toks = tokenize(R"(@str(test))", sstring::get("<test>"), flags);
    if (h.errors || h.warnings) return false;
    return toks == expected;
  }
}
#endif
