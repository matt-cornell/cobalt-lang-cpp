#include <llvm/Support/raw_ostream.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Host.h>
#include <llvm/IR/LegacyPassManager.h>
#include <filesystem>
#include "cobalt/version.hpp"
#include "cobalt/compile.hpp"
#ifndef _WIN32
#include <unistd.h>
#include <wait.h>
#endif
namespace fs = std::filesystem;
using rdi = fs::recursive_directory_iterator;
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
std::size_t len(cobalt::token const& tok) {return tok.loc.file.size() + long(std::log10(tok.loc.line) + 1) + long(std::log10(tok.loc.col) + 1);}
void pretty_print(llvm::raw_ostream& os, std::size_t sz, cobalt::token const& tok) {
  constexpr char chars[] = "0123456789abcdef";
  std::size_t sz2 = len(tok);
  os << tok.loc << ": ";
  for (std::size_t i = sz2; i < sz; ++i) os << ' ';
  if (tok.data.size()) {
    char c = tok.data.front();
    if (c >= '0' && c <= '9') {
      os << c;
      auto it = tok.data.begin();
      os << ' ';
      while (++it != tok.data.end()) {
        os << chars[(unsigned char)(*it) >> 4] << chars[*it & 15];
      }
    }
    else os << tok.data;
  }
  os << '\n';
}
template <int code> int cleanup() {llvm::errs().flush(); return code;}
llvm::raw_ostream& warn() {return llvm::errs().changeColor(llvm::raw_ostream::YELLOW, true).write("warning: ", 9).resetColor();}
llvm::raw_ostream& error() {return llvm::errs().changeColor(llvm::raw_ostream::RED, true).write("error: ", 7).resetColor();}
int add_jdl(std::vector<std::pair<std::string_view, bool>>& linked, std::string_view lib, std::unique_ptr<llvm::orc::LLJIT>& jit, fs::path const& path) {
  for (auto& [l, p] : linked) if (!p && l == lib) {
    p = true;
    auto ex_jdl = jit->createJITDylib(std::string(lib));
    if (ex_jdl) {
      auto& jdl = ex_jdl.get();
      std::unique_ptr<llvm::orc::DynamicLibrarySearchGenerator> dlsg;
      if (auto err = llvm::orc::DynamicLibrarySearchGenerator::Load(path.c_str(), 0).moveInto(dlsg)) {
        llvm::errs() << err;
        return cleanup<6>();
      }
      jdl.addGenerator(std::move(dlsg));
      jit->getMainJITDylib().addToLinkOrder(jdl);
    }
    else {
      llvm::errs() << ex_jdl.takeError();
      return cleanup<6>();
    }
  }
  return 0;
}
int main(int argc, char** argv, char** env) {
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
        if (cmd == "help") {llvm::outs() << help; return cleanup<0>();}
        else if (cmd == "usage") {llvm::outs() << usage; return cleanup<0>();}
        else if (cmd == "aot") {llvm::outs() << aot_help; return cleanup<0>();}
        else if (cmd == "jit") {llvm::outs() << jit_help; return cleanup<0>();}
        else if (cmd == "build") {llvm::outs() << build_help; return cleanup<0>();}
        else if (cmd == "tokenize") {llvm::outs() << tokenize_help; return cleanup<0>();}
        else if (cmd == "parse") {llvm::outs() << parse_help; return cleanup<0>();}
        else {error() << "unknown subcommand '" << cmd << "'\n"; return cleanup<1>();}
      }
      default:
        error() << R"###(help option must be used as:
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
          error() << "error opening " << file << ": " << eo.getError().message() << '\n';
          fail = true;
        }
      }
      std::size_t sz = 0;
      for (auto const& tok : toks) sz = std::max(sz, len(tok));
      for (auto const& tok : toks) pretty_print(llvm::outs(), sz, tok);
      fail |= handler.errors;
    }
    return fail;
  }
  if (cmd == "parse") {
    cobalt::flags_t flags = cobalt::default_flags;
    cobalt::default_handler_t handler;
    flags.onerror = handler;
    bool fail = false;
    for (auto it = argv + 2; it != argv + argc; ++it) {
      handler = cobalt::default_handler;
      std::string_view file = *it;
      std::vector<cobalt::token> toks;
      if (file == "-c") toks = cobalt::tokenize(*++it, cobalt::sstring::get("<command line>"));
      else {
        auto eo = llvm::MemoryBuffer::getFileOrSTDIN(file);
        if (eo) toks = cobalt::tokenize(eo.get()->getBuffer(), cobalt::sstring::get(file == "-" ? "<stdin>" : file), flags);
        else {
          error() << "error opening " << file << ": " << eo.getError().message() << '\n';
          fail = true;
        }
      }
      auto ast = cobalt::parse({toks.begin(), toks.end()}, flags);
      ast.print(llvm::outs());
      fail |= handler.errors;
    }
    return fail;
  }
  if (cmd == "debug") {
    bool markdown = false;
    std::string_view output = "";
    std::string code = "";
    bool code_set = false;
    std::string_view source = "";
    for (auto it = argv + 2; it != argv + argc; ++it) {
      std::string_view flag = *it;
      if (flag.front() == '-' && flag.size() != 1) {
        flag.remove_prefix(1);
        if (flag.front() == '-') {
          flag.remove_prefix(1);
          if (flag == "markdown") {
            if (markdown) warn() << "reuse of --markdown flag\n";
            else markdown = true;
          }
          else {
            llvm::errs() << "unknown flag --" << flag << '\n';
            return cleanup<1>();
          }
        }
        for (char c : flag) switch (c) {
          case 'c':
            code_set = true;
            if (++it == argv + argc) {
              error() << "unspecified input for -c flag\n";
              return cleanup<1>();
            }
            code = *it;
            break;
          case 'm':
            if (markdown) warn() << "reuse of -m flag\n";
            else markdown = true;
            break;
          case 'o':
            if (output.size()) {
              error() << "redefinition of output file\n";
              return cleanup<1>();
            }
            if (++it == argv + argc) {
              error() << "unspecified output file\n";
              return cleanup<1>();
            }
            output = *it;
            break;
          default:
            error() << "unknown flag -" << c;
            return cleanup<1>();
        }
      }
      else {
        if (code_set) {
          error() << "redefiniton of input\n";
          return cleanup<1>();
        }
        code_set = true;
        source = flag;
        auto f = llvm::MemoryBuffer::getFileOrSTDIN(flag, true, false);
        if (!f) {
          error() << "error opening " << flag << ": " << f.getError().message() << '\n';
          return cleanup<1>();
        }
        code = f.get()->getBuffer();
      }
    }
    if (!code_set) {
      error() << "no input for debug\n";
      return cleanup<1>();
    }
    std::error_code ec;
    llvm::raw_fd_ostream os({output.empty() ? (std::string(source.empty() ? "cmdline" : (source == "-" ? "stdin" : source)) + (markdown ? ".md" : ".out")) : output}, ec);
    if (ec) {
      if (output.empty()) error() << (markdown ? "error opening cmdline.md: " : "error opening cmdline.out: ") << ec.message() << '\n';
      else if (output == "-") error() << (markdown ? "error opening stdin.md: " : "error opening stdin.out: ") << ec.message() << '\n';
      else error() << "error opening " << output << ": " << ec.message() << '\n';
      return cleanup<3>();
    }
    cobalt::flags_t flags;
    cobalt::default_handler_t h;
    flags.onerror = h;
    std::size_t tok_warn, tok_err, ast_warn, ast_err, ll_warn, ll_err;
    bool tok_crit, ast_crit, ll_crit;
    std::string_view pretty_src = source.empty() ? "<command line>" : (source == "-" ? "<stdin>" : source);
    auto toks = cobalt::tokenize(code, cobalt::sstring::get(pretty_src), flags);
    tok_warn = std::exchange(h.warnings, 0);
    tok_err  = std::exchange(h.errors, 0);
    tok_crit = std::exchange(h.critical, false);
    auto ast = cobalt::parse({toks.begin(), toks.end()}, flags);
    ast_warn = std::exchange(h.warnings, 0);
    ast_err  = std::exchange(h.errors, 0);
    ast_crit = std::exchange(h.critical, false);
    cobalt::compile_context ctx{std::string(pretty_src)};
    ast(ctx);
    ll_warn = h.warnings;
    ll_err  = h.errors;
    ll_crit = h.critical;
    std::size_t sz = 0;
    for (auto const& tok : toks) sz = std::max(sz, len(tok));
    if (markdown) {
      if (source.empty()) os << "# Original\nThis is the original source from the command line:\n```\n";
      else if (source == "-") os << "# Original\nThis is the original source from the standard input:\n```\n";
      else os << "# Original\nThis is the original source from `" << source << "`:\n```\n";
      os << code;
      os << "\n```\n# Tokens\nThese are the generated tokens:\n```\n";
      for (auto const& tok : toks) pretty_print(os, sz, tok);
      os << "```\nThere were " << tok_warn << " warnings and " << tok_err << " errors.";
      if (tok_crit) os << "\nThere was at least one critical error.\n";
      else os << "\nThere were no critical errors.\n";
      os << "\n# AST\nThis is the generated AST:\n```\n";
      ast.print(os);
      os << "```\nThere were " << ast_warn << " warnings and " << ast_err << " errors.";
      if (ast_crit) os << "\nThere was at least one critical error.\n";
      else os << "\nThere were no critical errors.\n";
      os << "\n# LLVM IR\nThis is the generated IR:\n```\n";
      os << *ctx.module;
      os << "```\nThere were " << ll_warn << " warnings and " << ll_err << " errors.";
      if (ll_crit) os << "\nThere was at least one critical error.";
      else os << "\nThere were no critical errors.\n";
    }
    else {
      if (source.empty()) os << "Original\nThis is the original source from the command line:\n\n";
      else if (source == "-") os << "Original\nThis is the original source from the standard input:\n\n";
      else os << "Original\nThis is the original source from `" << source << "`:\n\n";
      os << code;
      os << "\n\nTokens\nThese are the generated tokens:\n\n";
      for (auto const& tok : toks) pretty_print(os, sz, tok);
      os << "\nThere were " << tok_warn << " warnings and " << tok_err << " errors.";
      if (tok_crit) os << "\nThere was at least one critical error.\n";
      else os << "\nThere were no critical errors.\n";
      os << "\nAST\nThis is the generated AST:\n\n";
      ast.print(os);
      os << "\nThere were " << ast_warn << " warnings and " << ast_err << " errors.";
      if (ast_crit) os << "\nThere was at least one critical error.\n";
      else os << "\nThere were no critical errors.\n";
      os << "\nLLVM IR\nThis is the generated IR:\n\n";
      os << *ctx.module;
      os << "\nThere were " << ll_warn << " warnings and " << ll_err << " errors.";
      if (ll_crit) os << "\nThere was at least one critical error.";
      else os << "\nThere were no critical errors.\n";
    }
    return cleanup<0>();
  }
  if (cmd == "build") {
    // TODO: add build command
    return cleanup<0>();
  }
  if (cmd == "aot") {
    std::string_view input = "";
    std::string output = "";
    std::uint8_t opt_lvl = -1;
    std::vector<std::string_view> linked;
    enum {UNSPEC, LLVM, ASM, BC, OBJ} output_type = UNSPEC;
    enum {DEFAULT, QUIET, WERROR} error_type = DEFAULT;
    auto triple = llvm::sys::getDefaultTargetTriple();
    for (char** it = argv + 2; it < argv + argc; ++it) {
      std::string_view cmd = *it;
      if (cmd.front() == '-') {
        cmd.remove_prefix(1);
        if (cmd.empty()) {
          if (!input.empty()) {
            error() << "redefinition of input file\n";
            return cleanup<1>();
          }
          input = "-";
        }
        else switch (cmd.front()) {
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
                error() << "unknown flag --emit-" << cmd << '\n';
                return cleanup<1>();
              }
              if (spec) warn() << "redefinition of output type\n";
            }
            else if (cmd == "quiet") {
              if (error_type != DEFAULT) warn() << "redefinition or override of error mode\n";
              error_type = QUIET;
            }
            else if (cmd == "werrror") {
              if (error_type != DEFAULT) warn() << "redefinition or override of error mode\n";
              error_type = WERROR;
            }
            else {
              error() << "unkown flag --" << cmd << '\n';
              return cleanup<1>();
            }
            break;
          case 'O':
            if (cmd.size() > 2) {
              error() << "optimization level should be a single-digit number\n";
              return cleanup<1>();
            }
            if (opt_lvl != 255) warn() << "redefinition of optimization level\n";
            if (cmd[1] < '0' || cmd[1] > '9') {
              error() << "invalid value for optimization level\n";
              return cleanup<1>();
            }
            opt_lvl = cmd[1] - '0';
            break;
          default:
            for (char c : cmd) switch (c) {
              case 'O': error() << "-O flag must not be specified with any other flags\n"; return cleanup<1>();
              case 'l':
                ++it;
                if (it == argv + argc) {
                  error() << "unspecified linked library\n";
                  return cleanup<1>();
                }
                linked.push_back(*it);
                break;
              case 'o':
                if (!output.empty()) {
                  error() << "redefinition of output file\n";
                  return cleanup<1>();
                }
                ++it;
                if (it == argv + argc) {
                  error() << "unspecified output file\n";
                  return cleanup<1>();
                }
                output = *it;
                break;
            }
        }
      }
      else {
        if (!input.empty()) {
          error() << "redefinition of input file\n";
          return cleanup<1>();
        }
        input = cmd;
      }
    }
    if (input.empty()) {
      error() << "input file not specified\n";
      return cleanup<1>();
    }
    if (output.empty()) switch (output_type) {
      case UNSPEC: output = "a.out"; break;
      case ASM:
        if (input == "-") output = "cmdline.s";
        else {
          auto idx = input.rfind('.');
          output = idx == std::string::npos ? input : input.substr(idx + 1);
          output += "s";
        }
        break;
      case LLVM:
        if (input == "-") output = "cmdline.ll";
        else {
          auto idx = input.rfind('.');
          output = idx == std::string::npos ? input : input.substr(idx + 1);
          output += "ll";
        }
        break;
      case BC:
        if (input == "-") output = "cmdline.bc";
        else {
          auto idx = input.rfind('.');
          output = idx == std::string::npos ? input : input.substr(idx + 1);
          output += "bc";
        }
        break;
      case OBJ:
        if (input == "-") output = "cmdline.o";
        else {
          auto idx = input.rfind('.');
          output = idx == std::string::npos ? input : input.substr(idx + 1);
          output += "o";
        }
        break;
    }
    auto f = llvm::MemoryBuffer::getFileOrSTDIN(input, true, false);
    if (input == "-") input = "<stdin>";
    if (!f) {
      error() << "error opening " << input << ": " << f.getError().message() << '\n';
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
    auto toks = cobalt::tokenize(code, cobalt::sstring::get(input), flags);
    if (*critical) return cleanup<2>();
    cobalt::AST ast = cobalt::parse({toks.begin(), toks.end()}, flags);
    cobalt::compile_context ctx{std::string(input)};
    ast(ctx);
    std::error_code ec;
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();
    std::string err;
    auto target = llvm::TargetRegistry::lookupTarget(triple, err);
    auto tm = target->createTargetMachine(triple, "generic", "", {}, llvm::Reloc::PIC_);
    if (!target) {
      error() << "error initializing target: " << err << '\n';
      return cleanup<1>();
    }
    switch (output_type) {
      case LLVM: {
        llvm::raw_fd_ostream os({output}, ec);
        if (ec) {
          error() << "error opening " << output << ": " << ec.message() << '\n';
          return cleanup<3>();
        }
        os << *ctx.module;
        os.flush();
      } break;
      case BC: {
        llvm::raw_fd_ostream os({output}, ec);
        if (ec) {
          error() << "error opening " << output << ": " << ec.message() << '\n';
          return cleanup<3>();
        }
        llvm::WriteBitcodeToFile(*ctx.module, os);
        os.flush();
      } break;
      #ifndef _WIN32
      case UNSPEC: {
        char fname[] = "/tmp/co-tmpXXXXXX.o\0-nolibc\0-o";
        auto fd = mkstemps(fname, 2);
        llvm::raw_fd_ostream bos(fd, true);
        llvm::legacy::PassManager pm;
        if (tm->addPassesToEmitFile(pm, bos, nullptr, output_type == ASM ? llvm::CGFT_AssemblyFile : llvm::CGFT_ObjectFile)) {
          error() << "native compilation is not supported for this target\n";
          return cleanup<4>();
        }
        pm.run(*ctx.module);
        std::vector<char*> args(linked.size() + 6);
        std::string owner{output};
        owner.push_back(0);
        char pgm_name[6];
        args[0] = pgm_name;
        args[1] = fname;
        args[2] = fname + 20;
        args[3] = fname + 28;
        {
          std::vector<std::size_t> offsets(linked.size());
          std::size_t idx = 0;
          std::size_t off = output.size() + 1;
          for (auto l : linked) {
            owner += "-l";
            owner += l;
            owner.push_back(0);
            offsets[idx++] = off;
            off += l.size() + 3;
          }
          args[4] = owner.data();
          idx = 4;
          for (auto o : offsets) args[++idx] = owner.data() + o;
          args.back() = nullptr;
        }
        int pid = fork();
        if (!pid) {
          std::memset(pgm_name, 0, 6);
          std::strcpy(pgm_name, "cc");
          execvp("cc", args.data());
          std::memset(pgm_name, 0, 6);
          std::strcpy(pgm_name, "clang");
          execvp("clang", args.data());
          std::memset(pgm_name, 0, 6);
          std::strcpy(pgm_name, "gcc");
          error() << "couldn't find C compiler (cc, gcc, clang) to link\n";
          return cleanup<5>();
        }
        wait(nullptr);
        std::remove(fname);
      } break;
      #endif
      default: {
        llvm::raw_fd_ostream os({output}, ec);
        if (ec) {
          llvm::errs() << ec.message() << '\n';
          return cleanup<3>();
        }
        llvm::legacy::PassManager pm;
        if (tm->addPassesToEmitFile(pm, os, nullptr, output_type == ASM ? llvm::CGFT_AssemblyFile : llvm::CGFT_ObjectFile)) {
          error() << "native compilation is not supported for this target\n";
          return cleanup<4>();
        }
        pm.run(*ctx.module);
        os.flush();
      }
    }
    return cleanup<0>();
  }
  if (cmd == "jit") {
    std::string_view input = "";
    std::uint8_t opt_lvl = -1;
    std::vector<std::pair<std::string_view, bool>> linked;
    enum {DEFAULT, QUIET, WERROR} error_type = DEFAULT;
    std::vector<std::string_view> link_dirs = {"/usr/local/lib", "/usr/lib/", "/lib"};
    std::size_t first_idx = 0;
    for (char** it = argv + 2; !first_idx && it < argv + argc; ++it) {
      std::string_view cmd = *it;
      if (cmd.front() == '-') {
        cmd.remove_prefix(1);
        if (cmd.empty()) {
          if (!input.empty()) {
            error() << "redefinition of input file\n";
            return cleanup<1>();
          }
          input = "-";
        }
        else switch (cmd.front()) {
          case '-':
            cmd.remove_prefix(1);
            if (cmd.empty()) first_idx = it - argv;
            else if (cmd == "quiet") {
              if (error_type != DEFAULT) warn() << "redefinition or override of error mode\n";
              error_type = QUIET;
            }
            else if (cmd == "werrror") {
              if (error_type != DEFAULT) warn() << "redefinition or override of error mode\n";
              error_type = WERROR;
            }
            else {
              error() << "unkown flag --" << cmd << '\n';
              return cleanup<1>();
            }
            break;
          case 'O':
            if (cmd.size() > 2) {
              error() << "optimization level should be a single-digit number\n";
              return cleanup<1>();
            }
            if (opt_lvl != 255) warn() << "redefinition of optimization level\n";
            if (cmd[1] < '0' || cmd[1] > '9') {
              error() << "invalid value for optimization level\n";
              return cleanup<1>();
            }
            opt_lvl = cmd[1] - '0';
            break;
          default:
            for (char c : cmd) switch (c) {
              case 'O': error() << "-O flag must not be specified with any other flags\n"; return cleanup<1>();
              case 'l':
                ++it;
                if (it == argv + argc) {
                  error() << "unspecified linked library\n";
                  return cleanup<1>();
                }
                linked.push_back({*it, false});
                break;
            }
        }
      }
      else {
        if (!input.empty()) {
          error() << "redefinition of input file\n";
          return cleanup<1>();
        }
        input = cmd;
      }
    }
    if (input.empty()) {
      error() << "input file not specified\n";
      return cleanup<1>();
    }
    auto f = llvm::MemoryBuffer::getFileOrSTDIN(input, true, false);
    if (input == "-") input = "<stdin>";
    if (!f) {
      error() << "error opening " << input << ": " << f.getError().message() << '\n';
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
    auto toks = cobalt::tokenize(code, cobalt::sstring::get(input), flags);
    if (*critical) return cleanup<2>();
    cobalt::AST ast = cobalt::parse({toks.begin(), toks.end()}, flags);
    cobalt::compile_context ctx{std::string(input)};
    ast(ctx);
    std::error_code ec;
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();
    std::string err;
    auto target = llvm::TargetRegistry::lookupTarget(llvm::sys::getDefaultTargetTriple(), err);
    if (!target) {
      error() << "error initializing target: " << err << '\n';
      return cleanup<1>();
    }
    std::unique_ptr<llvm::orc::LLJIT> jit;
    if (auto err = llvm::orc::LLJITBuilder{}.create().moveInto(jit)) {
      error() << err << '\n';
      return cleanup<6>();
    }
    for (auto& [p, l] : linked) if (p.front() == ':') {
      fs::path path = p.substr(1);
      if (!fs::exists(path)) {
        error() << path.c_str() << " does not exist\n";
        return cleanup<4>();
      }
      auto ex_jdl = jit->createJITDylib(path.stem().string());
      if (ex_jdl) {
        auto& jdl = ex_jdl.get();
        std::unique_ptr<llvm::orc::DynamicLibrarySearchGenerator> dlsg;
        if (auto err = llvm::orc::DynamicLibrarySearchGenerator::Load(path.c_str(), 0).moveInto(dlsg)) {
          llvm::errs() << err << '\n';
          return cleanup<6>();
        }
        jdl.addGenerator(std::move(dlsg));
      }
      else {
        llvm::errs() << ex_jdl.takeError() << '\n';
        return cleanup<6>();
      }
      l = true;
    }
    if (auto home = std::getenv("HOME")) {
      fs::path local_lib = home;
      local_lib /= ".local/lib";
      if (fs::exists(local_lib)) for (auto const& entry : rdi(local_lib)) {
        auto const& path = entry.path();
        auto ln = path.filename().string();
        #if defined(_WIN32) // xxx.dll
        if (ln.ends_with(".dll")) if (int c = add_jdl(linked, std::string_view{ln.data(), 0, idx}, jit, path)) return c;
        #elif defined(__OSX__) || defined(__APPLE__) // xxx.dylib
        if (ln.ends_with(".dylib")) if (int c = add_jdl(linked, std::string_view{ln.data(), 0, idx}, jit, path)) return c;
        #else // libxxx[.n]
        if (ln.starts_with("lib")) {
          std::string_view libname = std::string_view{ln.data() + 3, ln.size() - 3};
          std::size_t idx = libname.rfind('.');
          if (idx == std::string::npos) continue;
          if (libname.substr(idx + 1) == "so") {
            auto eo = llvm::MemoryBuffer::getFile(path.string());
            constexpr char data[] = "\x7f\x65LF";
            if (eo && !std::memcmp(eo.get()->getBufferStart(), data, 4)) if (int c = add_jdl(linked, libname.substr(0, idx), jit, path)) return c;
          }
          if (libname.substr(idx).find_first_not_of("0123456789", idx) != std::string::npos) continue;
          std::size_t old = idx;
          do {
            old = idx;
            idx = libname.rfind('.', idx - 1);
            auto lns = libname.substr(idx + 1, old - idx - 1);
            if (lns == "so") {
              auto eo = llvm::MemoryBuffer::getFile(path.string());
              constexpr char data[] = "\x7f\x65LF";
              if (eo && !std::memcmp(eo.get()->getBufferStart(), data, 4)) if (int c = add_jdl(linked, libname.substr(0, idx), jit, path)) return c;
            }
            else if (lns.find_first_not_of("0123456789", idx + 1) != std::string::npos) goto LOOP_EXIT_1;
          } while (idx != std::string::npos);
          LOOP_EXIT_1:;
        }
        #endif
      }
    }
    for (auto dir : link_dirs) for (auto const& entry : rdi(dir)) {
      auto const& path = entry.path();
      auto ln = path.filename().string();
      #if defined(_WIN32) // xxx.dll
      if (ln.ends_with(".dll")) if (int c = add_jdl(linked, std::string_view{ln.data(), 0, idx}, jit, path)) return c;
      #elif defined(__OSX__) || defined(__APPLE__) // xxx.dylib
      if (ln.ends_with(".dylib")) if (int c = add_jdl(linked, std::string_view{ln.data(), 0, idx}, jit, path)) return c;
      #else // libxxx[.n]
      if (ln.starts_with("lib")) {
        std::string_view libname = std::string_view{ln.data() + 3, ln.size() - 3};
        std::size_t idx = libname.rfind('.');
        if (idx == std::string::npos) continue;
        if (libname.substr(idx + 1) == "so") {
          auto eo = llvm::MemoryBuffer::getFile(path.string());
          constexpr char data[] = "\x7f\x45LF";
          if (eo && !std::memcmp(eo.get()->getBufferStart(), data, 4)) if (int c = add_jdl(linked, libname.substr(0, idx), jit, path)) return c;
        }
        else if (libname.substr(idx).find_first_not_of("0123456789", idx) != std::string::npos) continue;
        std::size_t old = idx;
        do {
          old = idx;
          idx = libname.rfind('.', idx - 1);
          auto lns = libname.substr(idx + 1, old - idx - 1);
          if (lns == "so") {
            auto eo = llvm::MemoryBuffer::getFile(path.string());
            constexpr char data[] = "\x7f\x45LF";
            if (eo && !std::memcmp(eo.get()->getBufferStart(), data, 4)) if (int c = add_jdl(linked, libname.substr(0, idx), jit, path)) return c;
          }
          else if (lns.find_first_not_of("0123456789", idx + 1) != std::string::npos) goto LOOP_EXIT_2;
        } while (idx != std::string::npos);
        LOOP_EXIT_2:;
      }
      #endif
    }
    bool bad = false;
    for (auto [l, p] : linked) if (!p) {
      error() << "couldn't find library '" << l << "'\n";
      bad = true;
    }
    if (bad) return cleanup<8>();
    if (auto err = jit->addIRModule({std::move(ctx.module), std::move(ctx.context)})) {
      llvm::errs() << err << '\n';
      return cleanup<6>();
    }
    llvm::orc::ExecutorAddr addr;
    if (auto err = jit->lookup("main").moveInto(addr)) {
      llvm::errs() << err << '\n';
      return cleanup<7>();
    }
    llvm::errs().flush();
    auto main_fn = addr.toPtr<int(int, char**, char**)>();
    if (first_idx) {
      argv[first_idx] = const_cast<char*>(input.data());
      return main_fn(argc - first_idx, argv + first_idx, env);
    }
    else {
      char* args[] = {const_cast<char*>(input.data()), nullptr};
      return main_fn(1, args, env);
    }
  }
  error() << "unknown command '" << cmd << "'\n";
  return cleanup<1>();
}
