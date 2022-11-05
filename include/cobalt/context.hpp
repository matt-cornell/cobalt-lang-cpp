#ifndef COBALT_CONTEXT_HPP
#define COBALT_CONTEXT_HPP
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include "cobalt/flags.hpp"
#include "cobalt/varmap.hpp"
namespace cobalt {
  struct base_context {
    varmap* vars;
    flags_t flags;
    base_context(varmap* vars, flags_t flags = default_flags) : vars(vars), flags(flags) {}
  };
  struct compile_context : base_context {
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;
    unsigned init_count = 0;
    explicit compile_context(std::string const& name, flags_t flags = default_flags) : base_context(new varmap, flags), context(std::make_unique<llvm::LLVMContext>()), module(std::make_unique<llvm::Module>(name, *context)), builder(*context) {}
  };
  inline compile_context global{"<anonymous>"};
}
#endif