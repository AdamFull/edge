#ifndef EDGE_MPMC_QUEUE_H
#define EDGE_MPMC_QUEUE_H

#include "allocator.hpp"
#include "math.hpp"

#include <atomic>
#include <cstring>

namespace edge {
	template<TrivialType T>
	struct alignas(64) MPMCNode {
		std::atomic<usize> sequence;
		T data;
	};

	template<TrivialType T>
	struct MPMCQueue {
		MPMCNode<T>* m_buffer = nullptr;
		usize m_capacity = 0ull;
		usize m_mask = 0ull;
		alignas(64) std::atomic<usize> m_enqueue_pos = 0ull;
		alignas(64) std::atomic<usize> m_dequeue_pos = 0ull;
		alignas(64) const Allocator* m_allocator = nullptr;
	};

	template<TrivialType T>
	bool mpmc_queue_create(const Allocator* alloc, MPMCQueue<T>* queue, usize capacity) {
		if (!alloc || !queue || capacity == 0) {
			return false;
		}

		if (!is_pow2(capacity)) {
			capacity = next_pow2(capacity);
		}

		if (capacity > SIZE_MAX / 2) {
			return false;
		}

		queue->m_buffer = allocate_array<MPMCNode<T>>(alloc, capacity);
		if (!queue->m_buffer) {
			return false;
		}

		for (usize i = 0; i < capacity; i++) {
			queue->m_buffer[i].sequence.store(i, std::memory_order_relaxed);
		}

		queue->m_capacity = capacity;
		queue->m_mask = capacity - 1;
		queue->m_allocator = alloc;

		queue->m_enqueue_pos.store(0, std::memory_order_relaxed);
		queue->m_dequeue_pos.store(0, std::memory_order_relaxed);

		return true;
	}

	template<TrivialType T>
	void mpmc_queue_destroy(MPMCQueue<T>* queue) {
		if (!queue) {
			return;
		}

		if (queue->m_buffer) {
			deallocate_array(queue->m_allocator, queue->m_buffer, queue->m_capacity);
		}
	}

	template<TrivialType T>
	bool mpmc_queue_enqueue(MPMCQueue<T>* queue, const T& element) {
		if (!queue) {
			return false;
		}

		usize pos;
		MPMCNode<T>* node;
		usize seq;

		for (;;) {
			pos = queue->m_enqueue_pos.load(std::memory_order_relaxed);
			node = &queue->m_buffer[pos & queue->m_mask];
			seq = node->sequence.load(std::memory_order_acquire);

			isize diff = (isize)seq - (isize)pos;

			if (diff == 0) {
				if (queue->m_enqueue_pos.compare_exchange_weak(pos, pos + 1,
					std::memory_order_relaxed,
					std::memory_order_relaxed)) {
					break;
				}
			}
			else if (diff < 0) {
				return false;
			}
		}

		node->data = element;
		node->sequence.store(pos + 1, std::memory_order_release);

		return true;
	}

	template<TrivialType T>
	bool mpmc_queue_dequeue(MPMCQueue<T>* queue, T* out_element) {
		if (!queue) {
			return false;
		}

		usize pos;
		MPMCNode<T>* node;
		usize seq;

		for (;;) {
			pos = queue->m_dequeue_pos.load(std::memory_order_relaxed);
			node = &queue->m_buffer[pos & queue->m_mask];
			seq = node->sequence.load(std::memory_order_acquire);

			isize diff = (isize)seq - (isize)(pos + 1);

			if (diff == 0) {
				if (queue->m_dequeue_pos.compare_exchange_weak(pos, pos + 1,
					std::memory_order_relaxed,
					std::memory_order_relaxed)) {
					break;
				}
			}
			else if (diff < 0) {
				return false;
			}
		}

		if (out_element) {
			*out_element = node->data;
		}

		node->sequence.store(pos + queue->m_mask + 1, std::memory_order_release);

		return true;
	}

	template<TrivialType T>
	bool mpmc_queue_try_enqueue(MPMCQueue<T>* queue, const T& element, usize max_retries) {
		if (!queue) {
			return false;
		}

		usize pos;
		MPMCNode<T>* node;
		usize seq;
		usize retries = 0;

		for (;;) {
			if (retries >= max_retries) {
				return false;
			}

			pos = queue->m_enqueue_pos.load(std::memory_order_relaxed);
			node = &queue->m_buffer[pos & queue->m_mask];
			seq = node->sequence.load(std::memory_order_acquire);

			isize diff = (isize)seq - (isize)pos;

			if (diff == 0) {
				if (queue->m_enqueue_pos.compare_exchange_weak(pos, pos + 1,
					std::memory_order_relaxed,
					std::memory_order_relaxed)) {
					break;
				}
				retries++;
			}
			else if (diff < 0) {
				return false;
			}
			else {
				retries++;
			}
		}

		node->data = element;
		node->sequence.store(pos + 1, std::memory_order_release);

		return true;
	}

	template<TrivialType T>
	bool mpmc_queue_try_dequeue(MPMCQueue<T>* queue, T* out_element, usize max_retries) {
		if (!queue) {
			return false;
		}

		usize pos;
		MPMCNode<T>* node;
		usize seq;
		usize retries = 0;

		for (;;) {
			if (retries >= max_retries) {
				return false;
			}

			pos = queue->m_dequeue_pos.load(std::memory_order_relaxed);
			node = &queue->m_buffer[pos & queue->m_mask];
			seq = node->sequence.load(std::memory_order_acquire);

			isize diff = (isize)seq - (isize)(pos + 1);

			if (diff == 0) {
				if (queue->m_dequeue_pos.compare_exchange_weak(pos, pos + 1,
					std::memory_order_relaxed,
					std::memory_order_relaxed)) {
					break;
				}
				retries++;
			}
			else if (diff < 0) {
				return false;
			}
			else {
				retries++;
			}
		}

		if (out_element) {
			*out_element = node->data;
		}

		node->sequence.store(pos + queue->m_mask + 1, std::memory_order_release);

		return true;
	}

	template<TrivialType T>
	usize mpmc_queue_size_approx(const MPMCQueue<T>* queue) {
		if (!queue) {
			return 0;
		}

		usize enqueue = queue->m_enqueue_pos.load(std::memory_order_relaxed);
		usize dequeue = queue->m_dequeue_pos.load(std::memory_order_relaxed);

		if (enqueue >= dequeue) {
			return enqueue - dequeue;
		}
		else {
			return (SIZE_MAX - dequeue) + enqueue + 1;
		}
	}

	template<TrivialType T>
	usize mpmc_queue_capacity(const MPMCQueue<T>* queue) {
		return queue ? queue->m_capacity : 0;
	}

	template<TrivialType T>
	bool mpmc_queue_empty_approx(const MPMCQueue<T>* queue) {
		if (!queue) {
			return true;
		}

		return mpmc_queue_size_approx(queue) == 0;
	}

	template<TrivialType T>
	bool mpmc_queue_full_approx(const MPMCQueue<T>* queue) {
		if (!queue) {
			return false;
		}

		return mpmc_queue_size_approx(queue) >= queue->m_capacity;
	}
}

#endif