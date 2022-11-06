#include "cobalt/ast.hpp"
#include "cobalt/context.hpp"
#include "cobalt/varmap.hpp"
#include "cobalt/types.hpp"
using namespace cobalt;
// flow.hpp
const static auto f16 = sstring::get("f16"), f32 = sstring::get("f32"), f64 = sstring::get("f64"), f128 = sstring::get("f128"), isize = sstring::get("isize"), usize = sstring::get("usize");
typed_value cobalt::ast::top_level_ast::codegen(compile_context& ctx) const {
  for (auto const& ast : insts) ast(ctx);
  return nullval;
}
typed_value cobalt::ast::group_ast::codegen(compile_context& ctx) const {
  typed_value last {};
  for (auto const& ast : insts) last = ast(ctx);
  return last;
}
typed_value cobalt::ast::block_ast::codegen(compile_context& ctx) const {
  ctx.vars = new varmap(ctx.vars);
  typed_value last {};
  for (auto const& ast : insts) last = ast(ctx);
  auto vars = ctx.vars;
  ctx.vars = ctx.vars->parent;
  delete vars;
  return last;
}
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
typed_value cobalt::ast::integer_ast::codegen(compile_context& ctx) const {
  if (suffix.empty()) return {llvm::Constant::getIntegerValue(llvm::Type::getInt64Ty(*ctx.context), val), types::integer::get(64)};
  switch (suffix.front()) {
    case 'i':
      if (suffix == isize) return {llvm::Constant::getIntegerValue(llvm::Type::getIntNTy(*ctx.context, sizeof(void*) * 8), val), types::integer::get(sizeof(void*) * 8, false)};
      else if (suffix.find_first_not_of("012456789", 1) == std::string::npos) {
        unsigned width = 0;
        for (char c : suffix.substr(1)) {
          width *= 10;
          width += c - '0';
        }
        return {llvm::ConstantInt::get(llvm::Type::getIntNTy(*ctx.context, width), val), types::integer::get(width, false)};
      }
      else goto UNKNOWN;
    case 'u':
      if (suffix == usize) return {llvm::Constant::getIntegerValue(llvm::Type::getIntNTy(*ctx.context, sizeof(void*) * 8), val), types::integer::get(sizeof(void*) * 8, true)};
      else if (suffix.find_first_not_of("012456789", 1) == std::string::npos) {
        unsigned width = 0;
        for (char c : suffix.substr(1)) {
          width *= 10;
          width += c - '0';
        }
        return {llvm::ConstantInt::get(llvm::Type::getIntNTy(*ctx.context, width), val), types::integer::get(width, true)};
      }
      else goto UNKNOWN;
    default: UNKNOWN:
      ctx.flags.onerror(loc, (llvm::Twine("unknown suffix '") + suffix + "' on integer literal").str(), ERROR);
      return nullval;
  }
}
typed_value cobalt::ast::float_ast::codegen(compile_context& ctx) const {
  if (suffix.empty()) return {llvm::ConstantFP::get(llvm::Type::getDoubleTy(*ctx.context), val), types::float64::get()};
  if (suffix.front() == 'f' && suffix.find_first_not_of("0123456789") == std::string::npos) {
    if (suffix == f16) return {llvm::ConstantFP::get(llvm::Type::getHalfTy(*ctx.context), val), types::float16::get()};
    else if (suffix == f32) return {llvm::ConstantFP::get(llvm::Type::getFloatTy(*ctx.context), val), types::float32::get()};
    else if (suffix == f64) return {llvm::ConstantFP::get(llvm::Type::getDoubleTy(*ctx.context), val), types::float64::get()};
    else if (suffix == f128) return {llvm::ConstantFP::get(llvm::Type::getFP128Ty(*ctx.context), val), types::float128::get()};
    else {
      ctx.flags.onerror(loc, "invalid floating point suffix, only f16, f32, f64, and f128 are allowed", ERROR);
      return nullval;
    }
  }
  else {
    ctx.flags.onerror(loc, (llvm::Twine("unknown suffix '") + suffix + "' on integer literal").str(), ERROR);
    return nullval;
  }
}
typed_value cobalt::ast::string_ast::codegen(compile_context& ctx) const {
  std::vector<llvm::Constant*> elems;
  elems.reserve(val.size() + 1);
  auto i8_ty = llvm::Type::getInt8Ty(*ctx.context);
  for (char c : val) elems.push_back(llvm::ConstantInt::get(i8_ty, c));
  elems.push_back(llvm::ConstantInt::get(i8_ty, 0));
  return {llvm::ConstantArray::get(llvm::ArrayType::get(i8_ty, 0), elems), types::pointer::get(types::integer::get(8))};
}
typed_value cobalt::ast::char_ast::codegen(compile_context& ctx) const {
  if (!suffix.empty()) {
    ctx.flags.onerror(loc, "character literal cannot have a suffix", ERROR);
    return nullval;
  }
  llvm::APInt out(32, 0, true);
  std::size_t shift = 0;
  for (char c : val) {
    out |= (unsigned char)c << shift;
    shift += 4;
  }
  return {llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx.context), out), types::integer::get(32)};
}
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
  if (global) {
    if (val.is_const()) {
      auto tv = val(ctx);
      auto gv = new llvm::GlobalVariable(*ctx.module, tv.type->llvm_type(loc, ctx), true, llvm::GlobalValue::LinkageTypes::ExternalLinkage, llvm::cast<llvm::Constant>(tv.value), name);
      auto type = types::reference::get(tv.type);
      vm->insert(sstring::get(local), variable{{gv, type}, true});
      return {gv, type};
    }
    else {
      auto t = llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx.context), false);
      if (!t) return nullval;
      auto f = llvm::Function::Create(t, llvm::GlobalValue::LinkageTypes::PrivateLinkage, "global.init." + llvm::Twine(ctx.init_count++), ctx.module.get());
      if (!f) return nullval;
      auto gv = new llvm::GlobalVariable(*ctx.module, val.type(ctx)->llvm_type(loc, ctx), true, llvm::GlobalValue::LinkageTypes::ExternalLinkage, nullptr, name);
      auto bb = llvm::BasicBlock::Create(*ctx.context, "entry", f);
      ctx.builder.SetInsertPoint(bb);
      auto tv = val(ctx);
      ctx.builder.CreateStore(gv, tv.value);
      ctx.builder.SetInsertPoint((llvm::BasicBlock*)nullptr);
      auto type = types::reference::get(tv.type);
      vm->insert(sstring::get(local), variable{{gv, type}, true});
      return {gv, type};
    }
  }
  else {
    auto tv = val(ctx);
    vm->insert(sstring::get(local), variable{tv, false});
    return tv;
  }
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
  if (global) {
    if (val.is_const()) {
      auto tv = val(ctx);
      auto gv = new llvm::GlobalVariable(*ctx.module, tv.type->llvm_type(loc, ctx), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, llvm::cast<llvm::Constant>(tv.value), name);
      auto type = types::reference::get(tv.type);
      vm->insert(sstring::get(local), variable{{gv, type}, true});
      return {gv, type};
    }
    else {
      auto t = llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx.context), false);
      if (!t) return nullval;
      auto f = llvm::Function::Create(t, llvm::GlobalValue::LinkageTypes::PrivateLinkage, "global.init." + llvm::Twine(ctx.init_count++), ctx.module.get());
      if (!f) return nullval;
      auto gv = new llvm::GlobalVariable(*ctx.module, val.type(ctx)->llvm_type(loc, ctx), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, nullptr, name);
      auto bb = llvm::BasicBlock::Create(*ctx.context, "entry", f);
      ctx.builder.SetInsertPoint(bb);
      auto tv = val(ctx);
      ctx.builder.CreateStore(gv, tv.value);
      ctx.builder.SetInsertPoint((llvm::BasicBlock*)nullptr);
      auto type = types::reference::get(tv.type);
      vm->insert(sstring::get(local), variable{{gv, type}, true});
      return {gv, type};
    }
  }
  else {
    auto tv = val(ctx);
    auto a = ctx.builder.CreateAlloca(tv.type->llvm_type(loc, ctx));
    ctx.builder.CreateStore(a, tv.value);
    auto type = types::reference::get(tv.type);
    vm->insert(sstring::get(local), variable{{a, type}, true});
    return {a, type};
  }
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
