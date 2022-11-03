#ifndef COBALT_TYPES_POINTERS_HPP
#define COBALT_TYPES_POINTERS_HPP
#include "types.hpp"
#include <llvm/IR/DerivedTypes.h>
namespace cobalt::types {
  struct pointer : type_base {
    type_ptr base;
    sstring name() const override {return sstring::get(base->name() + "*");}
    std::size_t size() const override {return 8;} // TODO: support 32-bit platforms
    std::size_t align() const override {return 8;}
    llvm::Type* llvm_type(location loc, compile_context& ctx) const override {return llvm::PointerType::get(base->llvm_type(loc, ctx), 0);}
    static pointer const* get(type_ptr base) {
      auto it = instances.find(base);
      if (it == instances.end()) it = instances.insert({base, COBALT_MAKE_UNIQUE(pointer, base)}).first;
      return it->second.get();
    }
  private:
    pointer(type_ptr base) : type_base(POINTER), base(base) {}
    inline static std::unordered_map<type_ptr, std::unique_ptr<pointer>> instances;
  };
  struct reference : type_base {
    type_ptr base;
    sstring name() const override {return sstring::get(base->name() + "&");}
    std::size_t size() const override {return 8;}
    std::size_t align() const override {return 8;}
    llvm::Type* llvm_type(location loc, compile_context& ctx) const override {return llvm::PointerType::get(base->llvm_type(loc, ctx), 0);}
    static reference const* get(type_ptr base) {
      auto it = instances.find(base);
      if (it == instances.end()) it = instances.insert({base, COBALT_MAKE_UNIQUE(reference, base)}).first;
      return it->second.get();
    }
  private:
    reference(type_ptr base) : type_base(base->type), base(base) {}
    inline static std::unordered_map<type_ptr, std::unique_ptr<reference>> instances;
  };
  struct borrow : type_base {
    type_ptr base;
    sstring name() const override {return sstring::get(base->name() + "^");}
    std::size_t size() const override {return base->size();}
    std::size_t align() const override {return base->align();}
    llvm::Type* llvm_type(location loc, compile_context& ctx) const override {return base->llvm_type(loc, ctx);}
    static borrow const* get(type_ptr base) {
      auto it = instances.find(base);
      if (it == instances.end()) it = instances.insert({base, COBALT_MAKE_UNIQUE(borrow, base)}).first;
      return it->second.get();
    }
  private:
    borrow(type_ptr base) : type_base(base->type), base(base) {}
    inline static std::unordered_map<type_ptr, std::unique_ptr<borrow>> instances;
  };
}
#endif