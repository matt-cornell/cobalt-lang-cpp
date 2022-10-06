#include "cobalt/tokenizer.hpp"
#include <cmath>
#include <numbers>
#include <optional>
#include <llvm/ADT/APInt.h>
#if __cplusplus >= 202002
#include <bit>
#define countl1(...) std::countl_one(__VA_ARGS__)
#define countl0(...) std::countl_zero(__VA_ARGS__)
#define bitcast(T, ...) std::bit_cast<T>(__VA_ARGS__)
#else
static unsigned char countl0(unsigned char c) {
  unsigned char out = 8;
  while (c && out) c >>= 1;
  return out;
}
static unsigned char countl1(unsigned char c) {return countl0(~c);}
template <class T, class U> static T bit_cast(U const& val) {return reinterpret_cast<T const&>(val);}
#define bitcast(T, ...) bit_cast<T>(__VA_ARGS__)
#endif
using namespace std::literals;
using namespace cobalt;
#pragma region misc-functions
template <class I> static bool advance(I& it, I& end, char32_t& c) noexcept(noexcept(*it++)) {
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
static void append(std::string& str, char32_t val) {
  if (val < 0x80) str.append({char(val & 0xFF)});
  else if (val < 0x800) str.append({char(val >> 6 | 0xC0), char((val & 0x3F) | 0x80)});
  else if (val < 0x10000) str.append({char(val >> 12 | 0xE0), char(((val >> 6) & 0x3F) | 0x80), char((val & 0x3F) | 0x80)});
  else if (val < 0x110000) str.append({char(val >> 18 | 0xF0), char(((val >> 12) & 0x3F) | 0x80), char(((val >> 6) & 0x3F) | 0x80), char((val & 0x3F) | 0x80)});
  else std::exit(-1); // This is unreachable because an invalid character would've been caught in the advance function
}
static std::string to_string(char32_t val) {
  if (val < 0x80) return {char(val & 0xFF)};
  else if (val < 0x800) return {char(val >> 6 | 0xC0), char((val & 0x3F) | 0x80)};
  else if (val < 0x10000) return {char(val >> 12 | 0xE0), char(((val >> 6) & 0x3F) | 0x80), char((val & 0x3F) | 0x80)};
  else if (val < 0x110000) return {char(val >> 18 | 0xF0), char(((val >> 12) & 0x3F) | 0x80), char(((val >> 6) & 0x3F) | 0x80), char((val & 0x3F) | 0x80)};
  else return "";
}
static std::string as_hex(char32_t c) {
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
template <class T> static std::string as_dec(T c) {
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
static unsigned char c2x(char32_t c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 255;
}
static bool is_nl(char32_t c) {
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
static bool is_hex(char32_t c) {
  if (c >= '0' && c <= '9') return true;
  if (c >= 'a' && c <= 'f') return true;
  if (c >= 'A' && c <= 'F') return true;
  return false;
}
template <class I> static std::string parse_num(I& it, I end, bound_handler const& onerror, borrow_function<char32_t(char32_t)> step) {
  uint32_t decimal_places = 0;
  constexpr double log2_10 = std::numbers::ln10_v<double> / std::numbers::ln2_v<double>;
  llvm::APInt int_part;
  double float_part = 0;
  double bits = 0;
  if (*it == '0') switch (*++it) {
    case 'x':
    case 'X':
    #pragma region hex_parsing
      ++it;
      while (it != end) {
        char c = *it;
        switch (c) {
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
            if (decimal_places) float_part += (c - '0') * std::pow(0.0625, decimal_places++);
            else {
              bits += 4;
              int_part = int_part.zext((unsigned)std::ceil(bits));
              int_part *= 16;
              int_part += c - '0';
            }
            break;
          case 'a':
          case 'b':
          case 'c':
          case 'd':
          case 'e':
          case 'f':
            if (decimal_places) float_part += (c - 'a' + 10) * std::pow(0.0625, decimal_places++);
            else {
              bits += 4;
              int_part = int_part.zext((unsigned)std::ceil(bits));
              int_part *= 16;
              int_part += c - 'a' + 10;
            }
            break;
          case 'A':
          case 'B':
          case 'C':
          case 'D':
          case 'E':
          case 'F':
            if (decimal_places) float_part += (c - 'A' + 10) * std::pow(0.0625, decimal_places++);
            else {
              bits += 4;
              int_part = int_part.zext((unsigned)std::ceil(bits));
              int_part *= 16;
              int_part += c - 'A' + 10;
            }
            break;
          case '.': {
            if (it + 1 >= end) goto end;
            char c2 = *(it + 1);
            if (c2 & 0x80 || ((c2 < '0' || c2 > '9') && (c2 < 'a' || c2 > 'f') && (c2 < 'A' || c2 > 'F'))) {
              decimal_places = 1;
              step(*++it);
              goto end;
            }
            if (decimal_places) {
              step(*++it);
              onerror("identifier cannot start with a number", ERROR);
              goto end;
            }
            decimal_places = 1;
          } break;
          default:
            --it;
            goto end;
        }
        ++it;
        step(c);
      }
      break;
    #pragma endregion
    case 'b':
    case 'B':
    #pragma region bin_parsing
      ++it;
      while (it != end) {
        char c = *it;
        switch (c) {
          case '0':
          case '1':
            if (decimal_places) float_part += (c - '0') * std::pow(0.5, decimal_places++);
            else {
              bits += 1;
              int_part = int_part.zext((unsigned)std::ceil(bits));
              int_part *= 2;
              int_part += c - '0';
            }
            break;
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
            onerror("invalid decimal digit in binary literal", ERROR);
            break;
          case '.': {
            if (it + 1 >= end) goto end;
            char c2 = *(it + 1);
            if (c2 & 0x80 || c2 < '0' || c2 > '1') {
              decimal_places = 1;
              step(*++it);
              goto end;
            }
            if (decimal_places) {
              step(*++it);
              onerror("identifier cannot start with a number", ERROR);
              goto end;
            }
            decimal_places = 1;
          } break;
          default:
            --it;
            goto end;
        }
        ++it;
        step(c);
      }
      break;
    #pragma endregion
    case '.':
    #pragma region float_parsing
      decimal_places = 1;
      ++it;
      while (it != end) {
        char c = *it;
        switch (c) {
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
            if (decimal_places) float_part += (c - '0') * std::pow(0.1, decimal_places++);
            else {
              bits += log2_10;
              int_part = int_part.zext((unsigned)std::ceil(bits));
              int_part *= 10;
              int_part += c - '0';
            }
            break;
          case '.': {
            if (it + 1 >= end) goto end;
            step(*++it);
            onerror("identifier cannot start with a number", ERROR);
            goto end;
          } break;
          default:
            --it;
            goto end;
        }
        ++it;
        step(c);
      }
      break;
    #pragma endregion
    default:
    #pragma region oct_parsing
      while (it != end) {
        char c = *it;
        switch (c) {
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
            if (decimal_places) float_part += (c - '0') * std::pow(0.125, decimal_places++);
            else {
              bits += 3;
              int_part = int_part.zext((unsigned)std::ceil(bits));
              int_part *= 8;
              int_part += c - '0';
            }
            break;
          case '8':
          case '9':
            onerror("invalid decimal character in octal literal", ERROR);
            break;
          case '.': {
            if (it + 1 >= end) goto end;
            char c2 = *(it + 1);
            if (c2 & 0x80 || c2 < '0' || c2 > '9') {
              decimal_places = 1;
              step(*++it);
              goto end;
            }
            if (decimal_places) {
              step(*++it);
              onerror("identifier cannot start with a number", ERROR);
              goto end;
            }
            decimal_places = 1;
          } break;
          default:
            --it;
            goto end;
        }
        ++it;
        step(c);
      }
    #pragma endregion
  }
  #pragma region dec_parsing
  else while (it != end) {
    char c = *it;
    switch (c) {
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
        if (decimal_places) float_part += (c - '0') * std::pow(0.1, decimal_places++);
        else {
          bits += log2_10;
          int_part = int_part.zext((unsigned)std::ceil(bits));
          int_part *= 10;
          int_part += c - '0';
        }
        break;
      case '.': {
        if (it + 1 >= end) goto end;
        char c2 = *(it + 1);
        if (c2 & 0x80 || c2 < '0' || c2 > '9') {
          decimal_places = 1;
          step(*++it);
          goto end;
        }
        if (decimal_places) {
          step(*++it);
          onerror("identifier cannot start with a number", ERROR);
          goto end;
        }
        decimal_places = 1;
      } break;
      default:
        --it;
        goto end;
    }
    ++it;
    step(c);
  }
  #pragma endregion
  end:
  ++it;
  std::string out;
  if (decimal_places) {
    float_part += int_part.trunc(llvm::APInt::APINT_WORD_SIZE).getZExtValue();
    out.resize(sizeof(double) + 2);
    out[0] = '1';
    std::memcpy(out.data() + 2, &float_part, sizeof(double));
  }
  else {
    out.resize(int_part.getNumWords() * llvm::APInt::APINT_WORD_SIZE + 2);
    out[0] = '0';
    std::memcpy(out.data() + 1, int_part.getRawData(), int_part.getNumWords() * llvm::APInt::APINT_WORD_SIZE);
  }
  return out;
}
template <class I> std::optional<std::string> parse_macro(I& it, I end, macro_map& macros, flags_t& flags, location& loc, bool recursing = false) {
  char32_t c;
  auto start = it;
  auto start_l = loc;
  enum {BAD, WS, PAREN} estate = BAD;
  auto step = [&loc, u = flags.update_location] (char32_t c) {
    if (!u) return c;
    if (is_nl(c)) {
      ++loc.line;
      loc.col = 1;
    }
    else ++loc.col;
    return c;
  };
  while (!estate && advance(it, end, c)) {
    switch (c) {
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
#pragma endregion
#pragma region other-breaks
      case ')':
      case '+':
      case '-':
      case '*':
      case '/':
      case '%':
      case '^':
      case '&':
      case '<':
      case '>':
      case ',':
      case ';':
      case '\'':
      case '"':
      case '[':
      case ']':
      case '{':
      case '}':
      case '~':
      case '|':
      case '!':
      case '\\':
#pragma endregion
        estate = WS;
        break;
      case '(':
        estate = PAREN;
        break;
    }
    step(c);
  }
  if (estate == BAD) {
    if (it == end) estate = WS;
    else flags.onerror(loc, "invalid UTF-8 character", CRITICAL);
  }
  std::string_view macro_id;
  std::string args;
  if (estate == PAREN) {
    macro_id = std::string_view{start, it - 1};
    start = it;
    std::size_t depth = 1;
    while (depth && advance(it, end, c)) {
      switch (c) {
        case '"': {
          bool cont;
          while (cont) switch (step(*it++)) {
            case '"': cont = false; break;
            case '\\': switch (step(*it++)) {
              case 'x': for (uint8_t count = 1; count;) if (step(*it++) & 128) --count; break;
              case 'u': for (uint8_t count = 3; count;) if (step(*it++) & 128) --count; break;
              case 'U': for (uint8_t count = 7; count;) if (step(*it++) & 128) --count; break;
            } break;
            default:
              while (*it & 128) step(*++it);
          } break;
        } break;
        case '\'':
          switch (step(*it++)) {
            case '\\':
              switch (step(*it++)) {
                case 'x':
                  for (uint8_t count = 1; count;) if (step(*it++) & 128) --count;
                  break;
                case 'u':
                  for (uint8_t count = 3; count;) if (step(*it++) & 128) --count;
                  break;
                case 'U':
                  for (uint8_t count = 7; count;) if (step(*it++) & 128) --count;
                  break;
              }
            default:
              while (step(*it++) != '\'');
          }
          break;
        case '@': {
          args.append(start, it - 1);
          auto res = parse_macro(it, end, macros, flags, loc, true);
          if (res) args += *res;
          else return std::nullopt;
          start = it;
        } break;
        case '(': ++depth; break;
        case ')': --depth; break;
      }
      step(c);
    }
    args.append(start, it - 1);
  }
  else macro_id = std::string_view{start, --it};
  if (macro_id == "define") { // @define needs to be specially defined because it adds a macro
    flags.onerror(loc, "macro definition is not yet supported", CRITICAL);
    return std::nullopt;
  }
  else {
    auto it = macros.find(sstring::get(macro_id));
    if (it == macros.end()) return "@" + std::string(macro_id) + "(" + args + ")";
    else return it->second(args, {loc, flags.onerror});
  }
}
#pragma endregion
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
std::vector<token> cobalt::tokenize(std::string_view code, location loc, flags_t flags, macro_map macros) {
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
            c2 <<= 4;
            c2 |= c3;
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
            uint16_t c2 = c2x(c);
            if (c2 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            ADV
            step(c);
            unsigned char c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 <<= 4;
            c2 |= c3;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 <<= 4;
            c2 |= c3;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 <<= 4;
            c2 |= c3;
            if (topb) {out.push_back({loc, "\""}); topb = false;}
            append(out.back().data, (char32_t)int32_t(c2));
            lwbs = false;
          }
          else {
            if (topb) {out.push_back({loc, "\"u"}); topb = false;}
            else out.back().data.push_back('u');
          }
          break;
        case 'U':
          if (lwbs) {
            ADV
            step(c);
            uint32_t c2 = c2x(c);
            if (c2 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            ADV
            step(c);
            unsigned char c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 <<= 4;
            c2 |= c3;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 <<= 4;
            c2 |= c3;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 <<= 4;
            c2 |= c3;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 <<= 4;
            c2 |= c3;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 <<= 4;
            c2 |= c3;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 <<= 4;
            c2 |= c3;
            ADV
            step(c);
            c3 = c2x(c);
            if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
            c2 <<= 4;
            c2 |= c3;
            if (topb) {out.push_back({loc, "\""}); topb = false;}
            append(out.back().data, (char32_t)c2);
            lwbs = false;
          }
          else {
            if (topb) {out.push_back({loc, "\"U"}); topb = false;}
            else out.back().data.push_back('U');
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
          auto start = loc;
          auto res = parse_macro(it, end, macros, flags, loc);
          if (!res) return out;
          if (res->front() == '@') out.push_back({start, *res});
          else {
            bool u = flags.update_location;
            flags.update_location = false;
            auto toks = tokenize(*res, start, flags, macros);
            out.insert(out.end(), toks.begin(), toks.end());
            flags.update_location = u;
          }
        } break;
        case '\'':
          ADV
          if (flags.update_location) {STEP}
          switch (c) {
            case '\'':
              flags.onerror(loc, "empty character literal", WARNING);
              out.push_back({loc, {'\'', '\0'}});
              continue;
            case '\\':
              ADV
              if (flags.update_location) {STEP}
              switch (c) {
                case '0': out.push_back({loc, {'\'', '\0'}}); break;
                case 'n': out.push_back({loc, {'\'', '\n'}}); break;
                case 'a': out.push_back({loc, {'\'', '\a'}}); break;
                case 'b': out.push_back({loc, {'\'', '\b'}}); break;
                case 'r': out.push_back({loc, {'\'', '\r'}}); break;
                case 't': out.push_back({loc, {'\'', '\t'}}); break;
                case 'v': out.push_back({loc, {'\'', '\v'}}); break;
                case '\\': out.push_back({loc, {'\'', '\\'}}); break;
                case '\'': out.push_back({loc, {'\'', '\''}}); break;
                case 'x': {
                  ADV
                  step(c);
                  unsigned char c2 = c2x(c);
                  if (c2 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  ADV
                  step(c);
                  unsigned char c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 <<= 4;
                  c2 |= c3;
                  out.push_back({loc, {'\'', (char)c2}});
                } break;
                case 'u': {
                  ADV
                  step(c);
                  uint16_t c2 = c2x(c);
                  if (c2 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  ADV
                  step(c);
                  unsigned char c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 <<= 4;
                  c2 |= c3;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 <<= 4;
                  c2 |= c3;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 |= c3 << 12;
                  std::string str(3, '\0');
                  str[0] = '\'';
                  std::memcpy(str.data() + 1, &c2, 2);
                  out.push_back({loc, std::move(str)});
                } break;
                case 'U': {
                  ADV
                  step(c);
                  uint32_t c2 = c2x(c);
                  if (c2 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  ADV
                  step(c);
                  unsigned char c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 <<= 4;
                  c2 |= c3;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 <<= 4;
                  c2 |= c3;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 <<= 4;
                  c2 |= c3;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 <<= 4;
                  c2 |= c3;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 <<= 4;
                  c2 |= c3;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 <<= 4;
                  c2 |= c3;
                  ADV
                  step(c);
                  c3 = c2x(c);
                  if (c3 == 255) flags.onerror(loc, '\'' + to_string(c) + "' is not a hexadecimal character", ERROR);
                  c2 <<= 4;
                  c2 |= c3;
                  std::string str(5, '\0');
                  str[0] = '\'';
                  std::memcpy(str.data() + 1, &c2, 4);
                  out.push_back({loc, std::move(str)});
                } break;
              }
              break;
            default: {
              std::string str(5, '\0');
              str[0] = '\'';
              std::memcpy(str.data() + 1, &c, 4);
              out.push_back({loc, std::move(str)});
            }
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
          if (topb) {
            --it;
            out.push_back({loc, parse_num(it, end, {loc, flags.onerror}, step)});
            if (flags.update_location) --loc.col;
          }
          else out.back().data.push_back((char)c);
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
        case ':':
        case ';':
        case ',':
        case '*':
        case '/':
        case '%':
        case '!':
          out.push_back({loc, {char(c)}});
          topb = true;
          break;
#pragma endregion
        case '+':
        case '-':
        case '&':
        case '|':
        case '^':
        case '<':
        case '>': {
          topb = true;
          char c2 = char((signed char)c);
          if (out.size() && out.back().data == std::string_view{&c2, 1}) out.back().data.push_back(c2);
          else out.push_back({loc, {c2}});
        } break;
        case '=':
          topb = true;
          if (out.size()) {
            if (out.back().data.size() == 1) switch (out.back().data.front()) {
              case '+':
              case '-':
              case '*':
              case '/':
              case '%':
              case '&':
              case '|':
              case '^':
              case '!':
              case '=':
              case '<':
              case '>':
                out.back().data.push_back('=');
                break;
              default:
                out.push_back({loc, "="});
            }
            else if (out.back().data.size() == 2 && out.back().data[0] == out.back().data[1]) switch (out.back().data.front()) {
              case '^':
              case '<':
              case '>':
                out.back().data.push_back('=');
                break;
              default:
                out.push_back({loc, "="});
            }
            else out.push_back({loc, "="});
          }
          else out.push_back({loc, "="});
          break;
        case '.':
          if (it == end) out.push_back({loc, "."});
          else {
            char c2 = *it;
            if (c2 >= '0' && c2 <= '9') {
              --it;
              out.push_back({loc, parse_num(it, end, {loc, flags.onerror}, step)});
              if (flags.update_location) --loc.col;
            }
            else out.push_back({loc, "."});
            topb = true;
          }
          break;
        default:
          if (topb) {out.push_back({loc, ""}); topb = false;}
          append(out.back().data, c);
      }
    }
    step(c);
  }
  if (it < end) flags.onerror(loc, "invalid UTF-8 character", CRITICAL);
  if (in_string) flags.onerror(loc, "unterminated string literal", ERROR);
  return out;
}