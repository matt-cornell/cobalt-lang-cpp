#ifndef TEST_HPP
#define TEST_HPP
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <variant>
#include <vector>
namespace test {
  inline namespace test_builders {struct finish_t;}
  struct test_case : std::function<bool()> {
    test_case() = delete;
  private:
    template <class F> test_case(F&& fn) : std::function<bool()>(std::move(fn)) {}
    friend struct test_builders::finish_t;
  };
  constexpr struct default_predicate_t {
    template <class T> bool operator()(T&& arg) const {return static_cast<bool>(std::move(arg));}
    template <class T> bool operator()(T const& arg) const {return static_cast<bool>(arg);}
  } default_predicate;

  inline namespace test_builders {
    struct default_test_config {
      using exception = std::nothrow_t;
      using args_type = std::tuple<>;
      using pred_type = default_predicate_t;
      constexpr static bool invert = false;
    };
    namespace {
      template <class C, class args_t> struct set_args {
        using args_type = args_t;
        using exception = typename C::exception;
        using pred_type = typename C::pred_type;
        constexpr static bool invert = C::invert;
      };
      template <class C, class exception_> struct set_ex {
        using args_type = typename C::args_type;
        using exception = exception_;
        using pred_type = typename C::pred_type;
        constexpr static bool invert = C::invert;
      };
      template <class C> struct set_invert {
        using args_type = typename C::args_type;
        using exception = typename C::exception;
        using pred_type = typename C::pred_type;
        constexpr static bool invert = true;
      };
      template <class C, class P> struct set_predicate {
        using args_type = typename C::args_type;
        using exception = typename C::exception;
        using pred_type = P;
        constexpr static bool invert = C::invert;
      };
      template <class F, class A> struct apply_result;
      template <class F, class... As> struct apply_result<F, std::tuple<As...>> {using type = std::invoke_result_t<F, As...>;};
      template <class F, class A> using apply_result_t = typename apply_result<F, A>::type;
    }
    template <class T> concept is_test_config = requires {
      {std::declval<typename T::exception>()};
      {std::declval<typename T::args_type>()};
      {std::declval<typename T::eq_functior>()};
      {T::invert} -> std::same_as<bool>;
    };
    template <class T> concept is_test = is_test_config<typename T::test_type>;
    template <class F, class C> struct test_data {
      using func_type = F;
      using test_type = C;
      F fn;
      typename C::args_type args = typename C::args_type {};
      typename C::pred_type pred = {};
    };
    template <class C> struct test_args {
      std::variant<typename C::args_type, std::size_t> args = typename C::args_type {};
      template <class F> test_data<F, C> operator()(F&& fn) & {return {std::move(fn), args};}
      template <class F> test_data<F, C> operator()(F const& fn) & {return {fn, args};}
      template <class F> test_data<F, C> operator()(F&& fn) && {return {std::move(fn), std::move(args)};}
      template <class F> test_data<F, C> operator()(F const& fn) && {return {fn, std::move(args)};}
    };
    constexpr struct test_t {
      template <class F> test_data<F, default_test_config> operator()(F&& fn) const {return {std::move(fn), {}};}
      template <class F> test_data<F, default_test_config> operator()(F const& fn) const {return {fn, {}};}
      inline operator test_args<default_test_config>() const {return {};}
    } mktest;
    template <class F> inline test_data<F, default_test_config> operator|(F&& fn, test_t) {return {std::move(fn), {}};}
    template <class F> inline test_data<F, default_test_config> operator-(F&& fn, test_t) {return {std::move(fn), {}};}
    template <class F> inline test_data<F, default_test_config> operator|(F const& fn, test_t) {return {fn, {}};}
    template <class F> inline test_data<F, default_test_config> operator-(F const& fn, test_t) {return {fn, {}};}
    constexpr struct args_t {
      template <class... args_t> struct closure {std::tuple<args_t...> args;};
      template <class... args_t> closure<args_t...> operator()(args_t&&... args) const {return {std::tuple<args_t...>(args...)};}
      template <class F, class C, class... args_t> set_args<F, std::tuple<args_t...>> operator()(test_data<F, C>&& test, args_t&&... args) const {return {std::move(test.fn), std::make_tuple<args...>(std::move(args)...), std::move(test.pred)};}
      template <class F, class C, class... args_t> set_args<F, std::tuple<args_t...>> operator()(test_data<F, C> const& test, args_t&&... args) const {return {test.fn, std::make_tuple<args...>(std::move(args)...), test.pred};}
    } args;
    template <class... As> struct typed_args_t {
      typename args_t::template closure<As...> operator()(As&&... args) const {return {std::make_tuple(args...)};}
      template <class F, class C> set_args<F, std::tuple<As...>> operator()(test_data<F, C>&& test, As&&... args) const {return {std::move(test.fn), std::make_tuple(std::move(args)...), std::move(test.pred)};}
      template <class F, class C> set_args<F, std::tuple<As...>> operator()(test_data<F, C> const& test, As&&... args) const {return {test.fn, std::make_tuple(std::move(args)...), test.pred};}
    };
    template <class... args_t> constexpr typed_args_t<args_t...> typed_args;
    template <class F, class C, class... As> inline test_data<F, set_args<C, std::tuple<As...>>> operator|(test_data<F, C>&& test, args_t::closure<As...> const& args) {return {std::move(test.fn), args.args, std::move(test.pred)};}
    template <class F, class C, class... As> inline test_data<F, set_args<C, std::tuple<As...>>> operator-(test_data<F, C>&& test, args_t::closure<As...> const& args) {return {std::move(test.fn), args.args, std::move(test.pred)};}
    template <class F, class C, class... As> inline test_data<F, set_args<C, std::tuple<As...>>> operator|(test_data<F, C> const& test, args_t::closure<As...> const& args) {return {test.fn, args.args, test.pred};}
    template <class F, class C, class... As> inline test_data<F, set_args<C, std::tuple<As...>>> operator-(test_data<F, C> const& test, args_t::closure<As...> const& args) {return {test.fn, args.args, test.pred};}
    template <class F, class C, class... As> inline test_data<F, set_args<C, std::tuple<As...>>> operator|(test_data<F, C>&& test, args_t::closure<As...>&& args) {return {std::move(test.fn), std::move(args.args), std::move(test.pred)};}
    template <class F, class C, class... As> inline test_data<F, set_args<C, std::tuple<As...>>> operator-(test_data<F, C>&& test, args_t::closure<As...>&& args) {return {std::move(test.fn), std::move(args.args), std::move(test.pred)};}
    template <class F, class C, class... As> inline test_data<F, set_args<C, std::tuple<As...>>> operator|(test_data<F, C> const& test, args_t::closure<As...>&& args) {return {test.fn, std::move(args.args), test.pred};}
    template <class F, class C, class... As> inline test_data<F, set_args<C, std::tuple<As...>>> operator-(test_data<F, C> const& test, args_t::closure<As...>&& args) {return {test.fn, std::move(args.args), test.pred};}
    template <class C, class... As> inline test_args<set_args<C, std::tuple<As...>>> operator|(test_args<C>&&, args_t::closure<As...> const& args) {return {args.args};}
    template <class C, class... As> inline test_args<set_args<C, std::tuple<As...>>> operator-(test_args<C>&&, args_t::closure<As...> const& args) {return {args.args};}
    template <class C, class... As> inline test_args<set_args<C, std::tuple<As...>>> operator|(test_args<C> const&, args_t::closure<As...> const& args) {return {args.args};}
    template <class C, class... As> inline test_args<set_args<C, std::tuple<As...>>> operator-(test_args<C> const&, args_t::closure<As...> const& args) {return {args.args};}
    template <class C, class... As> inline test_args<set_args<C, std::tuple<As...>>> operator|(test_args<C>&&, args_t::closure<As...>&& args) {return {std::move(args.args)};}
    template <class C, class... As> inline test_args<set_args<C, std::tuple<As...>>> operator-(test_args<C>&&, args_t::closure<As...>&& args) {return {std::move(args.args)};}
    template <class C, class... As> inline test_args<set_args<C, std::tuple<As...>>> operator|(test_args<C> const&, args_t::closure<As...>&& args) {return {std::move(args.args)};}
    template <class C, class... As> inline test_args<set_args<C, std::tuple<As...>>> operator-(test_args<C> const&, args_t::closure<As...>&& args) {return {std::move(args.args)};}
    constexpr struct inv_t {
      template <class F, class C> test_data<F, set_invert<C>> operator()(test_data<F, C>&& test) {return {std::move(test.fn)};}
      template <class F, class C> test_data<F, set_invert<C>> operator()(test_data<F, C> const& test) {return {test.fn};}
    } invert;
    template <class F, class C> inline test_data<F, set_invert<C>> operator|(test_data<F, C>&& test, inv_t) {return {std::move(test.fn), std::move(test.args), std::move(test.pred)};}
    template <class F, class C> inline test_data<F, set_invert<C>> operator-(test_data<F, C>&& test, inv_t) {return {std::move(test.fn), std::move(test.args), std::move(test.pred)};}
    template <class F, class C> inline test_data<F, set_invert<C>> operator|(test_data<F, C> const& test, inv_t) {return {test.fn, test.args, true, test.pred};}
    template <class F, class C> inline test_data<F, set_invert<C>> operator-(test_data<F, C> const& test, inv_t) {return {test.fn, test.args, true, test.pred};}
    template <class C> inline test_args<set_invert<C>> operator|(test_args<C>&& test, inv_t) {return {std::move(test.args)};}
    template <class C> inline test_args<set_invert<C>> operator-(test_args<C>&& test, inv_t) {return {std::move(test.args)};}
    template <class C> inline test_args<set_invert<C>> operator|(test_args<C> const& test, inv_t) {return {test.args};}
    template <class C> inline test_args<set_invert<C>> operator-(test_args<C> const& test, inv_t) {return {test.args};}
    template <class E> struct throw_t {
      template <class F, class C> test_data<F, set_ex<C, E>> operator()(test_data<F, C>&& test) const {return {std::move(test.fn), std::move(test.args), std::move(test.pred)};}
      template <class F, class C> test_data<F, set_ex<C, E>> operator()(test_data<F, C> const& test) const {return {test.fn};}
    };
    template <class E> constexpr throw_t<E> throws;
    template <class F, class C, class E> inline test_data<F, set_ex<C, E>> operator|(test_data<F, C>&& test, throw_t<E>) {return {std::move(test.fn), std::move(test.args), std::move(test.pred)};}
    template <class F, class C, class E> inline test_data<F, set_ex<C, E>> operator-(test_data<F, C>&& test, throw_t<E>) {return {std::move(test.fn), std::move(test.args), std::move(test.pred)};}
    template <class F, class C, class E> inline test_data<F, set_ex<C, E>> operator|(test_data<F, C> const& test, throw_t<E>) {return {test.fn, test.args, test.pred};}
    template <class F, class C, class E> inline test_data<F, set_ex<C, E>> operator-(test_data<F, C> const& test, throw_t<E>) {return {test.fn, test.args, test.pred};}
    template <class C, class E> inline test_args<set_ex<C, E>> operator|(test_args<C>&& test, throw_t<E>) {return {std::move(test.args)};}
    template <class C, class E> inline test_args<set_ex<C, E>> operator-(test_args<C>&& test, throw_t<E>) {return {std::move(test.args)};}
    template <class C, class E> inline test_args<set_ex<C, E>> operator|(test_args<C> const& test, throw_t<E>) {return {test.args};}
    template <class C, class E> inline test_args<set_ex<C, E>> operator-(test_args<C> const& test, throw_t<E>) {return {test.args};}
    constexpr struct predicate_t {
      template <class P> struct closure {P pred;};
      template <class P> closure<P> operator()(P&& pred) const {return {std::move(pred)};}
      template <class P, class F, class C> test_data<F, set_predicate<C, P>> operator()(test_data<F, C>&& test, P&& pred) const {return {std::move(test.fn), std::move(test.args), std::move(pred)};}
      template <class P, class F, class C> test_data<F, set_predicate<C, P>> operator()(test_data<F, C> const& test, P&& pred) const {return {test.fn, test.args, std::move(pred)};}
    } predicate;
    template <class P, class F, class C> inline test_data<F, set_predicate<C, P>> operator|(test_data<F, C>&& test, predicate_t::closure<P> cls) {return {std::move(test.fn), std::move(test.args), std::move(cls.pred)};}
    template <class P, class F, class C> inline test_data<F, set_predicate<C, P>> operator-(test_data<F, C>&& test, predicate_t::closure<P> cls) {return {std::move(test.fn), std::move(test.args), std::move(cls.pred)};}
    template <class P, class F, class C> inline test_data<F, set_predicate<C, P>> operator|(test_data<F, C> const& test, predicate_t::closure<P> cls) {return {test.fn, test.args, std::move(cls.pred)};}
    template <class P, class F, class C> inline test_data<F, set_predicate<C, P>> operator-(test_data<F, C> const& test, predicate_t::closure<P> cls) {return {test.fn, test.args, std::move(cls.pred)};}
    constexpr struct finish_t {
      template <class F, class C> test_case operator()(test_data<F, C>&& test) const {
        return [test] {
          try {
            bool res = test.pred(std::apply(test.fn, test.args));
            if constexpr (std::same_as<typename C::exception, std::nothrow_t>) return C::invert ? !res : res;
            else return false;
          }
          catch (typename C::exception) {return true;}
          catch (...) {return false;}
        };
      }
      template <class F, class C> test_case operator()(test_data<F, C> const& test) const {
        return [test] {
          try {
            bool res = test.pred(std::apply(test.fn, test.args));
            if constexpr (std::same_as<typename C::exception, std::nothrow_t>) return C::invert ? !res : res;
            else return false;
          }
          catch (typename C::exception) {return true;}
          catch (...) {return false;}
        };
      }
    } finish;
    template<class F, class C> inline auto operator|(test_data<F, C>&& test, finish_t) {return finish_t{}(std::move(test));}
    template<class F, class C> inline auto operator-(test_data<F, C>&& test, finish_t) {return finish_t{}(std::move(test));}
    template<class F, class C> inline auto operator|(test_data<F, C> const& test, finish_t) {return finish_t{}(test);}
    template<class F, class C> inline auto operator-(test_data<F, C> const& test, finish_t) {return finish_t{}(test);}
  }
  struct tester {
    std::string name;
    tester(std::string const& name) : name(name) {}
    tester(std::string const& name, test_case&& test) : name(name), tests_(std::move(test)) {}
    tester(std::string const& name, std::vector<tester>&& test) : name(name), tests_(std::move(test)) {}
    tester& add(tester&& test) {
      if (tests_.index()) {
        tester self(name);
        std::swap(self, *this);
        tests_ = std::vector{self, std::move(test)};
      }
      else std::get<0>(tests_).push_back(std::move(test));
      return *this;
    }
    bool operator()(std::ostream& os = std::cout) const {
      auto res = print_impl(os, "");
      if (res.first.size()) {
        os << "failed tests:\n";
        for (auto const& i : res.first) os << i << '\n';
        os.flush();
        return false;
      }
      else {
        os << "all tests passed" << std::endl;
        return true;
      }
    }
  private:
    std::pair<std::vector<std::string>, std::size_t> print_impl(std::ostream& os, std::string const& prefix) const {
      if (tests_.index()) {
        os << prefix << "testing " << name << "... " << std::flush;
        auto res = std::get<1>(tests_)();
        os << (res ? "passed" : "failed") << std::endl;
        return res ? std::pair<std::vector<std::string>, std::size_t>{{}, 1} : std::pair<std::vector<std::string>, std::size_t>{std::vector{name}, 1};
      }
      else {
        std::string pf = "  " + prefix;
        std::pair<std::vector<std::string>, std::size_t> res;
        os << prefix << "testing group " << name << "..." << std::endl;
        for (auto const& i : std::get<0>(tests_)) {
          auto r2 = i.print_impl(os, pf);
          res.first.insert(res.first.end(), r2.first.begin(), r2.first.end());
          res.second += r2.second;
        }
        os << prefix << (res.second - res.first.size()) << " out of " << res.second << " passed" << std::endl;
        return res;
      }
    }
    std::variant<std::vector<tester>, test_case> tests_;
  };
}
#endif // TEST_HPP
