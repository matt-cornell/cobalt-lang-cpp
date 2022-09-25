#include "cobalt/tokenizer.hpp"
#include "cobalt/version.hpp"
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/FileSystem.h>
using namespace cobalt;
#define DEF_PP(NAME, ...) {sstring::get(#NAME), [](std::string_view code, bound_handler onerror)->std::string __VA_ARGS__},
macro_map cobalt::default_macros {
  DEF_PP(file, {
    if (code.empty()) return "";
    if (code.front() != '/' && llvm::sys::fs::exists(onerror.loc.file)) {
      auto idx = onerror.loc.file.find_last_of('/');
      auto fname = idx == std::string::npos ? std::string_view{onerror.loc.file} : onerror.loc.file.substr(0, idx);
      auto eo = llvm::MemoryBuffer::getFile(fname, false, false);
      if (eo) {
        auto str = eo.get()->getBuffer();
        std::string out = "\"";
        out.reserve(str.size() + 2);
        for (char c : str) {
          if (c == '"') out += "\\\"";
          else if (c >= 32 && c <= 127) out.push_back(c);
          else {
            char buff[] = "\\x00";
            buff[2] = (unsigned char)c >> 4;
            buff[3] = c & 15;
            out += buff;
          }
        }
        out += "\"";
        return out;
      }
      else {
        auto msg = eo.getError().message();
        onerror(msg, ERROR);
        return msg;
      }
    }
    else {
      constexpr char chars[] = "0123456789abcdef";
      auto eo = llvm::MemoryBuffer::getFile(code, false, false);
      if (eo) {
        auto str = eo.get()->getBuffer();
        std::string out = "\"";
        out.reserve(str.size() + 2);
        for (char c : str) {
          if (c == '"') out += "\\\"";
          else if (c >= 32 && c <= 127) out.push_back(c);
          else {
            char buff[] = "\\x00";
            buff[2] = chars[(unsigned char)c >> 4];
            buff[3] = chars[c & 15];
            out += buff;
          }
        }
        out += "\"";
        return out;
      }
      else {
        auto msg = eo.getError().message();
        onerror(msg, ERROR);
        return msg;
      }
    }
  })
  DEF_PP(import, {
    if (code.empty()) return "";
    if (code.front() != '/' && llvm::sys::fs::exists(onerror.loc.file)) {
      auto idx = onerror.loc.file.find_last_of('/');
      auto fname = idx == std::string::npos ? std::string_view{onerror.loc.file} : onerror.loc.file.substr(0, idx);
      auto eo = llvm::MemoryBuffer::getFile(fname, false, false);
      if (eo) return std::string(eo.get()->getBuffer());
      else {
        auto msg = eo.getError().message();
        onerror(msg, ERROR);
        return "";
      }
    }
    else {
      auto eo = llvm::MemoryBuffer::getFile(code, false, false);
      if (eo) return std::string(eo.get()->getBuffer());
      else {
        auto msg = eo.getError().message();
        onerror(msg, ERROR);
        return "";
      }
    }
  })
  DEF_PP(str, {
    constexpr char chars[] = "0123456789abcdef";
    std::string out = "\"";
    out.reserve(code.size() + 2);
    for (char c : code) {
      if (c >= 32 && c <= 127) out.push_back(c);
      else {
        char buff[] = "\\x00";
        buff[2] = chars[(unsigned char)c >> 4];
        buff[3] = chars[c & 15];
        out += buff;
      }
    }
    out += "\"";
    return out;
  })
  DEF_PP(lex_lt, {
    auto idx = code.find(';');
    if (idx == std::string::npos) return "0";
    return (code.substr(0, idx) < code.substr(idx + 1)) ? "1" : "0";
  })
  DEF_PP(lex_gt, {
    auto idx = code.find(';');
    if (idx == std::string::npos) return "0";
    return (code.substr(0, idx) > code.substr(idx + 1)) ? "1" : "0";
  })
  DEF_PP(lex_le, {
    auto idx = code.find(';');
    if (idx == std::string::npos) return "1";
    return (code.substr(0, idx) <= code.substr(idx + 1)) ? "1" : "0";
  })
  DEF_PP(lex_ge, {
    auto idx = code.find(';');
    if (idx == std::string::npos) return "1";
    return (code.substr(0, idx) >= code.substr(idx + 1)) ? "1" : "0";
  })
  DEF_PP(lex_eq, {
    auto idx = code.find(';');
    if (idx == std::string::npos) return "1";
    return (code.substr(0, idx) == code.substr(idx + 1)) ? "1" : "0";
  })
  DEF_PP(lex_ne, {
    auto idx = code.find(';');
    if (idx == std::string::npos) return "0";
    return (code.substr(0, idx) != code.substr(idx + 1)) ? "1" : "0";
  })
  DEF_PP(region, {(void)code; return "";})
  DEF_PP(endregion, {(void)code; return "";})
  DEF_PP(version, {(void)code; return "\"" COBALT_VERSION "\"";})
  DEF_PP(print, {llvm::outs() << code; return "";})
  DEF_PP(eprint, {llvm::errs() << code; return "";})
  DEF_PP(println, {llvm::outs() << code << '\n'; return "";})
  DEF_PP(eprintln, {llvm::errs() << code << '\n'; return "";})
};
