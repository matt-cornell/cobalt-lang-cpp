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
std::pair<AST, span<token>::iterator> parse_expr(span<token> code, flags_t flags, std::string_view exit_chars = ";");
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
std::pair<sstring, span<token>::iterator> parse_type(span<token> code, flags_t flags, std::string_view exit_chars) {
  auto it = code.begin(), end = code.end();
  (void)flags;
  uint8_t lwp = 2; // last was period; 0=false, 1=true, 2=start
  std::string name;
  bool graceful = false;
  for (; it != end; ++it) {
    std::string_view tok = it->data;
    if (exit_chars.find(tok.front()) != std::string::npos) {
      graceful = true;
      goto PT_END;
    }
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
      case '=':
        flags.onerror(it->loc, (llvm::Twine("invalid character '") + tok + "' in type name").str(), ERROR);
        goto PT_END;
        break;
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
  return {sstring::get(graceful ? name : ""), it};
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
        return AST::create<ast::float_ast>(code.front().loc, reinterpret_cast<double const&>(tok[1]), sstring::get(""));
      case '\'':
        return AST::create<ast::char_ast>(code.front().loc, std::string(tok.substr(1)), sstring::get(""));
      case '"':
        return AST::create<ast::string_ast>(code.front().loc, std::string(tok.substr(1)), sstring::get(""));
      default:
        return AST::create<ast::varget_ast>(code.front().loc, sstring::get(tok));
    }
  }
  std::string str;
  for (auto const& tok : code) str += tok.data;
  return AST::create<ast::varget_ast>(code.empty() ? location{sstring::get("<unknown>"), 0, 0} : code.front().loc, sstring::get(std::move(str)));
}
AST parse_groups(span<token> code, flags_t flags) {
  if (code.empty()) return nullptr;
  switch (code.front().data.front()) {
    case '(': {
      if (code.back().data.front() != ')') flags.onerror(code.front().loc, "unmatched opening parententhesis", ERROR);
      std::size_t depth = 1;
      auto it = code.begin(), end = code.end();
      std::vector<AST> nodes;
      ++it;
      while (it != end && depth) {
        auto [a, i] = parse_expr({it, end}, flags, ";)");
        nodes.push_back(std::move(a));
        it = i;
        if (it != end) switch (it->data.front()) {
          case '(': ++depth; break;
          case ')': --depth; break;
          case ';': break;
          default: flags.onerror(it->loc, "missing semicolon in paranthetical grouping", ERROR);
        }
      }
      if (it == end && depth) flags.onerror((it - 1)->loc, "parenthetical grouping missing closing parenthesis", ERROR);
      switch (nodes.size()) {
        case 0: flags.onerror(it == end ? (it - 1)->loc : it->loc, "empty parenthetical groupings are not allowed", ERROR); return AST(nullptr);
        case 1: return std::move(nodes.front());
        default: return AST::create<ast::group_ast>(it == end ? (it - 1)->loc : it->loc, std::move(nodes));
      }
    } break;
    case '{': {
      if (code.back().data.front() != '}') flags.onerror(code.front().loc, "unmatched opening brace", ERROR);
      auto it = code.begin(), end = code.end();
      std::vector<AST> nodes;
      ++it;
      while (it != end) {
        auto [a, i] = parse_statement({it, end}, flags);
        nodes.push_back(std::move(a));
        it = i;
        if (it != end) switch (it->data.front()) {
          case ';':
          case '}': break;
          default: flags.onerror(it->loc, "missing semicolon in brace grouping", ERROR);
        }
        ++it;
      }
      return AST::create<ast::block_ast>(it == end ? (it - 1)->loc : it->loc, std::move(nodes));
    } break;
    default:
      return parse_literals(code, flags);
  }
}
AST parse_calls(span<token> code, flags_t flags) {
  if (code.empty()) return AST(nullptr);
  switch (code.back().data.front()) {
    case ')': {
      auto it = code.end() - 1, end = code.begin() - 1;
      std::size_t depth = 1;
      while (depth && --it != end) switch (it->data.front()) {
        case '(': --depth; break;
        case ')': ++depth; break;
      }
      if (it == end && depth) {
        flags.onerror((it + 1)->loc, "unmatched closing bracket", ERROR);
        return AST(nullptr);
      }
      std::vector<AST> args;
      auto it2 = it;
      while (it2 != end && it2->data.front() != ')') {
        auto [a, i] = parse_expr({it2 + 1, code.end() - 1}, flags, ",");
        it2 = i;
        if (a) args.push_back(std::move(a));
      }
      return AST::create<ast::call_ast>(code.front().loc, parse_calls({code.begin(), it}, flags), std::move(args));
    } break;
    case ']': {
      auto it = code.end() - 1, end = code.begin() - 1;
      std::size_t depth = 1;
      while (depth && --it != end) switch (it->data.front()) {
        case '[': --depth; break;
        case ']': ++depth; break;
      }
      if (it == end && depth) {
        flags.onerror((it + 1)->loc, "unmatched closing bracket", ERROR);
        return AST(nullptr);
      }
      std::vector<AST> args;
      auto it2 = it;
      while (it2 != end && it2->data.front() != ']') {
        auto [a, i] = parse_expr({it2 + 1, code.end() - 1}, flags, ",");
        it2 = i;
        if (a) args.push_back(std::move(a));
      }
      return AST::create<ast::subscr_ast>(code.front().loc, parse_calls({code.begin(), it}, flags), std::move(args));
    } break;
    default: return parse_groups(code, flags);
  }
}
AST parse_postfix(span<token> code, flags_t flags) {
  if (code.empty()) return AST(nullptr);
  for (auto op : post_ops) if (code.back().data == op) return AST::create<ast::unop_ast>(code.back().loc, sstring::get((llvm::Twine("p") + op).str()), parse_postfix(code.subspan(0, code.size() - 1), flags));
  return parse_calls(code, flags);
}
AST parse_prefix(span<token> code, flags_t flags) {
  if (code.empty()) return AST(nullptr);
  for (auto op : pre_ops) if (code.front().data == op) return AST::create<ast::unop_ast>(code.front().loc, sstring::get(op), parse_prefix(code.subspan(1), flags));
  return parse_postfix(code, flags);
}
AST parse_ltr_infix(span<token> code, flags_t flags, binary_operator const* start) {
  if (code.empty()) return AST(nullptr);
  binary_operator const* ptr = start;
  for (++ptr; ptr != bin_ops.end() && *ptr; ++ptr);
  span<binary_operator const> ops {start, ptr};
  auto it = code.begin(), end = code.end();
  switch (it->data.front()) {
    case '(': {
      std::size_t depth = 1;
      while (++it != end && depth) {
        switch (it->data.front()) {
          case '(': ++depth; break;
          case ')': --depth; break;
        }
      }
      --it;
    } break;
    case '[': {
      std::size_t depth = 1;
      while (++it != end && depth) {
        switch (it->data.front()) {
          case '[': ++depth; break;
          case ']': --depth; break;
        }
      }
      --it;
    } break;
    case '{': {
      std::size_t depth = 1;
      while (++it != end && depth) {
        switch (it->data.front()) {
          case '{': ++depth; break;
          case '}': --depth; break;
        }
      }
      --it;
    } break;
  }
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
        --it;
      } break;
      case '[': {
        std::size_t depth = 1;
        while (++it != end && depth) {
          switch (it->data.front()) {
            case '[': ++depth; break;
            case ']': --depth; break;
          }
        }
        --it;
      } break;
      case '{': {
        std::size_t depth = 1;
        while (++it != end && depth) {
          switch (it->data.front()) {
            case '{': ++depth; break;
            case '}': --depth; break;
          }
        }
        --it;
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
  if (code.empty()) return AST(nullptr);
  binary_operator const* ptr = start;
  for (++ptr; ptr != bin_ops.end() && *ptr; ++ptr);
  span<binary_operator const> ops {start, ptr};
  auto it = code.end() - 1, end = code.begin() - 1;
  switch (it->data.front()) {
    case ')': {
      std::size_t depth = 1;
      while (--it != end && depth) {
        switch (it->data.front()) {
          case '(': --depth; break;
          case ')': ++depth; break;
        }
      }
      ++it;
    } break;
    case ']': {
      std::size_t depth = 1;
      while (--it != end && depth) {
        switch (it->data.front()) {
          case '[': --depth; break;
          case ']': ++depth; break;
        }
      }
      ++it;
    } break;
    case '}': {
      std::size_t depth = 1;
      while (--it != end && depth) {
        switch (it->data.front()) {
          case '{': --depth; break;
          case '}': ++depth; break;
        }
      }
      ++it;
    } break;
  }
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
        ++it;
      } break;
      case ']': {
        std::size_t depth = 1;
        while (--it != end && depth) {
          switch (it->data.front()) {
            case '[': --depth; break;
            case ']': ++depth; break;
          }
        }
        ++it;
      } break;
      case '}': {
        std::size_t depth = 1;
        while (--it != end && depth) {
          switch (it->data.front()) {
            case '{': --depth; break;
            case '}': ++depth; break;
          }
        }
        ++it;
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
std::pair<AST, span<token>::iterator> parse_expr(span<token> code, flags_t flags, std::string_view exit_chars) {
  auto it = code.begin(), end = code.end();
  std::size_t paren = 0, brack = 0, brace = 0;
  auto nc = [exit_chars](char c) {return exit_chars.find(c) == std::string::npos;};
  for (; it != end && (paren || brack || brace || nc(it->data.front())); ++it) {
    switch (it->data.front()) {
      case '(': ++paren; break;
      case ')': --paren; break;
      case '[': ++brack; break;
      case ']': --brack; break;
      case '{': ++brace; break;
      case '}': --brace; break;
    }
  }
  return {parse_infix({code.begin(), it}, flags, &bin_ops[2]), it};
}
std::pair<AST, span<token>::iterator> parse_statement(span<token> code, flags_t flags) {
#define UNSUPPORTED(TYPE) {flags.onerror(it->loc, TYPE " definitions are not currently supported", CRITICAL); return {AST(nullptr), code.begin() + 1};}
  if (code.empty()) return {AST{nullptr}, code.end()};
  auto it = code.begin(), end = code.end();
  std::string_view tok = it->data;
  std::vector<std::string> annotations;
  switch (tok.front()) {
    case ';': break;
    case '@': annotations.push_back(std::string(tok.substr(1))); break;
    case 'c':
      if (tok == "cr") UNSUPPORTED("coroutine")
      else goto ST_DEFAULT;
      break;
    case 'm':
      if (tok == "module") {
        annotations.clear();
        flags.onerror(it->loc, "module definitions are only allowed at the top-level scope", ERROR);
        if (++it == end) return {AST(nullptr), end};
        tok = it->data;
        if (tok == ";") return {AST(nullptr), ++it};
        else if (tok == "{") {
          std::size_t depth = 0;
          while (++it != end && depth) switch (it->data.front()) {
            case '{': ++depth; break;
            case '}': --depth; break;
          }
          return {AST(nullptr), it};
        }
        else return {AST(nullptr), it};
      }
      else if (tok == "mut") {
        auto start = it->loc;
        std::string name = "";
        AST val = nullptr;
        if ((++it)->data != ":") {
          name = it++->data;
          bool to_skip = false;
          switch (name.front()) {
            case '.':
              flags.onerror((it - 1)->loc, "variable paths are not allowed in local variables", ERROR);
              to_skip = true;
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
              flags.onerror((it - 1)->loc, (llvm::Twine("invalid character ") + name + " in variable name").str(), ERROR);
              to_skip = true;
              break;
          }
          switch (it->data.front()) {
            case ':': break;
            case '.':
              to_skip = true;
              flags.onerror(it->loc, "variable paths are not allowed in local variables", ERROR);
              break;
            case '=':
              if (it->data.size() != 1) {
                to_skip = true;
                flags.onerror(it->loc, "unexpected identifier in local variable definition", ERROR);
              }
            break;
            default:
              to_skip = true;
              flags.onerror(it->loc, "unexpected identifier in local variable definition", ERROR);
          }
          if (to_skip) {
            name = "";
            while (it != end && it->data.front() != ':' && it->data != "=") ++it;
          }
          if (it->data.front() == ':') {
            auto start = (it - 1)->loc;
            ++it;
            auto [t, it2] = parse_type({it, end}, flags, "=");
            it = it2;
            auto [ast, it3] = parse_expr({it + 1, end}, flags);
            it = it3;
            val = AST::create<ast::cast_ast>(start, t, std::move(ast));
          }
          else {
            auto [ast, it2] = parse_expr({it + 1, end}, flags);
            val = std::move(ast);
            it = it2;
          }
        }
        else {
          auto start = (it - 1)->loc;
          ++it;
          auto [t, it2] = parse_type({it, end}, flags, "=");
          it = it2;
          auto [ast, it3] = parse_expr({it + 1, end}, flags);
          it = it3;
          val = AST::create<ast::cast_ast>(start, t, std::move(ast));
        }
        return {AST::create<ast::mutdef_ast>(start, sstring::get(name), std::move(val), std::move(annotations)), it};
      }
      else if (tok == "mixin") {annotations.clear(); UNSUPPORTED("mixin")}
      else goto ST_DEFAULT;
      break;
    case 's':
      if (tok == "struct") {annotations.clear(); UNSUPPORTED("struct")}
      else goto ST_DEFAULT;
      break;
    case 'f':
      if (tok == "fn") {
        auto start = it->loc;
        std::string name = (++it)->data;
        bool to_skip = false;
        switch (name.front()) {
          case '.':
            flags.onerror(start, "variable paths are not allowed in local function", ERROR);
            to_skip = true;
            break;
          case '(':
            flags.onerror(start, "anonymous functions will be ignored", WARNING);
            --it;
            break;
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
            flags.onerror(start, (llvm::Twine("invalid character ") + name + " in function name").str(), ERROR);
            to_skip = true;
            break;
        }
        switch ((++it)->data.front()) {
          case '(': break;
          case '.':
            to_skip = true;
            flags.onerror(it->loc, "variable paths are not allowed in local function", ERROR);
          default:
            to_skip = true;
            flags.onerror(it->loc, "unexpected identifier in local function definition", ERROR);
        }
        if (to_skip) {
          name = "";
          while (it != end && it->data.front() != '(') ++it;
        }
        bool graceful = true;
        if (graceful) {
          std::vector<std::pair<sstring, sstring>> params;
          graceful = false;
          while (++it != end) {
            auto tok = it->data;
            switch (tok.front()) {
              case '.':
              case '(':
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
                flags.onerror(it->loc, (llvm::Twine("invalid parameter name '") + tok + "'").str(), ERROR);
                goto PARAMS_END;
              case ')':
                graceful = true;
                goto PARAMS_END;
              case ':': {
                auto [type, it2] = parse_type({it + 1, end}, flags, ",)");
                it = it2;
                params.push_back({sstring::get(name), type});
                if (it == end) {
                  flags.onerror((it - 1)->loc, "unterminated function parameter list", ERROR);
                  return {AST(nullptr), it};
                }
                std::string_view next = it->data;
                switch (next.front()) {
                  case ',': break;
                  case ')': --it; break;
                  default:
                    flags.onerror(it->loc, "invalid character after type in parameter, did you forget a comma?", ERROR);
                    while (it != end && it->data.front() != ',' && it->data.front() != ')') ++it;
                }
              } break;
              default: {
                std::string_view name = it->data;
                ++it;
                switch (it->data.front()) {
                  case ':': break;
                  case '=':
                    flags.onerror(it->loc, "default parameters are not supported", ERROR);
                    break;
                  case '.':
                    flags.onerror(it->loc, "parameter name cannot be a path", ERROR);
                    break;
                  default:
                    flags.onerror(it->loc, "invalid consecutive identifiers in parameter name", ERROR);
                }
                auto [type, it2] = parse_type({it + 1, end}, flags, ",)");
                it = it2;
                params.push_back({sstring::get(name), type});
                if (it == end) {
                  flags.onerror((it - 1)->loc, "unterminated function parameter list", ERROR);
                  return {AST(nullptr), it};
                }
                std::string_view next = it->data;
                switch (next.front()) {
                  case ',': break;
                  case ')': --it; break;
                  default:
                    flags.onerror(it->loc, "invalid character after type in parameter, did you forget a comma?", ERROR);
                    while (it != end && it->data.front() != ',' && it->data.front() != ')') ++it;
                }
              } break;
            }
          }
          PARAMS_END:;
          auto return_type = sstring::get("<error>");
          if ((++it)->data.front() != ':') flags.onerror(it->loc, "functions must have an explicit return type", ERROR);
          else {
            auto [r, i] = parse_type({it + 1, end}, flags, "=");
            return_type = r;
            it = i;
          }
          if (it->data != "=") {
            flags.onerror(it->loc, "function must have a body", ERROR);
            return {AST::create<ast::fndef_ast>(start, sstring::get(name), return_type, std::move(params), AST(nullptr), std::move(annotations)), it};
          }
          else {
            auto [ast, i] = parse_expr({it + 1, end}, flags);
            it = i;
            return {AST::create<ast::fndef_ast>(start, sstring::get(name), return_type, std::move(params), std::move(ast), std::move(annotations)), it};
          }
        }
      }
      else goto ST_DEFAULT;
      break;
    case 'l':
      if (tok == "let") {
        auto start = it->loc;
        std::string name = "";
        AST val = nullptr;
        if ((++it)->data != ":") {
          name = it++->data;
          bool to_skip = false;
          switch (name.front()) {
            case '.':
              flags.onerror((it - 1)->loc, "variable paths are not allowed in local variables", ERROR);
              to_skip = true;
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
              flags.onerror((it - 1)->loc, (llvm::Twine("invalid character ") + name + " in variable name").str(), ERROR);
              to_skip = true;
              break;
          }
          switch (it->data.front()) {
            case ':': break;
            case '.':
              to_skip = true;
              flags.onerror(it->loc, "variable paths are not allowed in local variables", ERROR);
              break;
            case '=':
              if (it->data.size() != 1) {
                to_skip = true;
                flags.onerror(it->loc, "unexpected identifier in local variable definition", ERROR);
              }
            break;
            default:
              to_skip = true;
              flags.onerror(it->loc, "unexpected identifier in local variable definition", ERROR);
          }
          if (to_skip) {
            name = "";
            while (it != end && it->data.front() != ':' && it->data != "=") ++it;
          }
          if (it->data.front() == ':') {
            auto start = (it - 1)->loc;
            ++it;
            auto [t, it2] = parse_type({it, end}, flags, "=");
            it = it2;
            auto [ast, it3] = parse_expr({it + 1, end}, flags);
            it = it3;
            val = AST::create<ast::cast_ast>(start, t, std::move(ast));
          }
          else {
            auto [ast, it2] = parse_expr({it + 1, end}, flags);
            val = std::move(ast);
            it = it2;
          }
        }
        else {
          auto start = (it - 1)->loc;
          ++it;
          auto [t, it2] = parse_type({it, end}, flags, "=");
          it = it2;
          auto [ast, it3] = parse_expr({it + 1, end}, flags);
          it = it3;
          val = AST::create<ast::cast_ast>(start, t, std::move(ast));
        }
        return {AST::create<ast::vardef_ast>(start, sstring::get(name), std::move(val), std::move(annotations)), it};
      }
      else goto ST_DEFAULT;
      break;
    case 'i':
      if (tok == "import") {
        location start = it->loc;
        if (annotations.size()) flags.onerror(start, "annotations cannot be applied to an import statement", ERROR);
        std::vector<AST> paths;
        for (auto& path : parse_paths(it, code.end(), flags)) paths.push_back(AST::create<ast::import_ast>(start, std::move(path)));
        return {paths.size() == 1 ? std::move(paths.front()) : AST::create<ast::group_ast>(start, std::move(paths)), it};
      }
      else goto ST_DEFAULT;
      break;
    default: ST_DEFAULT:
      if (annotations.size()) flags.onerror(it->loc, "annotations cannot be applied to expressions", ERROR);
      return parse_expr({it, end}, flags, ";}");
  }
  return {AST(nullptr), it};
}
std::pair<std::vector<AST>, span<token>::iterator> parse_tl(span<token> code, flags_t flags) {
#undef UNSUPPORTED
#define UNSUPPORTED(TYPE) {flags.onerror(it->loc, "top-level " TYPE " definitions are not currently supported", CRITICAL);}
  if (code.empty()) return {std::vector<AST>{}, code.end()};
  std::vector<AST> tl_nodes;
  const auto end = code.end();
  std::vector<std::string> annotations;
  for (auto it = code.begin(); it != end; ++it) {
    std::string_view tok = it->data;
    switch (tok.front()) {
      case ';': llvm::outs() << ";\n"; break;
      case '@': annotations.push_back(std::string(tok.substr(1))); break;
      case 'c':
        if (tok == "cr") {annotations.clear(); UNSUPPORTED("coroutine")}
        else goto TL_DEFAULT;
        break;
      case 'm':
        if (tok == "module") {
          if (annotations.size()) {
            flags.onerror(it->loc, "annotations cannot be applied to a module", ERROR);
            annotations.clear();
          }
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
              auto [t, it2] = parse_type({it, end}, flags, "=");
              it = it2;
              auto [ast, it3] = parse_expr({it + 1, end}, flags);
              it = it2;
              val = AST::create<ast::cast_ast>(start, t, std::move(ast));
            }
            else {
              auto [ast, it2] = parse_expr({it + 1, end}, flags);
              val = std::move(ast);
              it = it2;
            }
          }
          else {
            auto start = (it - 1)->loc;
            ++it;
            auto [t, it2] = parse_type({it, end}, flags, "=");
            it = it2;
            auto [ast, it3] = parse_expr({it + 1, end}, flags);
            it = it3;
            val = AST::create<ast::cast_ast>(start, t, std::move(ast));
          }
          tl_nodes.push_back(AST::create<ast::mutdef_ast>(start, sstring::get(name), std::move(val), std::exchange(annotations, {})));
        }
        else if (tok == "mixin") {annotations.clear(); UNSUPPORTED("mixin")}
        else goto TL_DEFAULT;
        break;
      case 's':
        if (tok == "struct") {annotations.clear(); UNSUPPORTED("struct")}
        else goto TL_DEFAULT;
        break;
      case 'f':
        if (tok == "fn") {
          std::string name;
          uint8_t lwp = 2; // last was period; 0=false, 1=true, 2=start
          bool graceful;
          auto start = it->loc;
          while (++it < end) {
            std::string_view tok = it->data;
            switch (tok.front()) {
              case '"': flags.onerror(it->loc, "function name cannot contain a string literal", ERROR); goto FNDEF_END;
              case '\'': flags.onerror(it->loc, "function name cannot contain a character literal", ERROR); goto FNDEF_END;
              case '0':
              case '1':
                flags.onerror(it->loc, "function name cannot contain a numeric literal", ERROR);
                goto FNDEF_END;
              case '.':
                if (lwp == 1) flags.onerror(it->loc, "function name cannot contain consecutive periods", ERROR);
                else name.push_back('.');
                lwp = 1;
                break;
              case ')':
              case '[':
              case ']':
              case '{':
              case '}':
              case ';':
              case ',':
              case '=':
              case ':':
                flags.onerror(it->loc, (llvm::Twine("invalid character '") + tok + "' in function name").str(), ERROR);
                goto FNDEF_END;
              case '(':
                graceful = true;
                goto FNDEF_END;
              default:
                if (lwp == 0) {
                  flags.onerror(it->loc, "function name cannot contain consecutive identifiers, did you forget a period?", ERROR);
                  name.push_back('.');
                }
                lwp = 0;
                name += tok;
                break;
            }
          }
          FNDEF_END:;
          if (name.empty()) flags.onerror(start, "anonymous functions will be ignored", WARNING);
          if (graceful) {
            std::vector<std::pair<sstring, sstring>> params;
            graceful = false;
            while (++it != end) {
              auto tok = it->data;
              switch (tok.front()) {
                case '.':
                case '(':
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
                  flags.onerror(it->loc, (llvm::Twine("invalid parameter name '") + tok + "'").str(), ERROR);
                  goto PARAMS_END;
                case ')':
                  graceful = true;
                  goto PARAMS_END;
                case ':': {
                  auto [type, it2] = parse_type({it + 1, end}, flags, ",)");
                  it = it2;
                  params.push_back({sstring::get(name), type});
                  if (it == end) {
                    flags.onerror((it - 1)->loc, "unterminated function parameter list", ERROR);
                    return {std::move(tl_nodes), it};
                  }
                  std::string_view next = it->data;
                  switch (next.front()) {
                    case ',': break;
                    case ')': --it; break;
                    default:
                      flags.onerror(it->loc, "invalid character after type in parameter, did you forget a comma?", ERROR);
                      while (it != end && it->data.front() != ',' && it->data.front() != ')') ++it;
                  }
                } break;
                default: {
                  std::string_view name = it->data;
                  ++it;
                  switch (it->data.front()) {
                    case ':': break;
                    case '=':
                      flags.onerror(it->loc, "default parameters are not supported", ERROR);
                      break;
                    case '.':
                      flags.onerror(it->loc, "parameter name cannot be a path", ERROR);
                      break;
                    default:
                      flags.onerror(it->loc, "invalid consecutive identifiers in parameter name", ERROR);
                  }
                  auto [type, it2] = parse_type({it + 1, end}, flags, ",)");
                  it = it2;
                  params.push_back({sstring::get(name), type});
                  if (it == end) {
                    flags.onerror((it - 1)->loc, "unterminated function parameter list", ERROR);
                    return {std::move(tl_nodes), it};
                  }
                  std::string_view next = it->data;
                  switch (next.front()) {
                    case ',': break;
                    case ')': --it; break;
                    default:
                      flags.onerror(it->loc, "invalid character after type in parameter, did you forget a comma?", ERROR);
                      while (it != end && it->data.front() != ',' && it->data.front() != ')') ++it;
                  }
                } break;
              }
            }
            PARAMS_END:;
            auto return_type = sstring::get("<error>");
            if ((++it)->data.front() != ':') flags.onerror(it->loc, "functions must have an explicit return type", ERROR);
            else {
              auto [r, i] = parse_type({it + 1, end}, flags, "=");
              return_type = r;
              it = i;
            }
            if (it->data != "=") flags.onerror(it->loc, "function must have a body", ERROR);
            else {
              auto [ast, i] = parse_expr({it + 1, end}, flags);
              it = i;
              tl_nodes.push_back(AST::create<ast::fndef_ast>(start, sstring::get(name), return_type, std::move(params), std::move(ast), std::exchange(annotations, {})));
            }
          }
        }
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
              auto [t, it2] = parse_type({it, end}, flags, "=");
              it = it2;
              auto [ast, it3] = parse_expr({it + 1, end}, flags);
              it = it3;
              val = AST::create<ast::cast_ast>(start, t, std::move(ast));
            }
            else {
              auto [ast, it2] = parse_expr({it + 1, end}, flags);
              val = std::move(ast);
              it = it2;
            }
          }
          else {
            auto start = (it - 1)->loc;
            ++it;
            auto [t, it2] = parse_type({it, end}, flags, "=");
            it = it2;
            auto [ast, it3] = parse_expr({it + 1, end}, flags);
            it = it3;
            val = AST::create<ast::cast_ast>(start, t, std::move(ast));
          }
          tl_nodes.push_back(AST::create<ast::vardef_ast>(start, sstring::get(name), std::move(val), std::exchange(annotations, {})));
        }
        else goto TL_DEFAULT;
        break;
      case 'i':
        if (tok == "import") {
          if (annotations.size()) flags.onerror(it->loc, "annotations cannot be applied to an import statement", ERROR);
          location start = it->loc;
          for (auto& path : parse_paths(it, code.end(), flags)) tl_nodes.push_back(AST::create<ast::import_ast>(start, std::move(path)));
        }
        else goto TL_DEFAULT;
        break;
      case '}':
        if (annotations.size()) flags.onerror(it->loc, "annotations must have a target", ERROR);
        return {std::move(tl_nodes), it};
      default: TL_DEFAULT:
        annotations.clear();
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
