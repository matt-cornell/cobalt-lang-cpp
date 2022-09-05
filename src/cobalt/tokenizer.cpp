#include "cobalt/tokenizer.hpp"
#include <cmath>
#include <llvm/Support/MemoryBuffer.h>
#if __cplusplus >= 202002
#include <bit>
#define countl1(...) std::countl_one(__VA_ARGS__)
#define countl0(...) std::countl_zero(__VA_ARGS__)
#else
static unsigned char countl0(unsigned char c) {
  unsigned char out = 8;
  while (c && out) c >>= 1;
  return out;
}
static unsigned char countl1(unsigned char c) {return countl0(~c);}
#endif
using namespace std::literals;
using namespace cobalt;
template <class I> bool advance(I& it, I& end, char32_t& c) noexcept(noexcept(*it++)) {
  if (it == end) return false;
  switch (countl1((unsigned char)*it)) {
    case 0: c = char32_t(*it++); return true; break;
    case 2: {
      c = (*it++ & 0x3F) << 6;
      if (it == end) return false;
      if ((*it & 0xC0) ^ 0x80) return false;
      c |= (*it++ & 0x3F);
      if (it == end) return false;
      return true;
    } break;
    case 3: {
      c = (*it++ & 0x1F) << 12;
      if (it == end) return false;
      if ((*it & 0xC0) ^ 0x80) return false;
      c |= (*it++ & 0x3F) << 6;
      if (it == end) return false;
      if ((*it & 0xC0) ^ 0x80) return false;
      c |= (*it++ & 0x3F);
      return true;
    } break;
    case 4: {
      c = (*it++ & 0x0F) << 18;
      if (it == end) return false;
      if ((*it & 0xC0) ^ 0x80) return false;
      c |= (*it++ & 0x3F) << 12;
      if (it == end) return false;
      if ((*it & 0xC0) ^ 0x80) return false;
      c |= (*it++ & 0x3F) << 6;
      if (it == end) return false;
      if ((*it & 0xC0) ^ 0x80) return false;
      c |= (*it++ & 0x3F);
      return true;
    } break;
    default: return false;
  }
}
void append(std::string& str, char32_t val) {
  if (val < 0x80) str.append({char(val & 0xFF)});
  else if (val < 0x800) str.append({char(val >> 6 | 0xC0), char((val & 0x3F) | 0x80)});
  else if (val < 0x10000) str.append({char(val >> 12 | 0xE0), char(((val >> 6) & 0x3F) | 0x80), char((val & 0x3F) | 0x80)});
  else if (val < 0x110000) str.append({char(val >> 18 | 0xF0), char(((val >> 12) & 0x3F) | 0x80), char(((val >> 6) & 0x3F) | 0x80), char((val & 0x3F) | 0x80)});
  else std::exit(-1); // This is unreachable because an invalid character would've been caught in the advance function
}
std::string to_string(char32_t val) {
  if (val < 0x80) return {char(val & 0xFF)};
  else if (val < 0x800) return {char(val >> 6 | 0xC0), char((val & 0x3F) | 0x80)};
  else if (val < 0x10000) return {char(val >> 12 | 0xE0), char(((val >> 6) & 0x3F) | 0x80), char((val & 0x3F) | 0x80)};
  else if (val < 0x110000) return {char(val >> 18 | 0xF0), char(((val >> 12) & 0x3F) | 0x80), char(((val >> 6) & 0x3F) | 0x80), char((val & 0x3F) | 0x80)};
  else return "";
}
std::string as_hex(char32_t c) {
  constexpr char chars[] = "0123456789ABCDEF";
  unsigned char size = 32 - countl0((uint32_t)c);
  size = size > 16 ? (size + 3) / 4 : 4;
  std::string outs(size, '0');
  do {
    outs[size] = c & 0x0F;
    c >>= 4;
    --size;
  } while (size);
  return outs;
}
template <class T> std::string as_dec(T c) {
  if (!c) return 0;
  constexpr char chars[] = "0123456789";
  unsigned char size = std::ceil(std::log10((int32_t)c));
  std::string outs(size, '0');
  while (size) {
    outs[size] = c % 10;
    c /= 10;
    --size;
  }
  return outs;
}
unsigned char c2x(char32_t c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 255;
}
bool is_nl(char32_t c) {
  switch (c) {
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x85:
    case 0x2028:
    case 0x2029:
      return true;
      break;
    default:
      return false;
  }
}
#pragma region macros
#define ADV \
  if (!advance(it, end, c)) { \
    if (it == end) {flags.onerror(loc, "unterminated character literal", ERROR); return out;} \
    else flags.onerror(loc, "invalid UTF-8 character", CRITICAL); \
    return out; \
  }
