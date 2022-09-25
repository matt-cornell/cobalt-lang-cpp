#include "cobalt/parser.hpp"
#include "cobalt/ast.hpp"
using namespace cobalt;
std::vector<std::string_view> bin_ops[] {
  {"=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "^^="},
  {"||"},
  {"&&"},
  {"==", "!="},
  {"<", "<=", ">", ">="},
  {"|"},
  {"&"},
  {"<<", ">>"},
  {"+", "-"},
  {"*", "/", "%"},
  {"^^"}
};
std::string_view pre_ops[] = {"+", "-", "*", "!", "~", "++", "--"};
std::string_view post_ops[] = {"?", "!"};
AST parse_expr(span<token> code, flags_t flags) {
  
}
AST cobalt::parse(span<token> code, flags_t flags) {
  AST ast = AST::create<ast::block_ast, std::vector<AST>&&>({});
  (void)code;
  (void)flags;
  return ast;
}