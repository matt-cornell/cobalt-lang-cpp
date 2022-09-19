#include "cobalt/tokenizer.hpp"
#include <cmath>
#include <numbers>
#include <llvm/Support/MemoryBuffer.h>
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
bool is_hex(char32_t c) {
  if (c >= '0' && c <= '9') return true;
  if (c >= 'a' && c <= 'f') return true;
  if (c >= 'A' && c <= 'F') return true;
  return false;
}
template <class T> std::string encode(char c, T const& val) {
  std::array<char, sizeof(T) + 1> data;
  data[0] = c;
  std::memcpy(data.data() + 1, &val, sizeof(T));
  return std::string(data.data(), sizeof(T) + 1);
}
template <class I> std::string parse_num(I& it, I end, bound_handler const& onerror, borrow_function<char32_t(char32_t)> step) {
  uint32_t decimal_places = 0;
  constexpr double log2_10 = std::numbers::ln10_v<double> / std::numbers::ln2_v<double>;
  llvm::APInt int_part;
  double float_part = 0;
  bool negative = false, mode_set = true;
  enum {SIGNED, UNSIGNED, FLOAT} mode = SIGNED;
  uint16_t nbits = 64;
  double bits;
  --it;
  while (it != end) {
    char c = *it;
    step(c);
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
        char c2 = *(it + 1);
        if (c2 & 0x80 || c2 < '0' || c2 > '9') goto end;
        if (decimal_places) {
          onerror("identifier cannot start with a number", ERROR);
          goto end;
        }
        decimal_places = 1;
        mode = FLOAT;
      } break;
      case 'i':
        if (mode_set) goto end;
        mode = SIGNED;
        mode_set = true;
        break;
      case 'u':
        if (mode_set) goto end;
        mode = UNSIGNED;
        mode_set = true;
        break;
      case 'f':
        if (mode_set) goto end;
        mode = FLOAT;
        mode_set = true;
        break;
      default:
        goto end;
    }
    ++it;
  }
  end:
  std::string out;
  switch (mode) {
    case SIGNED:
    case UNSIGNED:
      out.resize(int_part.getNumWords() * llvm::APInt::APINT_WORD_SIZE + 2);
      out[0] = mode == UNSIGNED ? '0' : '2';
      out[0] |= negative << 1;
      std::memcpy(out.data() + 1, &nbits, 2);
      std::memcpy(out.data() + 3, int_part.getRawData(), int_part.getNumWords() * llvm::APInt::APINT_WORD_SIZE);
      break;
    case FLOAT:
      float_part += int_part.getLoBits(llvm::APInt::APINT_WORD_SIZE).getZExtValue();
      if (negative) float_part = -float_part;
      out.resize(sizeof(double) + 2);
      out[0] = '4';
      out[1] = char((signed char)nbits);
      std::memcpy(out.data() + 2, &float_part, sizeof(double));
  }
  return out;
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
#define DEF_PP(NAME, ...) {sstring::get(#NAME), [](std::string_view code, bound_handler onerror)->std::string __VA_ARGS__},
pp_map cobalt::default_directives;
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
        case '@':
          flags.onerror(loc, "macros are not currently supported", CRITICAL);
          return out;
          break;
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
          if (topb) out.push_back({loc, parse_num(it, end, {loc, flags.onerror}, step)});
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