#define STEP \
  if (is_nl(c)) { \
    ++loc.line; \
    loc.col = 1; \
    constexpr char chars[] = "0123456789ABCDEF"; \
    if (c < 128) flags.onerror(loc, "character literal cannot contain newline, use '\\x"s + chars[(c >> 4) & 0x0F] + chars[c & 0x0F] + '\'', ERROR); \
    else if (c < 65536) flags.onerror(loc, "character literal cannot contain newline, use '\\u"s + chars[(c >> 12) & 0x0F] + chars[(c >> 8) & 0x0F] + chars[(c >> 4) & 0x0F] + chars[c & 0x0F] + '\'', ERROR); \
    else flags.onerror(loc, "character literal cannot contain newline, use '\\U"s + chars[(c >> 28) & 0x0F] + chars[(c >> 24) & 0x0F] + chars[(c >> 20) & 0x0F] + chars[(c >> 16) & 0x0F] + chars[(c >> 12) & 0x0F] + chars[(c >> 8) & 0x0F] + chars[(c >> 4) & 0x0F] + chars[c & 0x0F] + '\'', ERROR); \
  } \
  else ++loc.col;
#pragma endregion
#define DEF_PP(NAME, ...) {sstring::get(#NAME), [](std::string_view code, bound_handler onerror)->std::string __VA_ARGS__},
pp_map cobalt::default_directives = {
  DEF_PP(file, {
    auto f = llvm::MemoryBuffer::getFile(code, true, false);
    if (!f) {
      auto str = f.getError().message();
      onerror(str, ERROR);
      return "\"error: " + str + '"';
    }
    return '"' + std::string(f.get()->getBuffer().data(), f.get()->getBufferSize()) + '"';
  })
  DEF_PP(include, {
    auto f = llvm::MemoryBuffer::getFile(code, true, false);
    if (!f) {
      onerror(f.getError().message(), ERROR);
      return "";
    }
    return std::string(f.get()->getBuffer().data(), f.get()->getBufferSize());
  })
};
std::vector<token> cobalt::tokenize(std::string_view code, location loc, flags_t flags, pp_map const& directives) {
  auto it = code.begin(), end = code.end();
  std::vector<token> out;
  char32_t c;
  bool in_string = false, lwbs = false, topb = true;
  auto step = [&loc, u = flags.update_location] (char32_t c) {
    if (!u) return c;
    if (is_nl(c)) {
      ++loc.line;
      loc.col = 1;
    }
    else ++loc.col;
    return c;
  };
  while (advance(it, end, c)) {
    if (in_string) {
      switch (c) {
        case '\\':
          if (lwbs) {
            if (topb) {out.push_back({loc, "\"\\"}); topb = false;}
            else out.back().data.push_back('\\');
            lwbs = false;
          }
          else lwbs = true;
          break;
        case '"':
          if (lwbs) {
            if (topb) {out.push_back({loc, "\"\""}); topb = false;}
            else out.back().data.push_back('"');
            lwbs = false;
          }
          else {
            topb = true;
            in_string = false;
          }
          break;
        case 'n':
          if (lwbs) {
            if (topb) {out.push_back({loc, "\"\n"}); topb = false;}
            else out.back().data.push_back('\n');
            lwbs = false;
          }
          else {
            if (topb) {out.push_back({loc, "\"n"}); topb = false;}
            else out.back().data.push_back('n');
          }
          break;
        case 'r':
          if (lwbs) {
            if (topb) {out.push_back({loc, "\"\r"}); topb = false;}
            else out.back().data.push_back('\r');
            lwbs = false;
          }
          else {
            if (topb) {out.push_back({loc, "\"r"}); topb = false;}
            else out.back().data.push_back('r');
          }
          break;
        case '0':
          if (lwbs) {
            if (topb) {out.push_back({loc, {'\'', '\0'}}); topb = false;}
            else out.back().data.push_back('\0');
            lwbs = false;
          }
          else {
            if (topb) {out.push_back({loc, "\"0"}); topb = false;}
            else out.back().data.push_back('0');
          }
          break;
        case 't':
          if (lwbs) {
            if (topb) {out.push_back({loc, "\"\t"}); topb = false;}
            else out.back().data.push_back('\t');
            lwbs = false;
          }
          else {
            if (topb) {out.push_back({loc, "\"t"}); topb = false;}
            else out.back().data.push_back('t');
          }
          break;
        case 'v':
          if (lwbs) {
            if (topb) {out.push_back({loc, "\"\v"}); topb = false;}
            else out.back().data.push_back('\v');
            lwbs = false;
          }
          else {
            if (topb) {out.push_back({loc, "\"v"}); topb = false;}
            else out.back().data.push_back('v');
          }
          break;
        case 'f':
          if (lwbs) {
            if (topb) {out.push_back({loc, "\"\f"}); topb = false;}
            else out.back().data.push_back('\f');
            lwbs = false;
          }
          else {
            if (topb) {out.push_back({loc, "\"f"}); topb = false;}
            else out.back().data.push_back('f');
          }
          break;
        case 'x':
          if (lwbs) {
            ADV
            step(c);
            unsigned char c2 = c2x(c);
            if (c2 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            ADV
            step(c);
            unsigned char c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 |= c3 << 4;
            if (topb) {out.push_back({loc, {'"', (char)c2}}); topb = false;}
            else out.back().data.push_back((char)c2);
            lwbs = false;
          }
          else {
            if (topb) {out.push_back({loc, "\"x"}); topb = false;}
            else out.back().data.push_back('x');
          }
          break;
        case 'u':
          if (lwbs) {
            ADV
            step(c);
            unsigned short c2 = c2x(c);
            if (c2 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            ADV
            step(c);
            unsigned char c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 |= c3 << 4;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 |= c3 << 8;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 |= c3 << 12;
            if (topb) {out.push_back({loc, "\""}); topb = false;}
            append(out.back().data, (char32_t)int32_t(c2));
            lwbs = false;
          }
          else {
            if (topb) {out.push_back({loc, "\"x"}); topb = false;}
            else out.back().data.push_back('x');
          }
          break;
        case 'U':
          if (lwbs) {
            ADV
            step(c);
            unsigned int c2 = c2x(c);
            if (c2 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            ADV
            step(c);
            unsigned char c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 |= c3 << 4;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 |= c3 << 8;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 |= c3 << 12;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 |= c3 << 16;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 |= c3 << 20;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 |= c3 << 24;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 |= c3 << 28;
            if (topb) {out.push_back({loc, "\""}); topb = false;}
            append(out.back().data, (char32_t)c2);
            lwbs = false;
          }
          else {
            if (topb) {out.push_back({loc, "\"x"}); topb = false;}
            else out.back().data.push_back('x');
          }
          break;
        default:
          lwbs = false;
          if (topb) {out.push_back({loc, "\""}); topb = false;}
          append(out.back().data, c);
      }
    }
    else {
      switch (c) {
        case '@': {
          topb = true;
          auto s = it, e = it;
          unsigned char ec = 0;
          while (!ec && advance(e, end, c)) {
            switch (c) {
              case 0x85:
              case 0xA0:
              case 0x1680:
              case 0x2000:
              case 0x2001:
              case 0x2002:
              case 0x2003:
              case 0x2004:
              case 0x2005:
              case 0x2006:
              case 0x2007:
              case 0x2008:
              case 0x2009:
              case 0x200A:
              case 0x2028:
              case 0x2029:
              case 0x202F:
              case 0x205F:
              case 0x3000:
                if (flags.warn_whitespace) flags.onerror(loc, "unusual whitespace character U+" + as_hex(c), WARNING);
              case 0x09:
              case 0x0A:
              case 0x0B:
              case 0x0C:
              case 0x0D:
              case 0x20:
                ec = 1;
                break;
              case '(':
                ec = 2;
                break;
            }
            switch (ec) {
              case 0:
                if (e != end) {
                  flags.onerror(loc, "invalid UTF-8 character", CRITICAL);
                  return out;
                }
              case 1: {
                auto macro = sstring::get(std::string_view{s, e});
                auto it = directives.find(macro);
                if (it == directives.end()) out.push_back({loc, "@"s + macro});
                auto d = it->second;
                flags_t f = flags;
                f.update_location = false;
                auto toks = tokenize(d("", {loc, flags.onerror}), loc, f, directives);
                out.insert(out.end(), toks.begin(), toks.end());
              } break;
              case 2: {
                auto e2 = e;
                bool graceful = false;
                //  (?<=.*?|"([^"]|(?<!\\)(\\\\)*\\"))"
                while (!graceful && advance(e2, end, c)) {
                  switch (c) {
                    case '\'':
                      switch (c) {
                        case '\'':
                          flags.onerror(loc, "empty character literal", WARNING);
                          break;
                        case '\\':
                          
                          break;
                        default:
                          if (!advance(e2, end, c)) {
                            if (e2 == end) {flags.onerror(loc, "unterminated character literal", ERROR); return out;}
                            else flags.onerror(loc, "invalid UTF-8 character", CRITICAL);
                          }
                          if (flags.update_location) {STEP}
                          if (!advance(e2, end, c)) {
                            if (e2 == end) {flags.onerror(loc, "unterminated character literal", ERROR); return out;}
                            else flags.onerror(loc, "invalid UTF-8 character", CRITICAL);
                          }
                          if (c != '\'') {
                            flags.onerror(loc, "character literal is too long", ERROR);
                            do {
                              if (!advance(e2, end, c)) {
                                if (e2 == end) {flags.onerror(loc, "unterminated character literal", ERROR); return out;}
                                else flags.onerror(loc, "invalid UTF-8 character", CRITICAL);
                              }
                              step(c);
                            } while (c != '\'');
                          }
                      }
                      break;
                    case '"': {
                      uint8_t state = 1; // 0 = exit, 1 = normal, 2 = lwbs
                      while (state && advance(e2, end, c)) switch (c) {
                        case '\\':
                          if (state == 2) state = 1;
                          else state = 2;
                          break;
                        case '"':
                          if (state == 2) state = 1;
                          else state = 0;
                          break;
                        default:
                          state = 1;
                      }
                      if (state) {
                        if (e2 == end) {flags.onerror(loc, "unterminated string literal", ERROR); return out;}
                        else flags.onerror(loc, "invalid UTF-8 character", ERROR);
                      }
                    } break;
                    case ')':
                      graceful = true;
                      break;
                  }
                }
                if (!graceful) {
                  if (e2 == end) {flags.onerror(loc, "unterminated macro", ERROR); return out;}
                  else flags.onerror(loc, "invalid UTF-8 character", CRITICAL);
                }
                auto macro = sstring::get(std::string_view{s, e});
                auto it = directives.find(macro);
                if (it == directives.end()) out.push_back({loc, "@"s + macro});
                auto d = it->second;
                flags_t f = flags;
                f.update_location = false;
                auto toks = tokenize(d({e, e2}, {loc, flags.onerror}), loc, f, directives);
                out.insert(out.end(), toks.begin(), toks.end());
              }
            }
          }
        } break;
        case '\'':
          ADV
          if (flags.update_location) {STEP}
          switch (c) {
            case '\'':
              flags.onerror(loc, "empty character literal", WARNING);
              out.push_back({loc, "0c"});
              continue;
            case '\\':
              ADV
              if (flags.update_location) {STEP}
              switch (c) {
                case '0': out.push_back({loc, "0c"}); break;
                case 'n': out.push_back({loc, "10c"}); break;
                case 'a': out.push_back({loc, "7c"}); break;
                case 'b': out.push_back({loc, "8c"}); break;
                case 'r': out.push_back({loc, "13c"}); break;
                case 't': out.push_back({loc, "9c"}); break;
                case 'v': out.push_back({loc, "12c"}); break;
                case '\\': out.push_back({loc, "92c"}); break;
                case '\'': out.push_back({loc, "44c"}); break;
                case 'x': {
                  ADV
                  step(c);
                  unsigned char c2 = c2x(c);
                  if (c2 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  ADV
                  step(c);
                  unsigned char c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 |= c3 << 4;
                  out.push_back({loc, as_dec(c2) + 'c'});
                } break;
                case 'u': {
                  ADV
                  step(c);
                  unsigned short c2 = c2x(c);
                  if (c2 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  ADV
                  step(c);
                  unsigned char c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 |= c3 << 4;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 |= c3 << 8;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 |= c3 << 12;
                  out.push_back({loc, as_dec(c2) + 'c'});
                } break;
                case 'U': {
                  ADV
                  step(c);
                  unsigned short c2 = c2x(c);
                  if (c2 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  ADV
                  step(c);
                  unsigned char c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 |= c3 << 4;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 |= c3 << 8;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 |= c3 << 12;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 |= c3 << 16;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 |= c3 << 20;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 |= c3 << 24;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 |= c3 << 28;
                  out.push_back({loc, as_dec(c2) + 'c'});
                } break;
              }
              break;
            default:
              out.push_back({loc, as_dec(c) + "c"});
          }
          ADV
          step(c);
          if (c != '\'') {
            flags.onerror(loc, "too many characters in character literal", ERROR);
            do {
              ADV
              step(c);
            } while (c != '\'');
          }
          break;
        case '"':
          in_string = true;
          break;
#pragma region whitespace_characters
        case 0x85:
        case 0xA0:
        case 0x1680:
        case 0x2000:
        case 0x2001:
        case 0x2002:
        case 0x2003:
        case 0x2004:
        case 0x2005:
        case 0x2006:
        case 0x2007:
        case 0x2008:
        case 0x2009:
        case 0x200A:
        case 0x2028:
        case 0x2029:
        case 0x202F:
        case 0x205F:
        case 0x3000:
          if (flags.warn_whitespace) flags.onerror(loc, "unusual whitespace character U+" + as_hex(c), WARNING);
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x20:
          topb = true;
          break;
#pragma endregion
#pragma region singular_characters
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
          out.push_back({loc, {char(c)}});
          topb = true;
          break;
#pragma endregion
        default:
          if (topb) {out.push_back({loc, ""}); topb = false;}
          append(out.back().data, c);
      }
    }
    step(c);
  }
  if (it != end) flags.onerror(loc, "invalid UTF-8 character", CRITICAL);
  if (in_string) flags.onerror(loc, "unterminated string literal", ERROR);
  return out;
}