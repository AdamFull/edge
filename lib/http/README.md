# libedgehttp - Http wrapper for edge engine

A lightweight, efficient HTTP client library for C with support for synchronous and asynchronous requests. 

## Dependencies

- **libcurl** (required) - For HTTP operations

## Quick Start

### Installation

```bash
# Clone or extract the library
cd libedgehttp

# Build with CMake
mkdir build && cd build
cmake ..
make
sudo make install
```

### Basic Usage

```c
#include <edge_http.h>

int main(void) {
    /* Initialize */
    edge_http_global_init();
    
    /* Simple GET request */
    edge_http_response_t* response = edge_http_get("https://api.example.com/data");
    
    if (response && response->curl_code == CURLE_OK) {
        printf("Status: %ld\n", response->status_code);
        printf("Body: %s\n", response->body);
    }
    
    edge_http_response_free(response);
    edge_http_global_cleanup();
    
    return 0;
}
```

Compile with:
```bash
gcc -o myapp myapp.c -ledge_http -lcurl
```

## API Overview

### Initialization

```c
/* Initialize with default allocator (system malloc) */
int edge_http_global_init(void);

/* Initialize with custom allocator */
int edge_http_global_init_allocator(const struct edge_http_allocator* allocator);

/* Cleanup */
void edge_http_global_cleanup(void);
```

### Request Management

```c
/* Create/Free */
edge_http_request_t* edge_http_request_create(const char* method, const char* url);
void edge_http_request_free(edge_http_request_t* request);

/* Configuration */
void edge_http_request_set_body(edge_http_request_t* request, const char* body, size_t size);
void edge_http_request_add_header(edge_http_request_t* request, const char* header);
void edge_http_request_set_timeout(edge_http_request_t* request, long seconds);
void edge_http_request_set_user_agent(edge_http_request_t* request, const char* ua);
```

### Synchronous API

```c
/* Perform blocking request */
edge_http_response_t* edge_http_request_perform(edge_http_request_t* request);

/* Convenience functions */
edge_http_response_t* edge_http_get(const char* url);
edge_http_response_t* edge_http_post(const char* url, const char* body, size_t size);
edge_http_response_t* edge_http_put(const char* url, const char* body, size_t size);
edge_http_response_t* edge_http_delete(const char* url);
```

### Async API

```c
/* Manager */
edge_http_async_manager_t* edge_http_async_manager_create(void);
void edge_http_async_manager_free(edge_http_async_manager_t* manager);
int edge_http_async_manager_add_request(edge_http_async_manager_t* manager, 
                                     edge_http_request_t* request);

/* Concurrent (blocking) */
void edge_http_async_manager_process_blocking(edge_http_async_manager_t* manager);

/* Truly Async (non-blocking) */
void edge_http_async_manager_start(edge_http_async_manager_t* manager);
int edge_http_async_manager_poll(edge_http_async_manager_t* manager);
int edge_http_async_manager_is_done(edge_http_async_manager_t* manager);
void edge_http_async_manager_wait(edge_http_async_manager_t* manager);
```

## Examples

### Example 1: POST with JSON

```c
edge_http_request_t* request = edge_http_request_create("POST", "https://api.example.com/users");

/* Add headers - no manual management needed */
edge_http_request_add_header(request, "Content-Type: application/json");
edge_http_request_add_header(request, "Authorization: Bearer token123");

/* Set body */
const char* json = "{\"name\":\"John\",\"age\":30}";
edge_http_request_set_body(request, json, strlen(json));

/* Execute */
edge_http_response_t* response = edge_http_request_perform(request);

/* Cleanup - frees headers automatically */
edge_http_response_free(response);
edge_http_request_free(request);
```

### Example 2: Truly Async (Non-Blocking)

```c
edge_http_async_manager_t* manager = edge_http_async_manager_create();

/* Add requests */
edge_http_request_t* req1 = edge_http_request_create("GET", "https://api.example.com/1");
edge_http_request_set_callback(req1, on_complete, NULL);
edge_http_async_manager_add_request(manager, req1);

edge_http_request_t* req2 = edge_http_request_create("GET", "https://api.example.com/2");
edge_http_request_set_callback(req2, on_complete, NULL);
edge_http_async_manager_add_request(manager, req2);

/* Start (returns immediately) */
edge_http_async_manager_start(manager);

/* Main thread continues working */
while (!edge_http_async_manager_is_done(manager)) {
    do_other_work();
    update_ui();
    
    /* Poll for updates (non-blocking) */
    edge_http_async_manager_poll(manager);
    
    sleep_ms(100);
}

edge_http_async_manager_free(manager);
```

### Example 3: Custom Allocator (mimalloc)

```c
#include <mimalloc.h>

struct edge_http_allocator allocator = {
    .malloc_fn  = mi_malloc,
    .free_fn    = mi_free,
    .realloc_fn = mi_realloc,
    .calloc_fn  = mi_calloc,
    .strdup_fn  = mi_strdup
};

/* Initialize with custom allocator */
edge_http_global_init_allocator(&allocator);

/* All operations now use mimalloc */
edge_http_response_t* response = edge_http_get("https://api.example.com");

/* Print memory statistics */
mi_stats_print(NULL);

edge_http_response_free(response);
edge_http_global_cleanup();
```

