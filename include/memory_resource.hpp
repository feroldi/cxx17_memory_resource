// The MIT License (MIT)
//
// Copyright (c) 2018 Mário Feroldi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef FEROLDI_CXX17_MEMORY_RESOURCE
#define FEROLDI_CXX17_MEMORY_RESOURCE

#include <cassert>
#include <cstddef>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#define FEROLDI_CXX17_MEMORY_RESOURCE_ASSERT(x) assert(x)

/*
memory_resource synopsis
namespace std::pmr {
  // [mem.res.class], class memory_­resource
  class memory_resource;

  bool operator==(const memory_resource& a, const memory_resource& b) noexcept;
  bool operator!=(const memory_resource& a, const memory_resource& b) noexcept;

  // [mem.poly.allocator.class], class template polymorphic_­allocator
  template<class Tp> class polymorphic_allocator;

  template<class T1, class T2>
    bool operator==(const polymorphic_allocator<T1>& a,
                    const polymorphic_allocator<T2>& b) noexcept;
  template<class T1, class T2>
    bool operator!=(const polymorphic_allocator<T1>& a,
                    const polymorphic_allocator<T2>& b) noexcept;

  // [mem.res.global], global memory resources
  memory_resource* new_delete_resource() noexcept;
  memory_resource* null_memory_resource() noexcept;
  memory_resource* set_default_resource(memory_resource* r) noexcept;
  memory_resource* get_default_resource() noexcept;

  // [mem.res.pool], pool resource classes
  struct pool_options;
  class synchronized_pool_resource;
  class unsynchronized_pool_resource;
  class monotonic_buffer_resource;
}
*/

namespace feroldi::pmr {

namespace detail {
template <class T>
struct always_false : ::std::false_type
{};
} // namespace detail

// [mem.res.class], class memory_resource
class memory_resource
{
public:
  // [mem.res.public], public member functions
  memory_resource(const memory_resource &) = default;
  virtual ~memory_resource() = default;

  memory_resource &operator=(const memory_resource &) = default;
  [[nodiscard]] void *
  allocate(::std::size_t bytes,
           ::std::size_t alignment = alignof(::std::max_align_t)) {
    return do_allocate(bytes, alignment);
  }

  void deallocate(void *p, ::std::size_t bytes,
                  ::std::size_t alignment = alignof(::std::max_align_t))
  {
    return do_deallocate(p, bytes, alignment);
  }

  bool is_equal(const memory_resource &other) const noexcept
  {
    return do_is_equal(other);
  }

private:
  // [mem.res.private], private member functions
  virtual void *do_allocate(::std::size_t bytes, ::std::size_t alignment) = 0;
  virtual void do_deallocate(void *p, ::std::size_t bytes,
                             ::std::size_t alignment) = 0;
  virtual bool do_is_equal(const memory_resource &other) const noexcept = 0;
};

inline bool operator==(const memory_resource &a,
                       const memory_resource &b) noexcept
{
  return &a == &b || a.is_equal(b);
}

inline bool operator!=(const memory_resource &a,
                       const memory_resource &b) noexcept
{
  return !(a == b);
}

memory_resource *new_delete_resource() noexcept;
memory_resource *null_memory_resource() noexcept;
memory_resource *set_default_resource(memory_resource *r) noexcept;
memory_resource *get_default_resource() noexcept;

template <class Tp>
class polymorphic_allocator
{
private:
  memory_resource *memory_rsrc;

public:
  using value_type = Tp;

  // [mem.poly.allocator.ctor], constructors
  polymorphic_allocator() noexcept : memory_rsrc(get_default_resource()) {}
  polymorphic_allocator(memory_resource *r) : memory_rsrc(r)
  {
    FEROLDI_CXX17_MEMORY_RESOURCE_ASSERT(r);
  }

  polymorphic_allocator(const polymorphic_allocator &other) = default;

  template <class U>
  polymorphic_allocator(const polymorphic_allocator<U> &other) noexcept
    : memory_rsrc(other.resource())
  {}

  polymorphic_allocator &operator=(const polymorphic_allocator &rhs) = delete;

  // [mem.poly.allocator.mem], member functions
  [[nodiscard]] Tp *allocate(::std::size_t n);
  {
    return static_cast<Tp *>(memory_rsrc->allocate(n * sizeof(Tp), alignof(T)));
  }

  void deallocate(Tp *p, ::std::size_t n)
  {
    return memory_rsrc->deallocate(p, n * sizeof(Tp), alignof(T));
  }

  template <class T, class... Args>
  void construct(T *p, Args &&... args)
  {
    // http://eel.is/c++draft/allocator.uses#construction
    using Alloc = polymorphic_allocator;
    if constexpr (!::std::uses_allocator_v<T, Alloc> &&
                  ::std::is_constructible_v<T, Args...>)
      ::new (p) T(std::forward<Args>(args)...);
    else if constexpr (::std::uses_allocator_v<T, Alloc> &&
                       ::std::is_constructible_v<T, ::std::allocator_arg_t,
                                                 Alloc, Args...>)
      ::new (p) T(::std::allocator_arg, *this, std::forward<Args>(args)...);
    else if constexpr (::std::uses_allocator_v<T, Alloc> &&
                       ::std::is_constructible_v<T, Args..., Alloc>)
      ::new (p) T(std::forward<Args>(args)..., *this);
    else
      static_assert(
        detail::always_false<T>::value,
        "the request for uses-allocator construction is ill-formed");
  }

