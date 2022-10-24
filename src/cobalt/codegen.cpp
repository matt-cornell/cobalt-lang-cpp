#include "cobalt/ast.hpp"
using namespace cobalt;
// flow.hpp
typed_value cobalt::ast::top_level_ast::codegen_impl(compile_context& ctx) const {
  for (auto const& ast : insts) ast(ctx);
  return nullval;
}
typed_value cobalt::ast::group_ast::codegen_impl(compile_context& ctx) const {
  for (auto const& ast : insts) ast(ctx);
  return nullval;
}
typed_value cobalt::ast::block_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::if_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::while_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::for_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
// funcs.hpp
typed_value cobalt::ast::cast_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::binop_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::unop_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::call_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::fndef_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
// literals.hpp
typed_value cobalt::ast::integer_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::float_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::string_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::char_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
// scope.hpp
typed_value cobalt::ast::module_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::import_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
// vars.hpp
typed_value cobalt::ast::vardef_ast::codegen_impl(compile_context& ctx) const {
  varmap* vm = ctx.vars;
  if (name.front() == '.') while (vm->parent) vm = vm->parent;
  std::size_t old = name.front() == '.', idx = name.find('.', 1);
  while (idx != std::string::npos) {
    auto local = name.substr(old + 1, idx - old - 1);
    auto it = vm->symbols.find(sstring::get(local));
    old = idx;
    idx = name.find('.', old);
    if (idx == std::string::npos) {if (ptr) flags.onerror(loc, (llvm::Twine("redefinition of ") + name).str(), ERROR);}
    else {
      if (it == vm->symbols.end()) {
        ptr = new varmap{vm};
        vm->insert(sstring::get(local), ptr);
      }
      else {
        auto idx = ptr->index();
        if (idx == 3) vm = ptr->get<3>();
        else flags.onerror(loc, (llvm::Twine(name.substr(idx)) + " is not a module").str(), ERROR);
      }
    }
  }
  auto local = name.substr(old + 1);
  auto tv = val(ctx);
  vm->insert(sstring::get(local), variable{tv, false});
  return tv;
}
typed_value cobalt::ast::mutdef_ast::codegen_impl(compile_context& ctx) const {
  varmap* vm = ctx.vars;
  if (name.front() == '.') while (vm->parent) vm = vm->parent;
  std::size_t old = name.front() == '.', idx = name.find('.', 1);
  while (idx != std::string::npos) {
    auto local = name.substr(old + 1, idx - old - 1);
    auto ptr = vm->get(local);
    old = idx;
    idx = name.find('.', old);
    if (idx == std::string::npos) {if (ptr) flags.onerror(loc, (llvm::Twine("redefinition of ") + name).str(), ERROR);}
    else {
      if (ptr) {
        auto idx = ptr->index();
        if (idx == 3) vm = ptr->get<3>();
        else flags.onerror(loc, (llvm::Twine(name.substr(idx)) + " is not a module").str(), ERROR);
      }
      else {
        ptr = new varmap{vm};
        vm->insert(sstring::get(local), ptr);
      }
    }
  }
  auto local = name.substr(old + 1);
  auto tv = val(ctx);
  auto a = ctx.builder.CreateAlloca(tv.type->llvm_type(loc, ctx));
  ctx.builder.CreateStore(a, tv.value);
  vm->insert(sstring::get(local), variable{a, true});
  return tv;
}
typed_value cobalt::ast::varget_ast::codegen_impl(compile_context& ctx) const {
  varmap* vm = ctx.vars;
  if (name.front() == '.') while (vm->parent) vm = vm->parent;
  std::size_t old = name.front() == '.', idx = name.find('.', 1);
  while (idx != std::string::npos) {
    auto local = name.substr(old + 1, idx - old - 1);
    auto ptr = vm->get(local);
    old = idx;
    idx = name.find('.', old);
    if (ptr) {
      auto idx = ptr->index();
      if (idx == std::string::npos) {
        if (idx == 0) return ptr->get<0>();
        else flags.onerror(loc, (llvm::Twine(name.substr(old)) + " is not a variable").str(), ERROR);
      }
      else {
        if (idx == 3) vm = ptr->get<3>();
        else flags.onerror(loc, (llvm::Twine(name.substr(idx)) + " is not a module").str(), ERROR);
      }
    }
    else {
      ptr = new varmap{vm};
      vm->insert(sstring::get(local), ptr);
    }
  }
  return nullval;
}
