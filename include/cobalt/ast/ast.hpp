#ifndef COBALT_AST_AST_HPP
#define COBALT_AST_AST_HPP
#include "cobalt/support/sstring.hpp"
#include "cobalt/support/location.hpp"
#include "cobalt/typed_value.hpp"
#define CO_INIT(ID) ID(std::move(ID))
namespace cobalt {
  struct compile_context;
  extern compile_context global;
  namespace ast {
    struct ast_base {
      location loc;
      ast_base(sstring file, std::size_t line, std::size_t col) : loc{file, line, col} {}
      ast_base(location loc) : loc(loc) {}
      virtual ~ast_base() noexcept = 0;
      virtual bool eq(ast_base const* other) const = 0;
      typed_value codegen(compile_context& ctx = global) const;
    private:
      virtual typed_value codegen_impl(compile_context& ctx) const = 0;
    };
    inline ast_base::~ast_base() noexcept {}
    inline typed_value ast_base::codegen(compile_context& ctx) const {return codegen_impl(ctx);}
  }
  class AST {
    ast::ast_base* ptr;
  public:
    AST(std::nullptr_t) noexcept : ptr(nullptr) {}
    AST(ast::ast_base* ptr) noexcept : ptr(ptr) {}
    AST(std::unique_ptr<ast::ast_base>&& ptr) : ptr(std::move(ptr).release()) {}
    AST(AST&& other) noexcept : ptr(other.ptr) {other.ptr = nullptr;}
    ~AST() noexcept {if (ptr) delete ptr;}
    AST& operator=(AST&& other) noexcept {
      delete ptr;
      ptr = other.ptr;
      other.ptr = nullptr;
      return *this;
    }
    location loc() const noexcept {return ptr->loc;}
    sstring file() const noexcept {return ptr->loc.file;}
    std::size_t line() const noexcept {return ptr->loc.line;}
    std::size_t col() const noexcept {return ptr->loc.col;}
    inline typed_value codegen(compile_context& ctx = global) const {return ptr->codegen(ctx);}
    inline typed_value operator()(compile_context& ctx = global) const {return ptr->codegen(ctx);}
    bool operator==(AST const& other) const {return ptr->eq(other.ptr);}
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