#ifndef COBALT_TYPES_FUNCTIONS_HPP
#define COBALT_TYPES_FUNCTIONS_HPP
namespace cobalt::types {
  namespace {
    struct fn_hash {
      std::size_t operator()(std::vector<type_ptr> const& val) const {
        std::size_t out = 0;
        for (type_ptr ptr : val) out ^= (uintptr_t)ptr + 0x9e3779b9 + (out << 6) + (out >> 2);
        return out;
      }
    };
    struct fn_eq {
      bool operator()(std::vector<type_ptr> const& lhs, std::vector<type_ptr> const& rhs) const {
        if (lhs.size() != rhs.size()) return false;
        return !std::memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(void*));
      }
    };
  }
  struct function : type_base {
    type_ptr ret;
    std::vector<type_ptr> args;
    sstring name() const override {
      std::string out{ret->name()};
      out += '(';
      for (auto arg : args) {
        out += arg->name();
        out += ", ";
      }
      if (args.empty()) out += ")";
      else {
        out.pop_back();
        out.back() = ')';
      }
      return sstring::get(out);
    }
    std::size_t size() const override {return 0;}
    std::size_t align() const override {return 1;}
    llvm::Type* llvm_type(location loc, compile_context& ctx) const override {
      auto r = ret->llvm_type(loc, ctx);
      std::vector<llvm::Type*> as(args.size());
      for (std::size_t i = 0; i < args.size(); ++i) as[i] = args[i]->llvm_type(loc, ctx);
      return llvm::FunctionType::get(r, as, false);
    }
    static function const* get(type_ptr ret, std::vector<type_ptr>&& args) {
      std::vector<type_ptr> lookup(args.size() + 1);
      lookup[0] = ret;
      std::memcpy(lookup.data() + 1, args.data(), args.size() * sizeof(type_ptr));
      auto it = instances.find(lookup);
      if (it == instances.end()) it = instances.insert({std::move(lookup), COBALT_MAKE_UNIQUE(function, ret, std::move(args))}).first;
      return it->second.get();
    }
  private:
    function(type_ptr ret, std::vector<type_ptr>&& args) : type_base(FUNCTION), ret(ret), CO_INIT(args) {}
    inline static std::unordered_map<std::vector<type_ptr>, std::unique_ptr<function>, fn_hash, fn_eq> instances;
  };
}
#endif