  template <class T1, class T2, class... Args1, class... Args2>
  void construct(::std::pair<T1, T2> *p, ::std::piecewise_construct_t,
                 ::std::tuple<Args1...> x, ::std::tuple<Args2...> y);

  template <class T1, class T2>
  void construct(::std::pair<T1, T2> *p);
  template <class T1, class T2, class U, class V>
  void construct(::std::pair<T1, T2> *p, U &&x, V &&y);
  template <class T1, class T2, class U, class V>
  void construct(::std::pair<T1, T2> *p, const ::std::pair<U, V> &pr);
  template <class T1, class T2, class U, class V>
  void construct(::std::pair<T1, T2> *p, ::std::pair<U, V> &&pr);

  template <class T>
  void destroy(T *p)
  {
    p->~T();
  }

  polymorphic_allocator select_on_container_copy_construction() const
  {
    return polymorphic_allocator();
  }

  memory_resource *resource() const { return memory_rsrc; }
}

template <class T1, class T2>
inline bool operator==(const polymorphic_allocator<T1> &a,
                       const polymorphic_allocator<T2> &b) noexcept
{
  return *a.resource() == *b.resource();
}

template <class T1, class T2>
inline bool operator!=(const polymorphic_allocator<T1> &a,
                       const polymorphic_allocator<T2> &b) noexcept
{
  return !(a == b);
}

struct pool_options
{
  size_t max_blocks_per_chunk = 0;
  size_t largest_required_pool_block = 0;
};

class synchronized_pool_resource : public memory_resource
{
public:
  synchronized_pool_resource(const pool_options &opts,
                             memory_resource *upstream);

  synchronized_pool_resource()
    : synchronized_pool_resource(pool_options(), get_default_resource())
  {}
  explicit synchronized_pool_resource(memory_resource *upstream)
    : synchronized_pool_resource(pool_options(), upstream)
  {}
  explicit synchronized_pool_resource(const pool_options &opts)
    : synchronized_pool_resource(opts, get_default_resource())
  {}

  synchronized_pool_resource(const synchronized_pool_resource &) = delete;
  virtual ~synchronized_pool_resource();

  synchronized_pool_resource &
  operator=(const synchronized_pool_resource &) = delete;

  void release();
  memory_resource *upstream_resource() const;
  pool_options options() const;

protected:
  void *do_allocate(::std::size_t bytes, ::std::size_t alignment) override;
  void do_deallocate(void *p, ::std::size_t bytes,
                     ::std::size_t alignment) override;
  bool do_is_equal(const memory_resource &other) const noexcept override;
};

class unsynchronized_pool_resource : public memory_resource
{
public:
  unsynchronized_pool_resource(const pool_options &opts,
                               memory_resource *upstream);

  unsynchronized_pool_resource()
    : unsynchronized_pool_resource(pool_options(), get_default_resource())
  {}
  explicit unsynchronized_pool_resource(memory_resource *upstream)
    : unsynchronized_pool_resource(pool_options(), upstream)
  {}
  explicit unsynchronized_pool_resource(const pool_options &opts)
    : unsynchronized_pool_resource(opts, get_default_resource())
  {}

  unsynchronized_pool_resource(const unsynchronized_pool_resource &) = delete;
  virtual ~unsynchronized_pool_resource();

  unsynchronized_pool_resource &
  operator=(const unsynchronized_pool_resource &) = delete;

  void release();
  memory_resource *upstream_resource() const;
  pool_options options() const;

protected:
  void *do_allocate(::std::size_t bytes, ::std::size_t alignment) override;
  void do_deallocate(void *p, ::std::size_t bytes,
                     ::std::size_t alignment) override;
  bool do_is_equal(const memory_resource &other) const noexcept override;
};

class monotonic_buffer_resource : public memory_resource
{
  memory_resource *upstream_rsrc;
  void *current_buffer;
  ::std::size_t next_buffer_size;

public:
  explicit monotonic_buffer_resource(memory_resource *upstream);
  monotonic_buffer_resource(::std::size_t initial_size,
                            memory_resource *upstream);
  monotonic_buffer_resource(void *buffer, ::std::size_t buffer_size,
                            memory_resource *upstream);

  monotonic_buffer_resource()
    : monotonic_buffer_resource(get_default_resource())
  {}
  explicit monotonic_buffer_resource(::std::size_t initial_size)
    : monotonic_buffer_resource(initial_size, get_default_resource())
  {}
  monotonic_buffer_resource(void *buffer, ::std::size_t buffer_size)
    : monotonic_buffer_resource(buffer, buffer_size, get_default_resource())
  {}

  monotonic_buffer_resource(const monotonic_buffer_resource &) = delete;

  virtual ~monotonic_buffer_resource();

  monotonic_buffer_resource &
  operator=(const monotonic_buffer_resource &) = delete;

  void release();
  memory_resource *upstream_resource() const;

protected:
  void *do_allocate(::std::size_t bytes, ::std::size_t alignment) override;
  void do_deallocate(void *p, ::std::size_t bytes,
                     ::std::size_t alignment) override;

  bool do_is_equal(const memory_resource &other) const noexcept override;
};

} // namespace feroldi::pmr

#endif
