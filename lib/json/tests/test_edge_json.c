/*
 * Comprehensive Test Suite for edge_json
 * Uses real edge_allocator with memory leak tracking wrapper
 */

#define _POSIX_C_SOURCE 200809L

#include "edge_json.h"
#include <edge_allocator.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>

/* ANSI color codes */
#define COLOR_GREEN "\033[0;32m"
#define COLOR_RED "\033[0;31m"
#define COLOR_BLUE "\033[0;34m"
#define COLOR_RESET "\033[0m"

static int tests_passed = 0;
static int tests_failed = 0;
static int tests_total = 0;

/* Memory tracking for leak detection */
typedef struct {
    size_t total_allocations;
    size_t total_frees;
    size_t current_allocations;
} allocator_stats_t;

static allocator_stats_t g_stats = {0};

/* Tracking wrapper functions */
static void* tracking_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr) {
        g_stats.total_allocations++;
        g_stats.current_allocations++;
    }
    return ptr;
}

static void tracking_free(void* ptr) {
    if (ptr) {
        g_stats.total_frees++;
        g_stats.current_allocations--;
        free(ptr);
    }
}

static void* tracking_realloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (new_ptr && ptr == NULL) {
        g_stats.total_allocations++;
        g_stats.current_allocations++;
    }
    return new_ptr;
}

static void* tracking_calloc(size_t nmemb, size_t size) {
    void* ptr = calloc(nmemb, size);
    if (ptr) {
        g_stats.total_allocations++;
        g_stats.current_allocations++;
    }
    return ptr;
}

static char* tracking_strdup(const char* str) {
    char* ptr = strdup(str);
    if (ptr) {
        g_stats.total_allocations++;
        g_stats.current_allocations++;
    }
    return ptr;
}

static edge_allocator_t create_tracking_allocator(void) {
    return edge_allocator_create(
        tracking_malloc,
        tracking_free,
        tracking_realloc,
        tracking_calloc,
        tracking_strdup
    );
}

static void allocator_reset_stats(void) {
    memset(&g_stats, 0, sizeof(g_stats));
}

static int allocator_has_leaks(void) {
    return g_stats.current_allocations != 0;
}

#define TEST_START(name) \
    do { \
        tests_total++; \
        allocator_reset_stats(); \
        printf(COLOR_BLUE "[TEST %d] " COLOR_RESET "%s ... ", tests_total, name); \
        fflush(stdout); \
    } while(0)

