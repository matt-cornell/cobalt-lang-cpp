#include <llvm/Support/raw_ostream.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include "cobalt/version.hpp"
#include "cobalt/compile.hpp"
// TODO: add help and usage messages
constexpr char help[] = R"(co- Cobalt compiler and build tool
subcommands:
co aot: compile file
co jit: execute file
co build: build project
co help: show help
use co help [subject] for more in-depth instructions
)";
constexpr char usage[] = R"(co build <project directory or file>
co aot [options] <file>
co jit [options] <file>
co build [options] [project directory]
co help [subject]
)";
constexpr char aot_help[] = R"(co aot [options] file
--emit-obj, --emit-object   emit as object file
--emit-asm, --emit-assembly emit assembly
--emit-llvm                 emit LLVM IR
--emit-bc, --emit-bitcode   emit LLVM bitcode
-o <output file>            select output file
-O<level>                   optimization level
-l<lib>                     link library
)";
constexpr char jit_help[] = R"(co jit [options] file
-O<level>                   optimization level
-l<lib>                     link library
)";
constexpr char build_help[] = R"(co build [options] [root]
[root] can the project file or the path to the directory containing it. It defaults to the current directory, searching upwards if a project file is not found.
-t <target>                 build target
-O<level>                   optimization level
-p <preset>                 build preset (debug, release, etc.)
)";
constexpr char tokenize_help[] = R"(co tokenize file1, file2...
-c                          interpret next argument as code to tokenize
tokens are printed as file:line:col: data, where data will be printed as hex for numeric values
)";
constexpr char parse_help[] = R"(co parse file1, file2...
-c                          interpret next argument as code to parse
)";
void pretty_print(cobalt::token const& tok) {
  constexpr char chars[] = "0123456789abcdef";
  llvm::outs() << tok.loc << ":\t";
  if (tok.data.size()) {
    char c = tok.data.front();
    if (c >= '0' && c <= '9') {
      llvm::outs() << c;
      auto it = tok.data.begin();
      llvm::outs() << ' ';
      while (++it != tok.data.end()) {
        llvm::outs() << chars[(unsigned char)(*it) >> 4] << chars[*it & 15];
      }
    }
    else llvm::outs() << tok.data;
  }
  llvm::outs() << '\n';
}
template <int code> int cleanup() {llvm::errs().flush(); return code;}
int main(int argc, char** argv) {
  if (argc == 1) {
    llvm::outs() << help;
    return cleanup<0>();
  }
  std::string_view cmd = argv[1];
  if (cmd == "help" || cmd == "--help" || cmd == "-h") {
    switch (argc) {
      case 2:
        llvm::outs() << help;
        return cleanup<0>();
      case 3: {
        cmd = argv[2];
        // TODO: add help messages
        if (cmd == "help") {llvm::outs() << help; return cleanup<0>();}
        else if (cmd == "usage") {llvm::outs() << usage; return cleanup<0>();}
        else if (cmd == "aot") {llvm::outs() << aot_help; return cleanup<0>();}
        else if (cmd == "jit") {llvm::outs() << jit_help; return cleanup<0>();}
        else if (cmd == "build") {llvm::outs() << build_help; return cleanup<0>();}
        else if (cmd == "tokenize") {llvm::outs() << tokenize_help; return cleanup<0>();}
        else if (cmd == "parse") {llvm::outs() << parse_help; return cleanup<0>();}
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
  if (cmd == "usage" || cmd == "--usage") {llvm::outs() << usage; return cleanup<0>();}
  if (cmd == "tokenize") {
    cobalt::flags_t flags = cobalt::default_flags;
    cobalt::default_handler_t handler;
    flags.onerror = handler;
    bool fail = false;
    std::string str;
    for (auto it = argv + 2; it != argv + argc; ++it) {
      handler = cobalt::default_handler;
      std::string_view file = *it;
      std::vector<cobalt::token> toks;
      if (file == "-c") toks = cobalt::tokenize(*++it, cobalt::sstring::get("<command line>"));
      else {
        auto eo = llvm::MemoryBuffer::getFileOrSTDIN(file);
        if (eo) toks = cobalt::tokenize(eo.get()->getBuffer(), cobalt::sstring::get(file == "-" ? "<stdin>" : file), flags);
        else {
          llvm::errs() << eo.getError().message() << '\n';
          fail = true;
        }
      }
      for (auto const& tok : toks) pretty_print(tok);
      fail |= handler.errors;
    }
    return fail;
  }
  if (cmd == "parse") {
    cobalt::flags_t flags = cobalt::default_flags;
    cobalt::default_handler_t handler;
    flags.onerror = handler;
    bool fail = false;
    std::string str;
    for (auto it = argv + 2; it != argv + argc; ++it) {
      handler = cobalt::default_handler;
      std::string_view file = *it;
      std::vector<cobalt::token> toks;
      if (file == "-c") toks = cobalt::tokenize(*++it, cobalt::sstring::get("<command line>"));
      else {
        auto eo = llvm::MemoryBuffer::getFileOrSTDIN(file);
        if (eo) toks = cobalt::tokenize(eo.get()->getBuffer(), cobalt::sstring::get(file == "-" ? "<stdin>" : file), flags);
        else {
          llvm::errs() << eo.getError().message() << '\n';
          fail = true;
        }
      }
      auto ast = cobalt::parse({toks.begin(), toks.end()}, flags);
      ast.print(llvm::outs());
      fail |= handler.errors;
    }
    return fail;
  }
  if (cmd == "build") {
    // TODO: add build command
    return cleanup<0>();
  }
  if (cmd == "aot") {
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
