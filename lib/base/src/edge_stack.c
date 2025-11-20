#include "edge_stack.h"
#include "edge_allocator.h"

#include <string.h>

#define EDGE_STACK_DEFAULT_CAPACITY 16

edge_stack_t* edge_stack_create(const edge_allocator_t* allocator, size_t element_size, size_t initial_capacity) {
    if (!allocator || element_size == 0) {
        return NULL;
    }

    edge_stack_t* stack = (edge_stack_t*)edge_allocator_malloc(allocator, sizeof(edge_stack_t));
    if (!stack) {
        return NULL;
    }

    if (initial_capacity == 0) {
        initial_capacity = EDGE_STACK_DEFAULT_CAPACITY;
    }

    stack->data = edge_allocator_malloc(allocator, element_size * initial_capacity);
    if (!stack->data) {
        edge_allocator_free(allocator, stack);
        return NULL;
    }

    stack->size = 0;
    stack->capacity = initial_capacity;
    stack->element_size = element_size;
    stack->allocator = allocator;

    return stack;
}

void edge_stack_destroy(edge_stack_t* stack) {
    if (!stack) {
        return;
    }

    if (stack->data) {
        edge_allocator_free(stack->allocator, stack->data);
    }
    edge_allocator_free(stack->allocator, stack);
}

void edge_stack_clear(edge_stack_t* stack) {
    if (stack) {
        stack->size = 0;
    }
}

bool edge_stack_reserve(edge_stack_t* stack, size_t capacity) {
    if (!stack) {
        return false;
    }

    if (capacity <= stack->capacity) {
        return true;
    }

    void* new_data = edge_allocator_realloc(stack->allocator, stack->data, stack->element_size * capacity);
    if (!new_data) {
        return false;
    }

    stack->data = new_data;
    stack->capacity = capacity;

    return true;
}

bool edge_stack_push(edge_stack_t* stack, const void* element) {
    if (!stack || !element) {
        return false;
    }

    if (stack->size >= stack->capacity) {
        if (!edge_stack_reserve(stack, stack->capacity * 2)) {
            return false;
        }
    }

    void* dest = (char*)stack->data + (stack->size * stack->element_size);
    memcpy(dest, element, stack->element_size);
    stack->size++;

    return true;
}

bool edge_stack_pop(edge_stack_t* stack, void* out_element) {
    if (!stack || stack->size == 0) {
        return false;
    }

    stack->size--;

    if (out_element) {
        void* src = (char*)stack->data + (stack->size * stack->element_size);
        memcpy(out_element, src, stack->element_size);
    }

    return true;
}

void* edge_stack_top(const edge_stack_t* stack) {
    if (!stack || stack->size == 0) {
        return NULL;
    }
    return (char*)stack->data + ((stack->size - 1) * stack->element_size);
}

size_t edge_stack_size(const edge_stack_t* stack) {
    return stack ? stack->size : 0;
}

bool edge_stack_empty(const edge_stack_t* stack) {
    return !stack || stack->size == 0;
}
