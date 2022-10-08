#ifndef COBALT_TESTS_PARSER_HPP
#define COBALT_TESTS_PARSER_HPP
#include "cobalt/tokenizer.hpp"
#include "cobalt/parser.hpp"
#include "cobalt/ast.hpp"
namespace tests::parser {
  using namespace cobalt;
  flags_t flags = default_flags;
#define DEF_LOC(LINE, COL) location{sstring::get("<test>"), LINE, COL}
  std::vector<AST> make_ast_vector(std::initializer_list<AST> args) { // curly brace initializer doesn't work for vectors of noncopyable types
    std::vector<AST> out;
    out.reserve(args.size());
    for (auto& arg : args) out.push_back((AST&&)arg);
    return out;
  }
  bool modules() {
    AST expected = AST::create<ast::top_level_ast>(
      DEF_LOC(1, 1),
      make_ast_vector({new ast::module_ast(
        DEF_LOC(1, 1),
        "x",
        make_ast_vector({
          new ast::import_ast(
            DEF_LOC(2, 3),
            "y"
          )
        })
      )})
    );
    quiet_handler_t h;
    flags.onerror = h;
    auto toks = tokenize(R"(module x {
  import y;
})", sstring::get("<test>"), flags);
    auto ast = parse({toks.begin(), toks.end()}, flags);
    if (h.errors || h.warnings) return false;
    return ast == expected;
  }
}
#endif
