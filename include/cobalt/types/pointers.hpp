#ifndef COBALT_TYPES_POINTERS_HPP
#define COBALT_TYPES_POINTERS_HPP
#include "types.hpp"
#include <llvm/IR/DerivedTypes.h>
namespace cobalt::types {
  struct pointer : type_base {
    type_ptr base;
    sstring::string name() const override {return sstring::get(base->name() + "*");}
    std::size_t size() const override {return 8;} // TODO: support 32-bit platforms
    std::size_t align() const override {return 8;}
    llvm::Type* llvm_type(location loc, compile_context& ctx) const override {return llvm::PointerType::get(base->llvm_type(loc, ctx), 0);}
  };
  struct reference : type_base {
    type_ptr base;
    sstring::string name() const override {return sstring::get(base->name() + "&");}
    std::size_t size() const override {return 8;}
    std::size_t align() const override {return 8;}
    llvm::Type* llvm_type(location loc, compile_context& ctx) const override {return llvm::PointerType::get(base->llvm_type(loc, ctx), 0);}
  };
  struct borrow : type_base {
    type_ptr base;
    sstring::string name() const override {return sstring::get(base->name() + "^");}
    std::size_t size() const override {return base->size();}
    std::size_t align() const override {return base->align();}
    llvm::Type* llvm_type(location loc, compile_context& ctx) const override {return base->llvm_type(loc, ctx);}
  };
}
#endif