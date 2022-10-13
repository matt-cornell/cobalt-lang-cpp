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
      else if (c == '"') out += "\\\"";
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
  DEF_PP(cond, {
    auto idx1 = code.find(';');
    if (idx1 == std::string::npos) {
      onerror("invalid format for @cond: fields must be separated by semicolons", ERROR);
      return "";
    }
    std::string_view cond = code.substr(0, idx1), if_true, if_false;
    auto idx2 = code.find(';', idx1 + 1);
    if (idx2 == std::string::npos) if_true = code.substr(idx1 + 1);
    else {
      if_true = code.substr(idx1 + 1, idx2 - idx1 - 1);
      if_false = code.substr(idx2 + 1);
    }
    if (cond.empty()) return std::string(if_false);
    cond.remove_prefix(std::min(cond.find_first_not_of(" \t\n\r\f\v"), cond.size()));
    auto idx = cond.find_last_not_of(" \t\n\r\f\v");
    cond.remove_suffix(idx == std::string::npos ? 0 : idx + 1);
    return std::string{(cond.find_first_not_of('0') == std::string::npos) ? if_false : if_true};
  })
  DEF_PP(repeat, {
    auto idx = code.find(';');
    if (idx == std::string::npos) {
      onerror("invalid format for @repeat: fields must be separated by semicolons", ERROR);
      return "";
    }
    auto count_str = code.substr(0, idx);
    std::string_view content = code.substr(idx + 1);
    count_str.remove_prefix(std::min(count_str.find_first_not_of(" \t\n\r\f\v"), count_str.size()));
    idx = count_str.find_last_not_of(" \t\n\r\f\v");
    count_str.remove_suffix(idx == std::string::npos ? 0 : count_str.size() - idx - 1);
    std::size_t count = 0;
    for (char c : count_str) switch (c) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        count *= 10;
        count += c - '0';
        break;
      default:
        if (c < 32 || c > 127) {
          constexpr char chars[] = "0123456789abcdef";
          char err[] = "invalid format for @repeat: non-numeric character '\\x00' in count specifier";
          err[53] = chars[(unsigned char)c >> 4];
          err[54] = chars[c & 15];
       }
        else {
          char err[] = "invalid format for @repeat: non-numeric character '0' in count specifier";
          err[51] = c;
        }
    }
    std::string out;
    out.resize(content.size() * count);
    while (count--) std::memcpy(out.data() + content.size() * count, content.data(), content.size());
    return out;
  })
  DEF_PP(region, {(void)code; return "";})
  DEF_PP(endregion, {(void)code; return "";})
  DEF_PP(version, {(void)code; return "\"" COBALT_VERSION "\"";})
  DEF_PP(major, {(void)code; return STR(COBALT_MAJOR);})
  DEF_PP(minor, {(void)code; return STR(COBALT_MINOR);})
  DEF_PP(patch, {(void)code; return STR(COBALT_PATCH);})
  DEF_PP(echo, {return std::string(code);})
  DEF_PP(print, {llvm::outs() << code; return "";})
  DEF_PP(eprint, {llvm::errs() << code; return "";})
  DEF_PP(println, {llvm::outs() << code << '\n'; return "";})
  DEF_PP(eprintln, {llvm::errs() << code << '\n'; return "";})
  DEF_PP(command, {
    std::string command = code + ">/tmp/copp-command.out";
    std::system(command.c_str());
    auto eo = llvm::MemoryBuffer::getFile("/tmp/copp-command.out", false, false);
    if (eo) return std::string(eo.get()->getBuffer());
    else {
      auto msg = eo.getError().message();
      onerror(msg, ERROR);
      return "";
    }
  })
};
