#include <edge_coro.h>
#include <edge_allocator.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* Pipeline communication structure */
typedef struct {
    int value;
    bool has_value;
} pipe_data_t;

/* Stage 1: Generate numbers 1-10 */
void number_source(void* arg) {
    pipe_data_t* data = (pipe_data_t*)arg;

    printf("[Source] Starting...\n");

    for (int i = 1; i <= 10; i++) {
        data->value = i;
        data->has_value = true;
        printf("[Source] Generated: %d\n", i);
        edge_coro_yield();  // Yield after producing each value
        data->has_value = false;
    }

    printf("[Source] Finished\n");
}

/* Simple pipeline example: just multiply values */
void transform_multiply(void* arg) {
    int* value = (int*)arg;
    *value = (*value) * 2;
}

int main(void) {
    printf("=== Pipeline Example ===\n");
    printf("Simple producer-consumer pipeline\n\n");

    edge_allocator_t allocator = edge_allocator_create_default();

    edge_coro_init_thread_context(&allocator);

    /* Create data pipe */
    pipe_data_t data = { 0 };

    /* Create source coroutine */
    edge_coro_t* source = edge_coro_create(number_source, &data);
    if (!source) {
        fprintf(stderr, "Failed to create source coroutine\n");
        return 1;
    }

    printf("--- Pipeline Execution ---\n\n");

    /* Pull values from source and transform them */
    while (edge_coro_alive(source)) {
        edge_coro_resume(source);

        if (data.has_value) {
            int original = data.value;
            transform_multiply(&data.value);
            int transformed = data.value;

            if (transformed > 10) {
                printf("[Pipeline] %d -> %d (PASS)\n", original, transformed);
            }
            else {
                printf("[Pipeline] %d -> %d (filtered out)\n", original, transformed);
            }
        }
    }

    printf("\n--- Pipeline Complete ---\n");

    edge_coro_destroy(source);

    edge_coro_shutdown_thread_context();

    return 0;
}
