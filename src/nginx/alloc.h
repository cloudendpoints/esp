/*
 * Copyright (C) Endpoints Server Proxy Authors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef NGINX_NGX_ESP_ALLOC_H_
#define NGINX_NGX_ESP_ALLOC_H_

// Nginx pool-aware allocator support.
//
// This header provides functions for constructing objects in nginx
// pools (placement-new).

#include <string>

extern "C" {
#include "third_party/nginx/src/core/ngx_core.h"
}

inline void *operator new(std::size_t sz, ngx_pool_t *pool) {
  return ngx_pcalloc(pool, sz);
}

inline void *operator new[](std::size_t sz, ngx_pool_t *pool) {
  return ngx_pcalloc(pool, sz);
}

inline void operator delete(void *ptr, ngx_pool_t *pool) {
  ngx_pfree(pool, ptr);
}

inline void operator delete[](void *ptr, ngx_pool_t *pool) {
  ngx_pfree(pool, ptr);
}

namespace google {
namespace api_manager {
namespace nginx {

// An Allocator that allocates memory from an nginx pool.  This may be
// used with any AllocatorAwareContainer.
template <typename T>
class PoolAllocator {
 public:
  PoolAllocator(ngx_pool_t *pool) : pool_(pool) {}

  template <class U>
  PoolAllocator(const PoolAllocator<U> &other) : pool_(other.pool()) {}

  PoolAllocator() : pool_(nullptr) {}  // Required by libg++

  T *allocate(std::size_t count) {
    return static_cast<T *>(ngx_pcalloc(pool_, count * sizeof(T)));
  }

  void deallocate(T *ptr, std::size_t count) {
    ngx_pfree(pool_, static_cast<void *>(ptr));
  }

  ngx_pool_t *pool() const { return pool_; }

  // C++ STL allocator definitions.  Note that some of these are not
  // actually required by the standard, but are required for
  // compatibility with various toolchains.
  typedef T *pointer;
  typedef const T *const_pointer;
  typedef void *void_pointer;
  typedef const void *const_void_pointer;
  typedef T &reference;
  typedef const T &const_reference;
  typedef T value_type;
  typedef std::size_t size_type;
  typedef std::ptrdiff_t difference_type;
  template <class U>
  struct rebind {
    typedef PoolAllocator<U> other;
  };
  typedef std::true_type propagate_on_container_copy_assignment;
  typedef std::true_type propagate_on_container_move_assignment;
  typedef std::true_type propagate_on_container_swap;

 private:
  ngx_pool_t *pool_;
};

template <typename T1, typename T2>
inline bool operator==(const PoolAllocator<T1> &t1,
                       const PoolAllocator<T2> &t2) {
  return t1.pool() == t2.pool();
}

template <typename T1, typename T2>
inline bool operator!=(const PoolAllocator<T1> &t1,
                       const PoolAllocator<T2> &t2) {
  return t1.pool() != t2.pool();
}

// A Deleter for objects allocated from an nginx pool.
class PoolDeleter {
 public:
  PoolDeleter(ngx_pool_t *pool) : pool_(pool) {}

  template <class T>
  void operator()(T *ptr) const {
    if (ptr) {
      ptr->~T();
      ngx_pfree(pool_, static_cast<void *>(ptr));
    }
  }

 private:
  ngx_pool_t *pool_;
};

typedef std::basic_string<char, std::char_traits<char>, PoolAllocator<char>>
    PoolString;

// Runs an object's destructor.  This is useful for destruction of
// arena-allocated objects.
template <typename T>
void CleanupHandler(void *iv) {
  T *it = static_cast<T *>(iv);
  it->~T();
}

// Registers a cleanup handler that runs the destructor of the
// provided object, such that the destructor will be invoked when the
// pool is destroyed (via ngx_destroy_pool()).
//
// If the pointer is nullptr or if the pool is out of memory, nullptr will
// be returned; otherwise, the supplied object will be returned.
template <typename T>
T *RegisterPoolCleanup(ngx_pool_t *pool, T *it) {
  if (!it) {
    return nullptr;
  }
  ngx_pool_cleanup_t *cleanup = ngx_pool_cleanup_add(pool, 0);
  if (!cleanup) {
    return nullptr;
  }
  cleanup->handler = &CleanupHandler<T>;
  cleanup->data = static_cast<void *>(it);
  return it;
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

#endif  // NGINX_NGX_ESP_ALLOC_H_
