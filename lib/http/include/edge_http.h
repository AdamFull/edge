#ifndef EDGE_HTTP_H
#define EDGE_HTTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <curl/curl.h>

#define EDGE_HTTP_VERSION_MAJOR 1
#define EDGE_HTTP_VERSION_MINOR 0
#define EDGE_HTTP_VERSION_PATCH 0

typedef void* (*edge_http_malloc_func)(size_t size);
typedef void (*edge_http_free_func)(void* ptr);
typedef void* (*edge_http_realloc_func)(void* ptr, size_t size);
typedef void* (*edge_http_calloc_func)(size_t nmemb, size_t size);
typedef char* (*edge_http_strdup_func)(const char* str);

typedef struct edge_http_allocator {
    edge_http_malloc_func  malloc_fn;
    edge_http_free_func    free_fn;
    edge_http_realloc_func realloc_fn;
    edge_http_calloc_func  calloc_fn;
    edge_http_strdup_func  strdup_fn;
} edge_http_allocator_t;

typedef struct edge_http_response {
    char* body;
    size_t body_size;
    char* headers;
    size_t headers_size;
    long status_code;
    double total_time;
    double download_speed;
    CURLcode curl_code;
    char error_message[256];
} edge_http_response_t;

typedef struct edge_http_request edge_http_request_t;
typedef struct edge_http_async_manager edge_http_async_manager_t;

/* Callback type for async requests */
typedef void (*edge_http_callback_func)(edge_http_response_t* response, void* userdata);

/**
 * Initialize the library with default allocator (system malloc/free)
 * Must be called before any other library functions
 *
 * @return 1 on success, 0 on failure
 */
int edge_http_global_init(void);

/**
 * Initialize the library with custom allocator
 * Must be called before any other library functions
 *
 * @param allocator Custom memory allocator callbacks
 * @return 1 on success, 0 on failure
 */
int edge_http_global_init_allocator(const struct edge_http_allocator* allocator);

/**
 * Cleanup the library
 * Should be called when done using the library
 */
void edge_http_global_cleanup(void);

/**
 * Get library version string
 *
 * @return Version string (e.g., "1.0.0")
 */
const char* edge_http_version(void);

/**
 * Create a new HTTP request
 *
 * @param method HTTP method (e.g., "GET", "POST", "PUT", "DELETE")
 * @param url Request URL
 * @return Pointer to request object, or NULL on failure
 */
edge_http_request_t* edge_http_request_create(const char* method, const char* url);

/**
 * Free a request and all associated resources
 *
 * @param request Request to free
 */
void edge_http_request_free(edge_http_request_t* request);

/**
 * Set request URL
 *
 * @param request Request object
 * @param url New URL
 */
void edge_http_request_set_url(edge_http_request_t* request, const char* url);

/**
 * Set request method
 *
 * @param request Request object
 * @param method HTTP method
 */
void edge_http_request_set_method(edge_http_request_t* request, const char* method);

/**
 * Set request body
 *
 * @param request Request object
 * @param body Body data
 * @param body_size Size of body data
 */
void edge_http_request_set_body(edge_http_request_t* request, const char* body, size_t body_size);

/**
 * Add a header to the request
 * Header should be in format "Name: Value"
 *
 * @param request Request object
 * @param header Header string
 */
void edge_http_request_add_header(edge_http_request_t* request, const char* header);

/**
 * Set request timeout in seconds
 *
 * @param request Request object
 * @param timeout_seconds Timeout in seconds
 */
void edge_http_request_set_timeout(edge_http_request_t* request, long timeout_seconds);

/**
 * Set connection timeout in seconds
 *
 * @param request Request object
 * @param timeout_seconds Connection timeout in seconds
 */
void edge_http_request_set_connect_timeout(edge_http_request_t* request, long timeout_seconds);

/**
 * Set user agent string
 *
 * @param request Request object
 * @param user_agent User agent string
 */
void edge_http_request_set_user_agent(edge_http_request_t* request, const char* user_agent);

/**
 * Enable or disable verbose output
 *
 * @param request Request object
 * @param verbose 1 to enable, 0 to disable
 */
void edge_http_request_set_verbose(edge_http_request_t* request, int verbose);

/**
 * Enable or disable following redirects
 *
 * @param request Request object
 * @param follow 1 to enable, 0 to disable
 */
void edge_http_request_set_follow_redirects(edge_http_request_t* request, int follow);

/**
 * Set callback for async requests
 *
 * @param request Request object
 * @param callback Callback function
 * @param userdata User data passed to callback
 */
void edge_http_request_set_callback(edge_http_request_t* request, edge_http_callback_func callback, void* userdata);


/**
 * Free a response object
 *
 * @param response Response to free
 */
void edge_http_response_free(edge_http_response_t* response);

/**
 * Perform a synchronous HTTP request (blocking)
 *
 * @param request Request to perform
 * @return Response object, or NULL on failure. Must be freed with edge_http_response_free()
 */
edge_http_response_t* edge_http_request_perform(edge_http_request_t* request);

/**
 * Convenience function for GET request
 *
 * @param url URL to request
 * @return Response object, or NULL on failure. Must be freed with edge_http_response_free()
 */
edge_http_response_t* edge_http_get(const char* url);

/**
 * Convenience function for POST request
 *
 * @param url URL to request
 * @param body Request body
 * @param body_size Size of request body
 * @return Response object, or NULL on failure. Must be freed with edge_http_response_free()
 */
edge_http_response_t* edge_http_post(const char* url, const char* body, size_t body_size);

/**
 * Convenience function for PUT request
 *
 * @param url URL to request
 * @param body Request body
 * @param body_size Size of request body
 * @return Response object, or NULL on failure. Must be freed with edge_http_response_free()
 */
edge_http_response_t* edge_http_put(const char* url, const char* body, size_t body_size);

/**
 * Convenience function for DELETE request
 *
 * @param url URL to request
 * @return Response object, or NULL on failure. Must be freed with edge_http_response_free()
 */
edge_http_response_t* edge_http_delete(const char* url);


/**
 * Create an async request manager
 *
 * @return Manager object, or NULL on failure
 */
edge_http_async_manager_t* edge_http_async_manager_create(void);

/**
 * Free async manager and all associated requests
 *
 * @param manager Manager to free
 */
void edge_http_async_manager_free(edge_http_async_manager_t* manager);

/**
 * Add a request to the async manager
 * The manager takes ownership of the request
 *
 * @param manager Async manager
 * @param request Request to add
 * @return 1 on success, 0 on failure
 */
int edge_http_async_manager_add_request(edge_http_async_manager_t* manager, edge_http_request_t* request);


/**
 * Start processing requests (non-blocking, returns immediately)
 *
 * @param manager Async manager with queued requests
 */
void edge_http_async_manager_start(edge_http_async_manager_t* manager);

/**
 * Poll for request updates (non-blocking, returns immediately)
 * Call this periodically to process completed requests
 *
 * @param manager Async manager
 * @return Number of active requests remaining
 */
int edge_http_async_manager_poll(edge_http_async_manager_t* manager);

/**
 * Check if all requests are complete
 *
 * @param manager Async manager
 * @return 1 if all complete, 0 if still running
 */
int edge_http_async_manager_is_done(edge_http_async_manager_t* manager);

/**
 * Wait for all requests to complete (blocking)
 *
 * @param manager Async manager
 */
void edge_http_async_manager_wait(edge_http_async_manager_t* manager);

/**
 * Get last error message
 *
 * @return Error message, or NULL if no error
 */
const char* edge_http_get_error(void);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_HTTP_H */