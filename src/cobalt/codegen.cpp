#include "cobalt/ast.hpp"
using namespace cobalt;
// flow.hpp
typed_value cobalt::ast::top_level_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::block_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::if_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::while_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::for_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
// funcs.hpp
typed_value cobalt::ast::binop_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::call_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::fndef_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
// meta.hpp
typed_value cobalt::ast::llvm_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::asm_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
// vars.hpp
typed_value cobalt::ast::vardef_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::mutdef_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}
typed_value cobalt::ast::varget_ast::codegen_impl(compile_context& ctx) const {(void)ctx; return nullval;}