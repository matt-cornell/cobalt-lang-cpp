#ifndef COBALT_AST_LITERALS_HPP
#define COBALT_AST_LITERALS_HPP
#include "cobalt/ast/ast.hpp"
#include <llvm/ADT/APInt.h>
namespace cobalt::ast {
  struct literal_ast : ast_base {
    sstring suffix;
    literal_ast(location loc, sstring suffix) : ast_base(loc), suffix(suffix) {}
    bool is_const() const noexcept override {return true;}
    ~literal_ast();
  };
  inline literal_ast::~literal_ast() {}
  struct integer_ast : literal_ast {
    llvm::APInt val;
    integer_ast(location loc, llvm::APInt val, sstring suffix) : literal_ast(loc, suffix), val(std::move(val)) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<integer_ast const*>(other)) return suffix == ptr->suffix && val == ptr->val; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct float_ast : literal_ast {
    double val;
    float_ast(location loc, double val, sstring suffix) : literal_ast(loc, suffix), val(val) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<float_ast const*>(other)) return suffix == ptr->suffix && val == ptr->val; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct string_ast : literal_ast {
    std::string val;
    string_ast(location loc, std::string&& val, sstring suffix) : literal_ast(loc, suffix), val(std::move(val)) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<string_ast const*>(other)) return suffix == ptr->suffix && val == ptr->val; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct char_ast : literal_ast {
    std::string val; // string for multibyte chars
    char_ast(location loc, std::string&& val, sstring suffix) : literal_ast(loc, suffix), val(std::move(val)) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<char_ast const*>(other)) return suffix == ptr->suffix && val == ptr->val; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct array_ast : ast_base {
    std::vector<AST> vals;
    bool is_static = false;
    array_ast(location loc, std::vector<AST>&& vals) : ast_base(loc), CO_INIT(vals) {}
    ~array_ast();
    bool is_const() const noexcept override {for (auto const& val : vals) if (!val.is_const()) return false; return true;}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<array_ast const*>(other)) return vals == ptr->vals; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  inline array_ast::~array_ast() {}
}
#endif
