#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "edge_testing.h"
#include "edge_string.h"
#include "edge_vector.h"
#include "edge_list.h"
#include "edge_queue.h"
#include "edge_stack.h"
#include "edge_hashmap.h"

static int compare_ints(const void* a, const void* b) {
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    return (ia > ib) - (ia < ib);
}

void test_string(const edge_allocator_t* allocator) {
    printf("\n=== Testing edge_string ===\n");

    edge_string_t* str = edge_string_create_from(allocator, "Hello");
    printf("Initial: '%s'\n", edge_string_cstr(str));

    edge_string_append(str, ", World");
    printf("After append: '%s'\n", edge_string_cstr(str));

    edge_string_append_char(str, '!');
    printf("After append char: '%s'\n", edge_string_cstr(str));

    edge_string_insert(str, 5, " Beautiful");
    printf("After insert: '%s'\n", edge_string_cstr(str));

    int pos = edge_string_find(str, "World");
    printf("Found 'World' at position: %d\n", pos);

    edge_string_destroy(str);
}

void test_vector(const edge_allocator_t* allocator) {
    printf("\n=== Testing edge_vector ===\n");

    edge_vector_t* vec = edge_vector_create(allocator, sizeof(int), 0);

    /* Push some integers */
    for (int i = 0; i < 10; i++) {
        edge_vector_push_back(vec, &i);
    }

    printf("Vector size: %zu\n", edge_vector_size(vec));
    printf("Vector contents: ");
    for (size_t i = 0; i < edge_vector_size(vec); i++) {
        int* val = (int*)edge_vector_at(vec, i);
        printf("%d ", *val);
    }
    printf("\n");

    /* Insert in the middle */
    int val = 99;
    edge_vector_insert(vec, 5, &val);
    printf("After inserting 99 at index 5: ");
    for (size_t i = 0; i < edge_vector_size(vec); i++) {
        int* v = (int*)edge_vector_at(vec, i);
        printf("%d ", *v);
    }
    printf("\n");

    /* Sort the vector */
    edge_vector_sort(vec, compare_ints);
    printf("After sorting: ");
    for (size_t i = 0; i < edge_vector_size(vec); i++) {
        int* v = (int*)edge_vector_at(vec, i);
        printf("%d ", *v);
    }
    printf("\n");

    edge_vector_destroy(vec);
}

void test_list(const edge_allocator_t* allocator) {
    printf("\n=== Testing edge_list ===\n");

    edge_list_t* list = edge_list_create(allocator, sizeof(int));

    /* Add elements */
    for (int i = 1; i <= 5; i++) {
        edge_list_push_back(list, &i);
    }

    printf("List size: %zu\n", edge_list_size(list));
    printf("List contents (using iterator): ");

    edge_list_iterator_t it = edge_list_begin(list);
    while (edge_list_iterator_valid(&it)) {
        int* val = (int*)edge_list_iterator_get(&it);
        printf("%d ", *val);
        edge_list_iterator_next(&it);
    }
    printf("\n");

    /* Reverse the list */
    edge_list_reverse(list);
    printf("After reverse: ");
    it = edge_list_begin(list);
    while (edge_list_iterator_valid(&it)) {
        int* val = (int*)edge_list_iterator_get(&it);
        printf("%d ", *val);
        edge_list_iterator_next(&it);
    }
    printf("\n");

    edge_list_destroy(list);
}

void test_queue(const edge_allocator_t* allocator) {
    printf("\n=== Testing edge_queue ===\n");

    edge_queue_t* queue = edge_queue_create(allocator, sizeof(int), 0);

    /* Enqueue elements */
    printf("Enqueuing: ");
    for (int i = 1; i <= 5; i++) {
        edge_queue_enqueue(queue, &i);
        printf("%d ", i);
    }
    printf("\n");

    /* Dequeue elements */
    printf("Dequeuing: ");
    while (!edge_queue_empty(queue)) {
        int val;
        edge_queue_dequeue(queue, &val);
        printf("%d ", val);
    }
    printf("\n");

    edge_queue_destroy(queue);
}

void test_stack(const edge_allocator_t* allocator) {
    printf("\n=== Testing edge_stack ===\n");

    edge_stack_t* stack = edge_stack_create(allocator, sizeof(int), 0);

    /* Push elements */
    printf("Pushing: ");
    for (int i = 1; i <= 5; i++) {
        edge_stack_push(stack, &i);
        printf("%d ", i);
    }
    printf("\n");

    /* Pop elements */
    printf("Popping: ");
    while (!edge_stack_empty(stack)) {
        int val;
        edge_stack_pop(stack, &val);
        printf("%d ", val);
    }
    printf("\n");

    edge_stack_destroy(stack);
}

void test_hashmap(const edge_allocator_t* allocator) {
    printf("\n=== Testing edge_hashmap ===\n");

    edge_hashmap_t* map = edge_hashmap_create(allocator, sizeof(int), sizeof(int), 0);

    /* Insert key-value pairs */
    printf("Inserting key-value pairs:\n");
    for (int i = 1; i <= 5; i++) {
        int value = i * 10;
        edge_hashmap_insert(map, &i, &value);
        printf("  %d -> %d\n", i, value);
    }

    printf("Map size: %zu\n", edge_hashmap_size(map));

    /* Retrieve values */
    printf("Getting values:\n");
    for (int i = 1; i <= 5; i++) {
        int* val = (int*)edge_hashmap_get(map, &i);
        if (val) {
            printf("  Key %d: %d\n", i, *val);
        }
    }

    /* Iterate through map */
    printf("Iterating through map:\n");
    edge_hashmap_iterator_t it = edge_hashmap_begin(map);
    while (edge_hashmap_iterator_valid(&it)) {
        int* key = (int*)edge_hashmap_iterator_key(&it);
        int* val = (int*)edge_hashmap_iterator_value(&it);
        printf("  %d -> %d\n", *key, *val);
        edge_hashmap_iterator_next(&it);
    }

    /* Remove an element */
    int key = 3;
    int removed_value;
    if (edge_hashmap_remove(map, &key, &removed_value)) {
        printf("Removed key %d with value %d\n", key, removed_value);
    }
    printf("Map size after removal: %zu\n", edge_hashmap_size(map));

    edge_hashmap_destroy(map);
}

int main(void) {
    printf("Edge Container Library Demo\n");
    printf("===========================\n");

    /* Create default allocator */
    edge_allocator_t allocator = edge_testing_allocator_create();

    /* Test all containers */
    test_string(&allocator);
    test_vector(&allocator);
    test_list(&allocator);
    test_queue(&allocator);
    test_stack(&allocator);
    test_hashmap(&allocator);

    printf("\nAll tests completed successfully!\n");

    size_t alloc_net = edge_testing_net_allocated();
    assert(alloc_net == 0 && "Memory leaks detected, some data was not freed.");

    return 0;
}