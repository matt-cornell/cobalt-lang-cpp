#ifndef COBALT_SUPPORT_BORROW_FUNC_HPP
#define COBALT_SUPPORT_BORROW_FUNC_HPP
namespace cobalt {
  template <class F> struct borrow_function;
  template <class R, class... As> struct borrow_function<R(As...)> {
    void* data;
    R (*call)(void*, As...);
    borrow_function() = delete;
    borrow_function(R (&fn)(As...)) : data((void*)&fn), call([](void* data, As... args) {return static_cast<R(*)(As...)>(data)(args...);}) {}
    template <class F> borrow_function(F& fn) : data((void*)&fn), call([](void* data, As... args) {return (*(F*)data)(args...);}) {}
    template <class F> borrow_function& operator=(F& fn) {
      data = (void*)&fn;
      call = [](void* data, As... args) {return (*(F*)data)(args...);};
      return *this;
    }
    R operator()(As... args) const {return call(data, args...);}
  };
  template <class F> struct global_function;
  template <class R, class... As> struct global_function<R(As...)> {
    void* data;
    R (*call)(void*, As...);
    global_function() = delete;
    template <class F> global_function(F const& fn) : data((void*)new F(fn)), call([](void* data, As... args) {return (*(F*)data)(args...);}) {}
    template <class F> global_function& operator=(F const& fn) {
      data = (void*)new F(fn);
      call = [](void* data, As... args) {return (*(F*)data)(args...);};
    }
    R operator()(As... args) const {return call(data, args...);}
  };
  template <class F> struct rc_function;
  template <class R, class... As> struct rc_function<R(As...)> {
    void* data;
    std::size_t* refs;
    R (*call)(void*, As...);
    void (*destroy)(void*);
    rc_function() = delete;
    template <class F> rc_function(F const& fn) : data((void*)new F(fn)), refs(new std::size_t{1}), call([](void* data, As... args) {return (*(F*)data)(args...);}), destroy([] (void* data) {delete (F*)data;}) {}
    rc_function(rc_function<R(As...)> const& other) : data(other.data), refs(other.refs), call(other.call), destroy(other.destroy) {++*refs;}
    ~rc_function() {
      if (!--*refs) {
        destroy(data);
        delete refs;
      }
    }
    rc_function& operator==(rc_function<R(As...)> other) {std::swap(*this, other);}
    R operator()(As... args) const {return call(data, args...);}
  };
}
#endif