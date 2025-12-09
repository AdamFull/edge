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
	edge::Allocator alloc = edge::allocator_create_tracking();
	edge::Array<i32> arr;

	SHOULD_EQUAL(edge::array_create(&alloc, &arr, 0), true);
	SHOULD_EQUAL(edge::array_empty(&arr), true);
	SHOULD_EQUAL(edge::array_push_back(&arr, 99), true);
	SHOULD_EQUAL(*edge::array_back(&arr), 99);

	edge::array_push_back(&arr, 80);
	edge::array_push_back(&arr, 60);

	SHOULD_EQUAL(*edge::array_front(&arr), 99);
	SHOULD_EQUAL(edge::array_size(&arr), 3);
	SHOULD_EQUAL(edge::array_insert(&arr, 1, 50), true);
	SHOULD_EQUAL(*edge::array_at(&arr, 1), 50);

	auto found = std::find_if(edge::begin(arr), edge::end(arr), [](const i32& val) { return val == 80; });
	SHOULD_EQUAL(*found, 80);

	std::sort(edge::begin(arr), edge::end(arr), [](const i32& a, const i32& b) { return a < b; });
	print_array(arr);
	SHOULD_EQUAL(*edge::array_front(&arr), 50);

	edge::array_destroy(&arr);
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(array_resize) {
	edge::Allocator alloc = edge::allocator_create_tracking();
	edge::Array<i32> arr;

	edge::array_create(&alloc, &arr, 4);
	SHOULD_EQUAL(edge::array_capacity(&arr), 4);

	SHOULD_EQUAL(edge::array_resize(&arr, 10), true);
	SHOULD_EQUAL(edge::array_size(&arr), 10);
	SHOULD_EQUAL(*edge::array_at(&arr, 5), 0);

	edge::array_set(&arr, 5, 42);
	SHOULD_EQUAL(*edge::array_get(&arr, 5), 42);

	edge::array_destroy(&arr);
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(array_remove) {
	edge::Allocator alloc = edge::allocator_create_tracking();
	edge::Array<i32> arr;

	edge::array_create(&alloc, &arr, 0);
	for (i32 i = 0; i < 5; i++) {
		edge::array_push_back(&arr, i * 10);
	}

	i32 removed;
	SHOULD_EQUAL(edge::array_remove(&arr, 2, &removed), true);
	SHOULD_EQUAL(removed, 20);
	SHOULD_EQUAL(edge::array_size(&arr), 4);
	SHOULD_EQUAL(*edge::array_at(&arr, 2), 30);

	edge::array_destroy(&arr);
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(list_basic) {
	edge::Allocator alloc = edge::allocator_create_tracking();
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
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(list_push_pop) {
	edge::Allocator alloc = edge::allocator_create_tracking();
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
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(list_insert_remove) {
	edge::Allocator alloc = edge::allocator_create_tracking();
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
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(list_reverse) {
	edge::Allocator alloc = edge::allocator_create_tracking();
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
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(list_sort) {
	edge::Allocator alloc = edge::allocator_create_tracking();
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
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(list_find) {
	edge::Allocator alloc = edge::allocator_create_tracking();
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
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(hashmap_basic) {
	edge::Allocator alloc = edge::allocator_create_tracking();
	edge::HashMap<i32, i32> map;

	SHOULD_EQUAL(edge::hashmap_create(&alloc, &map, 0), true);
	SHOULD_EQUAL(edge::hashmap_empty(&map), true);

	SHOULD_EQUAL(edge::hashmap_insert(&map, 1, 100), true);
	SHOULD_EQUAL(edge::hashmap_insert(&map, 2, 200), true);
	SHOULD_EQUAL(edge::hashmap_insert(&map, 3, 300), true);

	SHOULD_EQUAL(edge::hashmap_size(&map), 3);

	i32* val = edge::hashmap_get(&map, 2);
	SHOULD_EQUAL(val != nullptr, true);
	SHOULD_EQUAL(*val, 200);

	edge::hashmap_destroy(&map);
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(hashmap_update) {
	edge::Allocator alloc = edge::allocator_create_tracking();
	edge::HashMap<i32, i32> map;

	edge::hashmap_create(&alloc, &map, 0);

	edge::hashmap_insert(&map, 5, 50);
	SHOULD_EQUAL(*edge::hashmap_get(&map, 5), 50);

	// Update existing key
	edge::hashmap_insert(&map, 5, 500);
	SHOULD_EQUAL(*edge::hashmap_get(&map, 5), 500);
	SHOULD_EQUAL(edge::hashmap_size(&map), 1);

	edge::hashmap_destroy(&map);
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(hashmap_remove) {
	edge::Allocator alloc = edge::allocator_create_tracking();
	edge::HashMap<i32, i32> map;

	edge::hashmap_create(&alloc, &map, 0);

	edge::hashmap_insert(&map, 10, 100);
	edge::hashmap_insert(&map, 20, 200);
	edge::hashmap_insert(&map, 30, 300);

	i32 removed_val;
	SHOULD_EQUAL(edge::hashmap_remove(&map, 20, &removed_val), true);
	SHOULD_EQUAL(removed_val, 200);
	SHOULD_EQUAL(edge::hashmap_size(&map), 2);

	SHOULD_EQUAL(edge::hashmap_contains(&map, 20), false);
	SHOULD_EQUAL(edge::hashmap_contains(&map, 10), true);

	edge::hashmap_destroy(&map);
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(hashmap_iteration) {
	edge::Allocator alloc = edge::allocator_create_tracking();
	edge::HashMap<i32, i32> map;

	edge::hashmap_create(&alloc, &map, 0);

	for (i32 i = 0; i < 5; i++) {
		edge::hashmap_insert(&map, i, i * 10);
	}

	printf("  HashMap entries: ");
	i32 count = 0;
	for (auto& entry : map) {
		printf("[%d->%d] ", entry.key, entry.value);
		count++;
	}
	printf("\n");

	SHOULD_EQUAL(count, 5);

	edge::hashmap_destroy(&map);
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(hashmap_rehash) {
	edge::Allocator alloc = edge::allocator_create_tracking();
	edge::HashMap<i32, i32> map;

	edge::hashmap_create(&alloc, &map, 4);

	// Insert enough elements to trigger rehash
	for (i32 i = 0; i < 10; i++) {
		edge::hashmap_insert(&map, i, i * 100);
	}

	SHOULD_EQUAL(edge::hashmap_size(&map), 10);

	// Verify all elements are still accessible
	for (i32 i = 0; i < 10; i++) {
		i32* val = edge::hashmap_get(&map, i);
		SHOULD_EQUAL(val != nullptr, true);
		SHOULD_EQUAL(*val, i * 100);
	}

	edge::hashmap_destroy(&map);
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(hashmap_clear) {
	edge::Allocator alloc = edge::allocator_create_tracking();
	edge::HashMap<i32, i32> map;

	edge::hashmap_create(&alloc, &map, 0);

	for (i32 i = 0; i < 5; i++) {
		edge::hashmap_insert(&map, i, i);
	}

	SHOULD_EQUAL(edge::hashmap_size(&map), 5);

	edge::hashmap_clear(&map);

	SHOULD_EQUAL(edge::hashmap_empty(&map), true);
	SHOULD_EQUAL(edge::hashmap_size(&map), 0);

	edge::hashmap_destroy(&map);
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(bitarray_basic) {
	edge::BitArray<64> arr = {};

	edge::bitarray_clear_all(&arr);
	SHOULD_EQUAL(edge::bitarray_get(&arr, 0), false);

	edge::bitarray_set(&arr, 5);
	SHOULD_EQUAL(edge::bitarray_get(&arr, 5), true);

	edge::bitarray_clear(&arr, 5);
	SHOULD_EQUAL(edge::bitarray_get(&arr, 5), false);

	edge::bitarray_toggle(&arr, 10);
	SHOULD_EQUAL(edge::bitarray_get(&arr, 10), true);

	edge::bitarray_toggle(&arr, 10);
	SHOULD_EQUAL(edge::bitarray_get(&arr, 10), false);

	return 0;
}

TEST(bitarray_put) {
	edge::BitArray<32> arr = {};

	edge::bitarray_clear_all(&arr);

	edge::bitarray_put(&arr, 0, true);
	edge::bitarray_put(&arr, 1, false);
	edge::bitarray_put(&arr, 2, true);

	SHOULD_EQUAL(edge::bitarray_get(&arr, 0), true);
	SHOULD_EQUAL(edge::bitarray_get(&arr, 1), false);
	SHOULD_EQUAL(edge::bitarray_get(&arr, 2), true);

	return 0;
}

TEST(bitarray_set_all) {
	edge::BitArray<16> arr = {};

	edge::bitarray_set_all(&arr);

	for (i32 i = 0; i < 16; i++) {
		SHOULD_EQUAL(edge::bitarray_get(&arr, i), true);
	}

	return 0;
}

TEST(bitarray_count) {
	edge::BitArray<32> arr = {};

	edge::bitarray_clear_all(&arr);
	edge::bitarray_set(&arr, 0);
	edge::bitarray_set(&arr, 5);
	edge::bitarray_set(&arr, 10);
	edge::bitarray_set(&arr, 15);

	SHOULD_EQUAL(edge::bitarray_count_set(&arr), 4);

	return 0;
}

TEST(bitarray_find_first) {
	edge::BitArray<64> arr = {};

	edge::bitarray_clear_all(&arr);
	SHOULD_EQUAL(edge::bitarray_find_first_set(&arr), -1);

	edge::bitarray_set(&arr, 20);
	SHOULD_EQUAL(edge::bitarray_find_first_set(&arr), 20);

	edge::bitarray_set(&arr, 5);
	SHOULD_EQUAL(edge::bitarray_find_first_set(&arr), 5);

	return 0;
}

TEST(bitarray_any_all) {
	edge::BitArray<32> arr = {};

	edge::bitarray_clear_all(&arr);
	SHOULD_EQUAL(edge::bitarray_any_set(&arr), false);
	SHOULD_EQUAL(edge::bitarray_all_clear(&arr), true);

	edge::bitarray_set(&arr, 10);
	SHOULD_EQUAL(edge::bitarray_any_set(&arr), true);
	SHOULD_EQUAL(edge::bitarray_all_clear(&arr), false);

	return 0;
}

TEST(mpmc_queue_basic) {
	edge::Allocator alloc = edge::allocator_create_tracking();
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
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(mpmc_queue_full) {
	edge::Allocator alloc = edge::allocator_create_tracking();
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
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
	return 0;
}

TEST(mpmc_queue_try_operations) {
	edge::Allocator alloc = edge::allocator_create_tracking();
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
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
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
	edge::Allocator alloc = edge::allocator_create_tracking();
	edge::MPMCQueue<i32> queue;

	edge::mpmc_queue_create(&alloc, &queue, 1024);

	const i32 num_producers = 2;
	const i32 num_consumers = 2;
	const i32 items_per_producer = 100;
	const i32 total_items = num_producers * items_per_producer;

	edge::Array<edge::Thread> threads;
	edge::array_create(&alloc, &threads, num_producers + num_consumers);

	edge::Array<ProducerArgs> producer_args;
	edge::array_create(&alloc, &producer_args, num_producers);

	edge::Array<ConsumerArgs> consumer_args;
	edge::array_create(&alloc, &consumer_args, num_consumers);

	std::atomic<i32> consumed_count{ 0 };

	// Create producer threads
	for (i32 p = 0; p < num_producers; p++) {
		ProducerArgs args;
		args.queue = &queue;
		args.producer_id = p;
		args.items_count = items_per_producer;
		edge::array_push_back(&producer_args, args);

		edge::Thread thr;
		edge::thread_create(&thr, producer_thread, edge::array_at(&producer_args, p));
		edge::array_push_back(&threads, thr);
	}

	// Create consumer threads
	for (i32 c = 0; c < num_consumers; c++) {
		ConsumerArgs args;
		args.queue = &queue;
		args.consumed_count = &consumed_count;
		args.total_items = total_items;
		edge::array_push_back(&consumer_args, args);

		edge::Thread thr;
		edge::thread_create(&thr, consumer_thread, edge::array_at(&consumer_args, c));
		edge::array_push_back(&threads, thr);
	}

	// Join all threads
	for (usize i = 0; i < edge::array_size(&threads); i++) {
		edge::thread_join(*edge::array_at(&threads, i));
	}

	printf("  Consumed %d items\n", consumed_count.load());
	SHOULD_EQUAL(consumed_count.load(), total_items);

	edge::array_destroy(&consumer_args);
	edge::array_destroy(&producer_args);
	edge::array_destroy(&threads);
	edge::mpmc_queue_destroy(&queue);
	SHOULD_EQUAL(edge::allocator_get_net(&alloc), 0ull);
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