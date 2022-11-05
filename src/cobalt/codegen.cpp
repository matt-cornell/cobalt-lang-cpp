#include "cobalt/ast.hpp"
#include "cobalt/context.hpp"
#include "cobalt/varmap.hpp"
#include "cobalt/types.hpp"
using namespace cobalt;
// flow.hpp
typed_value cobalt::ast::top_level_ast::codegen(compile_context& ctx) const {
  for (auto const& ast : insts) ast(ctx);
  return nullval;
}
typed_value cobalt::ast::group_ast::codegen(compile_context& ctx) const {
  for (auto const& ast : insts) ast(ctx);
  return nullval;
}
typed_value cobalt::ast::block_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::if_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::while_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::for_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
// funcs.hpp
typed_value cobalt::ast::cast_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::binop_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::unop_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::subscr_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::call_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::fndef_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
// literals.hpp
typed_value cobalt::ast::integer_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::float_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::string_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::char_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
// scope.hpp
typed_value cobalt::ast::module_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::import_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
// vars.hpp
typed_value cobalt::ast::vardef_ast::codegen(compile_context& ctx) const {
  varmap* vm = ctx.vars;
  if (name.front() == '.') while (vm->parent) vm = vm->parent;
  std::size_t old = name.front() == '.', idx = name.find('.', 1);
  while (idx != std::string::npos) {
    auto local = name.substr(old + 1, idx - old - 1);
    auto ss = sstring::get(local);
    auto it = vm->symbols.find(ss);
    old = idx;
    idx = name.find('.', old);
    if (idx == std::string::npos) {
      if (it != vm->symbols.end()) ctx.flags.onerror(loc, "redefinition of " + name, ERROR);
      else break;
    }
    else {
      if (it == vm->symbols.end()) {
        if (it->second.index() == 3) vm = std::get<3>(it->second).get();
        else {
          ctx.flags.onerror(loc, name.substr(0, idx) + " is not a module", ERROR);
          return nullval;
        }
      }
      else {
        if (it->second.index() == 3) vm = std::get<3>(vm->symbols.insert({ss, symbol_type(std::make_shared<varmap>(std::get<3>(it->second).get()))}).first->second).get();
        else {
          ctx.flags.onerror(loc, name.substr(0, idx) + " is not a module", ERROR);
          return nullval;
        }
      }
    }
  }
  auto local = name.substr(old + 1);
  auto tv = val(ctx);
  vm->insert(sstring::get(local), variable{tv, false});
  return tv;
}
typed_value cobalt::ast::mutdef_ast::codegen(compile_context& ctx) const {
  varmap* vm = ctx.vars;
  if (name.front() == '.') while (vm->parent) vm = vm->parent;
  std::size_t old = name.front() == '.', idx = name.find('.', 1);
  while (idx != std::string::npos) {
    auto local = name.substr(old + 1, idx - old - 1);
    auto ss = sstring::get(local);
    auto it = vm->symbols.find(ss);
    old = idx;
    idx = name.find('.', old);
    if (idx == std::string::npos) {
      if (it != vm->symbols.end()) ctx.flags.onerror(loc, "redefinition of " + name, ERROR);
      else break;
    }
    else {
      if (it == vm->symbols.end()) {
        if (it->second.index() == 3) vm = std::get<3>(it->second).get();
        else {
          ctx.flags.onerror(loc, name.substr(0, idx) + " is not a module", ERROR);
          return nullval;
        }
      }
      else {
        if (it->second.index() == 3) vm = std::get<3>(vm->symbols.insert({ss, symbol_type(std::make_shared<varmap>(std::get<3>(it->second).get()))}).first->second).get();
        else {
          ctx.flags.onerror(loc, name.substr(0, idx) + " is not a module", ERROR);
          return nullval;
        }
      }
    }
  }
  auto local = name.substr(old + 1);
  auto tv = val(ctx);
  auto a = ctx.builder.CreateAlloca(tv.type->llvm_type(loc, ctx));
  ctx.builder.CreateStore(a, tv.value);
  vm->insert(sstring::get(local), variable{{a, tv.type}, true}); // TODO: return reference
  return tv;
}
typed_value cobalt::ast::varget_ast::codegen(compile_context& ctx) const {
  varmap* vm = ctx.vars;
  if (name.front() == '.') while (vm->parent) vm = vm->parent;
  std::size_t old = name.front() == '.', idx = name.find('.', 1);
  while (idx != std::string::npos) {
    auto local = name.substr(old + 1, idx - old - 1);
    auto ptr = vm->get(sstring::get(local));
    old = idx;
    idx = name.find('.', old);
    if (ptr) {
      auto idx = ptr->index();
      if (idx == std::string::npos) {
        if (idx == 0) {
          auto val = std::get<0>(*ptr);
          return val.is_mut ? typed_value{ctx.builder.CreateLoad(val.var.type->llvm_type(loc, ctx), val.var.value), val.var.type} : val.var;
        }
        else ctx.flags.onerror(loc, name.substr(0, old) + " is not a variable", ERROR);
      }
      else {
        if (idx == 3) vm = std::get<3>(*ptr).get();
        else ctx.flags.onerror(loc, name.substr(0, idx) + " is not a module", ERROR);
      }
    }
    else {
      ctx.flags.onerror(loc, name.substr(0, old) + " is not", ERROR);
      return nullval;
    }
  }
  return nullval;
}
