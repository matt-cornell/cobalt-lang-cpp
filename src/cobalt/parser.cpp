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
std::vector<std::string> parse_paths(span<token>::iterator& it, span<token>::iterator end, flags_t flags) {
  std::vector<std::string> paths = {""};
  auto pball = [&paths] (std::string_view sv) {for (auto& str : paths) str += sv;};
  uint8_t lwp = 2;
  while (++it != end) {
    std::string_view data = it->data;
    switch (data.front()) {
      case '{':
      case '}':
      case ',':
        flags.onerror(it->loc, "compound pattern matching is not currently supported", CRITICAL);
        break;
      case ';':
        return paths;
        break;
      case '*':
        if ((it + 1)->data == "*") {
          while (++it != end && it->data == "*");
          flags.onerror(it->loc, "recursive pattern matching is not currently supported", WARNING);
        }
        lwp = 0;
        pball("*");
        break;
      case '.':
        if (lwp == 1) flags.onerror(it->loc, "consecutive periods are not allowed in paths", ERROR);
        pball(".");
        lwp = 1;
        break;
      default:
        if (lwp != 0) pball(data);
        else {
          flags.onerror(it->loc, "imported identifier paths should be separated by a period or terminated by a semicolon", ERROR);
          return paths;
        }
        lwp = 0;
    }
  }
  return paths;
}
std::pair<std::vector<AST>, span<token>::iterator> parse_tl(span<token> code, flags_t flags) {
#define UNSUPPORTED(TYPE) {flags.onerror(it->loc, "top-level " TYPE " definitions are not currently supported", CRITICAL);}
  if (code.empty()) return {std::vector<AST>{}, code.end()};
  std::vector<AST> tl_nodes;
  const auto end = code.end();
  for (auto it = code.begin(); it != end; ++it) {
    std::string_view tok = it->data;
    switch (tok.front()) {
      case ';': break;
      case 'm':
        if (tok == "module") {
          auto start = it->loc;
          std::string module_path;
          uint8_t lwp = 2; // last was period; 0=false, 1=true, 2=start
          while (++it != end) {
            std::string_view tok = it->data;
            switch (tok.front()) {
              case ';': tl_nodes.push_back(AST::create<ast::module_ast>(start, std::move(module_path), std::vector<AST>{})); goto MODULE_END; // empty module declaration
              case '{': // module definition
                {
                  auto [asts, it2] = parse_tl({++it, code.end()}, flags);
                  tl_nodes.push_back(AST::create<ast::module_ast>(start, std::move(module_path), std::move(asts)));
                  it = it2;
                }
                if (it == code.end()) {
                  flags.onerror((it - 1)->loc, "unterminated module definition", ERROR);
                }
                goto MODULE_END;
              case '"': flags.onerror(it->loc, "module path cannot contain a string literal", ERROR); goto MODULE_END;
              case '\'': flags.onerror(it->loc, "module path cannot contain a character literal", ERROR); goto MODULE_END;
              case '0':
              case '1':
                flags.onerror(it->loc, "module path cannot contain a numeric literal", ERROR);
                goto MODULE_END;
              case '.':
                if (lwp == 1) flags.onerror(it->loc, "module path cannot contain consecutive periods", ERROR);
                else module_path.push_back('.');
                lwp = 1;
                break;
              default:
                if (lwp == 0) {
                  flags.onerror(it->loc, "module path cannot contain consecutive identifiers, did you forget a period?", ERROR);
                  module_path.push_back('.');
                }
                lwp = 0;
                module_path += tok;
                break;
            }
          }
          MODULE_END:;
        }
        else if (tok == "mut") UNSUPPORTED("variable")
        else if (tok == "mixin") UNSUPPORTED("mixin")
        else goto CP_DEFAULT;
        break;
      case 's':
        if (tok == "struct") UNSUPPORTED("struct")
        else goto CP_DEFAULT;
        break;
      case 'f':
        if (tok == "func") UNSUPPORTED("function")
        else goto CP_DEFAULT;
        break;
      case 'l':
        if (tok == "let") UNSUPPORTED("variable")
        else goto CP_DEFAULT;
        break;
      case 'i':
        if (tok == "import") {
          location start = it->loc;
          for (auto& path : parse_paths(it, code.end(), flags)) tl_nodes.push_back(AST::create<ast::import_ast>(start, std::move(path)));
        }
        else goto CP_DEFAULT;
        break;
      case '}': return {std::move(tl_nodes), it};
      default: CP_DEFAULT:
        flags.onerror(it->loc, (llvm::Twine("invalid top-level token '") + it->data + "'").str(), ERROR);
    }
  }
  return {std::move(tl_nodes), code.end()};
}
AST cobalt::parse(span<token> code, flags_t flags) {
  if (code.empty()) return nullptr;
  auto [asts, end] = parse_tl(code, flags);
  if (end != code.end()) {
    flags.onerror(end->loc, "unexpected closing brace", ERROR);
    return nullptr;
  }
  return AST::create<ast::top_level_ast>(code.front().loc, std::move(asts));
}
