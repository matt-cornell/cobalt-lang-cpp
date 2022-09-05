#include <llvm/Support/raw_ostream.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include "cobalt/version.hpp"
#include "cobalt/compile.hpp"
// TODO: add help and usage messages
constexpr char help[] = R"######(co- Cobalt compiler and build tool
)######";
constexpr char usage[] = R"######(co build <project directory or file>
co compile [options] file...
co jit [options] file
)######";
template <int code> int cleanup() {llvm::errs().flush(); return code;}
int main(int argc, char** argv) {
  if (argc == 1) {
    llvm::errs() << help;
    return cleanup<0>();
  }
  std::string_view cmd = argv[1];
  if (cmd == "help" || cmd == "--help" || cmd == "-h") {
    switch (argc) {
      case 2:
        llvm::errs() << help;
        return cleanup<0>();
      case 3: {
        cmd = argv[2];
        // TODO: add help messages
        if (cmd == "help") {llvm::errs() << "placeholder help message\n"; return cleanup<0>();}
        else if (cmd == "compile") {llvm::errs() << "placeholder help message\n"; return cleanup<0>();}
        else if (cmd == "build") {llvm::errs() << "placeholder help message\n"; return cleanup<0>();}
        else if (cmd == "jit") {llvm::errs() << "placeholder help message\n"; return cleanup<0>();}
        else {llvm::errs() << "unknown subcommand '" << cmd << "'\n"; return cleanup<1>();}
      }
      default:
        llvm::errs() << R"###(help option must be used as:
co help or
co help [category]
)###";
        return cleanup<1>();
    }
  }
  if (cmd == "usage" || cmd == "--usage") {
    llvm::errs() << usage;
    return cleanup<0>();
  }
  if (cmd == "build") {
    // TODO: add build command
    return cleanup<0>();
  }
  if (cmd == "run") {
    // TODO: add build command
  }
  if (cmd == "compile") {
    std::string_view input = "", output = "";
    std::uint8_t opt_lvl = -1;
    std::vector<std::string_view> linked;
    enum {UNSPEC, LLVM, ASM, BC, OBJ} output_type = UNSPEC;
    enum {DEFAULT, QUIET, WERROR} error_type = DEFAULT;
    for (char** it = argv + 2; it < argv + argc; ++it) {
      std::string_view cmd = *it;
      if (cmd.front() == '-') {
        cmd.remove_prefix(1);
        switch (cmd.front()) {
          case '-':
            cmd.remove_prefix(1);
            if (cmd.substr(0, 5) == "emit-") {
              cmd.remove_prefix(5);
              bool spec = output_type;
              if (cmd == "llvm") output_type = LLVM;
              else if (cmd == "asm" || cmd == "assembly") output_type = ASM;
              else if (cmd == "bc" || cmd == "bitcode") output_type = BC;
              else if (cmd == "obj" || cmd == "object") output_type = OBJ;
              else {
                llvm::errs() << "unknown flag --emit-" << cmd << '\n';
                return cleanup<1>();
              }
              if (spec) llvm::errs() << "redefinition of output type\n";
            }
            else if (cmd == "quiet") {
              if (error_type != DEFAULT) llvm::errs() << "redefinition or override of error mode\n";
              error_type = QUIET;
            }
            else if (cmd == "werrror") {
              if (error_type != DEFAULT) llvm::errs() << "redefinition or override of error mode\n";
              error_type = WERROR;
            }
            else {
              llvm::errs() << "unkown flag --" << cmd << '\n';
              return cleanup<1>();
            }
            break;
          case 'l':
            linked.push_back(cmd.substr(1));
            break;
          case 'O':
            if (cmd.size() > 2) {
              llvm::errs() << "optimization level should be a single-digit number\n";
              return cleanup<1>();
            }
            if (opt_lvl != 255) llvm::errs() << "redefinition of optimization level\n";
            if (cmd[1] < '0' || cmd[1] > '9') {
              llvm::errs() << "invalid value for optimization level\n";
              return cleanup<1>();
            }
            opt_lvl = cmd[1] - '0';
            break;
          default:
            for (char c : cmd) switch (c) {
              case 'l': llvm::errs() << "-l flag must not be specified with any other flags\n"; return cleanup<1>();
              case 'O': llvm::errs() << "-O flag must not be specified with any other flags\n"; return cleanup<1>();
              case 'o':
                if (!output.empty()) {
                  llvm::errs() << "redefinition of output file\n";
                  return cleanup<1>();
                }
                ++it;
                if (it == argv + argc) {
                  llvm::errs() << "unspecified output file\n";
                  return cleanup<1>();
                }
                break;
            }
        }
      }
      else {
        if (!input.empty()) {
          llvm::errs() << "redefinition of input file\n";
          return cleanup<1>();
        }
        input = cmd;
      }
    }
    auto f = llvm::MemoryBuffer::getFileOrSTDIN(input, true, false);
    if (!f) {
      llvm::errs() << f.getError().message();
      return cleanup<1>();
    }
    std::string_view code{f.get()->getBuffer().data(), f.get()->getBufferSize()};
    cobalt::flags_t flags = cobalt::default_flags;
    bool* critical = &cobalt::default_handler.critical;
    switch (error_type) {
      case DEFAULT: break;
      case QUIET:
        flags.onerror = cobalt::quiet_handler;
        critical = &cobalt::quiet_handler.critical;
        break;
      case WERROR:
        flags.onerror = cobalt::werror_handler;
        critical = &cobalt::werror_handler.critical;
        break;
    }
    auto toks = cobalt::tokenize(code, cobalt::sstring::get(input == "-" ? "<stdin>" : input), flags);
    if (*critical) return cleanup<2>();
    cobalt::AST ast = cobalt::parse({toks.begin(), toks.end()}, flags);
    ast(cobalt::global);
    std::error_code ec;
    llvm::raw_fd_ostream os({output}, ec);
    if (ec) {
      llvm::errs() << ec.message();
      return cleanup<3>();
    }
    switch (output_type) {
      case LLVM:
        os << *cobalt::global.module;
        break;
      case BC:
        llvm::WriteBitcodeToFile(*cobalt::global.module, os);
        break;
      default:
        
        break;
    }
    // TODO: AOT compiler
    return cleanup<0>();
  }
  if (cmd == "jit") {
    // TODO: JIT compiler
    
    return cleanup<0>();
  }
  llvm::errs() << "unknown command '" << cmd << "'\n";
  return cleanup<1>();
}