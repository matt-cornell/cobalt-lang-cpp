#ifndef COBALT_AST_AST_HPP
#define COBALT_AST_AST_HPP
#include "cobalt/context.hpp"
#include "cobalt/support/sstring.hpp"
#include "cobalt/support/location.hpp"
#include "cobalt/typed_value.hpp"
#define CO_INIT(ID) ID(std::move(ID))
namespace cobalt {
  class AST;
  namespace ast {
    struct ast_base {
      location loc;
      ast_base(sstring file, std::size_t line, std::size_t col) : loc{file, line, col} {}
      ast_base(location loc) : loc(loc) {}
      virtual ~ast_base() noexcept = 0;
      virtual bool is_const() const noexcept {return false;}
      virtual bool eq(ast_base const* other) const = 0;
      virtual typed_value codegen(compile_context& ctx = global) const = 0;
      virtual type_ptr type(base_context& ctx = global) const = 0;
      void print(llvm::raw_ostream& os) const {print_impl(os, "");}
    private:
      virtual void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const = 0;
      friend class ::cobalt::AST;
    protected:
      void print_self(llvm::raw_ostream& os, llvm::Twine name) const;
      void print_node(llvm::raw_ostream& os, llvm::Twine prefix, AST const& ast, bool last) const;
    };
    inline ast_base::~ast_base() noexcept {}
  }
  class AST {
    friend class ast::ast_base;
    ast::ast_base* ptr;
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {if (ptr) ptr->print_impl(os, prefix);}
  public:
    AST(std::nullptr_t) noexcept : ptr(nullptr) {}
    AST(ast::ast_base* ptr) noexcept : ptr(ptr) {}
    AST(std::unique_ptr<ast::ast_base>&& ptr) : ptr(std::move(ptr).release()) {}
    AST(AST&& other) noexcept : ptr(other.ptr) {other.ptr = nullptr;}
    ~AST() noexcept {delete ptr;}
    AST& operator=(AST&& other) noexcept {
      if (ptr) delete ptr;
      ptr = other.ptr;
      other.ptr = nullptr;
      return *this;
    }
    bool is_const() const noexcept {return ptr && ptr->is_const();}
    location loc() const noexcept {return ptr ? ptr->loc : nullloc;}
    sstring file() const noexcept {return ptr ? ptr->loc.file : sstring::get("");}
    std::size_t line() const noexcept {return ptr ? ptr->loc.line : 0;}
    std::size_t col() const noexcept {return ptr ? ptr->loc.col : 0;}
    typed_value codegen(compile_context& ctx = global) const {return ptr ? ptr->codegen(ctx) : nullval;}
    typed_value operator()(compile_context& ctx = global) const {return ptr ? ptr->codegen(ctx) : nullval;}
    type_ptr type(base_context& ctx = global) const {return ptr ? ptr->type(ctx) : nullptr;}
    void print(llvm::raw_ostream& os = llvm::outs()) const {if (ptr) ptr->print(os);}
    bool operator==(AST const& other) const {return ptr ? (other && ptr->eq(other.ptr)) : !bool(other);}
    explicit operator bool() const noexcept {return (bool)ptr;}
    ast::ast_base* get() const noexcept {return ptr;}
    template <class T> T* cast() const {return ptr ? static_cast<T*>(ptr) : (T*)nullptr;}
    template <class T> T* dyn_cast() const {return ptr ? dynamic_cast<T*>(ptr) : (T*)nullptr;}
    template <class T, class... As> static AST create(As&&... args) {return new T(std::forward<As>(args)...);}
    template <class T, class... As> static AST create_nothrow(As&&... args) noexcept {
      ast::ast_base* ptr;
      try {
        ptr = new(std::nothrow) T(std::forward<As>(args)...);
        return ptr;
      }
      catch (...) {
        if (ptr) delete ptr;
        return nullptr;
      }
    }
  };
}
#endif
