#ifndef COBALT_VARMAP_HPP
#define COBALT_VARMAP_HPP
#include "support/sstring.hpp"
#include "typed_value.hpp"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <memory>
#include <variant>
namespace cobalt {
  namespace {
    struct param_hash {
      std::size_t operator()(std::vector<types::type_base const*> const& val) const {
        std::size_t out = 0;
        for (auto ptr : val) out ^= (uintptr_t)ptr + 0x9e3779b9 + (out << 6) + (out >> 2);
        return out;
      }
    };
    struct param_eq {
      bool operator()(std::vector<types::type_base const*> const& lhs, std::vector<types::type_base const*> const& rhs) const {
        if (lhs.size() != rhs.size()) return false;
        return !std::memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(void*));
      }
    };
  }
  struct varmap;
  using symbol_type = std::variant<typed_value, types::type_base const*, std::shared_ptr<varmap>>;
  using symbol_ptr = symbol_type const*;
  struct varmap {
    varmap* parent;
    std::unordered_map<sstring, symbol_type> symbols;
    varmap(varmap* parent = nullptr, decltype(symbols)&& symbols = {}) : parent(parent), symbols(std::move(symbols)) {}
    bool imported(varmap* other) const {return this == other || (parent && parent->imported(other));}
    symbol_ptr get(sstring name) const {
      auto it = symbols.find(name);
      return it == symbols.end() ? (parent ? parent->get(name) : nullptr) : &it->second;
    }
    bool insert(sstring name, symbol_type sym) {return symbols.insert(std::pair{name, sym}).second;}
    std::unordered_set<sstring> include(varmap* other) {
      std::unordered_set<sstring> out;
      if (!imported(other)) {
        for (auto [name, sym] : other->symbols) {
          auto [it, succ] = symbols.insert({name, sym});
          if (!succ) {
            if (sym.index() == it->second.index() && sym.index() == 2) std::get<2>(it->second)->include(std::get<2>(sym).get());
            else out.insert(name);
          }
        }
        include(other->parent);
      }
      return out;
    } 
  };
}
#endif