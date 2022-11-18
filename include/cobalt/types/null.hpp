#ifndef COBALT_TYPES_NULL_HPP
#define COBALT_TYPES_NULL_HPP
#include "cobalt/types/types.hpp"
namespace cobalt::types {
  struct null : type_base {
    sstring name() const override {return name_;}
    std::size_t size() const override {return 0;}
    std::size_t align() const override {return 1;}
    llvm::Type* llvm_type(location loc, compile_context& ctx) const override {return llvm::Type::getVoidTy(*ctx.context);}
    static null const* get() {return &inst;}
  private:
    null() : type_base(NULLTYPE) {}
    const inline static sstring name_ = sstring::get("null");
    const static null inst;
  };
  const inline null null::inst;
}
#endif