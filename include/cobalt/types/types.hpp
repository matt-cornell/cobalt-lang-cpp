#ifndef COBALT_TYPES_TYPES_HPP
#define COBALT_TYPES_TYPES_HPP
#define COBALT_MAKE_UNIQUE(TYPE, ...) std::unique_ptr<TYPE>(new TYPE(__VA_ARGS__))
#include "cobalt/context.hpp"
#include "cobalt/support/sstring.hpp"
#include "cobalt/support/location.hpp"
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
namespace cobalt {
  struct compile_context;
  namespace types {
    struct type_base {
      enum kind_t {INTEGER, FLOAT, POINTER, REFERENCE, ARRAY, FUNCTION, NULLTYPE, CUSTOM};
      const kind_t kind;
      type_base(kind_t kind) : kind(kind) {}
      type_base(type_base const&) = delete;
      type_base(type_base&&) = delete;
      virtual ~type_base() = 0;
      virtual sstring name() const = 0;
      virtual std::size_t size() const = 0;
      virtual std::size_t align() const = 0;
      virtual llvm::Type* llvm_type(location loc, compile_context& ctx) const = 0;
    };
    inline type_base::~type_base() {}
  }
  using type_ptr = types::type_base const*;
}
#endif
