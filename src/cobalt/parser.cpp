#include "cobalt/parser.hpp"
#include "cobalt/ast.hpp"
#include <array>
using namespace cobalt;
struct binary_operator {
  std::string_view op;
  bool rtl;
  binary_operator(std::string_view op, bool rtl = false) : op(op), rtl(rtl) {}
  binary_operator(const char* c) : op(c), rtl(false) {}
  operator bool() const noexcept {return (bool)op.size();}
};
std::array<binary_operator, 42> bin_ops = {
  ";", "",
  {"=", true}, {"+=", true}, {"-=", true}, {"*=", true}, {"/=", true}, {"%=", true}, {"&=", true}, {"|=", true}, {"^=", true}, {"<<=", true}, {">>=", true}, {"^^=", true}, "",
  "||", "",
  "&&", "",
  "==", "!=", "",
  "<", "<=", ">", ">=", "",
  "|", "",
  "&", "",
  "<<", ">>", "",
  "+", "-", "",
  "*", "/", "%", "",
  {"^^", true}
};
std::array<std::string_view, 8> pre_ops = {"+", "-", "&", "*", "!", "~", "++", "--"};
std::array<std::string_view, 2> post_ops = {"?", "!"};
std::pair<AST, span<token>::iterator> parse_statement(span<token> code, flags_t flags);
std::pair<AST, span<token>::iterator> parse_expr(span<token> code, flags_t flags, char flag = ';');
AST parse_rtl_infix(span<token> code, flags_t flags, binary_operator const* start);
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
std::pair<type_ptr, span<token>::iterator> parse_type(span<token> code, flags_t flags) {
  auto it = code.begin(), end = code.end();
  (void)flags;
  uint8_t lwp = 2; // last was period; 0=false, 1=true, 2=start
  std::string name;
  for (; it != end; ++it) {
    std::string_view tok = it->data;
    switch (tok.front()) {
      case '"': flags.onerror(it->loc, "type name cannot contain a string literal", ERROR); goto PT_END;
      case '\'': flags.onerror(it->loc, "type name cannot contain a character literal", ERROR); goto PT_END;
      case '0':
      case '1':
        flags.onerror(it->loc, "type name cannot contain a numeric literal", ERROR);
        goto PT_END;
      case '.':
        if (lwp == 1) flags.onerror(it->loc, "type name cannot contain consecutive periods", ERROR);
        else name.push_back('.');
        lwp = 1;
        break;
      case '(':
      case ')':
      case '[':
      case ']':
      case '{':
      case '}':
      case ':':
      case ';':
      case ',':
      case '*':
      case '/':
      case '%':
      case '!':
      case '~':
      case '+':
      case '-':
      case '&':
      case '|':
      case '^':
      case '<':
      case '>':
        flags.onerror(it->loc, (llvm::Twine("invalid character '") + tok + "' in type name").str(), ERROR);
        goto PT_END;
        break;
      case '=':
        goto PT_END;
      default:
        if (lwp == 0) {
          flags.onerror(it->loc, "type name cannot contain consecutive identifiers, did you forget a period?", ERROR);
          name.push_back('.');
        }
        lwp = 0;
        name += tok;
        break;
    }
  }
  PT_END:
  llvm::outs() << "parsed type, name: " << name << '\n';
  return {nullptr, it};
}
AST parse_literals(span<token> code, flags_t flags) {
  if (code.size() == 1) {
    std::string_view tok = code.front().data;
    switch (tok.front()) {
      case '0': {
        std::vector<uint64_t> words((tok.size() - 1) / 8); // fix alignment
        std::memcpy(words.data(), tok.data() + 1, tok.size() - 1);
        return AST::create<ast::integer_ast>(code.front().loc, llvm::APInt(words.size() * 64, words), sstring::get(""));
      }
      case '1':
        return AST::create<ast::float_ast>(code.front().loc, reinterpret_cast<float const&>(tok[1]), sstring::get(""));
      case '"':
        return AST::create<ast::string_ast>(code.front().loc, (llvm::Twine("\"") + tok.substr(1) + "\"").str(), sstring::get(""));
      default:
        return AST::create<ast::varget_ast>(code.front().loc, sstring::get(tok));
    }
  }
  std::string str;
  for (auto const& tok : code) str += tok.data;
  return AST::create<ast::varget_ast>(code.empty() ? location{sstring::get("<unknown>"), 0, 0} : code.front().loc, sstring::get(std::move(str)));
}
AST parse_postfix(span<token> code, flags_t flags) {
  for (auto op : post_ops) if (code.back().data == op) return AST::create<ast::unop_ast>(code.back().loc, sstring::get((llvm::Twine("p") + op).str()), parse_postfix(code.subspan(0, code.size() - 1), flags));
  return parse_literals(code, flags);  // TODO: switch to different parse_* function
}
AST parse_prefix(span<token> code, flags_t flags) {
  for (auto op : pre_ops) if (code.front().data == op) return AST::create<ast::unop_ast>(code.front().loc, sstring::get(op), parse_prefix(code.subspan(1), flags));
  return parse_postfix(code, flags);
}
AST parse_ltr_infix(span<token> code, flags_t flags, binary_operator const* start) {
  binary_operator const* ptr = start;
  for (++ptr; ptr != bin_ops.end() && *ptr; ++ptr);
  span<binary_operator const> ops {start, ptr};
  auto it = code.begin(), end = code.end();
  for (++it; it != end; ++it) {
    auto tok = it->data;
    switch (tok.front()) {
      case '(': {
        std::size_t depth = 1;
        while (++it != end && depth) {
          switch (it->data.front()) {
            case '(': ++depth; break;
            case ')': --depth; break;
          }
        }
      } break;
      case '{': {
        std::size_t depth = 1;
        while (++it != end && depth) {
          switch (it->data.front()) {
            case '{': ++depth; break;
            case '}': --depth; break;
          }
        }
      } break;
      default:
        for (auto op : ops) if (tok == op.op) {
          AST lhs = nullptr, rhs = nullptr;
          if (ptr == bin_ops.end()) {
            lhs = parse_prefix({code.begin(), it}, flags);
            rhs = parse_ltr_infix({it + 1, code.end()}, flags, start);
          }
          else if ((++ptr)->rtl) {
            lhs = parse_rtl_infix({code.begin(), it}, flags, ptr);
            rhs = parse_rtl_infix({it + 1, code.end()}, flags, start);
          }
          else {
            lhs = parse_ltr_infix({code.begin(), it}, flags, ptr);
            rhs = parse_ltr_infix({it + 1, code.end()}, flags, start);
          }
          return AST::create<ast::binop_ast>(it->loc, sstring::get(tok), std::move(lhs), std::move(rhs));
        }
    }
  }
  if (ptr == bin_ops.end()) return parse_prefix(code, flags);
  else if ((++ptr)->rtl) return parse_rtl_infix(code, flags, ptr);
  else return parse_ltr_infix(code, flags, ptr);
}
AST parse_rtl_infix(span<token> code, flags_t flags, binary_operator const* start) {
  binary_operator const* ptr = start;
  for (++ptr; ptr != bin_ops.end() && *ptr; ++ptr);
  span<binary_operator const> ops {start, ptr};
  auto it = code.end() - 1, end = code.begin() - 1;
  for (--it; it != end; --it) {
    auto tok = it->data;
    switch (tok.front()) {
      case ')': {
        std::size_t depth = 1;
        while (--it != end && depth) {
          switch (it->data.front()) {
            case '(': --depth; break;
            case ')': ++depth; break;
          }
        }
      } break;
      case '}': {
        std::size_t depth = 1;
        while (--it != end && depth) {
          switch (it->data.front()) {
            case '{': --depth; break;
            case '}': ++depth; break;
          }
        }
      } break;
      default:
        for (auto op : ops) if (tok == op.op) {
          AST lhs = nullptr, rhs = nullptr;
          if (ptr == bin_ops.end()) {
            lhs = parse_rtl_infix({code.begin(), it}, flags, start);
            rhs = parse_prefix({it + 1, code.end()}, flags);
          }
          else if ((++ptr)->rtl) {
            lhs = parse_rtl_infix({code.begin(), it}, flags, start);
            rhs = parse_rtl_infix({it + 1, code.end()}, flags, ptr);
          }
          else {
            lhs = parse_ltr_infix({code.begin(), it}, flags, start);
            rhs = parse_ltr_infix({it + 1, code.end()}, flags, ptr);
          }
          return AST::create<ast::binop_ast>(it->loc, sstring::get(tok), std::move(lhs), std::move(rhs));
        }
    }
  }
  if (ptr == bin_ops.end()) return parse_prefix(code, flags);
  else if ((++ptr)->rtl) return parse_rtl_infix(code, flags, ptr);
  else return parse_ltr_infix(code, flags, ptr);
}
AST parse_infix(span<token> code, flags_t flags, binary_operator const* ptr = &bin_ops[2]) {
  if (ptr->rtl) return parse_rtl_infix(code, flags, ptr);
  else return parse_ltr_infix(code, flags, ptr);
}
std::pair<AST, span<token>::iterator> parse_expr(span<token> code, flags_t flags, char flag) {
  code = code.subspan(1);
  auto it = code.begin(), end = code.end();
  std::size_t depth = 1;
  switch (flag) {
    case '{': while (it != end && depth) {
      std::string_view tok = it->data;
      switch (tok.front()) {
        case '{': ++depth;
        case '}': --depth;
      }
    } break;
    case '[': while (it != end && depth) {
      std::string_view tok = it->data;
      switch (tok.front()) {
        case '[': ++depth;
        case ']': --depth;
      }
    } break;
    case '(': while (it != end && depth) {
      std::string_view tok = it->data;
      switch (tok.front()) {
        case '(': ++depth;
        case ')': --depth;
      }
    } break;
    default:
      while (it != end && it->data.front() != flag) ++it;
      return {parse_infix({code.begin(), it}, flags, &bin_ops[2]), it};
  }
  return {parse_infix({code.begin(), it}, flags), it};
}
std::pair<AST, span<token>::iterator> parse_statement(span<token> code, flags_t flags) {
#define UNSUPPORTED(TYPE) {flags.onerror(it->loc, TYPE " definitions are not currently supported", CRITICAL);}
  if (code.empty()) return {AST{nullptr}, code.end()};
  auto it = code.begin(), end = code.end();
  std::string_view tok = it->data;
  switch (tok.front()) {
    case ';': break;
    case 'm':
      if (tok == "module") {
        flags.onerror(it->loc, "module definitions are only allowed at the top-level scope", ERROR);
        goto ST_DEFAULT;
      }
      else if (tok == "mut") {
        auto start = it->loc;
        std::string name = "";
        AST val = nullptr;
        if (it->data != ":") {
          uint8_t lwp = 2; // last was period; 0=false, 1=true, 2=start
          while (++it != end) {
            std::string_view tok = it->data;
            switch (tok.front()) {
              case '"': flags.onerror(it->loc, "variable name cannot contain a string literal", ERROR); goto MUTDEF_END;
              case '\'': flags.onerror(it->loc, "variable name cannot contain a character literal", ERROR); goto MUTDEF_END;
              case '0':
              case '1':
                flags.onerror(it->loc, "variable name cannot contain a numeric literal", ERROR);
                goto MUTDEF_END;
              case '.':
                if (lwp == 1) flags.onerror(it->loc, "variable name cannot contain consecutive periods", ERROR);
                else name.push_back('.');
                lwp = 1;
                break;
              case '(':
              case ')':
              case '[':
              case ']':
              case '{':
              case '}':
              case ';':
              case ',':
              case '*':
              case '/':
              case '%':
              case '!':
              case '~':
              case '+':
              case '-':
              case '&':
              case '|':
              case '^':
              case '<':
              case '>':
                flags.onerror(it->loc, (llvm::Twine("invalid character '") + tok + "' in variable name").str(), ERROR);
                goto MUTDEF_END;
                break;
              case '=':
              case ':':
                goto MUTDEF_END;
              default:
                if (lwp == 0) {
                  flags.onerror(it->loc, "variable name cannot contain consecutive identifiers, did you forget a period?", ERROR);
                  name.push_back('.');
                }
                lwp = 0;
                name += tok;
                break;
            }
          }
          MUTDEF_END:;
          if (tok == ":") {
            auto start = (it - 1)->loc;
            ++it;
            auto [t, it2] = parse_type({it, end}, flags);
            it = it2;
            auto [ast, it3] = parse_expr({it, end}, flags);
            it = it2;
            val = AST::create<ast::cast_ast>(start, t, std::move(ast));
          }
          else {
            auto [ast, it2] = parse_expr({it, end}, flags);
            val = std::move(ast);
            it = it2;
          }
        }
        else {
          auto start = (it - 1)->loc;
          ++it;
          auto [t, it2] = parse_type({it, end}, flags);
          it = it2;
          auto [ast, it3] = parse_expr({it, end}, flags);
          it = it2;
          val = AST::create<ast::cast_ast>(start, t, std::move(ast));
        }
        return {AST::create<ast::mutdef_ast>(start, sstring::get(name), std::move(val)), it};
      }
      else if (tok == "mixin") UNSUPPORTED("mixin")
      else goto ST_DEFAULT;
      break;
    case 's':
      if (tok == "struct") UNSUPPORTED("struct")
      else goto ST_DEFAULT;
      break;
    case 'f':
      if (tok == "func") UNSUPPORTED("function")
      else goto ST_DEFAULT;
      break;
    case 'l':
      if (tok == "let") {
        auto start = it->loc;
        std::string name = "";
        AST val = nullptr;
        if (it->data != ":") {
          uint8_t lwp = 2; // last was period; 0=false, 1=true, 2=start
          while (++it != end) {
            std::string_view tok = it->data;
            switch (tok.front()) {
              case '"': flags.onerror(it->loc, "variable name cannot contain a string literal", ERROR); goto VARDEF_END;
              case '\'': flags.onerror(it->loc, "variable name cannot contain a character literal", ERROR); goto VARDEF_END;
              case '0':
              case '1':
                flags.onerror(it->loc, "variable name cannot contain a numeric literal", ERROR);
                goto VARDEF_END;
              case '.':
                if (lwp == 1) flags.onerror(it->loc, "variable name cannot contain consecutive periods", ERROR);
                else name.push_back('.');
                lwp = 1;
                break;
              case '(':
              case ')':
              case '[':
              case ']':
              case '{':
              case '}':
              case ';':
              case ',':
              case '*':
              case '/':
              case '%':
              case '!':
              case '~':
              case '+':
              case '-':
              case '&':
              case '|':
              case '^':
              case '<':
              case '>':
                flags.onerror(it->loc, (llvm::Twine("invalid character '") + tok + "' in variable name").str(), ERROR);
                goto VARDEF_END;
                break;
              case '=':
              case ':':
                goto VARDEF_END;
              default:
                if (lwp == 0) {
                  flags.onerror(it->loc, "variable name cannot contain consecutive identifiers, did you forget a period?", ERROR);
                  name.push_back('.');
                }
                lwp = 0;
                name += tok;
                break;
            }
          }
          VARDEF_END:;
          if (tok == ":") {
            auto start = (it - 1)->loc;
            ++it;
            auto [t, it2] = parse_type({it, end}, flags);
            it = it2;
            auto [ast, it3] = parse_expr({it, end}, flags);
            it = it3;
            val = AST::create<ast::cast_ast>(start, t, std::move(ast));
          }
          else {
            auto [ast, it2] = parse_expr({it, end}, flags);
            val = std::move(ast);
            it = it2;
          }
        }
        else {
          auto start = (it - 1)->loc;
          ++it;
          auto [t, it2] = parse_type({it, end}, flags);
          it = it2;
          auto [ast, it3] = parse_expr({it, end}, flags);
          it = it3;
          val = AST::create<ast::cast_ast>(start, t, std::move(ast));
        }
        return {AST::create<ast::vardef_ast>(start, sstring::get(name), std::move(val)), it};
      }
      else goto ST_DEFAULT;
      break;
    case 'i':
      if (tok == "import") {
        location start = it->loc;
        std::vector<AST> paths;
        for (auto& path : parse_paths(it, code.end(), flags)) paths.push_back(AST::create<ast::import_ast>(start, std::move(path)));
        return {AST::create<ast::group_ast>(start, std::move(paths)), it};
      }
      else goto ST_DEFAULT;
      break;
    default: ST_DEFAULT:
      return parse_expr({it, end}, flags);
  }
  return {AST(nullptr), it};
}
std::pair<std::vector<AST>, span<token>::iterator> parse_tl(span<token> code, flags_t flags) {
#undef UNSUPPORTED
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
                case '(':
                case ')':
                case '[':
                case ']':
                case '}':
                case ':':
                case ',':
                case '*':
                case '/':
                case '%':
                case '!':
                case '~':
                case '+':
                case '-':
                case '&':
                case '|':
                case '^':
                case '<':
                case '>':
                case '=':
                  flags.onerror(it->loc, (llvm::Twine("invalid character '") + tok + "' in module name").str(), ERROR);
                  goto MODULE_END;
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
        else if (tok == "mut") {
          auto start = it->loc;
          std::string name = "";
          AST val = nullptr;
          if (it->data != ":") {
            uint8_t lwp = 2; // last was period; 0=false, 1=true, 2=start
            while (++it != end) {
              std::string_view tok = it->data;
              switch (tok.front()) {
                case '"': flags.onerror(it->loc, "variable name cannot contain a string literal", ERROR); goto MUTDEF_END;
                case '\'': flags.onerror(it->loc, "variable name cannot contain a character literal", ERROR); goto MUTDEF_END;
                case '0':
                case '1':
                  flags.onerror(it->loc, "variable name cannot contain a numeric literal", ERROR);
                  goto MUTDEF_END;
                case '.':
                  if (lwp == 1) flags.onerror(it->loc, "variable name cannot contain consecutive periods", ERROR);
                  else name.push_back('.');
                  lwp = 1;
                  break;
                case '(':
                case ')':
                case '[':
                case ']':
                case '{':
                case '}':
                case ';':
                case ',':
                case '*':
                case '/':
                case '%':
                case '!':
                case '~':
                case '+':
                case '-':
                case '&':
                case '|':
                case '^':
                case '<':
                case '>':
                  flags.onerror(it->loc, (llvm::Twine("invalid character '") + tok + "' in variable name").str(), ERROR);
                  goto MUTDEF_END;
                  break;
                case '=':
                case ':':
                  goto MUTDEF_END;
                default:
                  if (lwp == 0) {
                    flags.onerror(it->loc, "variable name cannot contain consecutive identifiers, did you forget a period?", ERROR);
                    name.push_back('.');
                  }
                  lwp = 0;
                  name += tok;
                  break;
              }
            }
            MUTDEF_END:;
            if (tok == ":") {
              auto start = (it - 1)->loc;
              ++it;
              auto [t, it2] = parse_type({it, end}, flags);
              it = it2;
              auto [ast, it3] = parse_expr({it, end}, flags);
              it = it2;
              val = AST::create<ast::cast_ast>(start, t, std::move(ast));
            }
            else {
              auto [ast, it2] = parse_expr({it, end}, flags);
              val = std::move(ast);
              it = it2;
            }
          }
          else {
            auto start = (it - 1)->loc;
            ++it;
            auto [t, it2] = parse_type({it, end}, flags);
            it = it2;
            auto [ast, it3] = parse_expr({it, end}, flags);
            it = it3;
            val = AST::create<ast::cast_ast>(start, t, std::move(ast));
          }
          tl_nodes.push_back(AST::create<ast::mutdef_ast>(start, sstring::get(name), std::move(val)));
        }
        else if (tok == "mixin") UNSUPPORTED("mixin")
        else goto TL_DEFAULT;
        break;
      case 's':
        if (tok == "struct") UNSUPPORTED("struct")
        else goto TL_DEFAULT;
        break;
      case 'f':
        if (tok == "func") UNSUPPORTED("function")
        else goto TL_DEFAULT;
        break;
      case 'l':
        if (tok == "let") {
          auto start = it->loc;
          std::string name = "";
          AST val = nullptr;
          if (it->data != ":") {
            uint8_t lwp = 2; // last was period; 0=false, 1=true, 2=start
            while (++it != end) {
              std::string_view tok = it->data;
              switch (tok.front()) {
                case '"': flags.onerror(it->loc, "variable name cannot contain a string literal", ERROR); goto VARDEF_END;
                case '\'': flags.onerror(it->loc, "variable name cannot contain a character literal", ERROR); goto VARDEF_END;
                case '0':
                case '1':
                  flags.onerror(it->loc, "variable name cannot contain a numeric literal", ERROR);
                  goto VARDEF_END;
                case '.':
                  if (lwp == 1) flags.onerror(it->loc, "variable name cannot contain consecutive periods", ERROR);
                  else name.push_back('.');
                  lwp = 1;
                  break;
                case '(':
                case ')':
                case '[':
                case ']':
                case '{':
                case '}':
                case ';':
                case ',':
                case '*':
                case '/':
                case '%':
                case '!':
                case '~':
                case '+':
                case '-':
                case '&':
                case '|':
                case '^':
                case '<':
                case '>':
                  flags.onerror(it->loc, (llvm::Twine("invalid character '") + tok + "' in variable name").str(), ERROR);
                  goto VARDEF_END;
                  break;
                case '=':
                case ':':
                  goto VARDEF_END;
                default:
                  if (lwp == 0) {
                    flags.onerror(it->loc, "variable name cannot contain consecutive identifiers, did you forget a period?", ERROR);
                    name.push_back('.');
                  }
                  lwp = 0;
                  name += tok;
                  break;
              }
            }
            VARDEF_END:;
            if (it->data == ":") {
              auto start = (it - 1)->loc;
              ++it;
              auto [t, it2] = parse_type({it, end}, flags);
              it = it2;
              auto [ast, it3] = parse_expr({it, end}, flags);
              it = it3;
              val = AST::create<ast::cast_ast>(start, t, std::move(ast));
            }
            else {
              auto [ast, it2] = parse_expr({it, end}, flags);
              val = std::move(ast);
              it = it2;
            }
          }
          else {
            auto start = (it - 1)->loc;
            ++it;
            auto [t, it2] = parse_type({it, end}, flags);
            it = it2;
            auto [ast, it3] = parse_expr({it, end}, flags);
            it = it3;
            val = AST::create<ast::cast_ast>(start, t, std::move(ast));
          }
          tl_nodes.push_back(AST::create<ast::vardef_ast>(start, sstring::get(name), std::move(val)));
        }
        else goto TL_DEFAULT;
        break;
      case 'i':
        if (tok == "import") {
          location start = it->loc;
          for (auto& path : parse_paths(it, code.end(), flags)) tl_nodes.push_back(AST::create<ast::import_ast>(start, std::move(path)));
        }
        else goto TL_DEFAULT;
        break;
      case '}': return {std::move(tl_nodes), it};
      default: TL_DEFAULT:
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
