#ifndef COBALT_SUPPORT_TYPED_VALUE_HPP
#define COBALT_SUPPORT_TYPED_VALUE_HPP
namespace llvm {struct Value;}
namespace cobalt {
  namespace types {struct type_base;}
  struct typed_value {
    llvm::Value* value;
    types::type_base const* type;
  };
}
#endif