#define TEST_END() \
    do { \
        if (allocator_has_leaks()) { \
            printf(COLOR_RED "LEAK" COLOR_RESET " (%zu allocs, %zu frees, %zu leaked)\n", \
                   g_stats.total_allocations, g_stats.total_frees, g_stats.current_allocations); \
            tests_failed++; \
        } else { \
            printf(COLOR_GREEN "OK" COLOR_RESET " (%zu allocs)\n", g_stats.total_allocations); \
            tests_passed++; \
        } \
    } while(0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf(COLOR_RED "FAIL" COLOR_RESET ": %s\n", msg); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(a, b, msg) ASSERT((a) == (b), msg)
#define ASSERT_STR_EQ(a, b, msg) ASSERT(strcmp(a, b) == 0, msg)
#define ASSERT_NOT_NULL(ptr, msg) ASSERT((ptr) != NULL, msg)

/* Global allocator for tests */
static edge_allocator_t test_allocator;

void test_create_null(void) {
    TEST_START("Create NULL value");
    edge_json_value_t* val = edge_json_null(&test_allocator);
    ASSERT_NOT_NULL(val, "Failed to create null");
    ASSERT(edge_json_is_null(val), "Wrong type");
    edge_json_free_value(val);
    TEST_END();
}

void test_create_bool(void) {
    TEST_START("Create boolean values");
    edge_json_value_t* val_true = edge_json_bool(1, &test_allocator);
    edge_json_value_t* val_false = edge_json_bool(0, &test_allocator);
    ASSERT_NOT_NULL(val_true, "Failed");
    ASSERT_NOT_NULL(val_false, "Failed");
    ASSERT_EQ(edge_json_get_bool(val_true, 0), 1, "Wrong value");
    ASSERT_EQ(edge_json_get_bool(val_false, 1), 0, "Wrong value");
    edge_json_free_value(val_true);
    edge_json_free_value(val_false);
    TEST_END();
}

void test_create_number(void) {
    TEST_START("Create number values");
    edge_json_value_t* val1 = edge_json_number(3.14, &test_allocator);
    edge_json_value_t* val2 = edge_json_int(42, &test_allocator);
    ASSERT_NOT_NULL(val1, "Failed");
    ASSERT_NOT_NULL(val2, "Failed");
    ASSERT_EQ(edge_json_get_int(val2, 0), 42, "Wrong value");
    edge_json_free_value(val1);
    edge_json_free_value(val2);
    TEST_END();
}

void test_create_string(void) {
    TEST_START("Create string values");
    edge_json_value_t* val = edge_json_string("Hello", &test_allocator);
    ASSERT_NOT_NULL(val, "Failed");
    ASSERT_STR_EQ(edge_json_get_string(val, ""), "Hello", "Wrong value");
    edge_json_free_value(val);
    TEST_END();
}

void test_array_operations(void) {
    TEST_START("Array operations");
    edge_json_value_t* arr = edge_json_array(&test_allocator);
    edge_json_array_append(arr, edge_json_int(1, &test_allocator));
    edge_json_array_append(arr, edge_json_int(2, &test_allocator));
    edge_json_array_append(arr, edge_json_int(3, &test_allocator));
    ASSERT_EQ(edge_json_array_size(arr), 3, "Wrong size");
    edge_json_array_remove(arr, 1);
    ASSERT_EQ(edge_json_array_size(arr), 2, "Wrong size after remove");
    edge_json_free_value(arr);
    TEST_END();
}

void test_object_operations(void) {
    TEST_START("Object operations");
    edge_json_value_t* obj = edge_json_object(&test_allocator);
    edge_json_object_set(obj, "name", edge_json_string("John", &test_allocator));
    edge_json_object_set(obj, "age", edge_json_int(30, &test_allocator));
    ASSERT_EQ(edge_json_object_size(obj), 2, "Wrong size");
    ASSERT(edge_json_object_has(obj, "name"), "Key not found");
    edge_json_free_value(obj);
    TEST_END();
}

void test_nested_structures(void) {
    TEST_START("Nested structures");
    edge_json_value_t* root = edge_json_object(&test_allocator);
    edge_json_value_t* arr = edge_json_array(&test_allocator);
    edge_json_value_t* obj = edge_json_object(&test_allocator);
    edge_json_object_set(obj, "name", edge_json_string("Alice", &test_allocator));
    edge_json_array_append(arr, obj);
    edge_json_object_set(root, "users", arr);
    edge_json_free_value(root);
    TEST_END();
}

void test_parser(void) {
    TEST_START("JSON parser");
    edge_json_value_t* val = edge_json_parse("{\"name\":\"John\",\"age\":30}", &test_allocator);
    ASSERT_NOT_NULL(val, "Parse failed");
    ASSERT(edge_json_is_object(val), "Wrong type");
    ASSERT_EQ(edge_json_object_size(val), 2, "Wrong size");
    edge_json_free_value(val);
    TEST_END();
}

void test_serializer(void) {
    TEST_START("JSON serializer");
    edge_json_value_t* arr = edge_json_array(&test_allocator);
    edge_json_array_append(arr, edge_json_int(1, &test_allocator));
    edge_json_array_append(arr, edge_json_int(2, &test_allocator));
    char* json = edge_json_stringify(arr);
    ASSERT_NOT_NULL(json, "Stringify failed");
    ASSERT_STR_EQ(json, "[1,2]", "Wrong JSON");
    edge_allocator_free(&test_allocator, json); // Use allocator's free
    edge_json_free_value(arr);
    TEST_END();
}

void test_leak_array_clear(void) {
    TEST_START("LEAK: Array clear with nested objects");
    edge_json_value_t* arr = edge_json_array(&test_allocator);
    for (int i = 0; i < 5; i++) {
        edge_json_value_t* obj = edge_json_object(&test_allocator);
        edge_json_object_set(obj, "id", edge_json_int(i, &test_allocator));
        edge_json_array_append(arr, obj);
    }
    edge_json_array_clear(arr);
    edge_json_free_value(arr);
    TEST_END();
}

void test_leak_object_replace(void) {
    TEST_START("LEAK: Object key replacement");
    edge_json_value_t* obj = edge_json_object(&test_allocator);
    edge_json_object_set(obj, "key", edge_json_string("old", &test_allocator));
    edge_json_object_set(obj, "key", edge_json_string("new", &test_allocator));
    edge_json_free_value(obj);
    TEST_END();
}

void test_leak_deep_nesting(void) {
    TEST_START("LEAK: Deep nesting (10 levels)");
    edge_json_value_t* root = edge_json_object(&test_allocator);
    edge_json_value_t* current = root;
    for (int i = 0; i < 10; i++) {
        edge_json_value_t* next = edge_json_object(&test_allocator);
        edge_json_object_set(next, "data", edge_json_string("value", &test_allocator));
        char key[32];
        snprintf(key, sizeof(key), "level%d", i);
        edge_json_object_set(current, key, next);
        current = next;
    }
    edge_json_free_value(root);
    TEST_END();
}

void test_leak_large_array(void) {
    TEST_START("LEAK: Large array (1000 strings)");
    edge_json_value_t* arr = edge_json_array(&test_allocator);
    for (int i = 0; i < 1000; i++) {
        char str[64];
        snprintf(str, sizeof(str), "item_%d", i);
        edge_json_array_append(arr, edge_json_string(str, &test_allocator));
    }
    edge_json_free_value(arr);
    TEST_END();
}

int main(void) {
    test_allocator = create_tracking_allocator();
    
    printf("\n===========================================================\n");
    printf("  edge_json Test Suite - Memory Leak Detection\n");
    printf("===========================================================\n\n");
    
    test_create_null();
    test_create_bool();
    test_create_number();
    test_create_string();
    test_array_operations();
    test_object_operations();
    test_nested_structures();
    test_parser();
    test_serializer();
    test_leak_array_clear();
    test_leak_object_replace();
    test_leak_deep_nesting();
    test_leak_large_array();
    
    printf("\n===========================================================\n");
    printf("  Results: %d total, ", tests_total);
    printf(COLOR_GREEN "%d passed" COLOR_RESET ", ", tests_passed);
    if (tests_failed > 0) {
        printf(COLOR_RED "%d FAILED" COLOR_RESET "\n", tests_failed);
    } else {
        printf("%d failed\n", tests_failed);
    }
    printf("===========================================================\n\n");
    
    if (tests_failed > 0) {
        printf(COLOR_RED "MEMORY LEAKS DETECTED!" COLOR_RESET "\n\n");
        return 1;
    } else {
        printf(COLOR_GREEN "ALL TESTS PASSED - NO LEAKS!" COLOR_RESET "\n\n");
        return 0;
    }
}
