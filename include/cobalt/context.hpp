#ifndef COBALT_CONTEXT_HPP
#define COBALT_CONTEXT_HPP
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include "cobalt/flags.hpp"
namespace cobalt {
  struct varmap;
  struct compile_context {
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;
    varmap* vars;
    flags_t flags;
    explicit compile_context(std::string const& name, flags_t flags = default_flags) : context(std::make_unique<llvm::LLVMContext>()), module(std::make_unique<llvm::Module>(name, *context)), builder(*context), flags(flags) {}
  };
  inline compile_context global{"<anonymous>"};
}
#endif