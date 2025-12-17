#include <array.hpp>
#include <bitarray.hpp>
#include <hashmap.hpp>
#include <list.hpp>
#include <mpmc_queue.hpp>
#include <string.hpp>

#include <random.hpp>
#include <threads.hpp>

#include <cstdio>
#include <cassert>
#include <algorithm>

#define TEST(name) \
    int test_##name(); \
    int test_##name()

#define RUN_TEST(name) \
    do { \
		printf("=====[%s]=====\n", #name); \
        int result = test_##name(); \
        if (result != 0) { \
            printf("Test %s failed!\n", #name); \
            return 1; \
        } \
		printf("Test %s finished!\n", #name); \
    } while(0)

#define SHOULD_EQUAL(expr, req) { \
	auto res = expr; \
	assert(res == req && "Unexpected result"); \
	if((res) != (req)) { return -1; } \
	printf(#expr": "); \
	printer(res); \
	printf("\n"); \
}

template<typename T>
void printer(const T& value) {
	printf("0x");
	const u8* bytes = (const u8*)(&value);
	for (usize i = 0; i < sizeof(T); ++i) {
		printf("%02x", bytes[i]);
	}
}

template<edge::Character T>
void printer(const T& value) {
	printf("%c", (char)value);
}

template<typename T>
	requires std::same_as<T, bool>
void printer(const T& value) {
	printf("%s", value ? "true" : "false");
}

template<edge::FloatingPoint T>
void printer(const T& value) {
	printf("%g", (double)value);
}

template<edge::SignedArithmetic T>
	requires (!edge::FloatingPoint<T> && !edge::Character<T> && !std::same_as<T, bool>)
void printer(const T& value) {
	if constexpr (sizeof(T) <= sizeof(i32)) {
		printf("%d", (i32)value);
	}
	else {
		printf("%lld", (i64)value);
	}
}

template<edge::UnsignedArithmetic T>
	requires (!edge::Character<T> && !std::same_as<T, bool>)
void printer(const T& value) {
	if constexpr (sizeof(T) <= sizeof(u32)) {
		printf("%u", (u32)value);
	}
	else {
		printf("%llu", (u64)value);
	}
}

template<typename Iterator, typename F>
void for_each(Iterator&& ibegin, Iterator&& iend, F&& func) {
	for (auto it = ibegin; it != iend; ++it) {
		func(*it);
	}
}

template<edge::TrivialType T>
void print_array(const edge::Array<T>& arr) {
	printf("[");
	for_each(edge::begin(arr), edge::end(arr), [first = true](const T& elem) mutable {
		if (!first) printf(", ");
		first = false;
		printer(elem);
		});
	printf("]\n");
}

template<edge::TrivialType T>
void print_list(const edge::List<T>& list) {
	printf("  [");
	for_each(edge::begin(list), edge::end(list), [first = true](const T& elem) mutable {
		if (!first) printf(", ");
		first = false;
		printer(elem);
	});
	printf("]\n");
}

TEST(array_basic) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::Array<i32> arr;

	arr.reserve(&alloc, 10);
	SHOULD_EQUAL(arr.empty(), true);
	SHOULD_EQUAL(arr.push_back(&alloc, 99), true);
	SHOULD_EQUAL(*arr.back(), 99);

	arr.push_back(&alloc, 80);
	arr.push_back(&alloc, 60);

	SHOULD_EQUAL(*arr.front(), 99);
	SHOULD_EQUAL(arr.m_size, 3);
	SHOULD_EQUAL(arr.insert(&alloc, 1, 50), true);
	SHOULD_EQUAL(*arr.get(1), 50);

	auto found = std::find_if(edge::begin(arr), edge::end(arr), [](const i32& val) { return val == 80; });
	SHOULD_EQUAL(*found, 80);

	std::sort(edge::begin(arr), edge::end(arr), [](const i32& a, const i32& b) { return a < b; });
	print_array(arr);
	SHOULD_EQUAL(*arr.front(), 50);

	arr.destroy(&alloc);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(array_resize) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::Array<i32> arr;

	arr.reserve(&alloc, 4);
	SHOULD_EQUAL(arr.m_capacity, 4);

	SHOULD_EQUAL(arr.resize(&alloc, 10), true);
	SHOULD_EQUAL(arr.m_size, 10);

	arr.set(5, 42);
	SHOULD_EQUAL(*arr.get(5), 42);

	arr.destroy(&alloc);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(array_remove) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::Array<i32> arr;

	arr.reserve(&alloc, 5);
	for (i32 i = 0; i < 5; i++) {
		arr.push_back(&alloc, i * 10);
	}

	i32 removed;
	SHOULD_EQUAL(arr.remove(2, &removed), true);
	SHOULD_EQUAL(removed, 20);
	SHOULD_EQUAL(arr.m_size, 4);
	SHOULD_EQUAL(*arr.get(2), 30);

	arr.destroy(&alloc);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(list_basic) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::List<i32> list;

	SHOULD_EQUAL(edge::list_create(&alloc, &list), true);
	SHOULD_EQUAL(edge::list_empty(&list), true);

	SHOULD_EQUAL(edge::list_push_back(&list, 10), true);
	SHOULD_EQUAL(edge::list_push_back(&list, 20), true);
	SHOULD_EQUAL(edge::list_push_back(&list, 30), true);

	SHOULD_EQUAL(*edge::list_front(&list), 10);
	SHOULD_EQUAL(*edge::list_back(&list), 30);
	SHOULD_EQUAL(edge::list_size(&list), 3);

	print_list(list);

	edge::list_destroy(&list);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(list_push_pop) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::List<i32> list;

	edge::list_create(&alloc, &list);

	edge::list_push_front(&list, 5);
	edge::list_push_front(&list, 3);
	edge::list_push_front(&list, 1);
	SHOULD_EQUAL(*edge::list_front(&list), 1);

	i32 val;
	SHOULD_EQUAL(edge::list_pop_front(&list, &val), true);
	SHOULD_EQUAL(val, 1);
	SHOULD_EQUAL(*edge::list_front(&list), 3);

	SHOULD_EQUAL(edge::list_pop_back(&list, &val), true);
	SHOULD_EQUAL(val, 5);
	SHOULD_EQUAL(edge::list_size(&list), 1);

	edge::list_destroy(&list);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(list_insert_remove) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::List<i32> list;

	edge::list_create(&alloc, &list);

	edge::list_push_back(&list, 10);
	edge::list_push_back(&list, 30);
	edge::list_push_back(&list, 40);

	SHOULD_EQUAL(edge::list_insert(&list, 1, 20), true);
	SHOULD_EQUAL(*edge::list_get(&list, 1), 20);
	SHOULD_EQUAL(edge::list_size(&list), 4);

	i32 val;
	SHOULD_EQUAL(edge::list_remove(&list, 1, &val), true);
	SHOULD_EQUAL(val, 20);
	SHOULD_EQUAL(edge::list_size(&list), 3);

	edge::list_destroy(&list);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(list_reverse) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::List<i32> list;

	edge::list_create(&alloc, &list);

	for (i32 i = 1; i <= 5; i++) {
		edge::list_push_back(&list, i);
	}

	printf("  Before reverse: ");
	print_list(list);

	edge::list_reverse(&list);

	printf("  After reverse: ");
	print_list(list);

	SHOULD_EQUAL(*edge::list_front(&list), 5);
	SHOULD_EQUAL(*edge::list_back(&list), 1);

	edge::list_destroy(&list);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(list_sort) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::List<i32> list;

	edge::list_create(&alloc, &list);

	i32 values[] = { 5, 2, 8, 1, 9, 3 };
	for (i32 val : values) {
		edge::list_push_back(&list, val);
	}

	printf("  Before sort: ");
	print_list(list);

	edge::list_sort(&list, [](const i32& a, const i32& b) -> i32 {
		return a - b;
		});

	printf("  After sort: ");
	print_list(list);

	SHOULD_EQUAL(*edge::list_front(&list), 1);
	SHOULD_EQUAL(*edge::list_back(&list), 9);

	edge::list_destroy(&list);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(list_find) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::List<i32> list;

	edge::list_create(&alloc, &list);

	for (i32 i = 10; i <= 50; i += 10) {
		edge::list_push_back(&list, i);
	}

	auto* node = edge::list_find(&list, 30);
	SHOULD_EQUAL(node != nullptr, true);
	SHOULD_EQUAL(node->data, 30);

	auto* not_found = edge::list_find(&list, 99);
	SHOULD_EQUAL(not_found == nullptr, true);

	auto* found_if = edge::list_find_if(&list, [](const i32& val) {
		return val > 35;
		});
	SHOULD_EQUAL(found_if != nullptr, true);
	SHOULD_EQUAL(found_if->data, 40);

	edge::list_destroy(&list);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(hashmap_basic) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::HashMap<i32, i32> map;

	SHOULD_EQUAL(map.create(&alloc, 0), true);
	SHOULD_EQUAL(map.empty(), true);

	SHOULD_EQUAL(map.insert(&alloc, 1, 100), true);
	SHOULD_EQUAL(map.insert(&alloc, 2, 200), true);
	SHOULD_EQUAL(map.insert(&alloc, 3, 300), true);

	SHOULD_EQUAL(map.m_size, 3);

	i32* val = map.get(2);
	SHOULD_EQUAL(val != nullptr, true);
	SHOULD_EQUAL(*val, 200);

	map.destroy(&alloc);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(hashmap_update) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::HashMap<i32, i32> map;

	map.create(&alloc, 0);

	map.insert(&alloc, 5, 50);
	SHOULD_EQUAL(*map.get(5), 50);

	// Update existing key
	map.insert(&alloc, 5, 500);
	SHOULD_EQUAL(*map.get(5), 500);
	SHOULD_EQUAL(map.m_size, 1);

	map.destroy(&alloc);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(hashmap_remove) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::HashMap<i32, i32> map;

	map.create(&alloc, 0);

	map.insert(&alloc, 10, 100);
	map.insert(&alloc, 20, 200);
	map.insert(&alloc, 30, 300);

	i32 removed_val;
	SHOULD_EQUAL(map.remove(&alloc, 20, &removed_val), true);
	SHOULD_EQUAL(removed_val, 200);
	SHOULD_EQUAL(map.m_size, 2);

	SHOULD_EQUAL(map.contains(20), false);
	SHOULD_EQUAL(map.contains(10), true);

	map.destroy(&alloc);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(hashmap_iteration) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::HashMap<i32, i32> map;

	map.create(&alloc, 5);

	for (i32 i = 0; i < 5; i++) {
		map.insert(&alloc, i, i * 10);
	}

	printf("  HashMap entries: ");
	i32 count = 0;
	for (auto& entry : map) {
		printf("[%d->%d] ", entry.key, entry.value);
		count++;
	}
	printf("\n");

	SHOULD_EQUAL(count, 5);

	map.destroy(&alloc);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(hashmap_rehash) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::HashMap<i32, i32> map;

	map.create(&alloc, 4);

	// Insert enough elements to trigger rehash
	for (i32 i = 0; i < 10; i++) {
		map.insert(&alloc, i, i * 100);
	}

	SHOULD_EQUAL(map.m_size, 10);

	// Verify all elements are still accessible
	for (i32 i = 0; i < 10; i++) {
		i32* val = map.get(i);
		SHOULD_EQUAL(val != nullptr, true);
		SHOULD_EQUAL(*val, i * 100);
	}

	map.destroy(&alloc);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(hashmap_clear) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::HashMap<i32, i32> map;

	map.create(&alloc, 5);

	for (i32 i = 0; i < 5; i++) {
		map.insert(&alloc, i, i);
	}

	SHOULD_EQUAL(map.m_size, 5);

	map.clear(&alloc);

	SHOULD_EQUAL(map.empty(), true);
	SHOULD_EQUAL(map.m_size, 0);

	map.destroy(&alloc);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(bitarray_basic) {
	edge::BitArray<64> arr = {};

	arr.clear_all();
	SHOULD_EQUAL(arr.get(0), false);

	arr.set(5);
	SHOULD_EQUAL(arr.get(5), true);

	arr.clear(5);
	SHOULD_EQUAL(arr.get(5), false);

	arr.toggle(10);
	SHOULD_EQUAL(arr.get(10), true);

	arr.toggle(10);
	SHOULD_EQUAL(arr.get(10), false);

	return 0;
}

TEST(bitarray_put) {
	edge::BitArray<32> arr = {};

	arr.clear_all();

	arr.put(0, true);
	arr.put(1, false);
	arr.put(2, true);

	SHOULD_EQUAL(arr.get(0), true);
	SHOULD_EQUAL(arr.get(1), false);
	SHOULD_EQUAL(arr.get(2), true);

	return 0;
}

TEST(bitarray_set_all) {
	edge::BitArray<16> arr = {};

	arr.set_all();

	for (i32 i = 0; i < 16; i++) {
		SHOULD_EQUAL(arr.get(i), true);
	}

	return 0;
}

TEST(bitarray_count) {
	edge::BitArray<32> arr = {};

	arr.clear_all();
	arr.set(0);
	arr.set(5);
	arr.set(10);
	arr.set(15);

	SHOULD_EQUAL(arr.count_set(), 4);

	return 0;
}

TEST(bitarray_find_first) {
	edge::BitArray<64> arr = {};

	arr.clear_all();
	SHOULD_EQUAL(arr.find_first_set(), -1);

	arr.set(20);
	SHOULD_EQUAL(arr.find_first_set(), 20);

	arr.set(5);
	SHOULD_EQUAL(arr.find_first_set(), 5);

	return 0;
}

TEST(bitarray_any_all) {
	edge::BitArray<32> arr = {};

	arr.clear_all();
	SHOULD_EQUAL(arr.any_set(), false);
	SHOULD_EQUAL(arr.all_clear(), true);

	arr.set(10);
	SHOULD_EQUAL(arr.any_set(), true);
	SHOULD_EQUAL(arr.all_clear(), false);

	return 0;
}

TEST(mpmc_queue_basic) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::MPMCQueue<i32> queue;

	SHOULD_EQUAL(edge::mpmc_queue_create(&alloc, &queue, 8), true);
	SHOULD_EQUAL(edge::mpmc_queue_empty_approx(&queue), true);
	SHOULD_EQUAL(edge::mpmc_queue_capacity(&queue), 8);

	SHOULD_EQUAL(edge::mpmc_queue_enqueue(&queue, 10), true);
	SHOULD_EQUAL(edge::mpmc_queue_enqueue(&queue, 20), true);
	SHOULD_EQUAL(edge::mpmc_queue_enqueue(&queue, 30), true);

	SHOULD_EQUAL(edge::mpmc_queue_size_approx(&queue), 3);

	i32 val;
	SHOULD_EQUAL(edge::mpmc_queue_dequeue(&queue, &val), true);
	SHOULD_EQUAL(val, 10);

	SHOULD_EQUAL(edge::mpmc_queue_dequeue(&queue, &val), true);
	SHOULD_EQUAL(val, 20);

	SHOULD_EQUAL(edge::mpmc_queue_size_approx(&queue), 1);

	edge::mpmc_queue_destroy(&queue);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(mpmc_queue_full) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::MPMCQueue<i32> queue;

	edge::mpmc_queue_create(&alloc, &queue, 4);

	// Fill the queue
	for (i32 i = 0; i < 4; i++) {
		SHOULD_EQUAL(edge::mpmc_queue_enqueue(&queue, i), true);
	}

	SHOULD_EQUAL(edge::mpmc_queue_full_approx(&queue), true);

	// Try to enqueue to full queue should fail
	SHOULD_EQUAL(edge::mpmc_queue_enqueue(&queue, 100), false);

	// Dequeue one element
	i32 val;
	edge::mpmc_queue_dequeue(&queue, &val);

	// Now we should be able to enqueue again
	SHOULD_EQUAL(edge::mpmc_queue_enqueue(&queue, 200), true);

	edge::mpmc_queue_destroy(&queue);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

TEST(mpmc_queue_try_operations) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::MPMCQueue<i32> queue;

	edge::mpmc_queue_create(&alloc, &queue, 4);

	SHOULD_EQUAL(edge::mpmc_queue_try_enqueue(&queue, 100, 10), true);
	SHOULD_EQUAL(edge::mpmc_queue_try_enqueue(&queue, 200, 10), true);

	i32 val;
	SHOULD_EQUAL(edge::mpmc_queue_try_dequeue(&queue, &val, 10), true);
	SHOULD_EQUAL(val, 100);

	SHOULD_EQUAL(edge::mpmc_queue_try_dequeue(&queue, &val, 10), true);
	SHOULD_EQUAL(val, 200);

	// Should fail on empty queue
	SHOULD_EQUAL(edge::mpmc_queue_try_dequeue(&queue, &val, 1), false);

	edge::mpmc_queue_destroy(&queue);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

struct ProducerArgs {
	edge::MPMCQueue<i32>* queue;
	i32 producer_id;
	i32 items_count;
};

struct ConsumerArgs {
	edge::MPMCQueue<i32>* queue;
	std::atomic<i32>* consumed_count;
	i32 total_items;
};

i32 producer_thread(void* arg) {
	ProducerArgs* args = (ProducerArgs*)arg;

	for (i32 i = 0; i < args->items_count; i++) {
		while (!edge::mpmc_queue_enqueue(args->queue, args->producer_id * 1000 + i)) {
			// Retry on failure
			edge::thread_yield();
		}
	}

	return 0;
}

i32 consumer_thread(void* arg) {
	ConsumerArgs* args = (ConsumerArgs*)arg;

	i32 val;
	for (;;) {
		if (edge::mpmc_queue_dequeue(args->queue, &val)) {
			args->consumed_count->fetch_add(1);
		}
		else {
			// Small delay before retry
			edge::thread_yield();

			// Check if we should exit (all items produced)
			if (args->consumed_count->load() >= args->total_items) {
				break;
			}
		}
	}

	return 0;
}

TEST(mpmc_queue_multithreaded) {
	edge::Allocator alloc = edge::Allocator::create_tracking();
	edge::MPMCQueue<i32> queue;

	edge::mpmc_queue_create(&alloc, &queue, 1024);

	const i32 num_producers = 2;
	const i32 num_consumers = 2;
	const i32 items_per_producer = 100;
	const i32 total_items = num_producers * items_per_producer;

	edge::Array<edge::Thread> threads;
	threads.reserve(&alloc, num_producers + num_consumers);

	edge::Array<ProducerArgs> producer_args;
	producer_args.reserve(&alloc, num_producers);

	edge::Array<ConsumerArgs> consumer_args;
	consumer_args.reserve(&alloc, num_consumers);

	std::atomic<i32> consumed_count{ 0 };

	// Create producer threads
	for (i32 p = 0; p < num_producers; p++) {
		ProducerArgs args;
		args.queue = &queue;
		args.producer_id = p;
		args.items_count = items_per_producer;
		producer_args.push_back(&alloc, args);

		edge::Thread thr;
		edge::thread_create(&thr, producer_thread, producer_args.get(p));
		threads.push_back(&alloc, thr);
	}

	// Create consumer threads
	for (i32 c = 0; c < num_consumers; c++) {
		ConsumerArgs args;
		args.queue = &queue;
		args.consumed_count = &consumed_count;
		args.total_items = total_items;
		consumer_args.push_back(&alloc, args);

		edge::Thread thr;
		edge::thread_create(&thr, consumer_thread, consumer_args.get(c));
		threads.push_back(&alloc, thr);
	}

	// Join all threads
	for (usize i = 0; i < threads.m_size; i++) {
		edge::thread_join(*threads.get(i));
	}

	printf("  Consumed %d items\n", consumed_count.load());
	SHOULD_EQUAL(consumed_count.load(), total_items);

	consumer_args.destroy(&alloc);
	producer_args.destroy(&alloc);
	threads.destroy(&alloc);
	edge::mpmc_queue_destroy(&queue);
	SHOULD_EQUAL(alloc.get_net(), 0ull);
	return 0;
}

int main(void) {
	RUN_TEST(array_basic);
	RUN_TEST(array_resize);
	RUN_TEST(array_remove);

	RUN_TEST(list_basic);
	RUN_TEST(list_push_pop);
	RUN_TEST(list_insert_remove);
	RUN_TEST(list_reverse);
	RUN_TEST(list_sort);
	RUN_TEST(list_find);

	RUN_TEST(hashmap_basic);
	RUN_TEST(hashmap_update);
	RUN_TEST(hashmap_remove);
	RUN_TEST(hashmap_iteration);
	RUN_TEST(hashmap_rehash);
	RUN_TEST(hashmap_clear);

	RUN_TEST(bitarray_basic);
	RUN_TEST(bitarray_put);
	RUN_TEST(bitarray_set_all);
	RUN_TEST(bitarray_count);
	RUN_TEST(bitarray_find_first);
	RUN_TEST(bitarray_any_all);

	RUN_TEST(mpmc_queue_basic);
	RUN_TEST(mpmc_queue_full);
	RUN_TEST(mpmc_queue_try_operations);
	RUN_TEST(mpmc_queue_multithreaded);

	return 0;
}