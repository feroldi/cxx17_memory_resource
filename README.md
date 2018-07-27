# C++17's `<memory_resource>` header

This is a work in progress implementation of the `<memory_resource>` header from
C++17. I haven't found any standard library implementation that implemented this
header up until now, and I needed some of its utilities, so that's my motivation
to work on it.

Features implemented so far are:

- [x] Class `memory_resource`
- [x] Class `polymorphic_allocator`
- [ ] Class `synchronized_pool_resource`
- [ ] Class `unsynchronized_pool_resource`
- [x] Class `monotonic_buffer_resource`
- [x] Function `new_delete_resource()`
- [x] Function `null_memory_resource()`
- [x] Function `set_default_resource()`
- [x] Function `get_default_resource()`

## License

Distributed under the [Boost Software License, Version 1.0][1]. See LICENSE.

[1]: http://www.boost.org/LICENSE_1_0.txt
