/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef TEMPLATE_H
#define TEMPLATE_H

#include "nghttp2_config.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <array>
#include <functional>
#include <typeinfo>

namespace nghttp2 {

template <typename T, typename... U>
typename std::enable_if<!std::is_array<T>::value, std::unique_ptr<T>>::type
make_unique(U &&... u) {
  return std::unique_ptr<T>(new T(std::forward<U>(u)...));
}

template <typename T>
typename std::enable_if<std::is_array<T>::value, std::unique_ptr<T>>::type
make_unique(size_t size) {
  return std::unique_ptr<T>(new typename std::remove_extent<T>::type[size]());
}

// std::forward is constexpr since C++14
template <typename... T>
constexpr std::array<
    typename std::decay<typename std::common_type<T...>::type>::type,
    sizeof...(T)>
make_array(T &&... t) {
  return std::array<
      typename std::decay<typename std::common_type<T...>::type>::type,
      sizeof...(T)>{{std::forward<T>(t)...}};
}

template <typename T, size_t N> constexpr size_t array_size(T(&)[N]) {
  return N;
}

template <typename T, size_t N> constexpr size_t str_size(T(&)[N]) {
  return N - 1;
}

// inspired by <http://blog.korfuri.fr/post/go-defer-in-cpp/>, but our
// template can take functions returning other than void.
template <typename F, typename... T> struct Defer {
  Defer(F &&f, T &&... t)
      : f(std::bind(std::forward<F>(f), std::forward<T>(t)...)) {}
  Defer(Defer &&o) : f(std::move(o.f)) {}
  ~Defer() { f(); }

  using ResultType = typename std::result_of<typename std::decay<F>::type(
      typename std::decay<T>::type...)>::type;
  std::function<ResultType()> f;
};

template <typename F, typename... T> Defer<F, T...> defer(F &&f, T &&... t) {
  return Defer<F, T...>(std::forward<F>(f), std::forward<T>(t)...);
}

template <typename T, typename F> bool test_flags(T t, F flags) {
  return (t & flags) == flags;
}

// doubly linked list of element T*.  T must have field T *dlprev and
// T *dlnext, which point to previous element and next element in the
// list respectively.
template <typename T> struct DList {
  DList() : head(nullptr), tail(nullptr) {}

  DList(const DList &) = delete;

  DList &operator=(const DList &) = delete;

  DList(DList &&other) : head(other.head), tail(other.tail) {
    other.head = other.tail = nullptr;
  }

  DList &operator=(DList &&other) {
    if (this == &other) {
      return *this;
    }
    head = other.head;
    tail = other.tail;
    other.head = other.tail = nullptr;
    return *this;
  }

  void append(T *t) {
    if (tail) {
      tail->dlnext = t;
      t->dlprev = tail;
      tail = t;
      return;
    }
    head = tail = t;
  }

  void remove(T *t) {
    auto p = t->dlprev;
    auto n = t->dlnext;
    if (p) {
      p->dlnext = n;
    }
    if (head == t) {
      head = n;
    }
    if (n) {
      n->dlprev = p;
    }
    if (tail == t) {
      tail = p;
    }
    t->dlprev = t->dlnext = nullptr;
  }

  bool empty() const { return head == nullptr; }

  T *head, *tail;
};

template <typename T> void dlist_delete_all(DList<T> &dl) {
  for (auto e = dl.head; e;) {
    auto next = e->dlnext;
    delete e;
    e = next;
  }
}

// User-defined literals for K, M, and G (powers of 1024)

constexpr unsigned long long operator"" _k(unsigned long long k) {
  return k * 1024;
}

constexpr unsigned long long operator"" _m(unsigned long long m) {
  return m * 1024 * 1024;
}

constexpr unsigned long long operator"" _g(unsigned long long g) {
  return g * 1024 * 1024 * 1024;
}

// User-defined literals for time, converted into double in seconds

constexpr double operator"" _h(unsigned long long h) { return h * 60 * 60; }

constexpr double operator"" _min(unsigned long long min) { return min * 60; }

// Returns a copy of NULL-terminated string [first, last).
template <typename InputIt>
std::unique_ptr<char[]> strcopy(InputIt first, InputIt last) {
  auto res = make_unique<char[]>(last - first + 1);
  *std::copy(first, last, res.get()) = '\0';
  return res;
}

// Returns a copy of NULL-terminated string |val|.
inline std::unique_ptr<char[]> strcopy(const char *val) {
  return strcopy(val, val + strlen(val));
}

inline std::unique_ptr<char[]> strcopy(const char *val, size_t n) {
  return strcopy(val, val + n);
}

// Returns a copy of val.c_str().
inline std::unique_ptr<char[]> strcopy(const std::string &val) {
  return strcopy(std::begin(val), std::end(val));
}

inline std::unique_ptr<char[]> strcopy(const std::unique_ptr<char[]> &val) {
  if (!val) {
    return nullptr;
  }
  return strcopy(val.get());
}

inline std::unique_ptr<char[]> strcopy(const std::unique_ptr<char[]> &val,
                                       size_t n) {
  if (!val) {
    return nullptr;
  }
  return strcopy(val.get(), val.get() + n);
}

// ImmutableString represents string that is immutable unlike
// std::string.  It has c_str() and size() functions to mimic
// std::string.  It manages buffer by itself.  Just like std::string,
// c_str() returns NULL-terminated string, but NULL character may
// appear before the final terminal NULL.
class ImmutableString {
public:
  using traits_type = std::char_traits<char>;
  using value_type = traits_type::char_type;
  using allocator_type = std::allocator<char>;
  using size_type = std::allocator_traits<allocator_type>::size_type;
  using difference_type =
      std::allocator_traits<allocator_type>::difference_type;
  using const_reference = const value_type &;
  using const_pointer = const value_type *;
  using const_iterator = const_pointer;

