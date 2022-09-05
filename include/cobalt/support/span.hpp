#ifndef COBALT_SUPPORT_SPAN_HPP
#define COBALT_SUPPORT_SPAN_HPP
#if __cplusplus >= 202002LL
#include <span>
namespace cobalt {template <class T> using span = std::span<T>;}
#else
namespace cobalt {
  template <class T> class span {
    T* data_;
    unsigned long size_;
  public:
    using value_type = T;
    using reference = T&;
    using iterator = T*;
    using const_iterator = T*;
    span(T* data, unsigned long size) : data_(data), size_(size) {}
    template <class I> span(I begin, I end) : data_(&*begin), size_(end - begin) {}
    unsigned long size() const {return size_;}
    bool empty() const {return size_ == 0;}
    T* data() const {return data_;}
    T* begin() const {return data_;}
    T* end() const {return data_ + size_;}
    T& front() const {return *data_;}
    T& back() const {return *(data_ + size_ - 1);}
    T& operator[](unsigned long idx) const {return data_[idx];}
    span<T> first(unsigned long idx) const {return {data_, idx};}
    span<T> last(unsigned long idx) const {return {data_ + size_ - idx, idx};}
    span<T> subspan(unsigned long start) const {return {data_ + start, size_ - start};}
    span<T> subspan(unsigned long start, unsigned long size) const {return {data_ + start, size};}
  };
}
#endif
#endif