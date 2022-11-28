#ifndef COBALT_TYPES_NUMERIC_HPP
#define COBALT_TYPES_NUMERIC_HPP
#include "types.hpp"
#include "cobalt/context.hpp"
#include <llvm/ADT/Twine.h>
namespace cobalt::types {
  using cobalt::compile_context;
  struct integer : type_base {
    int nbits;
    sstring name() const override {return sstring::get((nbits < 0 ? llvm::Twine("u") + llvm::Twine(-nbits) : llvm::Twine("i") + llvm::Twine(nbits)).str());}
    std::size_t size() const override {return ((nbits < 0 ? -nbits : nbits) + 7) / 8;}
    std::size_t align() const override {
      auto anb = nbits < 0 ? -nbits : nbits;
      if (anb <= 8) return 1;
      if (anb <= 16) return 2;
      if (anb <= 32) return 4;
      return 8;
    }
    llvm::Type* llvm_type(location, compile_context& ctx) const override {return llvm::Type::getIntNTy(*ctx.context, nbits < 0 ? -nbits : nbits);}
    static integer const* get(unsigned bits, bool is_unsigned = false) {
      int val = is_unsigned ? -(int)bits : (int)bits;
      auto it = instances.find(val);
      if (it == instances.end()) it = instances.insert({val, COBALT_MAKE_UNIQUE(integer, val)}).first;
      return it->second.get();
    }
    static integer const* word(llvm::DataLayout const& layout) {return get(layout.getPointerSize() * 8, false);}
    static integer const* uword(llvm::DataLayout const& layout) {return get(layout.getPointerSize() * 8, true);}
  private:
    integer(int nbits) : type_base(INTEGER), nbits(nbits) {}
    inline static std::unordered_map<int, std::unique_ptr<integer>> instances;
  };
  struct float16 : type_base {
    sstring name() const override {return name_;}
    std::size_t size() const override {return 2;}
    std::size_t align() const override {return 2;}
    llvm::Type* llvm_type(location, compile_context& ctx) const override {return llvm::Type::getHalfTy(*ctx.context);}
    static float16 const* get() {return &inst;}
  private:
    float16() : type_base(FLOAT) {}
    const inline static sstring name_ = sstring::get("f16");
    const static float16 inst;
  };
  struct float32 : type_base {
    sstring name() const override {return name_;}
    std::size_t size() const override {return 4;}
    std::size_t align() const override {return 4;}
    llvm::Type* llvm_type(location, compile_context& ctx) const override {return llvm::Type::getFloatTy(*ctx.context);}
    static float32 const* get() {return &inst;}
  private:
    float32() : type_base(FLOAT) {}
    const inline static sstring name_ = sstring::get("f32");
    const static float32 inst;
  };
  struct float64 : type_base {
    sstring name() const override {return name_;}
    std::size_t size() const override {return 8;}
    std::size_t align() const override {return 8;}
    llvm::Type* llvm_type(location, compile_context& ctx) const override {return llvm::Type::getDoubleTy(*ctx.context);}
    static float64 const* get() {return &inst;}
  private:
    float64() : type_base(FLOAT) {}
    const inline static sstring name_ = sstring::get("f64");
    const static float64 inst;
  };
  struct float128 : type_base {
    sstring name() const override {return name_;}
    std::size_t size() const override {return 16;}
    std::size_t align() const override {return 8;}
    llvm::Type* llvm_type(location, compile_context& ctx) const override {return llvm::Type::getFP128Ty(*ctx.context);}
    static float128 const* get() {return &inst;}
  private:
    float128() : type_base(FLOAT) {}
    const inline static sstring name_ = sstring::get("f128");
    const static float128 inst;
  };
  const inline float16 float16::inst;
  const inline float32 float32::inst;
  const inline float64 float64::inst;
  const inline float128 float128::inst;
}
#endif
