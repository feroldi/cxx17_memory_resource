// Copyright (c) 2018 Mário Feroldi
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at
// http://www.boost.org/LICENSE_1_0.txt

#ifndef FEROLDI_CXX17_MEMORY_RESOURCE
#define FEROLDI_CXX17_MEMORY_RESOURCE

#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

/*
memory_resource synopsis
namespace std::pmr {
  // [mem.res.class], class memory_­resource
  class memory_resource;

  bool operator==(const memory_resource& a, const memory_resource& b)
noexcept; bool operator!=(const memory_resource& a, const
memory_resource& b) noexcept;

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

// [mem.res.class], class memory_resource
class memory_resource
{
  static constexpr std::size_t _max_align = alignof(std::max_align_t);

public:
  // [mem.res.public], public member functions
  memory_resource() = default;
  memory_resource(const memory_resource &) = default;
  virtual ~memory_resource() = default;

  memory_resource &operator=(const memory_resource &) = default;

  [[nodiscard]] void *allocate(std::size_t bytes,
                               std::size_t alignment = _max_align) {
    return do_allocate(bytes, alignment);
  }

  void deallocate(void *p, std::size_t bytes,
                  std::size_t alignment = _max_align)
  {
    return do_deallocate(p, bytes, alignment);
  }

  bool is_equal(const memory_resource &other) const noexcept
  {
    return do_is_equal(other);
  }

private:
  // [mem.res.private], private member functions
  virtual void *do_allocate(std::size_t bytes, std::size_t alignment) = 0;
  virtual void do_deallocate(void *p, std::size_t bytes,
                             std::size_t alignment) = 0;
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
  memory_resource *res;

public:
  using value_type = Tp;

  // [mem.poly.allocator.ctor], constructors
  polymorphic_allocator() noexcept : res(get_default_resource()) {}
  polymorphic_allocator(memory_resource *r) : res(r) { assert(r); }

  polymorphic_allocator(const polymorphic_allocator &) = default;

  template <class U>
  polymorphic_allocator(const polymorphic_allocator<U> &other) noexcept
    : res(other.resource())
  {}

  polymorphic_allocator &operator=(const polymorphic_allocator &) = delete;

  // [mem.poly.allocator.mem], member functions
  [[nodiscard]] Tp *allocate(std::size_t n) {
    return static_cast<Tp *>(res->allocate(n * sizeof(Tp), alignof(Tp)));
  }

  void deallocate(Tp *p, std::size_t n)
  {
    return res->deallocate(p, n * sizeof(Tp), alignof(Tp));
  }

  template <class T, class... Args>
  void construct(T *p, Args &&... args)
  {
    using uses_alloc_tag =
      uses_alloc_ctor_t<T, polymorphic_allocator &, Args...>;
    return _construct(uses_alloc_tag(), p, std::forward<Args>(args)...);
  }

  template <class T1, class T2, class... Args1, class... Args2>
  void construct(std::pair<T1, T2> *p, std::piecewise_construct_t,
                 std::tuple<Args1...> x, std::tuple<Args2...> y)
  {
    using x_uses_alloc_tag =
      uses_alloc_ctor_t<T1, polymorphic_allocator &, Args1...>;
    using y_uses_alloc_tag =
      uses_alloc_ctor_t<T2, polymorphic_allocator &, Args2...>;

    ::new (p) std::pair<T1, T2>(
      std::piecewise_construct,
      _construct_p(x_uses_alloc_tag(), std::forward<Args1>(x)...),
      _construct_p(y_uses_alloc_tag(), std::forward<Args2>(y)...));
  }

  template <class T1, class T2>
  void construct(std::pair<T1, T2> *p)
  {
    return construct(p, std::piecewise_construct, std::tuple(), std::tuple());
  }

  template <class T1, class T2, class U, class V>
  void construct(std::pair<T1, T2> *p, U &&x, V &&y)
  {
    return construct(p, std::forward_as_tuple(std::forward<U>(x)),
                     std::forward_as_tuple(std::forward<V>(y)));
  }

  template <class T1, class T2, class U, class V>
  void construct(std::pair<T1, T2> *p, const std::pair<U, V> &pr)
  {
    return construct(p, std::forward_as_tuple(pr.first),
                     std::forward_as_tuple(pr.second));
  }

  template <class T1, class T2, class U, class V>
  void construct(std::pair<T1, T2> *p, std::pair<U, V> &&pr)
  {
    return construct(p, std::piecewise_construct,
                     std::forward_as_tuple(std::forward<U>(pr.first)),
                     std::forward_as_tuple(std::forward<V>(pr.second)));
  }

  template <class T>
  void destroy(T *p)
  {
    p->~T();
  }

  polymorphic_allocator select_on_container_copy_construction() const
  {
    return polymorphic_allocator();
  }

  memory_resource *resource() const { return res; }

private:
  template <bool UsesAlloc, typename T, typename Alloc, typename... Args>
  struct uses_alloc_ctor_impl
  {
    static const int value = 0;
  };

  template <typename T, typename Alloc, typename... Args>
  struct uses_alloc_ctor_impl<true, T, Alloc, Args...>
  {
    static const bool first_ctor =
      std::is_constructible_v<T, std::allocator_arg_t, Alloc, Args...>;
    static const bool second_ctor =
      std::conditional_t<first_ctor, std::false_type,
                         std::is_constructible<T, Args..., Alloc>>::value;

    static_assert(first_ctor || second_ctor,
                  " request for uses-allocator construction is ill-formed");

    static const int value = first_ctor ? 1 : 2;
  };

  // FIXME: std::uses_allocator might not consider std::erased_type for
  // libraries that don't implement it.
  template <typename T, typename Alloc, typename... Args>
  struct uses_alloc_ctor
  {
    using type = std::integral_constant<
      int, uses_alloc_ctor_impl<std::uses_allocator_v<T, Alloc>, T, Alloc,
                                Args...>::value>;
  };

  template <typename T, typename Alloc, typename... Args>
  using uses_alloc_ctor_t = typename uses_alloc_ctor<T, Alloc, Args...>::type;

  using uses_alloc0 = std::integral_constant<int, 0>;
  using uses_alloc1 = std::integral_constant<int, 1>;
  using uses_alloc2 = std::integral_constant<int, 2>;

  template <typename T, typename... Args>
  void _construct(uses_alloc0, T *storage, Args &&... args)
  {
    ::new (storage) T(std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  void _construct(uses_alloc1, T *storage, Args &&... args)
  {
    ::new (storage) T(std::allocator_arg, *this, std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  void _construct(uses_alloc2, T *storage, Args &&... args)
  {
    ::new (storage) T(std::forward<Args>(args)..., *this);
  }

  // Piecewise construction.

  template <typename Tuple>
  Tuple &&_construct_p(uses_alloc0, Tuple &t)
  {
    return std::move(t);
  }

  template <typename... Args>
  decltype(auto) _construct_p(uses_alloc1, std::tuple<Args...> &t)
  {
    return std::tuple_cat(std::tuple(std::allocator_arg, *this), std::move(t));
  }

  template <typename... Args>
  decltype(auto) _construct_p(uses_alloc2, std::tuple<Args...> &t)
  {
    return std::tuple_cat(std::move(t), std::tuple(*this));
  }
};

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

// TODO: synchronized_pool_resource
// TODO: unsynchronized_pool_resource
// TODO: monotonic_buffer_resource
#if 0
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
  void *do_allocate(std::size_t bytes, std::size_t alignment) override;
  void do_deallocate(void *p, std::size_t bytes,
                     std::size_t alignment) override;
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
  void *do_allocate(std::size_t bytes, std::size_t alignment) override;
  void do_deallocate(void *p, std::size_t bytes,
                     std::size_t alignment) override;
  bool do_is_equal(const memory_resource &other) const noexcept override;
};
#endif

class monotonic_buffer_resource : public memory_resource
{
  memory_resource *upstream;
  void *current_buffer;
  std::size_t next_buffer_size;

public:
  explicit monotonic_buffer_resource(memory_resource *upstream);
  monotonic_buffer_resource(std::size_t initial_size,
                            memory_resource *upstream);
  monotonic_buffer_resource(void *buffer, std::size_t buffer_size,
                            memory_resource *upstream);

  monotonic_buffer_resource()
    : monotonic_buffer_resource(get_default_resource())
  {}
  explicit monotonic_buffer_resource(std::size_t initial_size)
    : monotonic_buffer_resource(initial_size, get_default_resource())
  {}
  monotonic_buffer_resource(void *buffer, std::size_t buffer_size)
    : monotonic_buffer_resource(buffer, buffer_size, get_default_resource())
  {}

  monotonic_buffer_resource(const monotonic_buffer_resource &) = delete;

  virtual ~monotonic_buffer_resource();

  monotonic_buffer_resource &
  operator=(const monotonic_buffer_resource &) = delete;

  void release();
  memory_resource *upstream_resource() const { return upstream; }

protected:
  void *do_allocate(std::size_t bytes, std::size_t alignment) override;
  void do_deallocate(void *p, std::size_t bytes,
                     std::size_t alignment) override;

  bool do_is_equal(const memory_resource &other) const noexcept override;
};

inline memory_resource *new_delete_resource() noexcept
{
  struct : memory_resource
  {
    void *do_allocate(std::size_t bytes, std::size_t alignment) override
    {
      if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
      {
        std::align_val_t al = std::align_val_t(alignment);
        return ::operator new(bytes, al);
      }
      return ::operator new(bytes);
    }

    void do_deallocate(void *p, std::size_t,
                       std::size_t alignment) noexcept override
    {
      if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
      {
        std::align_val_t al = std::align_val_t(alignment);
        return ::operator delete(p, al);
      }
      return ::operator delete(p);
    }

    bool do_is_equal(const memory_resource &other) const noexcept override
    {
      return this == &other;
    }
  } static r;
  return &r;
}

inline memory_resource *null_memory_resource() noexcept
{
  struct : memory_resource
  {
    void *do_allocate(std::size_t, std::size_t) override
    {
      throw std::bad_alloc();
    }

    void do_deallocate(void *, std::size_t, std::size_t) noexcept override {}

    bool do_is_equal(const memory_resource &other) const noexcept override
    {
      return this == &other;
    }
  } static r;
  return &r;
}

namespace detail {
std::atomic<memory_resource *> &get_default_resource_impl() noexcept
{
  static std::atomic<memory_resource *> r(new_delete_resource());
  return r;
}
} // namespace detail

inline memory_resource *get_default_resource() noexcept
{
  return detail::get_default_resource_impl().load();
}

inline memory_resource *set_default_resource(memory_resource *r) noexcept
{
  if (!r)
    r = new_delete_resource();
  return detail::get_default_resource_impl().exchange(r);
}

} // namespace feroldi::pmr
#endif
