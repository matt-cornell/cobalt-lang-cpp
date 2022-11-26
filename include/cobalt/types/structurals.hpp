#ifndef COBALT_TYPES_STRUCTURALS_HPP
#define COBALT_TYPES_STRUCTURALS_HPP
#include "types.hpp"
namespace cobalt::types {
  namespace {
    struct tuple_hash {
      std::size_t operator()(std::vector<type_ptr> const& val) const {
        std::size_t out = 0;
        for (type_ptr ptr : val) out ^= (uintptr_t)ptr + 0x9e3779b9 + (out << 6) + (out >> 2);
        return out;
      }
    };
    struct tuple_eq {
      bool operator()(std::vector<type_ptr> const& lhs, std::vector<type_ptr> const& rhs) const {
        if (lhs.size() != rhs.size()) return false;
        return !std::memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(void*));
      }
    };
    struct variant_hash {
      std::size_t operator()(std::unordered_set<type_ptr> const& val) const {
        std::size_t out = 0;
        for (type_ptr ptr : val) out += (uintptr_t)ptr;
        return out;
      }
    };
    struct arr_hash {
      std::size_t operator()(std::pair<type_ptr, std::size_t> val) const noexcept {
        std::size_t out = (uintptr_t)val.first + 0x9e3779b9;
        out ^= val.second + 0x9e3779b9 + (out << 6) + (out >> 2);
        return out;
      }
    };
  }
  struct tuple : type_base {
    std::vector<type_ptr> types;
    sstring name() const override;
    std::size_t size() const override;
    std::size_t align() const override;
    llvm::Type* llvm_type(location loc, compile_context& ctx) const override;
    static tuple const* get(std::vector<type_ptr> const& types) {
      auto it = instances.find(types);
      if (it == instances.end()) it = instances.insert({types, COBALT_MAKE_UNIQUE(tuple, types)}).first;
      return it->second.get();
    }
  private:
    tuple(std::vector<type_ptr> const& types) : type_base(CUSTOM), types(types) {}
    inline static std::unordered_map<std::vector<type_ptr>, std::unique_ptr<tuple>, tuple_hash, tuple_eq> instances;
  };
  struct variant : type_base {
    std::unordered_set<type_ptr> types;
    sstring name() const override;
    std::size_t size() const override;
    std::size_t align() const override;
    llvm::Type* llvm_type(location loc, compile_context& ctx) const override;
    static variant const* get(std::unordered_set<type_ptr> const& types) {
      auto it = instances.find(types);
      if (it == instances.end()) it = instances.insert({types, COBALT_MAKE_UNIQUE(variant, types)}).first;
      return it->second.get();
    }
  private:
    variant(std::unordered_set<type_ptr> const& types) : type_base(CUSTOM), types(types) {}
    inline static std::unordered_map<std::unordered_set<type_ptr>, std::unique_ptr<variant>, variant_hash> instances;
  };
  struct array : type_base {
    type_ptr base;
    std::size_t length;
    sstring name() const override {return sstring::get((llvm::Twine(base->name()) + "[" + (length + 1 ? llvm::Twine(length) : llvm::Twine("")) + "]").str());}
    std::size_t size() const override {return sizeof(void*) * (length + 1 ? 2 : 1);}
    std::size_t align() const override {return sizeof(void*);}
    llvm::Type* llvm_type(location loc, compile_context& ctx) const override {return length + 1 ? (llvm::Type*)llvm::ArrayType::get(base->llvm_type(loc, ctx), length) : (llvm::Type*)llvm::StructType::get(llvm::ArrayType::get(base->llvm_type(loc, ctx), 0), llvm::Type::getInt64Ty(*ctx.context));}
    static array const* get(type_ptr base, std::size_t length = -1) {
      auto it = instances.find(std::pair<type_ptr, std::size_t>{base, length});
      if (it == instances.end()) it = instances.insert({std::pair<type_ptr, std::size_t>{base, length}, COBALT_MAKE_UNIQUE(array, base, length)}).first;
      return it->second.get();
    }
  private:
    array(type_ptr base, std::size_t length) : type_base(ARRAY), base(base), length(length) {}
    inline static std::unordered_map<std::pair<type_ptr, std::size_t>, std::unique_ptr<array>, arr_hash> instances;
  };
  struct struct_ : type_base {
    enum layout_t {C, EFFICIENT, PACKED};
    layout_t layout;
  };
  struct union_ : type_base {

  };
  struct tag_enum : type_base {

  };
}
#endif
