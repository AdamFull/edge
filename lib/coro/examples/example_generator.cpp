#include <coro.hpp>

#include <stdio.h>

/* Generator state for communication */
typedef struct {
    uint64_t value;
    bool has_value;
} generator_state_t;

/* Fibonacci generator coroutine */
void fibonacci_generator(void* arg) {
    generator_state_t* state = (generator_state_t*)arg;

    uint64_t a = 0, b = 1;

    while (1) {
        state->value = a;
        state->has_value = true;
        edge::coro_yield();

        uint64_t next = a + b;
        a = b;
        b = next;

        /* Prevent overflow - stop at reasonable limit */
        if (a > 10000000000ULL) {
            break;
        }
    }

    state->has_value = false;
}

/* Range generator - yields numbers from start to end */
void range_generator(void* arg) {
    void** params = (void**)arg;
    int start = *(int*)params[0];
    int end = *(int*)params[1];
    int step = *(int*)params[2];
    generator_state_t* state = (generator_state_t*)params[3];

    for (int i = start; i <= end; i += step) {
        state->value = i;
        state->has_value = true;
        edge::coro_yield();
    }

    state->has_value = false;
}

int main(void) {
    printf("=== Generator Example ===\n\n");

    edge::Allocator allocator = edge::Allocator::create_tracking();
    edge::coro_init_thread_context(&allocator);

    /* Example 1: Fibonacci generator */
    printf("Fibonacci sequence (first 20 numbers):\n");
    generator_state_t fib_state = { 0 };
    edge::Coro* fib_coro = edge::coro_create(fibonacci_generator, &fib_state);

    if (!fib_coro) {
        fprintf(stderr, "Failed to create Fibonacci coroutine\n");
        return 1;
    }

    for (int i = 0; i < 20 && edge::coro_alive(fib_coro); i++) {
        edge::coro_resume(fib_coro);
        if (fib_state.has_value) {
            printf("%llu ", (unsigned long long)fib_state.value);
        }
    }
    printf("\n\n");

    edge::coro_destroy(fib_coro);

    /* Example 2: Range generator */
    printf("Range(0, 100, 7):\n");
    generator_state_t range_state = { 0 };
    int range_start = 0, range_end = 100, range_step = 7;
    void* range_params[] = { &range_start, &range_end, &range_step, &range_state };
    edge::Coro* range_coro = edge::coro_create(range_generator, range_params);

    if (!range_coro) {
        fprintf(stderr, "Failed to create range coroutine\n");
        return 1;
    }

    while (edge::coro_alive(range_coro)) {
        edge::coro_resume(range_coro);
        if (range_state.has_value) {
            printf("%llu ", (unsigned long long)range_state.value);
        }
    }
    printf("\n\n");

    edge::coro_destroy(range_coro);

    edge::coro_shutdown_thread_context();

    printf("Generators completed!\n");
    return 0;
}
