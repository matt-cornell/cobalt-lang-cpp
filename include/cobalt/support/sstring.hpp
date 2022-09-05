#ifndef COBALT_SUPPORT_SSTRING_HPP
#define COBALT_SUPPORT_SSTRING_HPP
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
namespace cobalt::sstring {
  class string : public std::string_view {
    using std::string_view::basic_string_view;
    string() = delete;
    string(std::string_view str) : std::string_view(str) {}
    friend class getter_t;
  };
  inline std::string operator+(std::string_view lhs, std::string_view rhs) {
    std::string out(lhs.size() + rhs.size(), 0);
    std::memcpy(out.data(), lhs.data(), lhs.size());
    std::memcpy(out.data() + lhs.size(), rhs.data(), rhs.size());
    return out;
  }
  inline bool operator==(string lhs, string rhs) {return lhs.data() == rhs.data();}
  #if __cplusplus < 202002LL
  inline bool operator!=(string lhs, string rhs) {return lhs.data() != rhs.data();}
  #endif
  class getter_t {
    struct optional_deleter {
      bool del;
      optional_deleter(bool del) : del(del) {}
      template <class T> void operator()(T const* ptr) const noexcept(noexcept(std::declval<T*>()->~T())) {if (del) delete ptr;}
    };
    using ptr_t = std::unique_ptr<std::string, optional_deleter>;
    struct hash_t {
      using is_transparent = void;
      template <class T> std::size_t operator()(T const& val) const {return std::hash<std::decay_t<T const>>{}(val);}
      template <class T> std::size_t operator()(std::unique_ptr<T, optional_deleter> const& val) const {return std::hash<T>{}(*val);}
    };
    struct eq_t {
      using is_transparent = void;
      template <class L, class R> bool operator()(L const& lhs, R const& rhs) const {return lhs == rhs;}
      template <class L, class R> bool operator()(std::unique_ptr<L, optional_deleter> const& lhs, R const& rhs) const {return *lhs == rhs;}
      template <class L, class R> bool operator()(L const& lhs, std::unique_ptr<R, optional_deleter> const& rhs) const {return lhs == *rhs;}
      template <class L, class R> bool operator()(std::unique_ptr<L, optional_deleter> const& lhs, std::unique_ptr<R, optional_deleter> const& rhs) const {return *lhs == *rhs;}
    };
    using set_t = std::unordered_set<ptr_t, hash_t, eq_t>;
    template <class C, class K> struct heterogenous_lookup {
      template <class T> static int8_t test(...);
      template <class T, class = decltype(std::declval<C>().find(std::declval<T>()))> static int16_t test(int);
      enum {value = sizeof(decltype(test<K>(0))) > 1};
    };
    inline static set_t strings;
  public:
    template <class K> string operator()(K&& val) const {
      if constexpr(heterogenous_lookup<set_t, decltype((std::forward<K>(val)))>::value) {
        auto it = strings.find(std::forward<K>(val));
        if (it == strings.end()) it = strings.insert(ptr_t(new std::string(std::forward<K>(val)), true)).first;
        return string(**it);
      }
      else {
        std::string str(std::forward<K>(val));
        auto it = strings.find(ptr_t(&str, false));
        if (it == strings.end()) it = strings.insert(ptr_t(new std::string(std::move(str)), true)).first;
        return string(**it);
      }
    }
  };
  inline const getter_t get;
}
namespace std {template <> struct hash<cobalt::sstring::string> {std::size_t operator()(cobalt::sstring::string const& val) const {return uintptr_t(val.data());}};}
#endif