## Build Options

CMake options:

```bash
# Build shared library (default: ON)
cmake -DHTTPC_BUILD_SHARED=ON ..

# Build static library (default: ON)
cmake -DHTTPC_BUILD_STATIC=ON ..

# Build examples (default: ON)
cmake -DHTTPC_BUILD_EXAMPLES=ON ..

# Use mimalloc (default: OFF)
cmake -DHTTPC_USE_MIMALLOC=ON ..

# Set install prefix
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
```

### Complete Build Example

```bash
mkdir build && cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DHTTPC_BUILD_SHARED=ON \
    -DHTTPC_BUILD_STATIC=ON \
    -DHTTPC_BUILD_EXAMPLES=ON \
    -DHTTPC_USE_MIMALLOC=OFF

make -j$(nproc)
sudo make install
```

## Using in Your Project

### With CMake

```cmake
find_package(edge_http REQUIRED)
target_link_libraries(your_app PRIVATE edge_http::edge_http)
```

### With pkg-config

```bash
gcc myapp.c $(pkg-config --cflags --libs edge_http) -o myapp
```

### Manual Linking

```bash
gcc myapp.c -I/usr/local/include -L/usr/local/lib -ledge_http -lcurl -o myapp
```

## Memory Management

### Key Features

- ✅ **Automatic cleanup** - Single free call cleans everything
- ✅ **No manual header management** - Headers freed with request
- ✅ **No memory leaks** - Proper resource tracking
- ✅ **Custom allocators** - Use your own malloc/free

### Example

```c
edge_http_request_t* request = edge_http_request_create("POST", url);

/* All these allocations are tracked internally */
edge_http_request_add_header(request, "Header1: Value1");
edge_http_request_add_header(request, "Header2: Value2");
edge_http_request_set_body(request, data, size);

/* Single call frees EVERYTHING */
edge_http_request_free(request);  /* Frees: url, headers, body, handles */
```

## Three Request Modes Explained

### 1. Synchronous (Sequential)
```
Request 1 [====]
              Request 2 [====]
                            Request 3 [====]
Time: 3 seconds
Use: Simple scripts, one-off requests
```

### 2. Concurrent (Parallel, Blocking)
```
Request 1 [====]
Request 2 [====]
Request 3 [====]
Time: 1 second (all parallel)
Main thread: BLOCKED until all done
Use: Batch processing, CLI tools
```

### 3. Truly Async (Non-Blocking)
```
Request 1 [====]
Request 2 [====]
Request 3 [====]
Main Thread: [work][work][poll][work][poll]
Time: 1 second
Main thread: NEVER blocked
Use: GUI apps, games, servers
```

## Performance

**Scenario: 10 requests, 1 second each**

| Mode | Time | Main Thread Blocked |
|------|------|---------------------|
| Synchronous | ~10s | 100% |
| Concurrent | ~1s | 100% (but shorter) |
| Truly Async | ~1s | 0% |

## Thread Safety

- ✅ **Library initialization** is thread-safe
- ⚠️ **Request objects** are NOT thread-safe (use one per thread)
- ✅ **Multiple managers** can be used in different threads
- ✅ **Custom allocators** must be thread-safe if using threads

## Error Handling

```c
edge_http_response_t* response = edge_http_get(url);

if (!response) {
    /* Allocation failure */
    return -1;
}

if (response->curl_code != CURLE_OK) {
    /* Network/CURL error */
    printf("Error: %s\n", response->error_message);
    edge_http_response_free(response);
    return -1;
}

if (response->status_code >= 400) {
    /* HTTP error */
    printf("HTTP Error: %ld\n", response->status_code);
    edge_http_response_free(response);
    return -1;
}

/* Success */
printf("Body: %s\n", response->body);
edge_http_response_free(response);
```

## API Comparison

### Before (Raw libcurl)
```c
CURL* curl = curl_easy_init();
struct curl_slist* headers = NULL;
headers = curl_slist_append(headers, "Header1");
headers = curl_slist_append(headers, "Header2");

curl_easy_setopt(curl, CURLOPT_URL, url);
curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
/* ... many more setopt calls ... */

curl_easy_perform(curl);

/* Manual cleanup */
curl_slist_free_all(headers);
curl_easy_cleanup(curl);
```

### After (libedgehttp)
```c
edge_http_request_t* request = edge_http_request_create("GET", url);
edge_http_request_add_header(request, "Header1");
edge_http_request_add_header(request, "Header2");

edge_http_response_t* response = edge_http_request_perform(request);

/* Simple cleanup */
edge_http_response_free(response);
edge_http_request_free(request);
```

## Examples Directory

The library includes several examples:

- `example.c` - Basic usage of all features

Run examples:
```bash
cd build/examples
./example
./custom_allocator
```

## License

MIT License - See LICENSE file for details

## Support

- **Issues**: Report bugs on GitHub
- **Documentation**: See header file for full API
- **Examples**: Check examples/ directory

## Version

Current version: 1.0.0

Query at runtime:
```c
printf("Version: %s\n", edge_http_version());
```