  ImmutableString() : len(0), base("") {}
  ImmutableString(const char *s, size_t slen)
      : len(slen), holder(strcopy(s, len)), base(holder.get()) {}
  ImmutableString(const char *s)
      : len(strlen(s)), holder(strcopy(s, len)), base(holder.get()) {}
  ImmutableString(std::unique_ptr<char[]> s)
      : len(strlen(s.get())), holder(std::move(s)), base(holder.get()) {}
  ImmutableString(std::unique_ptr<char[]> s, size_t slen)
      : len(slen), holder(std::move(s)), base(holder.get()) {}
  ImmutableString(const std::string &s)
      : len(s.size()), holder(strcopy(s)), base(holder.get()) {}
  template <typename InputIt>
  ImmutableString(InputIt first, InputIt last)
      : len(std::distance(first, last)), holder(strcopy(first, last)),
        base(holder.get()) {}
  ImmutableString(const ImmutableString &other)
      : len(other.len), holder(strcopy(other.holder, other.len)),
        base(holder.get()) {}
  ImmutableString(ImmutableString &&other) noexcept
      : len(other.len),
        holder(std::move(other.holder)),
        base(holder.get()) {
    other.len = 0;
    other.base = "";
  }

  ImmutableString &operator=(const ImmutableString &other) {
    if (this == &other) {
      return *this;
    }
    len = other.len;
    holder = strcopy(other.holder, other.len);
    base = holder.get();
    return *this;
  }
  ImmutableString &operator=(ImmutableString &&other) noexcept {
    if (this == &other) {
      return *this;
    }
    len = other.len;
    holder = std::move(other.holder);
    base = holder.get();
    other.len = 0;
    other.base = "";
    return *this;
  }

  template <size_t N> static ImmutableString from_lit(const char(&s)[N]) {
    return ImmutableString(s, N - 1);
  }

  const char *c_str() const { return base; }
  size_type size() const { return len; }

private:
  size_type len;
  std::unique_ptr<char[]> holder;
  const char *base;
};

// StringRef is a reference to a string owned by something else.  So
// it behaves like simple string, but it does not own pointer.  When
// it is default constructed, it has empty string.  You can freely
// copy or move around this struct, but never free its pointer.  str()
// function can be used to export the content as std::string.
class StringRef {
public:
  using traits_type = std::char_traits<char>;
  using value_type = traits_type::char_type;
  using allocator_type = std::allocator<char>;
  using size_type = std::allocator_traits<allocator_type>::size_type;
  using difference_type =
      std::allocator_traits<allocator_type>::difference_type;
  using const_reference = const value_type &;
  using const_pointer = const value_type *;
  using const_iterator = const_pointer;

  StringRef() : base(""), len(0) {}
  StringRef(const std::string &s) : base(s.c_str()), len(s.size()) {}
  StringRef(const ImmutableString &s) : base(s.c_str()), len(s.size()) {}
  StringRef(const char *s) : base(s), len(strlen(s)) {}
  StringRef(const char *s, size_t n) : base(s), len(n) {}

  template <size_t N> static StringRef from_lit(const char(&s)[N]) {
    return StringRef(s, N - 1);
  }

  const_iterator begin() const { return base; };
  const_iterator cbegin() const { return base; };

  const_iterator end() const { return base + len; };
  const_iterator cend() const { return base + len; };

  const char *c_str() const { return base; }
  size_type size() const { return len; }

  std::string str() const { return std::string(base, len); }

private:
  const char *base;
  size_type len;
};

inline bool operator==(const StringRef &lhs, const std::string &rhs) {
  return lhs.size() == rhs.size() &&
         std::equal(std::begin(lhs), std::end(lhs), std::begin(rhs));
}

inline bool operator==(const std::string &lhs, const StringRef &rhs) {
  return rhs == lhs;
}

inline bool operator==(const StringRef &lhs, const char *rhs) {
  return lhs.size() == strlen(rhs) &&
         std::equal(std::begin(lhs), std::end(lhs), rhs);
}

inline bool operator==(const char *lhs, const StringRef &rhs) {
  return rhs == lhs;
}

inline bool operator!=(const StringRef &lhs, const std::string &rhs) {
  return !(lhs == rhs);
}

inline bool operator!=(const std::string &lhs, const StringRef &rhs) {
  return !(rhs == lhs);
}

inline bool operator!=(const StringRef &lhs, const char *rhs) {
  return !(lhs == rhs);
}

inline bool operator!=(const char *lhs, const StringRef &rhs) {
  return !(rhs == lhs);
}

inline int run_app(std::function<int(int, char **)> app, int argc,
                   char **argv) {
  try {
    return app(argc, argv);
  } catch (const std::bad_alloc &) {
    fputs("Out of memory\n", stderr);
  } catch (const std::exception &x) {
    fprintf(stderr, "Caught %s:\n%s\n", typeid(x).name(), x.what());
  } catch (...) {
    fputs("Unknown exception caught\n", stderr);
  }
  return EXIT_FAILURE;
}

} // namespace nghttp2

#endif // TEMPLATE_H
