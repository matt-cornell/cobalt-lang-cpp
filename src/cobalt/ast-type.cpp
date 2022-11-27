#include "cobalt/ast.hpp"
#include "cobalt/types.hpp"
using namespace cobalt;
using enum types::type_base::kind_t;
const static auto f16 = sstring::get("f16"), f32 = sstring::get("f32"), f64 = sstring::get("f64"), f128 = sstring::get("f128"), isize = sstring::get("isize"), usize = sstring::get("usize"), bool_ = sstring::get("bool"), null = sstring::get("null");
type_ptr get_call(type_ptr t, std::vector<type_ptr> const& args) {
  switch (t->kind) {
    case REFERENCE: return get_call(static_cast<types::reference const*>(t)->base, args);
    case FUNCTION: return static_cast<types::function const*>(t)->ret;
    default: return nullptr;
  }
}
type_ptr get_sub(type_ptr t, std::vector<type_ptr> const& args) {
  switch (t->kind) {
    case REFERENCE: return args.size() == 1 && args.front()->kind == INTEGER ? get_sub(static_cast<types::reference const*>(t)->base, args) : nullptr;
    case ARRAY: return args.size() == 1 && args.front()->kind == INTEGER ? get_sub(static_cast<types::reference const*>(t)->base, args) : nullptr;
    case POINTER: return types::reference::get(static_cast<types::pointer const*>(t)->base);
    default: return nullptr;
  }
}
type_ptr get_unary(type_ptr t, sstring op) {
  if (op == "!") return types::integer::get(1);
  if (op == "p?" || op == "p!") return nullptr;
  switch (t->kind) {
    case INTEGER:
      if (op == "~") return t;
    case FLOAT:
      if (op == "+" || op == "-" || op == "++" || op == "--") return t;
      return nullptr;
    case POINTER:
      if (op == "*") return types::reference::get(static_cast<types::pointer const*>(t)->base);
      if (op == "++" || op == "--") return t;
      return nullptr;
      break;
    case REFERENCE:
      if (op == "&") return types::pointer::get(static_cast<types::reference const*>(t)->base);
      return get_unary(static_cast<types::reference const*>(t), op);
    case ARRAY: return nullptr;
    case NULLTYPE: return nullptr;
    case FUNCTION:
      if (op == "&") return t;
      return nullptr;
    case CUSTOM: return nullptr;
  }
}
type_ptr get_binary(type_ptr lhs, type_ptr rhs, sstring op) {
  if (op == "&&" || op == "||") return rhs;
  switch (lhs->kind) {
    case INTEGER: switch (rhs->kind) {
      case INTEGER:
        if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
          auto l = static_cast<types::integer const*>(lhs)->nbits, r = static_cast<types::integer const*>(rhs)->nbits;
          if (l < 0) {
            if (r < 0) return types::integer::get(l < r ? -l : -r, true);
            else return types::integer::get(-l < r ? r : -l, false);
          }
          else {
            if (r < 0) return types::integer::get(l < -r ? -r : l, false);
            else return types::integer::get(l < r ? r : l, false);
          }
        }
        if (op == "&" || op == "|" || op == "^") {
          auto l = static_cast<types::integer const*>(lhs)->nbits, r = static_cast<types::integer const*>(rhs)->nbits;
          if (l < 0) l = -l;
          if (r < 0) r = -r;
          return types::integer::get(l < r ? r : l, true);
        }
        if (op == "<<" || op == ">>") return lhs;
      case FLOAT:
        if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%" || op == "^^") return rhs;
        return nullptr;
      case POINTER:
        if (op == "+" || op == "-") return rhs;
        return nullptr;
      case REFERENCE: return get_binary(lhs, static_cast<types::reference const*>(rhs)->base, op);
      case ARRAY: return nullptr;
      case FUNCTION: return nullptr;
      case NULLTYPE: return nullptr;
      case CUSTOM: return nullptr;
    }
    case FLOAT: switch (rhs->kind) {
      case INTEGER:
        if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%" || op == "^^") return lhs;
        return nullptr;
      case FLOAT:
        if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%" || op == "^^") return lhs->size() < rhs->size() ? rhs : lhs;
        return nullptr;
      case POINTER: return nullptr;
      case REFERENCE: return get_binary(lhs, static_cast<types::reference const*>(rhs)->base, op);
      case ARRAY: return nullptr;
      case FUNCTION: return nullptr;
      case NULLTYPE: return nullptr;
      case CUSTOM: return nullptr;
    }
    case POINTER: switch (rhs->kind) {
      case INTEGER:
        if (op == "+" || op == "-") return lhs;
        return nullptr;
      case FLOAT: return nullptr;
      case POINTER:
        if (op == "-") return types::integer::get(sizeof(void*) * 8);
        return nullptr;
      case REFERENCE: return get_binary(lhs, static_cast<types::reference const*>(rhs)->base, op);
      case ARRAY: return nullptr;
      case FUNCTION: return nullptr;
      case NULLTYPE: return nullptr;
      case CUSTOM: return nullptr;
    }
    case REFERENCE: return get_binary(static_cast<types::reference const*>(lhs)->base, rhs, op);
    case ARRAY: return nullptr;
    case FUNCTION: return nullptr;
    case NULLTYPE: return nullptr;
    case CUSTOM: return nullptr;
  }
}
static type_ptr get_call(type_ptr self, std::vector<type_ptr>&& args) {
  switch (self->kind) {
    case INTEGER:
    case FLOAT:
    case POINTER:
    case NULLTYPE:
    case ARRAY:
      return nullptr;
    case REFERENCE: return get_call(static_cast<types::reference const*>(self)->base, args);
    case FUNCTION: return static_cast<types::function const*>(self)->ret;
    case CUSTOM: return nullptr;
  }
}
static type_ptr parse_type(sstring str) {
  if (str.empty()) return nullptr;
  switch (str.back()) {
    case '&': return types::reference::get(parse_type(sstring::get(str.substr(0, str.size() - 1))));
    case '*': return types::pointer::get(parse_type(sstring::get(str.substr(0, str.size() - 1))));
    case '^': return types::borrow::get(parse_type(sstring::get(str.substr(0, str.size() - 1))));
    case ']': {
      std::size_t depth = 1;
      auto it = &str.back();
      for (; depth && it != str.data(); --it) switch (*it) {
        case '[': --depth; break;
        case ']': ++depth; break;
      }
      if (depth) return nullptr;
      if (it == &str.back() - 2) return types::array::get(parse_type(sstring::get(str.substr(0, it - str.data() + 1))), -1);
      std::size_t len = 0;
      for (auto it2 = it + 2; it2 != &str.back(); ++it2) {
        if (*it2 < '0' || *it2 > '9') return nullptr;
        len *= 10;
        len += *it2 - '0';
      }
      return types::array::get(parse_type(sstring::get(str.substr(0, it - str.data() + 1))), len);
    };
  }
  if (str == bool_) return types::integer::get(1);
  if (str == null) return types::null::get();
  switch (str.front()) {
    case 'i':
      if (str == isize) return types::integer::get(sizeof(void*) * 8, false);
      if (str.size() > 1 && str.find_first_not_of("0123456789", 1) == std::string::npos) {
        unsigned width = 0;
        for (char c : str.substr(1)) {
          width *= 10;
          width += c - '0';
        }
        return width ? types::integer::get(width, false) : nullptr;
      }
    case 'u':
      if (str == usize) return types::integer::get(sizeof(void*) * 8, true);
      if (str.size() > 1 && str.find_first_not_of("0123456789", 1) == std::string::npos) {
        unsigned width = 0;
        for (char c : str.substr(1)) {
          width *= 10;
          width += c - '0';
        }
        return width ? types::integer::get(width, true) : nullptr;
      }
    case 'f':
      if (str == f16) return types::float16::get();
      if (str == f32) return types::float32::get();
      if (str == f64) return types::float64::get();
      if (str == f128) return types::float128::get();
  }
  return nullptr;
}
// flow.hpp
type_ptr cobalt::ast::top_level_ast::type(base_context& ctx) const {(void)ctx; return nullptr;}
type_ptr cobalt::ast::group_ast::type(base_context& ctx) const {return insts.empty() ? nullptr : insts.back().type(ctx);}
type_ptr cobalt::ast::block_ast::type(base_context& ctx) const {return insts.empty() ? nullptr : insts.back().type(ctx);}
type_ptr cobalt::ast::if_ast::type(base_context& ctx) const {auto t = if_true.type(ctx); return t == if_false.type(ctx) ? t : nullptr;}
type_ptr cobalt::ast::while_ast::type(base_context& ctx) const {return body.type(ctx);}
type_ptr cobalt::ast::for_ast::type(base_context& ctx) const {return body.type(ctx);}
// funcs.hpp
type_ptr cobalt::ast::cast_ast::type(base_context& ctx) const {(void)ctx; return parse_type(target);}
type_ptr cobalt::ast::binop_ast::type(base_context& ctx) const {
  auto l = lhs.type(ctx);
  if (!l) return nullptr;
  auto r = rhs.type(ctx);
  if (!r) return nullptr;
  return get_binary(l, r, op);
}
type_ptr cobalt::ast::unop_ast::type(base_context& ctx) const {
  auto t = val.type(ctx);
  if (!t) return nullptr;
  return get_unary(t, op);
}
type_ptr cobalt::ast::subscr_ast::type(base_context& ctx) const {
  std::vector<type_ptr> targs;
  targs.reserve(args.size());
  for (auto const& arg : args) {
    auto t = arg.type(ctx);
    if (!t) return nullptr;
    targs.push_back(t);
  }
  auto t = val.type(ctx);
  return get_sub(t, targs);
}
type_ptr cobalt::ast::call_ast::type(base_context& ctx) const {
  std::vector<type_ptr> targs;
  targs.reserve(args.size());
  for (auto const& arg : args) {
    auto t = arg.type(ctx);
    if (!t) return nullptr;
    targs.push_back(t);
  }
  auto t = val.type(ctx);
  return get_call(t, targs);
}
type_ptr cobalt::ast::fndef_ast::type(base_context& ctx) const {(void)ctx; return nullptr;}
// keyvals.hpp
type_ptr cobalt::ast::null_ast::type(base_context& ctx) const {(void)ctx; return types::null::get();}
// literals.hpp
type_ptr cobalt::ast::integer_ast::type(base_context& ctx) const {
  if (suffix.empty()) return types::integer::get(0);
  if (!(suffix.front() == 'i' || suffix.front() == 'u')) return nullptr;
  if (suffix == isize) return types::integer::get(sizeof(void*) * 8, false);
  if (suffix == usize) return types::integer::get(sizeof(void*) * 8, true);
  if (suffix.find_first_not_of("0123456789", 1) != std::string::npos) return nullptr;
  unsigned width = 0;
  for (char c : suffix.substr(1)) {
    width *= 10;
    width += c - '0';
  }
  return types::integer::get(width, suffix.front() == 'u');
}
type_ptr cobalt::ast::float_ast::type(base_context& ctx) const {
  if (suffix.empty()) return types::float64::get();
  if (suffix.front() != 'f') return nullptr;
  if (suffix == f16) return types::float16::get();
  if (suffix == f32) return types::float32::get();
  if (suffix == f64) return types::float64::get();
  if (suffix == f128) return types::float128::get();
  return nullptr;
}
type_ptr cobalt::ast::string_ast::type(base_context& ctx) const {(void)ctx; return suffix.empty() ? types::pointer::get(types::integer::get(8)) : nullptr;}
type_ptr cobalt::ast::char_ast::type(base_context& ctx) const {(void)ctx; return suffix.empty() ?  types::integer::get(32) : nullptr;}
// scope.hpp
type_ptr cobalt::ast::module_ast::type(base_context& ctx) const {(void)ctx; return nullptr;}
type_ptr cobalt::ast::import_ast::type(base_context& ctx) const {(void)ctx; return nullptr;}
// vars.hpp
type_ptr cobalt::ast::vardef_ast::type(base_context& ctx) const {(void)ctx; return nullptr;}
type_ptr cobalt::ast::mutdef_ast::type(base_context& ctx) const {(void)ctx; return nullptr;}
type_ptr cobalt::ast::varget_ast::type(base_context& ctx) const {
  varmap* vm = ctx.vars;
  if (name.front() == '.') while (vm->parent) vm = vm->parent;
  std::size_t old = name.front() == '.', idx = name.find('.', 1);
  while (idx != std::string::npos) {
    auto local = name.substr(old + 1, idx - old - 1);
    auto ptr = vm->get(sstring::get(local));
    old = idx;
    idx = name.find('.', old);
    if (ptr) {
      auto pidx = ptr->index();
      if (idx == std::string::npos) return pidx == 0 ? std::get<0>(*ptr).type : nullptr;
      else {
        if (pidx == 2) vm = std::get<2>(*ptr).get();
        else return nullptr;
      }
    }
    else return nullptr;
  }
  return nullptr;
}
