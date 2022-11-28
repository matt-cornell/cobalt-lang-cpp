#include "cobalt/ast.hpp"
#include "cobalt/context.hpp"
#include "cobalt/varmap.hpp"
#include "cobalt/types.hpp"
using namespace cobalt;
using enum types::type_base::kind_t;
const static auto f16 = sstring::get("f16"), f32 = sstring::get("f32"), f64 = sstring::get("f64"), f128 = sstring::get("f128"), isize = sstring::get("isize"), usize = sstring::get("usize"), bool_ = sstring::get("bool"), null = sstring::get("null");
static type_ptr parse_type(sstring str) {
  if (str.empty()) return nullptr;
  switch (str.back()) {
    case '&': return types::reference::get(parse_type(sstring::get(str.substr(0, str.size() - 1))));
    case '*': return types::pointer::get(parse_type(sstring::get(str.substr(0, str.size() - 1))));
    case '^': return types::borrow::get(parse_type(sstring::get(str.substr(0, str.size() - 1))));
    case ']': {
      std::size_t depth = 1;
      auto it = &str.back() - 1;
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
static std::pair<types::integer const*, bool> is_signed(type_ptr lhs, type_ptr rhs) {
  auto l = static_cast<types::integer const*>(lhs), r = static_cast<types::integer const*>(rhs);
  switch ((int(l->nbits < 0) << 1) | int(r->nbits < 0)) {
    case 0: return {l->nbits > r->nbits ? l : r, true};
    case 1: return {-l->nbits > r->nbits ? l : r, true};
    case 2: return {l->nbits > -r->nbits ? l : r, true};
    case 3: return {l->nbits < r->nbits ? l : r, false};
  }
  return {nullptr, false}; // unreachable
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
        else if (lb) {
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
        else return ctx.builder.CreateZExtOrTrunc(v, t2->llvm_type(loc, ctx));
      }
      case FLOAT: {
        auto bits = static_cast<types::integer const*>(t1)->nbits;
        if (bits < 0) return ctx.builder.CreateUIToFP(v, t2->llvm_type(loc, ctx));
        else return ctx.builder.CreateSIToFP(v, t2->llvm_type(loc, ctx));
      }
      case POINTER: return nullptr;
      case REFERENCE: return nullptr;
      case ARRAY: return nullptr;
      case FUNCTION: return nullptr;
      case NULLTYPE: return nullptr;
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
      case ARRAY: return nullptr;
      case FUNCTION: return nullptr;
      case NULLTYPE: return nullptr;
      case CUSTOM: return nullptr;
    }
    case POINTER: return nullptr;
    case REFERENCE: {
      auto t = static_cast<types::reference const*>(t1)->base;
      return impl_convert(ctx.builder.CreateLoad(t->llvm_type(loc, ctx), v), t, t2, loc, ctx);
    }
    case ARRAY: return nullptr;
    case NULLTYPE: return nullptr;
    case FUNCTION: return nullptr;
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
        if (rb == 1 || rb == -1) return ctx.builder.CreateIsNotNull(v);
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
      case ARRAY: return nullptr;
      case FUNCTION: return nullptr;
      case NULLTYPE: return nullptr;
      case CUSTOM: return nullptr;
    }
    case FLOAT: switch (t2->kind) {
      case INTEGER: {
        auto b = static_cast<types::integer const*>(t2)->nbits;
        if (b == 1 || b == -1) return ctx.builder.CreateFCmpONE(v, llvm::ConstantFP::getZero(t1->llvm_type(loc, ctx)));
        else if (b < 0) return ctx.builder.CreateFPToUI(v, t2->llvm_type(loc, ctx));
        else return ctx.builder.CreateFPToSI(v, t2->llvm_type(loc, ctx));
      }
      case FLOAT:
        if (t2->size() > t1->size()) return ctx.builder.CreateFPExt(v, t2->llvm_type(loc, ctx));
        else return ctx.builder.CreateFPTrunc(v, t2->llvm_type(loc, ctx));
      case POINTER: return nullptr;
      case REFERENCE: return nullptr;
      case ARRAY: return nullptr;
      case FUNCTION: return nullptr;
      case NULLTYPE: return nullptr;
      case CUSTOM: return nullptr;
    }
    case POINTER: switch (t2->kind) {
      case INTEGER:
        if (static_cast<types::integer const*>(t2)->nbits == 1) return ctx.builder.CreateIsNotNull(v);
        else ctx.builder.CreatePtrToInt(v, t2->llvm_type(loc, ctx));
      case FLOAT: return nullptr;
      case POINTER: return ctx.builder.CreateBitCast(v, t2->llvm_type(loc, ctx));
      case REFERENCE: return nullptr;
      case ARRAY: return static_cast<types::pointer const*>(t1)->base == static_cast<types::array const*>(t2)->base && static_cast<types::array const*>(t2)->length + 1 ? v : nullptr;
      case FUNCTION: return nullptr;
      case NULLTYPE: return nullptr;
      case CUSTOM: return nullptr;
    }
    case REFERENCE: {
      auto t = static_cast<types::reference const*>(t1)->base;
      return expl_convert(ctx.builder.CreateLoad(t->llvm_type(loc, ctx), v), t, t2, loc, ctx);
    }
    case NULLTYPE: return nullptr;
    case ARRAY: switch (t2->kind) {
      case INTEGER: return nullptr;
      case FLOAT: return nullptr;
      case POINTER: {
        auto a1 = static_cast<types::array const*>(t1);
        if (static_cast<types::pointer const*>(t2)->base != a1->base) return nullptr;
        if (a1->length + 1) return v;
        else return ctx.builder.CreateConstGEP2_32(llvm::PointerType::get(a1->llvm_type(loc, ctx), 0), v, 0, 0);
      }
      case REFERENCE: return nullptr;
      case ARRAY: {
        auto a1 = static_cast<types::array const*>(t1), a2 = static_cast<types::array const*>(t2);
        if (a1->base != a2->base) return nullptr;
        if (a1->length + 1) {
          if (a2->length + 1) return v;
          else {
            auto a = ctx.builder.CreateAlloca(a1->llvm_type(loc, ctx));
            ctx.builder.CreateStore(ctx.builder.CreateConstGEP2_32(llvm::PointerType::get(a1->llvm_type(loc, ctx), 0), a, 0, 0), v);
            ctx.builder.CreateStore(ctx.builder.CreateConstGEP2_32(llvm::PointerType::get(a1->llvm_type(loc, ctx), 0), a, 0, 1), ctx.builder.getInt64(a2->length));
            return a;
          }
        }
        else return ctx.builder.CreateConstGEP2_32(llvm::PointerType::get(a1->llvm_type(loc, ctx), 0), v, 0, 0);
      }
      case FUNCTION: return nullptr;
      case NULLTYPE: return nullptr;
      case CUSTOM: return nullptr;
    }
    case FUNCTION: return nullptr;
    case CUSTOM: return nullptr;
  }
}
static typed_value unary_op(typed_value tv, std::string_view op, location loc, compile_context& ctx) {
  if (!tv.type) return nullval;
  switch (tv.type->kind) {
    case INTEGER:
      if (op == "+") return tv;
      if (op == "-") return {ctx.builder.CreateNeg(tv.value), tv.type};
      if (op == "~") return {ctx.builder.CreateXor(tv.value, -1), tv.type};
      if (op == "!") return {ctx.builder.CreateIsNull(tv.value), types::integer::get(1)};
      return nullval;
    case FLOAT:
      if (op == "+") return tv;
      if (op == "-") return {ctx.builder.CreateFNeg(tv.value), tv.type};
      if (op == "!") return {ctx.builder.CreateFCmpOEQ(tv.value, llvm::ConstantFP::getZero(tv.type->llvm_type(loc, ctx))), types::integer::get(1)};
      return nullval;
    case POINTER:
      if (op == "*") {
        auto t = static_cast<types::pointer const*>(tv.type)->base;
        if (t->kind == FUNCTION) return {tv.value, t};
        else return {tv.value, types::reference::get(t)};
      }
      if (op == "!") return {ctx.builder.CreateIsNull(tv.value), types::integer::get(1)};
      return nullval;
    case REFERENCE: {
      auto t = static_cast<types::reference const*>(tv.type)->base;
      if (op == "&") return {tv.value, types::pointer::get(t)};
      switch (t->kind) {
        case INTEGER:
          if (op == "++") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v1 = ctx.builder.CreateLoad(t2, tv.value);
            auto v2 = ctx.builder.CreateAdd(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, tv.value);
            return {v3, tv.type};
          }
          if (op == "--") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v1 = ctx.builder.CreateLoad(t2, tv.value);
            auto v2 = ctx.builder.CreateSub(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, tv.value);
            return {v3, tv.type};
          }
          break;
        case FLOAT:
          if (op == "++") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v1 = ctx.builder.CreateLoad(t2, tv.value);
            auto v2 = ctx.builder.CreateFAdd(v1, llvm::ConstantFP::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, tv.value);
            return {v3, tv.type};
          }
          if (op == "--") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v1 = ctx.builder.CreateLoad(t2, tv.value);
            auto v2 = ctx.builder.CreateFSub(v1, llvm::ConstantFP::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, tv.value);
            return {v3, tv.type};
          }
          break;
        case POINTER:
          if (op == "++") {
            auto t2 = t->llvm_type(loc, ctx);
            auto t3 = llvm::Type::getIntNTy(*ctx.context, sizeof(void*) * 8);
            auto v1 = ctx.builder.CreateLoad(t2, tv.value);
            auto v2 = ctx.builder.CreateBitCast(v1, t3);
            auto v3 = ctx.builder.CreateAdd(v2, llvm::ConstantInt::get(t3, 1));
            auto v4 = ctx.builder.CreateBitCast(v1, t2);
            auto v5 = ctx.builder.CreateStore(v4, tv.value);
            return {v5, tv.type};
          }
          if (op == "--") {
            auto t2 = t->llvm_type(loc, ctx);
            auto t3 = llvm::Type::getIntNTy(*ctx.context, sizeof(void*) * 8);
            auto v1 = ctx.builder.CreateLoad(t2, tv.value);
            auto v2 = ctx.builder.CreateBitCast(v1, t3);
            auto v3 = ctx.builder.CreateSub(v2, llvm::ConstantInt::get(t3, 1));
            auto v4 = ctx.builder.CreateBitCast(v1, t2);
            auto v5 = ctx.builder.CreateStore(v4, tv.value);
            return {v5, tv.type};
          }
          break;
        case REFERENCE: break;
        case ARRAY: break;
        case FUNCTION: break;
        case NULLTYPE: break;
        case CUSTOM: break;
      }
      return unary_op({ctx.builder.CreateLoad(t->llvm_type(loc, ctx), tv.value), t}, op, loc, ctx);
    }
    case ARRAY: return nullval;
    case FUNCTION:
      if (op == "&") return {ctx.builder.CreateBitCast(tv.value, llvm::Type::getInt8PtrTy(*ctx.context)), types::pointer::get(tv.type)};
      return nullval;
    case NULLTYPE: return nullval;
    case CUSTOM: return nullval;
  }
}
static typed_value binary_op(typed_value lhs, typed_value rhs, std::string_view op, location loc, compile_context& ctx) {
  if (!lhs.type) return nullval;
  if (!rhs.type) return nullval;
  switch (lhs.type->kind) {
    case INTEGER: switch (rhs.type->kind) {
      case INTEGER:
        if (op == "+") {
          auto lb = static_cast<types::integer const*>(lhs.type)->nbits, rb = static_cast<types::integer const*>(rhs.type)->nbits;
          switch ((int(lb < 0) << 1) | int(rb < 0)) {
            case 0: return {ctx.builder.CreateAdd(lhs.value, rhs.value), lb > rb ? lhs.type : rhs.type};
            case 1: return {ctx.builder.CreateAdd(lhs.value, rhs.value), lb > -rb ? lhs.type : rhs.type};
            case 2: return {ctx.builder.CreateAdd(lhs.value, rhs.value), -lb > rb ? lhs.type : rhs.type};
            case 3: return {ctx.builder.CreateAdd(lhs.value, rhs.value), lb < rb ? lhs.type : rhs.type};
          }
          return nullval; // unreachable
        }
        if (op == "-") {
          auto lb = static_cast<types::integer const*>(lhs.type)->nbits, rb = static_cast<types::integer const*>(rhs.type)->nbits;
          switch ((int(lb < 0) << 1) | int(rb < 0)) {
            case 0: return {ctx.builder.CreateSub(lhs.value, rhs.value), lb > rb ? lhs.type : rhs.type};
            case 1: return {ctx.builder.CreateSub(lhs.value, rhs.value), lb > -rb ? lhs.type : rhs.type};
            case 2: return {ctx.builder.CreateSub(lhs.value, rhs.value), -lb > rb ? lhs.type : rhs.type};
            case 3: return {ctx.builder.CreateSub(lhs.value, rhs.value), lb < rb ? lhs.type : rhs.type};
          }
          return nullval; // unreachable
        }
        if (op == "*") {
          auto lb = static_cast<types::integer const*>(lhs.type)->nbits, rb = static_cast<types::integer const*>(rhs.type)->nbits;
          switch ((int(lb < 0) << 1) | int(rb < 0)) {
            case 0: return {ctx.builder.CreateMul(lhs.value, rhs.value), lb > rb ? lhs.type : rhs.type};
            case 1: return {ctx.builder.CreateMul(lhs.value, rhs.value), lb > -rb ? lhs.type : rhs.type};
            case 2: return {ctx.builder.CreateMul(lhs.value, rhs.value), -lb > rb ? lhs.type : rhs.type};
            case 3: return {ctx.builder.CreateMul(lhs.value, rhs.value), lb < rb ? lhs.type : rhs.type};
          }
          return nullval; // unreachable
        }
        if (op == "/") {
          auto lb = static_cast<types::integer const*>(lhs.type)->nbits, rb = static_cast<types::integer const*>(rhs.type)->nbits;
          switch ((int(lb < 0) << 1) | int(rb < 0)) {
            case 0: return {ctx.builder.CreateSDiv(lhs.value, rhs.value), lb > rb ? lhs.type : rhs.type};
            case 1: return {ctx.builder.CreateSDiv(lhs.value, rhs.value), lb > -rb ? lhs.type : rhs.type};
            case 2: return {ctx.builder.CreateSDiv(lhs.value, rhs.value), -lb > rb ? lhs.type : rhs.type};
            case 3: return {ctx.builder.CreateUDiv(lhs.value, rhs.value), lb < rb ? lhs.type : rhs.type};
          }
          return nullval; // unreachable
        }
        if (op == "%") {
          auto lb = static_cast<types::integer const*>(lhs.type)->nbits, rb = static_cast<types::integer const*>(rhs.type)->nbits;
          switch ((int(lb < 0) << 1) | int(rb < 0)) {
            case 0: return {ctx.builder.CreateSRem(lhs.value, rhs.value), lb > rb ? lhs.type : rhs.type};
            case 1: return {ctx.builder.CreateSRem(lhs.value, rhs.value), lb > -rb ? lhs.type : rhs.type};
            case 2: return {ctx.builder.CreateSRem(lhs.value, rhs.value), -lb > rb ? lhs.type : rhs.type};
            case 3: return {ctx.builder.CreateURem(lhs.value, rhs.value), lb < rb ? lhs.type : rhs.type};
          }
          return nullval; // unreachable
        }
        if (op == "&") {
          auto lb = static_cast<types::integer const*>(lhs.type)->nbits, rb = static_cast<types::integer const*>(rhs.type)->nbits;
          switch ((int(lb < 0) << 1) | int(rb < 0)) {
            case 0: return {ctx.builder.CreateAnd(lhs.value, rhs.value), lb > rb ? lhs.type : rhs.type};
            case 1: return {ctx.builder.CreateAnd(lhs.value, rhs.value), lb > -rb ? lhs.type : rhs.type};
            case 2: return {ctx.builder.CreateAnd(lhs.value, rhs.value), -lb > rb ? lhs.type : rhs.type};
            case 3: return {ctx.builder.CreateAnd(lhs.value, rhs.value), lb < rb ? lhs.type : rhs.type};
          }
          return nullval; // unreachable
        }
        if (op == "|") {
          auto lb = static_cast<types::integer const*>(lhs.type)->nbits, rb = static_cast<types::integer const*>(rhs.type)->nbits;
          switch ((int(lb < 0) << 1) | int(rb < 0)) {
            case 0: return {ctx.builder.CreateOr(lhs.value, rhs.value), lb > rb ? lhs.type : rhs.type};
            case 1: return {ctx.builder.CreateOr(lhs.value, rhs.value), lb > -rb ? lhs.type : rhs.type};
            case 2: return {ctx.builder.CreateOr(lhs.value, rhs.value), -lb > rb ? lhs.type : rhs.type};
            case 3: return {ctx.builder.CreateOr(lhs.value, rhs.value), lb < rb ? lhs.type : rhs.type};
          }
          return nullval; // unreachable
        }
        if (op == "^") {
          auto lb = static_cast<types::integer const*>(lhs.type)->nbits, rb = static_cast<types::integer const*>(rhs.type)->nbits;
          switch ((int(lb < 0) << 1) | int(rb < 0)) {
            case 0: return {ctx.builder.CreateXor(lhs.value, rhs.value), lb > rb ? lhs.type : rhs.type};
            case 1: return {ctx.builder.CreateXor(lhs.value, rhs.value), lb > -rb ? lhs.type : rhs.type};
            case 2: return {ctx.builder.CreateXor(lhs.value, rhs.value), -lb > rb ? lhs.type : rhs.type};
            case 3: return {ctx.builder.CreateXor(lhs.value, rhs.value), lb < rb ? lhs.type : rhs.type};
          }
          return nullval; // unreachable
        }
        if (op == "<<") {
          auto lb = static_cast<types::integer const*>(lhs.type)->nbits, rb = static_cast<types::integer const*>(rhs.type)->nbits;
          switch ((int(lb < 0) << 1) | int(rb < 0)) {
            case 0: return {ctx.builder.CreateShl(lhs.value, rhs.value), lb > rb ? lhs.type : rhs.type};
            case 1: return {ctx.builder.CreateShl(lhs.value, rhs.value), lb > -rb ? lhs.type : rhs.type};
            case 2: return {ctx.builder.CreateShl(lhs.value, rhs.value), -lb > rb ? lhs.type : rhs.type};
            case 3: return {ctx.builder.CreateShl(lhs.value, rhs.value), lb < rb ? lhs.type : rhs.type};
          }
          return nullval; // unreachable
        }
        if (op == ">>") {
          auto lb = static_cast<types::integer const*>(lhs.type)->nbits, rb = static_cast<types::integer const*>(rhs.type)->nbits;
          switch ((int(lb < 0) << 1) | int(rb < 0)) {
            case 0: return {ctx.builder.CreateLShr(lhs.value, rhs.value), lb > rb ? lhs.type : rhs.type};
            case 1: return {ctx.builder.CreateLShr(lhs.value, rhs.value), lb > -rb ? lhs.type : rhs.type};
            case 2: return {ctx.builder.CreateLShr(lhs.value, rhs.value), -lb > rb ? lhs.type : rhs.type};
            case 3: return {ctx.builder.CreateLShr(lhs.value, rhs.value), lb < rb ? lhs.type : rhs.type};
          }
          return nullval; // unreachable
        }
        return nullval;
      case FLOAT:
        if (op == "+") {
          auto v0 = impl_convert(lhs.value, lhs.type, rhs.type, loc, ctx);
          auto v1 = ctx.builder.CreateFAdd(v0, rhs.value);
          return {v1, rhs.type};
        }
        if (op == "-") {
          auto v0 = impl_convert(lhs.value, lhs.type, rhs.type, loc, ctx);
          auto v1 = ctx.builder.CreateFSub(v0, rhs.value);
          return {v1, rhs.type};
        }
        if (op == "*") {
          auto v0 = impl_convert(lhs.value, lhs.type, rhs.type, loc, ctx);
          auto v1 = ctx.builder.CreateFMul(v0, rhs.value);
          return {v1, rhs.type};
        }
        if (op == "/") {
          auto v0 = impl_convert(lhs.value, lhs.type, rhs.type, loc, ctx);
          auto v1 = ctx.builder.CreateFDiv(v0, rhs.value);
          return {v1, rhs.type};
        }
        if (op == "%") {
          auto v0 = impl_convert(lhs.value, lhs.type, rhs.type, loc, ctx);
          auto v1 = ctx.builder.CreateFRem(v0, rhs.value);
          return {v1, rhs.type};
        }
        return nullval;
      case POINTER:
        if (op == "+") {
          auto t2 = rhs.type->llvm_type(loc, ctx);
          auto t3 = llvm::Type::getIntNTy(*ctx.context, sizeof(void*) * 8);
          auto v0 = impl_convert(lhs.value, lhs.type, types::integer::get(sizeof(void*) * 8), loc, ctx);
          auto v1 = ctx.builder.CreateBitCast(rhs.value, t3);
          auto v2 = ctx.builder.CreateAdd(v1, v0);
          auto v3 = ctx.builder.CreateBitCast(v2, t2);
          return {v3, rhs.type};
        }
        if (op == "-") {
          auto t2 = rhs.type->llvm_type(loc, ctx);
          auto t3 = llvm::Type::getIntNTy(*ctx.context, sizeof(void*) * 8);
          auto v0 = impl_convert(lhs.value, lhs.type, types::integer::get(sizeof(void*) * 8), loc, ctx);
          auto v1 = ctx.builder.CreateBitCast(rhs.value, t3);
          auto v2 = ctx.builder.CreateSub(v1, v0);
          auto v3 = ctx.builder.CreateBitCast(v2, t2);
          return {v3, rhs.type};
        }
        return nullval;
      case REFERENCE: {
        auto t = static_cast<types::reference const*>(lhs.type)->base;
        return binary_op(lhs, {ctx.builder.CreateLoad(t->llvm_type(loc, ctx), rhs.value), t}, op, loc, ctx);
      }
      case ARRAY: return nullval;
      case FUNCTION: return nullval;
      case NULLTYPE: return nullval;
      case CUSTOM: return nullval;
    }
    case FLOAT: switch (rhs.type->kind) {
      case INTEGER:
        if (op == "+") {
          auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
          auto v1 = ctx.builder.CreateFAdd(lhs.value, v0);
          return {v1, lhs.type};
        }
        if (op == "-") {
          auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
          auto v1 = ctx.builder.CreateFSub(lhs.value, v0);
          return {v1, lhs.type};
        }
        if (op == "*") {
          auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
          auto v1 = ctx.builder.CreateFMul(lhs.value, v0);
          return {v1, lhs.type};
        }
        if (op == "/") {
          auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
          auto v1 = ctx.builder.CreateFDiv(lhs.value, v0);
          return {v1, lhs.type};
        }
        if (op == "%") {
          auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
          auto v1 = ctx.builder.CreateFRem(lhs.value, v0);
          return {v1, lhs.type};
        }
        return nullval;
      case FLOAT:
        if (op == "+") {
          if (lhs.type->size() > rhs.type->size()) return {ctx.builder.CreateFAdd(lhs.value, ctx.builder.CreateFPExt(rhs.value, lhs.type->llvm_type(loc, ctx))), lhs.type};
          else if (lhs.type->size() < rhs.type->size()) return {ctx.builder.CreateFAdd(ctx.builder.CreateFPExt(lhs.value, rhs.type->llvm_type(loc, ctx)), rhs.value), rhs.type};
          else return {ctx.builder.CreateFAdd(lhs.value, rhs.value), lhs.type};
        }
        if (op == "-") {
          if (lhs.type->size() > rhs.type->size()) return {ctx.builder.CreateFSub(lhs.value, ctx.builder.CreateFPExt(rhs.value, lhs.type->llvm_type(loc, ctx))), lhs.type};
          else if (lhs.type->size() < rhs.type->size()) return {ctx.builder.CreateFSub(ctx.builder.CreateFPExt(lhs.value, rhs.type->llvm_type(loc, ctx)), rhs.value), rhs.type};
          else return {ctx.builder.CreateFSub(lhs.value, rhs.value), lhs.type};
        }
        if (op == "*") {
          if (lhs.type->size() > rhs.type->size()) return {ctx.builder.CreateFMul(lhs.value, ctx.builder.CreateFPExt(rhs.value, lhs.type->llvm_type(loc, ctx))), lhs.type};
          else if (lhs.type->size() < rhs.type->size()) return {ctx.builder.CreateFMul(ctx.builder.CreateFPExt(lhs.value, rhs.type->llvm_type(loc, ctx)), rhs.value), rhs.type};
          else return {ctx.builder.CreateFMul(lhs.value, rhs.value), lhs.type};
        }
        if (op == "/") {
          if (lhs.type->size() > rhs.type->size()) return {ctx.builder.CreateFDiv(lhs.value, ctx.builder.CreateFPExt(rhs.value, lhs.type->llvm_type(loc, ctx))), lhs.type};
          else if (lhs.type->size() < rhs.type->size()) return {ctx.builder.CreateFDiv(ctx.builder.CreateFPExt(lhs.value, rhs.type->llvm_type(loc, ctx)), rhs.value), rhs.type};
          else return {ctx.builder.CreateFDiv(lhs.value, rhs.value), lhs.type};
        }
        if (op == "%") {
          if (lhs.type->size() > rhs.type->size()) return {ctx.builder.CreateFRem(lhs.value, ctx.builder.CreateFPExt(rhs.value, lhs.type->llvm_type(loc, ctx))), lhs.type};
          else if (lhs.type->size() < rhs.type->size()) return {ctx.builder.CreateFRem(ctx.builder.CreateFPExt(lhs.value, rhs.type->llvm_type(loc, ctx)), rhs.value), rhs.type};
          else return {ctx.builder.CreateFRem(lhs.value, rhs.value), lhs.type};
        }
        return nullval;
      case POINTER: return nullval;
      case REFERENCE: {
        auto t = static_cast<types::reference const*>(lhs.type)->base;
        return binary_op(lhs, {ctx.builder.CreateLoad(t->llvm_type(loc, ctx), rhs.value), t}, op, loc, ctx);
      }
      case ARRAY: return nullval;
      case FUNCTION: return nullval;
      case NULLTYPE: return nullval;
      case CUSTOM: return nullval;
    }
    case POINTER: switch (rhs.type->kind) {
      case INTEGER:
        if (op == "+") {
          auto t2 = lhs.type->llvm_type(loc, ctx);
          auto t3 = llvm::Type::getIntNTy(*ctx.context, sizeof(void*) * 8);
          auto v0 = impl_convert(rhs.value, rhs.type, types::integer::get(sizeof(void*) * 8), loc, ctx);
          auto v1 = ctx.builder.CreateBitCast(lhs.value, t3);
          auto v2 = ctx.builder.CreateAdd(v1, v0);
          auto v3 = ctx.builder.CreateBitCast(v2, t2);
          return {v3, lhs.type};
        }
        if (op == "-") {
          auto t2 = lhs.type->llvm_type(loc, ctx);
          auto t3 = llvm::Type::getIntNTy(*ctx.context, sizeof(void*) * 8);
          auto v0 = impl_convert(rhs.value, rhs.type, types::integer::get(sizeof(void*) * 8), loc, ctx);
          auto v1 = ctx.builder.CreateBitCast(lhs.value, t3);
          auto v2 = ctx.builder.CreateSub(v1, v0);
          auto v3 = ctx.builder.CreateBitCast(v2, t2);
          return {v3, lhs.type};
        }
        return nullval;
      case FLOAT: return nullval;
      case POINTER:
        if (op == "-") {
          auto t2 = llvm::Type::getIntNTy(*ctx.context, sizeof(void*) * 8);
          auto v1 = ctx.builder.CreateBitCast(lhs.value, t2);
          auto v2 = ctx.builder.CreateBitCast(rhs.value, t2);
          auto v3 = ctx.builder.CreateSub(v1, v2);
          return {v3, types::integer::get(sizeof(void*) * 8)};
        }
        return nullval;
      case REFERENCE: {
        auto t = static_cast<types::reference const*>(lhs.type)->base;
        return binary_op(lhs, {ctx.builder.CreateLoad(t->llvm_type(loc, ctx), rhs.value), t}, op, loc, ctx);
      }
      case ARRAY: return nullval;
      case FUNCTION: return nullval;
      case NULLTYPE: return nullval;
      case CUSTOM: return nullval;
    }
    case REFERENCE: {
      auto t = static_cast<types::reference const*>(lhs.type)->base;
      switch (t->kind) {
        case INTEGER:
          if (op == "=") {
            auto v = impl_convert(rhs.value, rhs.type, t, loc, ctx);
            if (!v) return nullval;
            return {ctx.builder.CreateStore(v, lhs.value), lhs.type};
          }
          if (op == "+=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
            if (!v0) return nullval;
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = ctx.builder.CreateAdd(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, lhs.value);
            return {v3, lhs.type};
          }
          if (op == "-=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
            if (!v0) return nullval;
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = ctx.builder.CreateSub(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, lhs.value);
            return {v3, lhs.type};
          }
          if (op == "*=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
            if (!v0) return nullval;
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = ctx.builder.CreateMul(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, lhs.value);
            return {v3, lhs.type};
          }
          if (op == "/=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
            if (!v0) return nullval;
            auto [_, s] = is_signed(lhs.type, rhs.type);
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = s ? ctx.builder.CreateSDiv(v1, llvm::ConstantInt::get(t2, 1)) : ctx.builder.CreateUDiv(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, lhs.value);
            return {v3, lhs.type};
          }
          if (op == "%=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
            if (!v0) return nullval;
            auto [_, s] = is_signed(lhs.type, rhs.type);
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = s ? ctx.builder.CreateSRem(v1, llvm::ConstantInt::get(t2, 1)) : ctx.builder.CreateURem(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, lhs.value);
            return {v3, lhs.type};
          }
          if (op == "&=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
            if (!v0) return nullval;
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = ctx.builder.CreateAnd(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, lhs.value);
            return {v3, lhs.type};
          }
          if (op == "|=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
            if (!v0) return nullval;
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = ctx.builder.CreateOr(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, lhs.value);
            return {v3, lhs.type};
          }
          if (op == "^=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
            if (!v0) return nullval;
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = ctx.builder.CreateXor(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, lhs.value);
            return {v3, lhs.type};
          }
          if (op == "<<=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
            if (!v0) return nullval;
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = ctx.builder.CreateShl(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, lhs.value);
            return {v3, lhs.type};
          }
          if (op == ">>=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
            if (!v0) return nullval;
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = ctx.builder.CreateLShr(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, lhs.value);
            return {v3, lhs.type};
          }
          break;
        case FLOAT:
          if (op == "=") {
            auto v = impl_convert(rhs.value, rhs.type, t, loc, ctx);
            if (!v) return nullval;
            return {ctx.builder.CreateStore(v, lhs.value), lhs.type};
          }
          if (op == "+=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
            if (!v0) return nullval;
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = ctx.builder.CreateFAdd(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, lhs.value);
            return {v3, lhs.type};
          }
          if (op == "-=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
            if (!v0) return nullval;
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = ctx.builder.CreateFSub(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, lhs.value);
            return {v3, lhs.type};
          }
          if (op == "*=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
            if (!v0) return nullval;
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = ctx.builder.CreateFMul(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, lhs.value);
            return {v3, lhs.type};
          }
          if (op == "/=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
            if (!v0) return nullval;
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = ctx.builder.CreateFDiv(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, lhs.value);
            return {v3, lhs.type};
          }
          if (op == "%=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto v0 = impl_convert(rhs.value, rhs.type, lhs.type, loc, ctx);
            if (!v0) return nullval;
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = ctx.builder.CreateFRem(v1, llvm::ConstantInt::get(t2, 1));
            auto v3 = ctx.builder.CreateStore(v2, lhs.value);
            return {v3, lhs.type};
          }
          break;
        case POINTER:
          if (op == "=") {
            auto v = impl_convert(rhs.value, rhs.type, t, loc, ctx);
            if (!v) return nullval;
            return {ctx.builder.CreateStore(v, lhs.value), lhs.type};
          }
          if (op == "+=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto t3 = llvm::Type::getIntNTy(*ctx.context, sizeof(void*) * 8);
            auto v0 = impl_convert(rhs.value, rhs.type, types::integer::get(sizeof(void*) * 8), loc, ctx);
            if (!v0) return nullval;
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = ctx.builder.CreateBitCast(v1, t3);
            auto v3 = ctx.builder.CreateAdd(v2, v0);
            auto v4 = ctx.builder.CreateBitCast(v1, t2);
            auto v5 = ctx.builder.CreateStore(v4, lhs.value);
            return {v5, lhs.type};
          }
          if (op == "-=") {
            auto t2 = t->llvm_type(loc, ctx);
            auto t3 = llvm::Type::getIntNTy(*ctx.context, sizeof(void*) * 8);
            auto v0 = impl_convert(rhs.value, rhs.type, types::integer::get(sizeof(void*) * 8), loc, ctx);
            if (!v0) return nullval;
            auto v1 = ctx.builder.CreateLoad(t2, lhs.value);
            auto v2 = ctx.builder.CreateBitCast(v1, t3);
            auto v3 = ctx.builder.CreateSub(v2, v0);
            auto v4 = ctx.builder.CreateBitCast(v1, t2);
            auto v5 = ctx.builder.CreateStore(v4, lhs.value);
            return {v5, lhs.type};
          }
          break;
        case REFERENCE: break;
        case ARRAY: return nullval;
        case FUNCTION: return nullval;
        case NULLTYPE: return nullval;
        case CUSTOM: break;
      }
      return binary_op({ctx.builder.CreateLoad(t->llvm_type(loc, ctx), lhs.value), t}, rhs, op, loc, ctx);
    }
    case ARRAY: return nullval;
    case FUNCTION: return nullval;
    case NULLTYPE: return nullval;
    case CUSTOM: return nullval;
  }
}
static std::string invalid_args(typed_value tv, std::vector<typed_value>&& args) {
  std::string str = "cannot call value of type ";
  str += tv.type->name();
  if (args.empty()) str += " without arguments";
  else {
    str += " with argument types (";
    for (auto [_, t] : args) {
      str += t->name();
      str += ", ";
    }
    str.pop_back();
    str.back() = ')';
  }
  return str;
}
static std::string invalid_subs(typed_value tv, std::vector<typed_value>&& args) {
  std::string str = "cannot subscript value of type ";
  str += tv.type->name();
  if (args.empty()) str += " without arguments";
  else {
    str += " with argument types (";
    for (auto [_, t] : args) {
      str += t->name();
      str += ", ";
    }
    str.pop_back();
    str.back() = ')';
  }
  return str;
}
static typed_value subscr(typed_value tv, std::vector<typed_value>&& args, location loc, compile_context& ctx) {
  if (!tv.type) return nullval;
  switch (tv.type->kind) {
    case INTEGER:
    case FLOAT:
    case FUNCTION:
    case NULLTYPE:
    case CUSTOM:
      ctx.flags.onerror(loc, invalid_subs(tv, std::move(args)), ERROR);
      return nullval;
    case REFERENCE: {
      auto t = static_cast<types::reference const*>(tv.type)->base;
      return subscr({ctx.builder.CreateLoad(t->llvm_type(loc, ctx), tv.value), t}, std::move(args), loc, ctx);
    }
    case POINTER: {
      if (args.size() != 1) {
        ctx.flags.onerror(loc, invalid_subs(tv, std::move(args)), ERROR);
      }
      auto idx = args.front().value;
      auto aft = args.front().type;
      while (aft->kind == REFERENCE) {
        auto b = static_cast<types::reference const*>(aft);
        idx = ctx.builder.CreateLoad(b->llvm_type(loc, ctx), idx);
        aft = b;
      }
      if (aft->kind != INTEGER) {
        ctx.flags.onerror(loc, invalid_subs(tv, std::move(args)), ERROR);
        return nullval;
      }
      auto pt = llvm::Type::getIntNTy(*ctx.context, sizeof(void*) * 8);
      auto base = static_cast<types::pointer const*>(tv.type)->base;
      auto v1 = ctx.builder.CreatePtrToInt(tv.value, pt);
      auto v2 = ctx.builder.CreateMul(idx, llvm::ConstantInt::get(pt, base->size()));
      auto v3 = ctx.builder.CreateAdd(v1, v2);
      auto v4 = ctx.builder.CreateIntToPtr(v3, tv.type->llvm_type(loc, ctx));
      return {v4, types::reference::get(base)};
    }
    case ARRAY: {
      auto t = static_cast<types::array const*>(tv.type);
      if (args.size() != 1) {
        ctx.flags.onerror(loc, invalid_subs(tv, std::move(args)), ERROR);
      }
      auto idx = args.front().value;
      auto aft = args.front().type;
      while (aft->kind == REFERENCE) {
        auto b = static_cast<types::reference const*>(aft)->base;
        idx = ctx.builder.CreateLoad(b->llvm_type(loc, ctx), idx);
        aft = b;
      }
      if (aft->kind != INTEGER) {
        ctx.flags.onerror(loc, invalid_subs(tv, std::move(args)), ERROR);
        return nullval;
      }
      auto i = idx->getType();
      auto pt = llvm::Type::getInt64Ty(*ctx.context);
      auto ip = ctx.builder.GetInsertBlock();
      auto f = ip->getParent();
      auto
        bc = llvm::BasicBlock::Create(*ctx.context, "arr.bc"), // bounds check (<0)
        lt0 = llvm::BasicBlock::Create(*ctx.context, "arr.lt0"), // less than 0
        m1 = llvm::BasicBlock::Create(*ctx.context, "arr.m1"), // merge <0 and >=0 indices
        m2 = llvm::BasicBlock::Create(*ctx.context, "arr.m2"); // merge valid and invalid indices
      llvm::Value* arr, * len;
      llvm::Type* at;
      if (t->length + 1) {
        at = t->llvm_type(loc, ctx);
        arr = tv.value;
        len = llvm::ConstantInt::get(i, t->length);
      }
      else {
        auto st = t->llvm_type(loc, ctx);
        at = llvm::cast<llvm::StructType>(st)->getTypeAtIndex((unsigned)0);
        arr = ctx.builder.CreateConstGEP2_32(llvm::PointerType::get(st, 0), tv.value, 0, 0);
        if (i == pt) len = ctx.builder.CreateConstGEP2_32(llvm::PointerType::get(st, 0), tv.value, 0, 1);
        else len = ctx.builder.CreateZExtOrTrunc(ctx.builder.CreateConstGEP2_32(llvm::PointerType::get(st, 0), tv.value, 0, 1), i);
      }
      auto cmp1 = ctx.builder.CreateICmpSGE(idx, len); // idx >= len
      ctx.builder.CreateCondBr(cmp1, m2, bc); // nullptr : idx < 0
      f->getBasicBlockList().push_back(bc);
      ctx.builder.SetInsertPoint(bc);
      auto cmp2 = ctx.builder.CreateICmpSLT(idx, llvm::ConstantInt::get(i, 0)); // idx < 0
      ctx.builder.CreateCondBr(cmp2, lt0, m1); // idx + len : idx
      bc = ctx.builder.GetInsertBlock();
      f->getBasicBlockList().push_back(lt0);
      ctx.builder.SetInsertPoint(lt0);
      auto add1 = ctx.builder.CreateAdd(idx, len);
      auto bcv = ctx.builder.CreateZExtOrTrunc(add1, i);
      auto cmp3 = ctx.builder.CreateICmpSLT(add1, llvm::ConstantInt::get(i, 0)); // idx + len < 0
      ctx.builder.CreateCondBr(cmp3, m2, m1); // nullptr : idx + len
      lt0 = ctx.builder.GetInsertBlock();
      f->getBasicBlockList().push_back(m1);
      ctx.builder.SetInsertPoint(m1);
      auto phi1 = ctx.builder.CreatePHI(i, 2); // combine idx, idx + len
      phi1->addIncoming(idx, bc);
      phi1->addIncoming(bcv, lt0);
      auto av = ctx.builder.CreatePtrToInt(arr, pt);
      auto idx1 = ctx.builder.CreateMul(phi1, llvm::ConstantInt::get(pt, t->base->size()));
      auto add2 = ctx.builder.CreateAdd(av, idx1);
      auto pv = ctx.builder.CreateIntToPtr(add2, arr->getType());
      ctx.builder.CreateBr(m2);
      m1 = ctx.builder.GetInsertBlock();
      f->getBasicBlockList().push_back(m2);
      ctx.builder.SetInsertPoint(m2);
      auto np = llvm::Constant::getNullValue(arr->getType());
      auto phi2 = ctx.builder.CreatePHI(arr->getType(), 3); // combine arr[idx], nullptr
      phi2->addIncoming(pv, m1);
      phi2->addIncoming(np, ip);
      phi2->addIncoming(np, lt0);
      return {phi2, types::reference::get(t->base)};
    }
  }
}
static typed_value call(typed_value tv, std::vector<typed_value>&& args, location loc, compile_context& ctx) {
  if (!tv.type) return nullval;
  switch (tv.type->kind) {
    case INTEGER:
    case FLOAT:
    case POINTER:
    case ARRAY:
    case NULLTYPE:
    case CUSTOM:
      ctx.flags.onerror(loc, invalid_args(tv, std::move(args)), ERROR);
      return nullval;
    case REFERENCE: {
      auto t = static_cast<types::reference const*>(tv.type)->base;
      return call({ctx.builder.CreateLoad(t->llvm_type(loc, ctx), tv.value), t}, std::move(args), loc, ctx);
    }
    case FUNCTION: {
      auto f = static_cast<types::function const*>(tv.type);
      if (args.size() != f->args.size()) {
        ctx.flags.onerror(loc, invalid_args(tv, std::move(args)), ERROR);
        return nullval;
      }
      std::vector<llvm::Value*> args_v(args.size());
      for (std::size_t i = 0; i < args.size(); ++i) if (!(args_v[i] = impl_convert(args[i].value, args[i].type, f->args[i], loc, ctx))) {
        ctx.flags.onerror(loc, invalid_args(tv, std::move(args)), ERROR);
        return nullval;
      }
      return {ctx.builder.CreateCall(llvm::cast<llvm::FunctionType>(f->llvm_type(loc, ctx)), tv.value, args_v), f->ret};
    }
  }
}
static std::string concat(std::vector<std::string_view> const& vals, std::string_view other) {
  std::size_t sz = other.size() + vals.size();
  for (auto val : vals) sz += val.size();
  std::string out;
  out.reserve(sz);
  for (auto val : vals) {
    out += val;
    out.push_back('.');
  }
  out += other;
  return out;
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
typed_value cobalt::ast::binop_ast::codegen(compile_context& ctx) const {
  if (std::string_view(op) == "&&") {
    auto l = lhs(ctx);
    if (!l.type) return nullval;
    auto f = ctx.builder.GetInsertBlock()->getParent();
    auto 
      if_true = llvm::BasicBlock::Create(*ctx.context, "if_true", f), 
      if_false = llvm::BasicBlock::Create(*ctx.context, "if_false"), 
      merge = llvm::BasicBlock::Create(*ctx.context, "merge");
    auto v = expl_convert(l.value, l.type, types::integer::get(1), loc, ctx);
    ctx.builder.CreateCondBr(v, if_true, if_false);
    ctx.builder.SetInsertPoint(if_true);
    auto itv = rhs(ctx);
    while (itv.type->kind == REFERENCE) {
      auto t = static_cast<types::reference const*>(itv.type)->base;
      itv.value = ctx.builder.CreateLoad(t->llvm_type(loc, ctx), itv.value);
      itv.type = t;
    }
    auto llt = itv.type->llvm_type(loc, ctx);
    ctx.builder.CreateBr(merge);
    if_true = ctx.builder.GetInsertBlock();
    f->getBasicBlockList().push_back(if_false);
    ctx.builder.SetInsertPoint(if_false);
    auto ifv = llvm::Constant::getNullValue(llt);
    ctx.builder.CreateBr(merge);
    if_false = ctx.builder.GetInsertBlock();
    f->getBasicBlockList().push_back(merge);
    ctx.builder.SetInsertPoint(merge);
    auto pn = ctx.builder.CreatePHI(llt, 2);
    pn->addIncoming(itv.value, if_true);
    pn->addIncoming(ifv, if_false);
    return {pn, itv.type};
  }
  if (std::string_view(op) == "||") {
    auto l = lhs(ctx);
    if (!l.type) return nullval;
    auto f = ctx.builder.GetInsertBlock()->getParent();
    auto 
      if_true = llvm::BasicBlock::Create(*ctx.context, "if_true"), 
      if_false = llvm::BasicBlock::Create(*ctx.context, "if_false", f), 
      merge = llvm::BasicBlock::Create(*ctx.context, "merge");
    auto v = expl_convert(l.value, l.type, types::integer::get(1), loc, ctx);
    ctx.builder.CreateCondBr(v, if_true, if_false);
    ctx.builder.SetInsertPoint(if_false);
    auto ifv = rhs(ctx);
    while (ifv.type->kind == REFERENCE) {
      auto t = static_cast<types::reference const*>(ifv.type)->base;
      ifv.value = ctx.builder.CreateLoad(t->llvm_type(loc, ctx), ifv.value);
      ifv.type = t;
    }
    auto llt = ifv.type->llvm_type(loc, ctx);
    ctx.builder.CreateBr(merge);
    if_false = ctx.builder.GetInsertBlock();
    f->getBasicBlockList().push_back(if_true);
    ctx.builder.SetInsertPoint(if_true);
    auto itv = llvm::Constant::getAllOnesValue(llt);
    ctx.builder.CreateBr(merge);
    if_true = ctx.builder.GetInsertBlock();
    f->getBasicBlockList().push_back(merge);
    ctx.builder.SetInsertPoint(merge);
    auto pn = ctx.builder.CreatePHI(llt, 2);
    pn->addIncoming(itv, if_true);
    pn->addIncoming(ifv.value, if_false);
    return {pn, ifv.type};
  }
  auto ltv = lhs(ctx), rtv = rhs(ctx);
  auto tv = binary_op(ltv, rtv, op, loc, ctx);
  if (tv.value && tv.type) return tv;
  switch ((int(bool(ltv.type)) << 1) | int(bool(rtv.type))) {
    case 0: ctx.flags.onerror(loc, (llvm::Twine("invalid operator ") + op + " for values of types <error> and <error>").str(), ERROR); break;
    case 1: ctx.flags.onerror(loc, (llvm::Twine("invalid operator ") + op + " for values of types <error> and '" + rtv.type->name() + "'").str(), ERROR); break;
    case 2: ctx.flags.onerror(loc, (llvm::Twine("invalid operator ") + op + " for values of types '" + ltv.type->name() + "' and <error>").str(), ERROR); break;
    case 3: ctx.flags.onerror(loc, (llvm::Twine("invlaid operator ") + op + " for values of types '" + ltv.type->name() + "' and '" + rtv.type->name() + "'").str(), ERROR); break;
  }
  return nullval;
}
typed_value cobalt::ast::unop_ast::codegen(compile_context& ctx) const {
  auto tv = val(ctx);
  auto v2 = unary_op(tv, op, loc, ctx);
  if (v2.value && v2.type) return v2;
  if (tv.type) {
    if (op.front() == 'p') ctx.flags.onerror(loc, (llvm::Twine("invalid postfix operator ") + op.substr(1) + " for value of type '" + tv.type->name() + "'").str(), ERROR);
    else ctx.flags.onerror(loc, (llvm::Twine("invalid prefix operator ") + op + " for value of type '" + tv.type->name() + "'").str(), ERROR);
  }
  else {
    if (op.front() == 'p') ctx.flags.onerror(loc, (llvm::Twine("invalid postfix operator ") + op.substr(1) + " for value of type <error>").str(), ERROR);
    else ctx.flags.onerror(loc, (llvm::Twine("invalid prefix operator ") + op + " for value of type <error>").str(), ERROR);
  }
  return nullval;
}
typed_value cobalt::ast::subscr_ast::codegen(compile_context& ctx) const {
  auto self = val(ctx);
  std::vector<typed_value> args_v(args.size());
  for (std::size_t i = 0; i < args.size(); ++i) args_v[i] = args[i](ctx);
  auto tv = subscr(self, std::move(args_v), loc, ctx);
  return tv;
}
typed_value cobalt::ast::call_ast::codegen(compile_context& ctx) const {
  auto self = val(ctx);
  std::vector<typed_value> args_v(args.size());
  for (std::size_t i = 0; i < args.size(); ++i) args_v[i] = args[i](ctx);
  auto tv = call(self, std::move(args_v), loc, ctx);
  return tv;
}
typed_value cobalt::ast::fndef_ast::codegen(compile_context& ctx) const {
  std::vector<llvm::Type*> params_t(args.size());
  std::vector<type_ptr> args_t(args.size());
  std::vector<std::string_view> old_path;
  for (std::size_t i = 0; i < args.size(); ++i) {
    auto t = parse_type(args[i].second);
    if (!t) {
      ctx.flags.onerror(loc, (llvm::Twine("invalid type name '") + args[i].second + "' for function parameter").str(), ERROR);
      return nullval;
    }
    args_t[i] = t;
    params_t[i] = t->llvm_type(loc, ctx);
    if (t->needs_stack()) params_t[i] = llvm::PointerType::get(params_t[i], 0);
  }
  auto t = parse_type(ret);
  if (!t) {
    ctx.flags.onerror(loc, (llvm::Twine("invalid type name '") + ret + "' for function return types").str(), ERROR);
    return nullval;
  }
  bool is_extern = false, throws = false;
  std::string_view link_as = "";
  unsigned cconv = 8;
  {
    auto idx = name.rfind('.');
    if ((idx != std::string::npos ? name.substr(idx) : std::string_view{name}) == "main") cconv = 0; // main should use C calling convention for compatibility
  }
  llvm::GlobalValue::LinkageTypes link_type = llvm::GlobalValue::ExternalLinkage;
  bool ltset = false, ccset = false;
  for (auto const& ann : annotations) {
    if (ann.starts_with("extern(")) {
      if (is_extern) ctx.flags.onerror(loc, "reuse of @extern annotation", ERROR);
      is_extern = true;
      auto cc = std::string_view{ann.data() + 7, ann.size() - 8};
      if (!cc.empty()) {
        if (cc == "c" || cc == "C") cconv = llvm::CallingConv::C;
        else if (cc == "fast" || cc == "Fast") cconv = llvm::CallingConv::Fast;
        else if (cc == "cold" || cc == "Cold") cconv = llvm::CallingConv::Cold;
        else if (cc == "tail" || cc == "Tail") cconv = llvm::CallingConv::Tail;
        else if (cc == "any" || cc == "any_reg" || cc == "any-reg" || cc == "any reg" || cc == "Any" || cc == "AnyReg" || cc == "Any Reg") cconv = llvm::CallingConv::AnyReg;
        else if (cc == "swift" || cc == "Swift") cconv = llvm::CallingConv::Swift;
        else if (cc == "swift_tail" || cc == "swift-tail" || cc == "swift tail" || cc == "SwifTail" || cc == "Swift Tail") cconv = llvm::CallingConv::SwiftTail;
        else if (cc == "ghc" || cc == "GHC") cconv = llvm::CallingConv::GHC;
        else if (cc == "hipe" || cc == "HiPE") cconv = llvm::CallingConv::HiPE;
        else ctx.flags.onerror(loc, (llvm::Twine("unknown calling convention '") + cc + "'").str(), ERROR);
      }
    }
    else if (ann.starts_with("throws(")) {
      if (ann.size() != 8) ctx.flags.onerror(loc, "@throws annotation should be used without arguments", ERROR);
      if (throws) ctx.flags.onerror(loc, "reuse of @throws annotation", ERROR);
      throws = true;
    }
    else if (ann.starts_with("linkas(")) {
      if (!link_as.empty()) ctx.flags.onerror(loc, "reuse of @linkas annotation", ERROR);
      link_as = std::string_view{ann.data() + 7, ann.size() - 8};
      if (link_as.empty()) ctx.flags.onerror(loc, "@linkas must be used with arguments", ERROR);
    }
    else if (ann.starts_with("link(")) {
      if (ltset) ctx.flags.onerror(loc, "reuse of @link annotation", ERROR);
      auto lt = std::string_view{ann.data() + 5, ann.size() - 6};
      if (lt.empty()) ctx.flags.onerror(loc, "@link must be used with arguments", ERROR);
      ltset = true;
      if (lt == "external" || lt == "extern") link_type = llvm::GlobalValue::ExternalLinkage;
      else if (lt == "weak_any" || lt == "weak-any" || lt == "weak any") link_type = llvm::GlobalValue::WeakAnyLinkage;
      else if (lt == "weak_odr" || lt == "weak-odr" || lt == "weak odr" || lt == "weak ODR") link_type = llvm::GlobalValue::WeakODRLinkage;
      else if (lt == "internal" || lt == "intern") link_type = llvm::GlobalValue::InternalLinkage;
      else if (lt == "private") link_type = llvm::GlobalValue::PrivateLinkage;
      else if (lt == "weak" || lt == "extern_weak" || lt == "extern-weak" || lt == "extern weak" || lt == "external_weak" || lt == "external-weak" || lt == "external weak") link_type = llvm::GlobalValue::ExternalWeakLinkage;
      else ctx.flags.onerror(loc, (llvm::Twine("unknown linkage type '") + lt + "'").str(), ERROR);
    }
    else if (ann.starts_with("cconv(")) {
      if (ccset) ctx.flags.onerror(loc, "redefinition of calling convention annotation", ERROR);
      auto cc = std::string_view{ann.data() + 6, ann.size() - 7};
      if (cc.empty()) ctx.flags.onerror(loc, "@cconv must be used with arguments", ERROR);
      if (cc == "c" || cc == "C") cconv = llvm::CallingConv::C;
      else if (cc == "fast" || cc == "Fast") cconv = llvm::CallingConv::Fast;
      else if (cc == "cold" || cc == "Cold") cconv = llvm::CallingConv::Cold;
      else if (cc == "tail" || cc == "Tail") cconv = llvm::CallingConv::Tail;
      else if (cc == "any" || cc == "any_reg" || cc == "any-reg" || cc == "any reg" || cc == "Any" || cc == "AnyReg" || cc == "Any Reg") cconv = llvm::CallingConv::AnyReg;
      else if (cc == "swift" || cc == "Swift") cconv = llvm::CallingConv::Swift;
      else if (cc == "swift_tail" || cc == "swift-tail" || cc == "swift tail" || cc == "SwifTail" || cc == "Swift Tail") cconv = llvm::CallingConv::SwiftTail;
      else if (cc == "ghc" || cc == "GHC") cconv = llvm::CallingConv::GHC;
      else if (cc == "hipe" || cc == "HiPE") cconv = llvm::CallingConv::HiPE;
      else ctx.flags.onerror(loc, (llvm::Twine("unknown calling convention '") + cc + "'").str(), ERROR);
    }
    else ctx.flags.onerror(loc, "unknown annotation @" + ann, ERROR);
  }
  auto ft = llvm::FunctionType::get(t->llvm_type(loc, ctx), params_t, false);
  alignas(sstring) char local_mem[sizeof(sstring)];
  sstring& local = reinterpret_cast<sstring&>(local_mem[0]);
  {
    auto idx = name.rfind('.') + 1;
    if (idx) local = sstring::get(name.substr(idx));
    else local = sstring::get(name);
  }
  if (is_extern && !link_as.empty()) ctx.vars->insert(local, typed_value{llvm::GlobalAlias::create(ft, 0, link_type, concat(ctx.path, name), llvm::cast<llvm::Function>(ctx.module->getOrInsertFunction(link_as, ft).getCallee()), ctx.module.get()), types::function::get(t, std::vector<type_ptr>(args_t))});
  else {
    auto f = llvm::Function::Create(ft, link_type, name.front() == '.' ? std::string_view(name) : concat(ctx.path, name), *ctx.module);;
    if (!f) return nullval;
    f->setCallingConv(cconv);
    {
      std::size_t i = 0;
      for (auto& arg : f->args()) if (!args[i].first.empty()) arg.setName(args[i].first);
    }
    ctx.vars->insert(local, typed_value{f, types::function::get(t, std::vector<type_ptr>(args_t))});
    if (!is_extern) {
      if (name.front() == '.') {
        old_path = ctx.path;
        ctx.path = {name.substr(1)};
      }
      else ctx.path.push_back(name);
      auto bb = llvm::BasicBlock::Create(*ctx.context, "entry", f);
      auto ip = ctx.builder.GetInsertBlock();
      ctx.builder.SetInsertPoint(bb);
      ctx.vars = new varmap(ctx.vars);
      for (std::size_t i = 0; i < args.size(); ++i) if (!args[i].first.empty()) ctx.vars->insert(args[i].first, typed_value{f->getArg(i), args_t[i]});
      auto tv = body(ctx);
      if (t->kind != NULLTYPE) {
        if (!tv.type) ctx.builder.CreateRet(llvm::Constant::getNullValue(t->llvm_type(loc, ctx)));
        else {
          auto p = impl_convert(tv.value, tv.type, t, loc, ctx);
          if (!p) ctx.builder.CreateRet(llvm::Constant::getNullValue(t->llvm_type(loc, ctx)));
          else ctx.builder.CreateRet(p);
        }
        auto vars = ctx.vars;
        ctx.vars = ctx.vars->parent;
        delete vars;
        ctx.builder.SetInsertPoint(ip);
        if (!link_as.empty()) llvm::GlobalAlias::create(ft, 0, llvm::GlobalValue::ExternalLinkage, link_as, f, ctx.module.get());
      }
      else ctx.builder.CreateRetVoid();
      if (name.front() == '.') std::swap(ctx.path, old_path);
      else ctx.path.pop_back();
    }
  }
  return nullval;
}
// keyvals.hpp
typed_value cobalt::ast::null_ast::codegen(compile_context& ctx) const {return {nullptr, types::null::get()};}
// literals.hpp
typed_value cobalt::ast::integer_ast::codegen(compile_context& ctx) const {
  if (suffix.empty()) return {llvm::Constant::getIntegerValue(llvm::Type::getInt64Ty(*ctx.context), val), types::integer::get(0)};
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
  return {ctx.builder.CreateGlobalString(val, ".co.str." + llvm::Twine(ctx.init_count++)), types::pointer::get(types::integer::get(8))};
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
typed_value cobalt::ast::array_ast::codegen(compile_context& ctx) const {
  bool is_const = true;
  std::vector<llvm::Value*> outs;
  type_ptr base = nullptr;
  outs.reserve(vals.size());
  for (AST const& val : vals) {
    if (!val.is_const()) is_const = false;
    auto tv = val(ctx);
    if (base) {
      if (tv.type != base) {
        auto v = impl_convert(tv.value, tv.type, base, loc, ctx);
        if (v) outs.push_back(v);
        else {
          ctx.flags.onerror(val.loc(), "array must be homogenous", ERROR);
          return nullval;
        }
      }
      else outs.push_back(tv.value);
    }
    else {
      base = tv.type;
      outs.push_back(tv.value);
    }
  }
  if (base->kind == INTEGER && !static_cast<types::integer const*>(base)->nbits) base = types::integer::get(64);
  auto bt = base->llvm_type(loc, ctx);
  auto var_type = llvm::ArrayType::get(bt, vals.size());
  if (is_const) {
    auto arr = llvm::ConstantArray::get(var_type, reinterpret_cast<std::vector<llvm::Constant*>&>(outs));
    auto v = ctx.builder.CreateBitCast(arr, llvm::PointerType::get(bt, 0));
    return {v, types::array::get(base, outs.size())};
  }
  llvm::Value* insert_point;
  if (is_static) insert_point = new llvm::GlobalVariable(*ctx.module, var_type, false, llvm::GlobalValue::PrivateLinkage, nullptr);
  else insert_point = ctx.builder.CreateAlloca(var_type);
  std::vector<llvm::Constant*> couts(outs.size());
  for (std::size_t i = 0; i < outs.size(); ++i) couts[i] = llvm::dyn_cast_or_null<llvm::Constant>(outs[i]);
  ctx.builder.CreateStore(llvm::ConstantArray::get(var_type, couts), insert_point);
  for (std::size_t i = 0; i < outs.size(); ++i) if (!couts[i]) {
    auto p = ctx.builder.CreateConstGEP2_64(insert_point->getType(), insert_point, 0, i);
    ctx.builder.CreateStore(outs[i], p);
  }
  return {insert_point, types::array::get(base, vals.size())};
}
// scope.hpp
typed_value cobalt::ast::module_ast::codegen(compile_context& ctx) const {
  auto vm = ctx.vars;
  std::vector<std::string_view> old_path;
  if (name.front() == '.') {
    old_path = ctx.path;
    ctx.path = {name.substr(1)};
  }
  else ctx.path.push_back(name);
  if (name.front() == '.') while (vm->parent) vm = vm->parent;
  std::size_t old = name.front() == '.', idx = name.find('.', 1);
  while (idx != std::string::npos) {
    auto local = name.substr(old + 1, idx - old - 1);
    auto ss = sstring::get(local);
    auto it = vm->symbols.find(ss);
    old = idx;
    idx = name.find('.', old);
    if (it == vm->symbols.end()) {
      if (it->second.index() == 2) vm = std::get<2>(it->second).get();
      else {
        ctx.flags.onerror(loc, name.substr(0, idx) + " is not a module", ERROR);
        return nullval;
      }
    }
    else {
      auto nvm = std::make_shared<varmap>(vm);
      vm->symbols.insert({ss, symbol_type(nvm)});
      vm = nvm.get();
    }
  }
  {
    auto nvm = std::make_shared<varmap>(vm);
    vm->symbols.insert({sstring::get(name.substr(old + 1)), symbol_type(nvm)});
    vm = nvm.get();
  }
  std::swap(vm, ctx.vars);
  for (auto const& i : insts) i(ctx);
  if (name.front() == '.') std::swap(ctx.path, old_path);
  else ctx.path.pop_back();
  std::swap(vm, ctx.vars);
  return nullval;
}
typed_value cobalt::ast::import_ast::codegen(compile_context& ctx) const {
  {
    auto i1 = path.rfind('*'), i2 = path.rfind('.');
    if (i1 != std::string::npos && i2 != std::string::npos && i1 < i2) {
      ctx.flags.onerror(loc, "globbing expressions cannot be used in the middle of an import statement", ERROR);
      return nullval;
    }
  }
  varmap* vm = ctx.vars;
  if (path.front() == '.') while (vm->parent) vm = vm->parent;
  std::size_t old = path.front() == '.', idx = path.find('.', 1);
  while (idx != std::string::npos) {
    auto local = path.substr(old, idx - old - 1);
    auto ptr = vm->get(sstring::get(local));
    if (ptr) {
      auto pidx = ptr->index();
      if (pidx == 2) vm = std::get<2>(*ptr).get();
      else ctx.flags.onerror(loc, path.substr(0, idx) + " is not a module", ERROR);
    }
    else {
      ctx.flags.onerror(loc, path.substr(0, old) + " does not exist", ERROR);
      return nullval;
    }
    old = idx + 1;
    idx = path.find('.', old);
  }
  if (path.substr(old) == "*") {
    auto res = ctx.vars->include(vm);
    for (auto const& sym : res) ctx.flags.onerror(loc, (llvm::Twine("conflicting definitions for '") + sym + "' in '" + concat(ctx.path, "") + "' and '" + path + "'").str(), ERROR);
    return nullval;
  }
  auto ss = sstring::get(path.substr(old));
  auto ptr = vm->get(ss);
  if (!ptr) {
    ctx.flags.onerror(loc, path + " does not exist", ERROR);
    return nullval;
  }
  auto [it, succ] = ctx.vars->symbols.insert({ss, *ptr});
  if (!succ) {
    if (ptr->index() == it->second.index() && ptr->index() == 2) std::get<2>(it->second)->include(std::get<2>(*ptr).get());
    else ctx.flags.onerror(loc, (llvm::Twine("conflicting definitions for '") + ss + "' in '" + concat(ctx.path, "") + "' and '" + path + "'").str(), ERROR);
  }
  return nullval;
}
// vars.hpp
typed_value cobalt::ast::vardef_ast::codegen(compile_context& ctx) const {
  varmap* vm = ctx.vars;
  if (name.front() == '.') while (vm->parent) vm = vm->parent;
  std::size_t old = name.front() == '.', idx = name.find('.', 1);
  while (idx != std::string::npos) {
    auto local = name.substr(old , idx - old - 1);
    auto ss = sstring::get(local);
    auto it = vm->symbols.find(ss);
    if (idx == std::string::npos) {
      if (it != vm->symbols.end()) ctx.flags.onerror(loc, "redefinition of " + name, ERROR);
      else break;
    }
    else {
      if (it == vm->symbols.end()) {
        auto nvm = std::make_shared<varmap>(vm);
        vm->symbols.insert({ss, symbol_type(nvm)});
        vm = nvm.get();
      }
      else if (it->second.index() == 2) vm = std::get<2>(it->second).get();
      else {
        ctx.flags.onerror(loc, name.substr(0, idx) + " is not a module", ERROR);
        return nullval;
      }
    }
    old = idx + 1;
    idx = name.find('.', old);
  }
  auto local = name.substr(old);
  std::vector<std::string_view> old_path;
  std::string_view link_as = "";
  llvm::GlobalValue::LinkageTypes link_type = global ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::PrivateLinkage;
  bool ltset = false, is_static = false;
  for (auto const& ann : annotations) {
    if (ann.starts_with("static(")) {
      if (global) ctx.flags.onerror(loc, "@static annotation cannot be used on a global variable", ERROR);
      if (ann.size() != 8) ctx.flags.onerror(loc, "@static annotation should be used without arguments", ERROR);
      if (is_static) ctx.flags.onerror(loc, "reuse of @static annotation", ERROR);
      is_static = true;
    }
    else if (ann.starts_with("linkas(")) {
      if (!link_as.empty()) ctx.flags.onerror(loc, "reuse of @linkas annotation", ERROR);
      link_as = std::string_view{ann.data() + 7, ann.size() - 8};
      if (link_as.empty()) ctx.flags.onerror(loc, "@linkas must be used with arguments", ERROR);
    }
    else if (ann.starts_with("link(")) {
      if (ltset) ctx.flags.onerror(loc, "reuse of @link annotation", ERROR);
      auto lt = std::string_view{ann.data() + 5, ann.size() - 6};
      if (lt.empty()) ctx.flags.onerror(loc, "@link must be used with arguments", ERROR);
      ltset = true;
      if (lt == "external" || lt == "extern") link_type = llvm::GlobalValue::ExternalLinkage;
      else if (lt == "weak_any" || lt == "weak-any" || lt == "weak any") link_type = llvm::GlobalValue::WeakAnyLinkage;
      else if (lt == "weak_odr" || lt == "weak-odr" || lt == "weak odr" || lt == "weak ODR") link_type = llvm::GlobalValue::WeakODRLinkage;
      else if (lt == "internal" || lt == "intern") link_type = llvm::GlobalValue::InternalLinkage;
      else if (lt == "private") link_type = llvm::GlobalValue::PrivateLinkage;
      else if (lt == "weak" || lt == "extern_weak" || lt == "extern-weak" || lt == "extern weak" || lt == "external_weak" || lt == "external-weak" || lt == "external weak") link_type = llvm::GlobalValue::ExternalWeakLinkage;
      else ctx.flags.onerror(loc, (llvm::Twine("unknown linkage type '") + lt + "'").str(), ERROR);
    }
    else ctx.flags.onerror(loc, "unknown annotation @" + ann, ERROR);
  }
  if (global || is_static) {
    if (val.is_const()) {
      if (name.front() == '.') {
        old_path = ctx.path;
        ctx.path = {name.substr(1)};
      }
      else ctx.path.push_back(name);
      auto tv = val(ctx);
      if (name.front() == '.') std::swap(ctx.path, old_path);
      else ctx.path.pop_back();
      if (!tv.type) return nullval;
      auto ct = tv.type->kind == INTEGER && !static_cast<types::integer const*>(tv.type)->nbits ? types::integer::get(64) : tv.type;
      auto gv = new llvm::GlobalVariable(*ctx.module, ct->llvm_type(loc, ctx), true, link_type, llvm::cast<llvm::Constant>(tv.value), name.front() == '.' ? std::string_view(name) : std::string_view(concat(ctx.path, name)));
      auto type = types::reference::get(ct);
      vm->insert(sstring::get(local), typed_value{gv, type});
      return {gv, type};
    }
    else {
      auto ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx.context), false);
      if (!ft) return nullval;
      auto f = llvm::Function::Create(ft, llvm::GlobalValue::LinkageTypes::PrivateLinkage, "global.init." + llvm::Twine(ctx.init_count++), ctx.module.get());
      if (!f) return nullval;
      auto rt = val.type(ctx);
      auto ct = rt->kind == INTEGER && !static_cast<types::integer const*>(rt)->nbits ? types::integer::get(64) : rt;
      auto gv = new llvm::GlobalVariable(*ctx.module, ct->llvm_type(loc, ctx), true, link_type, nullptr, name.front() == '.' ? std::string_view(name) : std::string_view(concat(ctx.path, name)));
      auto bb = llvm::BasicBlock::Create(*ctx.context, "entry", f);
      ctx.builder.SetInsertPoint(bb);
      if (name.front() == '.') {
        old_path = ctx.path;
        ctx.path = {name.substr(1)};
      }
      else ctx.path.push_back(name);
      if (auto ast = val.dyn_cast<ast::array_ast>()) ast->is_static = true;
      auto tv = val(ctx);
      if (name.front() == '.') std::swap(ctx.path, old_path);
      else ctx.path.pop_back();
      if (!tv.type) {
        f->eraseFromParent();
        gv->eraseFromParent();
        return nullval;
      }
      ctx.builder.CreateStore(tv.value, gv);
      ctx.builder.CreateRetVoid();
      ctx.builder.SetInsertPoint((llvm::BasicBlock*)nullptr);
      auto type = types::reference::get(ct);
      vm->insert(sstring::get(local), typed_value{gv, type});
      return {gv, type};
    }
  }
  else {
    if (ltset) ctx.flags.onerror(loc, "@link annotation can only be used on global or static variables", ERROR);
    if (!link_as.empty()) ctx.flags.onerror(loc, "@linkas annotation can only be used on global or static variables", ERROR);
    if (name.front() == '.') {
      old_path = ctx.path;
      ctx.path = {name.substr(1)};
    }
    else ctx.path.push_back(name);
    auto tv = val(ctx);
    if (name.front() == '.') std::swap(ctx.path, old_path);
    else ctx.path.pop_back();
    if (!tv.type) return nullval;
    if (!llvm::isa<llvm::GlobalValue>(tv.value)) tv.value->setName(name);
    auto corr_t = tv.type->kind == INTEGER && !static_cast<types::integer const*>(tv.type)->nbits ? types::integer::get(64) : tv.type;
    if (tv.type->needs_stack()) {
      llvm::Value* a;
      switch (tv.type->kind) {
        case FUNCTION:
          a = ctx.builder.CreateAlloca(llvm::Type::getInt8Ty(*ctx.context), nullptr, name);
          break;
        case INTEGER:
          if (!static_cast<types::integer const*>(tv.type)->nbits) a = ctx.builder.CreateAlloca(llvm::Type::getInt64Ty(*ctx.context), nullptr, name);
          else goto ALLOCA_DEFAULT;
          break;
        case POINTER:
          if (static_cast<types::pointer const*>(tv.type)->base->kind == FUNCTION) a = ctx.builder.CreateAlloca(llvm::Type::getInt8PtrTy(*ctx.context), nullptr, name);
          else goto ALLOCA_DEFAULT;
          break;
        default: ALLOCA_DEFAULT:
          a = ctx.builder.CreateAlloca(tv.type->llvm_type(loc, ctx), nullptr, name);
      }
      ctx.builder.CreateStore(tv.value, a);
      ctx.vars->insert(sstring::get(local), typed_value{a, corr_t});
      return {a, tv.type};
    }
    else {
      ctx.vars->insert(sstring::get(local), typed_value{tv.value, corr_t});
      return tv;
    }
  }
}
typed_value cobalt::ast::mutdef_ast::codegen(compile_context& ctx) const {
  varmap* vm = ctx.vars;
  if (name.front() == '.') while (vm->parent) vm = vm->parent;
  std::size_t old = name.front() == '.', idx = name.find('.', 1);
  while (idx != std::string::npos) {
    auto local = name.substr(old , idx - old - 1);
    auto ss = sstring::get(local);
    auto it = vm->symbols.find(ss);
    if (idx == std::string::npos) {
      if (it != vm->symbols.end()) ctx.flags.onerror(loc, "redefinition of " + name, ERROR);
      else break;
    }
    else {
      if (it == vm->symbols.end()) {
        auto nvm = std::make_shared<varmap>(vm);
        vm->symbols.insert({ss, symbol_type(nvm)});
        vm = nvm.get();
      }
      else if (it->second.index() == 2) vm = std::get<2>(it->second).get();
      else {
        ctx.flags.onerror(loc, name.substr(0, idx) + " is not a module", ERROR);
        return nullval;
      }
    }
    old = idx + 1;
    idx = name.find('.', old);
  }
  auto local = name.substr(old);
  std::vector<std::string_view> old_path;
  if (global) {
    if (val.is_const()) {
      if (name.front() == '.') {
        old_path = ctx.path;
        ctx.path = {name.substr(1)};
      }
      else ctx.path.push_back(name);
      auto tv = val(ctx);
      if (name.front() == '.') std::swap(ctx.path, old_path);
      else ctx.path.pop_back();
      if (!tv.type) return nullval;
      auto ct = tv.type->kind == INTEGER && !static_cast<types::integer const*>(tv.type)->nbits ? types::integer::get(64) : tv.type;
      auto gv = new llvm::GlobalVariable(*ctx.module, ct->llvm_type(loc, ctx), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, llvm::cast<llvm::Constant>(tv.value), name.front() == '.' ? std::string_view(name) : std::string_view(concat(ctx.path, name)));
      auto type = types::reference::get(ct);
      vm->insert(sstring::get(local), typed_value{gv, type});
      return {gv, type};
    }
    else {
      auto ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx.context), false);
      if (!ft) return nullval;
      auto f = llvm::Function::Create(ft, llvm::GlobalValue::LinkageTypes::PrivateLinkage, "global.init." + llvm::Twine(ctx.init_count++), ctx.module.get());
      if (!f) return nullval;
      auto rt = val.type(ctx);
      auto ct = rt->kind == INTEGER && !static_cast<types::integer const*>(rt)->nbits ? types::integer::get(64) : rt;
      auto gv = new llvm::GlobalVariable(*ctx.module, ct->llvm_type(loc, ctx), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, nullptr, name.front() == '.' ? std::string_view(name) : std::string_view(concat(ctx.path, name)));
      auto bb = llvm::BasicBlock::Create(*ctx.context, "entry", f);
      ctx.builder.SetInsertPoint(bb);
      if (name.front() == '.') {
        old_path = ctx.path;
        ctx.path = {name.substr(1)};
      }
      else ctx.path.push_back(name);
      if (auto ast = val.dyn_cast<ast::array_ast>()) ast->is_static = true;
      auto tv = val(ctx);
      if (name.front() == '.') std::swap(ctx.path, old_path);
      else ctx.path.pop_back();
      if (!tv.type) {
        f->eraseFromParent();
        gv->eraseFromParent();
        return nullval;
      }
      ctx.builder.CreateStore(tv.value, gv);
      ctx.builder.CreateRetVoid();
      ctx.builder.SetInsertPoint((llvm::BasicBlock*)nullptr);
      auto type = types::reference::get(ct);
      vm->insert(sstring::get(local), typed_value{gv, type});
      return {gv, type};
    }
  }
  else {
    if (name.front() == '.') {
      old_path = ctx.path;
      ctx.path = {name.substr(1)};
    }
    else ctx.path.push_back(name);
    auto tv = val(ctx);
    if (name.front() == '.') std::swap(ctx.path, old_path);
    else ctx.path.pop_back();
    if (!tv.type) return nullval;
    llvm::Value* a;
    switch (tv.type->kind) {
      case FUNCTION:
        a = ctx.builder.CreateAlloca(llvm::Type::getInt8Ty(*ctx.context), nullptr, name);
        break;
      case INTEGER:
        if (!static_cast<types::integer const*>(tv.type)->nbits) a = ctx.builder.CreateAlloca(llvm::Type::getInt64Ty(*ctx.context), nullptr, name);
        else goto ALLOCA_DEFAULT;
        break;
      case POINTER:
        if (static_cast<types::pointer const*>(tv.type)->base->kind == FUNCTION) a = ctx.builder.CreateAlloca(llvm::Type::getInt8PtrTy(*ctx.context), nullptr, name);
        else goto ALLOCA_DEFAULT;
        break;
      default: ALLOCA_DEFAULT:
        a = ctx.builder.CreateAlloca(tv.type->llvm_type(loc, ctx), nullptr, name);
    }
    ctx.builder.CreateStore(tv.value, a);
    auto type = types::reference::get(tv.type->kind == INTEGER && !static_cast<types::integer const*>(tv.type)->nbits ? types::integer::get(64) : tv.type);
    vm->insert(sstring::get(local), typed_value{a, type});
    return {a, type};
  }
}
typed_value cobalt::ast::varget_ast::codegen(compile_context& ctx) const {
  varmap* vm = ctx.vars;
  if (name.front() == '.') while (vm->parent) vm = vm->parent;
  std::size_t old = name.front() == '.', idx = name.find('.', 1);
  while (idx != std::string::npos) {
    auto local = name.substr(old, idx - old - 1);
    auto ptr = vm->get(sstring::get(local));
    if (ptr) {
      auto pidx = ptr->index();
      if (pidx == 2) vm = std::get<2>(*ptr).get();
      else ctx.flags.onerror(loc, name.substr(0, idx) + " is not a module", ERROR);
    }
    else {
      ctx.flags.onerror(loc, name.substr(0, old) + " does not exist", ERROR);
      return nullval;
    }
    old = idx + 1;
    idx = name.find('.', old);
  }
  auto ptr = vm->get(sstring::get(name.substr(old)));
  if (ptr) {
    auto idx = ptr->index();
    if (idx == 0) return std::get<0>(*ptr);
    else ctx.flags.onerror(loc, name + " is not a variable", ERROR);
  }
  else ctx.flags.onerror(loc, name + " does not exist", ERROR);
  return nullval;
}
