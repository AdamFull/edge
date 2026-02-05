#ifndef EDGE_MPMC_QUEUE_H
#define EDGE_MPMC_QUEUE_H

#include "allocator.hpp"
#include "math.hpp"

#include <atomic>
#include <cstring>

namespace edge {
template <TrivialType T> struct alignas(64) MPMCNode {
  std::atomic<usize> sequence;
  T data;
};

template <TrivialType T> struct MPMCQueue {
  MPMCNode<T> *m_buffer = nullptr;
  usize m_capacity = 0ull;
  usize m_mask = 0ull;
  alignas(64) std::atomic<usize> m_enqueue_pos = 0ull;
  alignas(64) std::atomic<usize> m_dequeue_pos = 0ull;

  struct Iterator {
    MPMCQueue<T> *queue;
    usize index;
    usize end_index;

    Iterator(MPMCQueue<T> *q, usize idx, usize end)
        : queue(q), index(idx), end_index(end) {}

    T &operator*() { return queue->m_buffer[index & queue->m_mask].data; }

    Iterator &operator++() {
      ++index;
      return *this;
    }

    bool operator!=(const Iterator &other) const {
      return index != other.index;
    }
  };

  bool create(NotNull<const Allocator *> alloc, usize capacity) {
    if (capacity == 0) {
      return false;
    }

    if (!is_pow2(capacity)) {
      capacity = next_pow2(capacity);
    }

    if (capacity > SIZE_MAX / 2) {
      return false;
    }

    m_buffer = alloc->allocate_array<MPMCNode<T>>(capacity);
    if (!m_buffer) {
      return false;
    }

    for (usize i = 0; i < capacity; i++) {
      m_buffer[i].sequence.store(i, std::memory_order_relaxed);
    }

    m_capacity = capacity;
    m_mask = capacity - 1;

    m_enqueue_pos.store(0, std::memory_order_relaxed);
    m_dequeue_pos.store(0, std::memory_order_relaxed);

    return true;
  }

  void destroy(NotNull<const Allocator *> alloc) {
    if (m_buffer) {
      alloc->deallocate_array(m_buffer, m_capacity);
    }
  }

  bool enqueue(const T &element) {
    usize pos;
    MPMCNode<T> *node;
    usize seq;

    for (;;) {
      pos = m_enqueue_pos.load(std::memory_order_relaxed);
      node = &m_buffer[pos & m_mask];
      seq = node->sequence.load(std::memory_order_acquire);

      isize diff = (isize)seq - (isize)pos;

      if (diff == 0) {
        if (m_enqueue_pos.compare_exchange_weak(pos, pos + 1,
                                                std::memory_order_relaxed,
                                                std::memory_order_relaxed)) {
          break;
        }
      } else if (diff < 0) {
        return false;
      }
    }

    node->data = element;
    node->sequence.store(pos + 1, std::memory_order_release);

    return true;
  }

  bool dequeue(T *out_element) {
    usize pos;
    MPMCNode<T> *node;
    usize seq;

    for (;;) {
      pos = m_dequeue_pos.load(std::memory_order_relaxed);
      node = &m_buffer[pos & m_mask];
      seq = node->sequence.load(std::memory_order_acquire);

      isize diff = (isize)seq - (isize)(pos + 1);

      if (diff == 0) {
        if (m_dequeue_pos.compare_exchange_weak(pos, pos + 1,
                                                std::memory_order_relaxed,
                                                std::memory_order_relaxed)) {
          break;
        }
      } else if (diff < 0) {
        return false;
      }
    }

    if (out_element) {
      *out_element = node->data;
    }

    node->sequence.store(pos + m_mask + 1, std::memory_order_release);

    return true;
  }

  bool try_enqueue(const T &element, usize max_retries) {
    usize pos;
    MPMCNode<T> *node;
    usize seq;
    usize retries = 0;

    for (;;) {
      if (retries >= max_retries) {
        return false;
      }

      pos = m_enqueue_pos.load(std::memory_order_relaxed);
      node = &m_buffer[pos & m_mask];
      seq = node->sequence.load(std::memory_order_acquire);

      isize diff = (isize)seq - (isize)pos;

      if (diff == 0) {
        if (m_enqueue_pos.compare_exchange_weak(pos, pos + 1,
                                                std::memory_order_relaxed,
                                                std::memory_order_relaxed)) {
          break;
        }
        retries++;
      } else if (diff < 0) {
        return false;
      } else {
        retries++;
      }
    }

    node->data = element;
    node->sequence.store(pos + 1, std::memory_order_release);

    return true;
  }

  bool try_dequeue(T *out_element, usize max_retries) {
    usize pos;
    MPMCNode<T> *node;
    usize seq;
    usize retries = 0;

    for (;;) {
      if (retries >= max_retries) {
        return false;
      }

      pos = m_dequeue_pos.load(std::memory_order_relaxed);
      node = &m_buffer[pos & m_mask];
      seq = node->sequence.load(std::memory_order_acquire);

      isize diff = (isize)seq - (isize)(pos + 1);

      if (diff == 0) {
        if (m_dequeue_pos.compare_exchange_weak(pos, pos + 1,
                                                std::memory_order_relaxed,
                                                std::memory_order_relaxed)) {
          break;
        }
        retries++;
      } else if (diff < 0) {
        return false;
      } else {
        retries++;
      }
    }

    if (out_element) {
      *out_element = node->data;
    }

    node->sequence.store(pos + m_mask + 1, std::memory_order_release);

    return true;
  }

  usize size_approx() const {
    usize enqueue = m_enqueue_pos.load(std::memory_order_relaxed);
    usize dequeue = m_dequeue_pos.load(std::memory_order_relaxed);

    if (enqueue >= dequeue) {
      return enqueue - dequeue;
    }
    return (SIZE_MAX - dequeue) + enqueue + 1;
  }

  usize capacity() const { return m_capacity; }

  bool empty_approx() const { return size_approx() == 0; }

  bool full_approx() const { return size_approx() >= m_capacity; }

  Iterator begin() {
    usize dequeue = m_dequeue_pos.load(std::memory_order_relaxed);
    usize enqueue = m_enqueue_pos.load(std::memory_order_relaxed);
    return {this, dequeue, enqueue};
  }

  Iterator end() {
    usize enqueue = m_enqueue_pos.load(std::memory_order_relaxed);
    return {this, enqueue, enqueue};
  }
};
} // namespace edge

#endif