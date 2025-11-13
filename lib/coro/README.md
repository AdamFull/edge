# Edge Coro

A lightweight, cross-platform stackful coroutines library for C11.

## Supported Platforms

| Architecture | OS | ABI |
|--------------|------------|---------------|
| x86_64 | Linux | System V |
| x86_64 | Android | System V |
| x86_64 | Windows | Windows x64 |
| aarch64 | Linux | AAPCS64 |
| aarch64 | Android | AAPCS64 |

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_C_COMPILER=clang
cmake --build .
cmake --install .
```

### Build Options

- `EDGE_CORO_BUILD_EXAMPLES`: Build examples (default: ON)

## Quick Start

```c
#include <edge_coro.h>
#include <edge_allocator.h>
#include <stdio.h>
#include <stdlib.h>

// Simple allocator implementation

void my_coroutine(void* arg) {
    int* counter = (int*)arg;
    
    for (int i = 0; i < 5; i++) {
        printf("Coroutine: %d\n", (*counter)++);
        edge_coro_yield();  // Yield back to caller
    }
}

int main(void) {
    edge_allocator_t allocator = edge_allocator_create_default();

    edge_coro_init_thread_context(&allocator);
    
    int counter = 0;
    edge_coro_t* coro = edge_coro_create(my_coroutine, &counter);
    
    while (edge_coro_alive(coro)) {
        printf("Main: resuming coroutine\n");
        edge_coro_resume(coro);
    }
    
    edge_coro_destroy(coro);
    edge_coro_shutdown_thread_context();

    return 0;
}
```

## API Reference

### Types

#### `edge_coro_t`
Opaque coroutine handle.

#### `edge_coro_fn`
Coroutine entry point function signature.

```c
typedef void (*edge_coro_fn)(void* arg);
```

#### `edge_coro_state_t`
Coroutine execution status.

```c
typedef enum {
    EDGE_CORO_STATE_READY,      /* Ready to run */
	EDGE_CORO_STATE_RUNNING,    /* Currently executing */
	EDGE_CORO_STATE_SUSPENDED,  /* Suspended, waiting to resume */
	EDGE_CORO_STATE_FINISHED   /* Execution completed */
} edge_coro_state_t;
```

### Functions

#### `edge_coro_init_thread_context`
```c
void edge_coro_init_thread_context(edge_allocator_t* allocator);
```
Initializes the stream context if it has not been initialized previously.

#### `edge_coro_shutdown_thread_context`
```c
void edge_coro_shutdown_thread_context(void);
```
Releases the stream context if it was previously created.

#### `edge_coro_create`
```c
edge_coro_t* edge_coro_create(edge_coro_fn function, void* arg);
```
Create a new coroutine. Returns `NULL` on failure.

#### `edge_coro_destroy`
```c
void edge_coro_destroy(edge_coro_t* coro);
```
Destroy a coroutine and free its resources.

#### `edge_coro_resume`
```c
bool edge_coro_resume(edge_coro_t* coro);
```
Resume a suspended coroutine. Returns `false` if coroutine is dead.

#### `edge_coro_yield`
```c
void edge_coro_yield(void);
```
Yield from current coroutine back to its caller.

#### `edge_coro_current`
```c
edge_coro_t* edge_coro_current(void);
```
Get the currently running coroutine.

#### `edge_coro_state`
```c
edge_coro_state_t edge_coro_state(edge_coro_t* coro);
```
Get the status of a coroutine.

#### `edge_coro_alive`
```c
bool edge_coro_alive(edge_coro_t* coro);
```
Check if a coroutine can be resumed (not dead).

## Examples

See the `examples/` directory for complete working examples:

- `example_generator.c`: Implementing a Fibonacci generator
- `example_pipeline.c`: Coroutine pipeline pattern

## Memory Requirements

Each coroutine requires:
- Context structure: ~168 bytes (x86_64 Windows), ~56 bytes (x86_64 Linux/Android), ~168 bytes (aarch64)
- Stack: Configurable, default 2MB
- Coroutine handle: ~48 bytes

## Thread Safety

Each thread maintains its own coroutine context. Coroutines cannot migrate between threads. Multiple threads can use the library simultaneously without interference.

## License

MIT License - see LICENSE file for details.