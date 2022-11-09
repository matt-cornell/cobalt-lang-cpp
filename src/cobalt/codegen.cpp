#include "cobalt/ast.hpp"
#include "cobalt/context.hpp"
#include "cobalt/varmap.hpp"
#include "cobalt/types.hpp"
using namespace cobalt;
using enum types::type_base::kind_t;
const static auto f16 = sstring::get("f16"), f32 = sstring::get("f32"), f64 = sstring::get("f64"), f128 = sstring::get("f128"), isize = sstring::get("isize"), usize = sstring::get("usize");
static type_ptr parse_type(sstring str) {
  if (str.empty()) return nullptr;
  switch (str.back()) {
    case '&': return types::reference::get(parse_type(sstring::get(str.substr(0, str.size() - 1))));
    case '*': return types::pointer::get(parse_type(sstring::get(str.substr(0, str.size() - 1))));
    case '^': return types::borrow::get(parse_type(sstring::get(str.substr(0, str.size() - 1))));
  }
  switch (str.front()) {
    case 'i':
      if (str == isize) return types::integer::get(sizeof(void*) * 8, false);
      if (str.find_first_not_of("0123456789", 1) == std::string::npos) {
        unsigned width = 0;
        for (char c : str.substr(1)) {
          width *= 10;
          width += c - '0';
        }
        return types::integer::get(width, false);
      }
    case 'u':
      if (str == usize) return types::integer::get(sizeof(void*) * 8, true);
      if (str.find_first_not_of("0123456789", 1) == std::string::npos) {
        unsigned width = 0;
        for (char c : str.substr(1)) {
          width *= 10;
          width += c - '0';
        }
        return types::integer::get(width, true);
      }
    case 'f':
      if (str == f16) return types::float16::get();
      if (str == f32) return types::float32::get();
      if (str == f64) return types::float64::get();
      if (str == f128) return types::float128::get();
  }
  return nullptr;
}
static llvm::Value* impl_convert(llvm::Value* v, type_ptr t1, type_ptr t2, location loc, compile_context& ctx) {
  if (!(t1 && t2)) return nullptr;
  if (t1 == t2) return v;
  switch (t1->kind) {
    case INTEGER: switch (t2->kind) {
      case INTEGER: {
        auto lb = static_cast<types::integer const*>(t1)->nbits, rb = static_cast<types::integer const*>(t2)->nbits;
        if (lb < 0) {
          if (rb < 0) {
            if (rb < lb) return ctx.builder.CreateZExt(v, t2->llvm_type(loc, ctx));
            else {
              ctx.flags.onerror(loc, (llvm::Twine("implicit conversion from ") + t1->name() + " to " + t2->name() + " loses precision").str(), WARNING);
              return ctx.builder.CreateTrunc(v, t2->llvm_type(loc, ctx));
            }
          }
          else {
            if (rb > -lb) return ctx.builder.CreateZExt(v, t2->llvm_type(loc, ctx));
            else if (rb < -lb) {
              ctx.flags.onerror(loc, (llvm::Twine("implicit conversion from ") + t1->name() + " to " + t2->name() + " loses precision").str(), WARNING);
              return ctx.builder.CreateTrunc(v, t2->llvm_type(loc, ctx));
            }
            else {
              ctx.flags.onerror(loc, (llvm::Twine("implicit conversion from ") + t1->name() + " to " + t2->name() + " changes sign").str(), WARNING);
              return v;
            }
          }
        }
        else {
          if (rb < 0) {
            if (-rb > lb) {
              ctx.flags.onerror(loc, (llvm::Twine("implicit conversion from ") + t1->name() + " to " + t2->name() + " changes sign").str(), WARNING);
              return ctx.builder.CreateZExt(v, t2->llvm_type(loc, ctx));
            }
            else if (-rb < lb) {
              ctx.flags.onerror(loc, (llvm::Twine("implicit conversion from ") + t1->name() + " to " + t2->name() + " loses precision").str(), WARNING);
              return ctx.builder.CreateTrunc(v, t2->llvm_type(loc, ctx));
            }
            else {
              ctx.flags.onerror(loc, (llvm::Twine("implicit conversion from ") + t1->name() + " to " + t2->name() + " changes sign").str(), WARNING);
              return v;
            }
          }
          else {
            if (rb > lb) return ctx.builder.CreateSExt(v, t2->llvm_type(loc, ctx));
            else {
              ctx.flags.onerror(loc, (llvm::Twine("implicit conversion from ") + t1->name() + " to " + t2->name() + " loses precision").str(), WARNING);
              return ctx.builder.CreateTrunc(v, t2->llvm_type(loc, ctx));
            }
          }
        }
      }
      case FLOAT: {
        auto bits = static_cast<types::integer const*>(t1)->nbits;
        if (bits < 0) return ctx.builder.CreateUIToFP(v, t2->llvm_type(loc, ctx));
        else return ctx.builder.CreateSIToFP(v, t2->llvm_type(loc, ctx));
      }
      case POINTER: return nullptr;
      case REFERENCE: return nullptr;
      case CUSTOM: return nullptr;
    }
    case FLOAT: switch (t2->kind) {
      case INTEGER: return nullptr;
      case FLOAT:
        if (t2->size() > t1->size()) return ctx.builder.CreateFPExt(v, t2->llvm_type(loc, ctx));
        else {
          ctx.flags.onerror(loc, (llvm::Twine("implicit conversion from ") + t1->name() + " to " + t2->name() + " loses precision").str(), WARNING);
          return ctx.builder.CreateFPTrunc(v, t2->llvm_type(loc, ctx));
        }
      case POINTER: return nullptr;
      case REFERENCE: return nullptr;
      case CUSTOM: return nullptr;
    }
    case POINTER: return nullptr;
    case REFERENCE: {
      auto t = static_cast<types::reference const*>(t1)->base;
      return impl_convert(ctx.builder.CreateLoad(t->llvm_type(loc, ctx), v), t, t2, loc, ctx);
    }
    case CUSTOM: return nullptr;
  }
}
static llvm::Value* expl_convert(llvm::Value* v, type_ptr t1, type_ptr t2, location loc, compile_context& ctx) {
  if (!(t1 && t2)) return nullptr;
  if (t1 == t2) return v;
  switch (t1->kind) {
    case INTEGER: switch (t2->kind) {
      case INTEGER: {
        auto lb = static_cast<types::integer const*>(t1)->nbits, rb = static_cast<types::integer const*>(t2)->nbits;
        if (lb < 0) {
          if (rb < 0) {
            if (rb < lb) return ctx.builder.CreateZExt(v, t2->llvm_type(loc, ctx));
            else return ctx.builder.CreateTrunc(v, t2->llvm_type(loc, ctx));
          }
          else {
            if (rb > -lb) return ctx.builder.CreateZExt(v, t2->llvm_type(loc, ctx));
            else if (rb < -lb) return ctx.builder.CreateTrunc(v, t2->llvm_type(loc, ctx));
            else return v;
          }
        }
        else {
          if (rb < 0) {
            if (-rb > lb) return ctx.builder.CreateZExt(v, t2->llvm_type(loc, ctx));
            else if (-rb < lb) return ctx.builder.CreateTrunc(v, t2->llvm_type(loc, ctx));
            else return v;
          }
          else {
            if (rb > lb) return ctx.builder.CreateSExt(v, t2->llvm_type(loc, ctx));
            else return ctx.builder.CreateTrunc(v, t2->llvm_type(loc, ctx));
          }
        }
      }
      case FLOAT:
        if (static_cast<types::integer const*>(t1)->nbits < 0) return ctx.builder.CreateUIToFP(v, t2->llvm_type(loc, ctx));
        else return ctx.builder.CreateSIToFP(v, t2->llvm_type(loc, ctx));
      case POINTER: return ctx.builder.CreateIntToPtr(v, t2->llvm_type(loc, ctx));
      case REFERENCE: return nullptr;
      case CUSTOM: return nullptr;
    }
    case FLOAT: switch (t2->kind) {
      case INTEGER:
        if (static_cast<types::integer const*>(t1)->nbits < 0) return ctx.builder.CreateFPToUI(v, t2->llvm_type(loc, ctx));
        else return ctx.builder.CreateFPToSI(v, t2->llvm_type(loc, ctx));
      case FLOAT:
        if (t2->size() > t1->size()) return ctx.builder.CreateFPExt(v, t2->llvm_type(loc, ctx));
        else return ctx.builder.CreateFPTrunc(v, t2->llvm_type(loc, ctx));
      case POINTER: return nullptr;
      case REFERENCE: return nullptr;
      case CUSTOM: return nullptr;
    }
    case POINTER: switch (t2->kind) {
      case INTEGER: return ctx.builder.CreatePtrToInt(v, t2->llvm_type(loc, ctx));
      case FLOAT: return nullptr;
      case POINTER: return ctx.builder.CreateBitCast(v, t2->llvm_type(loc, ctx));
      case REFERENCE: return nullptr;
      case CUSTOM: return nullptr;
    }
    case REFERENCE: {
      auto t = static_cast<types::reference const*>(t1)->base;
      return impl_convert(ctx.builder.CreateLoad(t->llvm_type(loc, ctx), v), t, t2, loc, ctx);
    }
    case CUSTOM: return nullptr;
  }
}
// flow.hpp
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
typed_value cobalt::ast::cast_ast::codegen(compile_context& ctx) const {
  auto t = parse_type(target);
  if (!t) {
    ctx.flags.onerror(loc, (llvm::Twine("invalid type name '") + target + "' in cast").str(), ERROR);
    return nullval;
  }
  auto tv = val(ctx);
  auto v = expl_convert(tv.value, tv.type, t, loc, ctx);
  if (!v) {
    ctx.flags.onerror(loc, (llvm::Twine("cannot convert value of type '" + tv.type->name() + "' to '" + t->name() + "'")).str(), ERROR);
    return nullval;
  }
  return {v, t};
}
typed_value cobalt::ast::binop_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::unop_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::subscr_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::call_ast::codegen(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::fndef_ast::codegen(compile_context& ctx) const {
  std::vector<llvm::Type*> params_t;
  std::vector<type_ptr> args_t;
  params_t.reserve(args.size());
  args_t.reserve(args.size());
  for (auto [_, type] : args) {
    auto t = parse_type(type);
    if (!t) {
      ctx.flags.onerror(loc, (llvm::Twine("invalid type name '") + type + "' for function parameter").str(), ERROR);
      return nullval;
    }
    args_t.push_back(t);
    params_t.push_back(t->llvm_type(loc, ctx));
  }
  auto t = parse_type(ret);
  if (!t) {
    ctx.flags.onerror(loc, (llvm::Twine("invalid type name '") + ret + "' for function return types").str(), ERROR);
    return nullval;
  }
  auto ft = llvm::FunctionType::get(t->llvm_type(loc, ctx), params_t, false);
  auto f = llvm::Function::Create(ft, llvm::GlobalValue::ExternalLinkage, name, *ctx.module);
  if (!f) return nullval;
  auto bb = llvm::BasicBlock::Create(*ctx.context, "entry", f);
  auto ip = ctx.builder.GetInsertBlock();
  ctx.builder.SetInsertPoint(bb);
  ctx.vars = new varmap(ctx.vars);
  for (std::size_t i = 0; i < args.size(); ++i) ctx.vars->insert(args[i].first, typed_value{f->getArg(i), args_t[i]});
  auto tv = body(ctx);
  if (!tv.type) ctx.builder.CreateRet(llvm::Constant::getNullValue(t->llvm_type(loc, ctx)));
  else {
    auto p = impl_convert(tv.value, tv.type, t, loc, ctx);
    if (!p) ctx.builder.CreateRet(llvm::Constant::getNullValue(t->llvm_type(loc, ctx)));
    else ctx.builder.CreateRet(p);
  }
  auto vars = ctx.vars;
  ctx.vars = ctx.vars->parent;
  delete vars;
  return nullval;
}
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
  auto local = name.substr(old);
  if (global) {
    if (val.is_const()) {
      auto tv = val(ctx);
      auto gv = new llvm::GlobalVariable(*ctx.module, tv.type->llvm_type(loc, ctx), true, llvm::GlobalValue::LinkageTypes::ExternalLinkage, llvm::cast<llvm::Constant>(tv.value), name);
      auto type = types::reference::get(tv.type);
      vm->insert(sstring::get(local), typed_value{gv, type});
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
      vm->insert(sstring::get(local), typed_value{gv, type});
      return {gv, type};
    }
  }
  else {
    auto tv = val(ctx);
    vm->insert(sstring::get(local), tv);
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
  auto local = name.substr(old);
  if (global) {
    if (val.is_const()) {
      auto tv = val(ctx);
      auto gv = new llvm::GlobalVariable(*ctx.module, tv.type->llvm_type(loc, ctx), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, llvm::cast<llvm::Constant>(tv.value), name);
      auto type = types::reference::get(tv.type);
      vm->insert(sstring::get(local), typed_value{gv, type});
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
      ctx.builder.CreateStore(tv.value, gv);
      ctx.builder.SetInsertPoint((llvm::BasicBlock*)nullptr);
      auto type = types::reference::get(tv.type);
      vm->insert(sstring::get(local), typed_value{gv, type});
      return {gv, type};
    }
  }
  else {
    auto tv = val(ctx);
    auto a = ctx.builder.CreateAlloca(tv.type->llvm_type(loc, ctx));
    ctx.builder.CreateStore(tv.value, a);
    auto type = types::reference::get(tv.type);
    vm->insert(sstring::get(local), typed_value{a, type});
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
      auto pidx = ptr->index();
      if (idx == std::string::npos) {
        if (pidx == 0) return std::get<0>(*ptr);
        else ctx.flags.onerror(loc, name.substr(0, old) + " is not a variable", ERROR);
      }
      else {
        if (pidx == 3) vm = std::get<3>(*ptr).get();
        else ctx.flags.onerror(loc, name.substr(0, idx) + " is not a module", ERROR);
      }
    }
    else {
      ctx.flags.onerror(loc, name.substr(0, old) + " does not exist", ERROR);
      return nullval;
    }
  }
  auto ptr = vm->get(name);
  if (ptr) {
    auto idx = ptr->index();
    if (idx == 0) return std::get<0>(*ptr);
    else ctx.flags.onerror(loc, name.substr(0, old) + " is not a variable", ERROR);
  }
  else ctx.flags.onerror(loc, name + " does not exist", ERROR);
  return nullval;
}
