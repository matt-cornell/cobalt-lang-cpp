#ifndef COBALT_SUPPORT_TYPED_VALUE_HPP
#define COBALT_SUPPORT_TYPED_VALUE_HPP
#include "cobalt/types/types.hpp"
#include <llvm/IR/Value.h>
namespace cobalt {
  struct typed_value {
    llvm::Value* value;
    types::type_base const* type;
  };
  inline typed_value nullval {nullptr, nullptr};
}
#endif