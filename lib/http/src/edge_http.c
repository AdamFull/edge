/*
 * libedgehttp - HTTP Client Library for C
 * Implementation
 */

#include "edge_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct edge_http_context {
    edge_http_malloc_func  malloc_fn;
    edge_http_free_func    free_fn;
    edge_http_realloc_func realloc_fn;
    edge_http_calloc_func  calloc_fn;
    edge_http_strdup_func  strdup_fn;

    char error_message[256];
};

struct edge_http_request {
    char* url;
    char* method;
    char* body;
    size_t body_size;
    struct curl_slist* headers;
    long timeout;
    long connect_timeout;
    int follow_redirects;
    int verbose;
    char* user_agent;

    /* Internal state */
    CURL* easy_handle;
    edge_http_response_t* response;
    size_t body_read_offset;

    /* Async callback */
    edge_http_callback_func callback;
    void* userdata;

    /* Completion flag */
    int completed;
};

struct edge_http_async_manager {
    CURLM* multi_handle;
    edge_http_request_t** requests;
    int num_requests;
    int capacity;
    int started;
};

static void* edge_http_malloc(edge_http_context_t* ctx, size_t size) {
    if (!ctx) return NULL;
    return ctx->malloc_fn(size);
}

static void edge_http_free(edge_http_context_t* ctx, void* ptr) {
    if (!ctx || !ptr) return;
    ctx->free_fn(ptr);
}

static void* edge_http_realloc(edge_http_context_t* ctx, void* ptr, size_t size) {
    if (!ctx) return NULL;
    return ctx->realloc_fn(ptr, size);
}

static void* edge_http_calloc(edge_http_context_t* ctx, size_t nmemb, size_t size) {
    if (!ctx) return NULL;
    return ctx->calloc_fn(nmemb, size);
}

static char* edge_http_strdup(edge_http_context_t* ctx, const char* str) {
    if (!ctx) return NULL;
    return ctx->strdup_fn(str);
}

static void edge_http_set_error(edge_http_context_t* ctx, const char* format, ...) {
    if (!ctx) return;

    va_list args;
    va_start(args, format);
    vsnprintf(ctx->error_message, sizeof(ctx->error_message), format, args);
    va_end(args);
}

static void edge_http_clear_error(edge_http_context_t* ctx) {
    if (!ctx) return;
    ctx->error_message[0] = '\0';
}

const char* edge_http_get_error(edge_http_context_t* ctx) {
    return ctx->error_message[0] ? ctx->error_message : NULL;
}

edge_http_context_t* edge_http_create_context_default(void) {
    return edge_http_create_context(malloc, free, realloc, calloc, strdup);
}

edge_http_context_t* edge_http_create_context(edge_http_malloc_func pfn_malloc, edge_http_free_func pfn_free, edge_http_realloc_func pfn_realloc,
    edge_http_calloc_func pfn_calloc, edge_http_strdup_func pfn_strdup) {
    if (!pfn_malloc || !pfn_free || !pfn_realloc || !pfn_calloc || !pfn_strdup) return NULL;

    CURLcode res = curl_global_init_mem(
        CURL_GLOBAL_ALL,
        pfn_malloc,
        pfn_free,
        pfn_realloc,
        pfn_strdup,
        pfn_calloc
    );

    if (res != CURLE_OK) {
        return NULL;
    }

    struct edge_http_context* ctx = (struct edge_http_context*)pfn_malloc(sizeof(struct edge_http_context));
    if (!ctx) return NULL;

    ctx->malloc_fn = pfn_malloc;
    ctx->free_fn = pfn_free;
    ctx->realloc_fn = pfn_realloc;
    ctx->calloc_fn = pfn_calloc;
    ctx->strdup_fn = pfn_strdup;
    ctx->error_message[0] = '\0';

    return ctx;
}

void edge_http_destroy_context(edge_http_context_t* ctx) {
    if (!ctx) return;
    edge_http_free(ctx, ctx);
    curl_global_cleanup();
}

const char* edge_http_version(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d", EDGE_HTTP_VERSION_MAJOR, EDGE_HTTP_VERSION_MINOR, EDGE_HTTP_VERSION_PATCH);
    return version;
}

static edge_http_response_t* edge_http_response_create(edge_http_context_t* ctx) {
    edge_http_response_t* response = (edge_http_response_t*)edge_http_calloc(ctx, 1, sizeof(edge_http_response_t));
    if (!ctx || !response) {
        return NULL;
    }

    response->body = (char*)edge_http_malloc(ctx, 1);
    response->headers = (char*)edge_http_malloc(ctx, 1);
    if (!response->body || !response->headers) {
        edge_http_free(ctx, response->body);
        edge_http_free(ctx, response->headers);
        edge_http_free(ctx, response);
        return NULL;
    }

    response->body[0] = '\0';
    response->headers[0] = '\0';
    response->body_size = 0;
    response->headers_size = 0;
    response->status_code = 0;
    response->total_time = 0.0;
    response->download_speed = 0.0;
    response->curl_code = CURLE_OK;
    response->error_message[0] = '\0';

    response->ctx = ctx;

    return response;
}

void edge_http_response_free(edge_http_context_t* ctx, edge_http_response_t* response) {
    if (!ctx || !response) return;
    edge_http_free(ctx, response->body);
    edge_http_free(ctx, response->headers);
    edge_http_free(ctx, response);
}

static size_t write_body_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    edge_http_response_t* response = (edge_http_response_t*)userp;

    char* ptr = (char*)edge_http_realloc(response->ctx, response->body, response->body_size + realsize + 1);
    if (!ptr) return 0;

    response->body = ptr;
    memcpy(&(response->body[response->body_size]), contents, realsize);
    response->body_size += realsize;
    response->body[response->body_size] = '\0';

    return realsize;
}

static size_t write_header_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    edge_http_response_t* response = (edge_http_response_t*)userp;

    char* ptr = (char*)edge_http_realloc(response->ctx, response->headers, response->headers_size + realsize + 1);
    if (!ptr) return 0;

    response->headers = ptr;
    memcpy(&(response->headers[response->headers_size]), contents, realsize);
    response->headers_size += realsize;
    response->headers[response->headers_size] = '\0';

    return realsize;
}

static size_t read_body_callback(char* buffer, size_t size, size_t nitems, void* userp) {
    edge_http_request_t* request = (edge_http_request_t*)userp;
    size_t buffer_size = size * nitems;
    size_t remaining = request->body_size - request->body_read_offset;

    if (remaining == 0) return 0;

    size_t to_copy = (remaining < buffer_size) ? remaining : buffer_size;
    memcpy(buffer, request->body + request->body_read_offset, to_copy);
    request->body_read_offset += to_copy;

    return to_copy;
}

edge_http_request_t* edge_http_request_create(edge_http_context_t* ctx, const char* method, const char* url) {
    edge_http_request_t* request = (edge_http_request_t*)edge_http_calloc(ctx, 1, sizeof(edge_http_request_t));
    if (!request) return NULL;

    request->url = url ? edge_http_strdup(ctx, url) : NULL;
    request->method = method ? edge_http_strdup(ctx, method) : edge_http_strdup(ctx, "GET");
    request->body = NULL;
    request->body_size = 0;
    request->headers = NULL;
    request->timeout = 30L;
    request->connect_timeout = 10L;
    request->follow_redirects = 1;
    request->verbose = 0;
    request->user_agent = edge_http_strdup(ctx, "libedgehttp/1.0");

    request->easy_handle = NULL;
    request->response = NULL;
    request->body_read_offset = 0;
    request->callback = NULL;
    request->userdata = NULL;
    request->completed = 0;

    return request;
}

void edge_http_request_free(edge_http_context_t* ctx, edge_http_request_t* request) {
    if (!ctx || !request) return;

    edge_http_free(ctx, request->url);
    edge_http_free(ctx, request->method);
    edge_http_free(ctx, request->body);
    edge_http_free(ctx, request->user_agent);

    if (request->headers) {
        curl_slist_free_all(request->headers);
    }

    if (request->easy_handle) {
        curl_easy_cleanup(request->easy_handle);
    }

    if (request->response) {
        edge_http_response_free(ctx, request->response);
    }

    edge_http_free(ctx, request);
}

void edge_http_request_set_url(edge_http_context_t* ctx, edge_http_request_t* request, const char* url) {
    if (!ctx || !request || !url) return;
    edge_http_free(ctx, request->url);
    request->url = edge_http_strdup(ctx, url);
}

void edge_http_request_set_method(edge_http_context_t* ctx, edge_http_request_t* request, const char* method) {
    if (!ctx || !request || !method) return;
    edge_http_free(ctx, request->method);
    request->method = edge_http_strdup(ctx, method);
}

void edge_http_request_set_body(edge_http_context_t* ctx, edge_http_request_t* request, const char* body, size_t body_size) {
    if (!ctx || !request) return;

    edge_http_free(ctx, request->body);
    request->body = NULL;
    request->body_size = 0;

    if (body && body_size > 0) {
        request->body = (char*)edge_http_malloc(ctx, body_size);
        if (request->body) {
            memcpy(request->body, body, body_size);
            request->body_size = body_size;
        }
    }
}

void edge_http_request_add_header(edge_http_request_t* request, const char* header) {
    if (!request || !header) return;
    request->headers = curl_slist_append(request->headers, header);
}

void edge_http_request_set_timeout(edge_http_request_t* request, long timeout_seconds) {
    if (!request) return;
    request->timeout = timeout_seconds;
}

void edge_http_request_set_connect_timeout(edge_http_request_t* request, long timeout_seconds) {
    if (!request) return;
    request->connect_timeout = timeout_seconds;
}

void edge_http_request_set_user_agent(edge_http_context_t* ctx, edge_http_request_t* request, const char* user_agent) {
    if (!ctx || !request || !user_agent) return;
    edge_http_free(ctx, request->user_agent);
    request->user_agent = edge_http_strdup(ctx, user_agent);
}

void edge_http_request_set_verbose(edge_http_request_t* request, int verbose) {
    if (!request) return;
    request->verbose = verbose;
}

void edge_http_request_set_follow_redirects(edge_http_request_t* request, int follow) {
    if (!request) return;
    request->follow_redirects = follow;
}

void edge_http_request_set_callback(edge_http_request_t* request, edge_http_callback_func callback, void* userdata) {
    if (!request) return;
    request->callback = callback;
    request->userdata = userdata;
}

static int edge_http_request_setup_handle(edge_http_context_t* ctx, edge_http_request_t* request) {
    if (!ctx || !request || !request->url) return 0;

    if (!request->response) {
        request->response = edge_http_response_create(ctx);
        if (!request->response) return 0;
    }

    if (!request->easy_handle) {
        request->easy_handle = curl_easy_init();
        if (!request->easy_handle) return 0;
    }

    CURL* curl = request->easy_handle;
    edge_http_response_t* response = request->response;

    request->body_read_offset = 0;

    curl_easy_setopt(curl, CURLOPT_URL, request->url);

    if (strcmp(request->method, "GET") == 0) {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }
    else if (strcmp(request->method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (request->body) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)request->body_size);
        }
    }
    else if (strcmp(request->method, "PUT") == 0) {
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        if (request->body) {
            curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_body_callback);
            curl_easy_setopt(curl, CURLOPT_READDATA, request);
            curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)request->body_size);
        }
    }
    else if (strcmp(request->method, "DELETE") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    else if (strcmp(request->method, "PATCH") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        if (request->body) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)request->body_size);
        }
    }
    else if (strcmp(request->method, "HEAD") == 0) {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }
    else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request->method);
    }

    if (request->headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, request->headers);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, response);

    curl_easy_setopt(curl, CURLOPT_PRIVATE, request);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, (long)request->follow_redirects);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, request->timeout);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, request->connect_timeout);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, request->user_agent);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, response->error_message);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, (long)request->verbose);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    return 1;
}

edge_http_response_t* edge_http_request_perform(edge_http_context_t* ctx, edge_http_request_t* request) {
    if (!ctx) return NULL;

    if (!edge_http_request_setup_handle(ctx, request)) {
        return NULL;
    }

    request->response->curl_code = curl_easy_perform(request->easy_handle);

    if (request->response->curl_code == CURLE_OK) {
        curl_easy_getinfo(request->easy_handle, CURLINFO_RESPONSE_CODE, &request->response->status_code);
        curl_easy_getinfo(request->easy_handle, CURLINFO_TOTAL_TIME, &request->response->total_time);
        curl_easy_getinfo(request->easy_handle, CURLINFO_SPEED_DOWNLOAD, &request->response->download_speed);
    }

    edge_http_response_t* response_copy = edge_http_response_create(ctx);
    if (response_copy && request->response) {
        edge_http_free(ctx, response_copy->body);
        edge_http_free(ctx, response_copy->headers);

        response_copy->body = (char*)edge_http_malloc(ctx, request->response->body_size + 1);
        response_copy->headers = (char*)edge_http_malloc(ctx, request->response->headers_size + 1);

        if (response_copy->body && response_copy->headers) {
            memcpy(response_copy->body, request->response->body, request->response->body_size + 1);
            memcpy(response_copy->headers, request->response->headers, request->response->headers_size + 1);
            response_copy->body_size = request->response->body_size;
            response_copy->headers_size = request->response->headers_size;
            response_copy->status_code = request->response->status_code;
            response_copy->total_time = request->response->total_time;
            response_copy->download_speed = request->response->download_speed;
            response_copy->curl_code = request->response->curl_code;
            memcpy(response_copy->error_message, request->response->error_message, CURL_ERROR_SIZE);
        }
    }

    return response_copy;
}

edge_http_response_t* edge_http_get(edge_http_context_t* ctx, const char* url) {
    if (!ctx) return NULL;

    edge_http_request_t* request = edge_http_request_create(ctx, "GET", url);
    if (!request) return NULL;

    edge_http_response_t* response = edge_http_request_perform(ctx, request);
    edge_http_request_free(ctx, request);

    return response;
}

edge_http_response_t* edge_http_post(edge_http_context_t* ctx, const char* url, const char* body, size_t body_size) {
    if (!ctx) return NULL;

    edge_http_request_t* request = edge_http_request_create(ctx, "POST", url);
    if (!request) return NULL;

    edge_http_request_set_body(ctx, request, body, body_size);
    edge_http_response_t* response = edge_http_request_perform(ctx, request);
    edge_http_request_free(ctx, request);

    return response;
}

edge_http_response_t* edge_http_put(edge_http_context_t* ctx, const char* url, const char* body, size_t body_size) {
    if (!ctx) return NULL;

    edge_http_request_t* request = edge_http_request_create(ctx, "PUT", url);
    if (!request) return NULL;

    edge_http_request_set_body(ctx, request, body, body_size);
    edge_http_response_t* response = edge_http_request_perform(ctx, request);
    edge_http_request_free(ctx, request);

    return response;
}

edge_http_response_t* edge_http_delete(edge_http_context_t* ctx, const char* url) {
    if (!ctx) return NULL;

    edge_http_request_t* request = edge_http_request_create(ctx, "DELETE", url);
    if (!request) return NULL;

    edge_http_response_t* response = edge_http_request_perform(ctx, request);
    edge_http_request_free(ctx, request);

    return response;
}

edge_http_async_manager_t* edge_http_async_manager_create(edge_http_context_t* ctx) {
    if (!ctx) return NULL;

    edge_http_async_manager_t* manager = (edge_http_async_manager_t*)edge_http_calloc(ctx, 1, sizeof(edge_http_async_manager_t));
    if (!manager) return NULL;

    manager->multi_handle = curl_multi_init();
    if (!manager->multi_handle) {
        edge_http_free(ctx, manager);
        return NULL;
    }

    manager->capacity = 10;
    manager->requests = (edge_http_request_t**)edge_http_calloc(ctx, manager->capacity, sizeof(edge_http_request_t*));
    if (!manager->requests) {
        curl_multi_cleanup(manager->multi_handle);
        edge_http_free(ctx, manager);
        return NULL;
    }

    manager->num_requests = 0;
    manager->started = 0;

    return manager;
}

void edge_http_async_manager_free(edge_http_context_t* ctx, edge_http_async_manager_t* manager) {
    if (!ctx || !manager) return;

    for (int i = 0; i < manager->num_requests; i++) {
        edge_http_request_free(ctx, manager->requests[i]);
    }

    edge_http_free(ctx, manager->requests);
    curl_multi_cleanup(manager->multi_handle);
    edge_http_free(ctx, manager);
}

int edge_http_async_manager_add_request(edge_http_context_t* ctx, edge_http_async_manager_t* manager, edge_http_request_t* request) {
    if (!ctx || !manager || !request) return 0;

    if (manager->started) {
        return 0;
    }

    if (manager->num_requests >= manager->capacity) {
        int new_capacity = manager->capacity * 2;
        edge_http_request_t** new_requests = (edge_http_request_t**)edge_http_realloc(ctx, 
            manager->requests, new_capacity * sizeof(edge_http_request_t*));
        if (!new_requests) return 0;

        manager->requests = new_requests;
        manager->capacity = new_capacity;
    }

    if (!edge_http_request_setup_handle(ctx, request)) {
        return 0;
    }

    curl_multi_add_handle(manager->multi_handle, request->easy_handle);

    manager->requests[manager->num_requests++] = request;

    return 1;
}

void edge_http_async_manager_start(edge_http_async_manager_t* manager) {
    if (!manager || manager->started) return;

    manager->started = 1;
    int still_running = 0;
    curl_multi_perform(manager->multi_handle, &still_running);
}

int edge_http_async_manager_poll(edge_http_async_manager_t* manager) {
    if (!manager || !manager->started) return 0;

    int still_running = 0;

    curl_multi_poll(manager->multi_handle, NULL, 0, 0, NULL);
    curl_multi_perform(manager->multi_handle, &still_running);

    int msgs_left;
    CURLMsg* msg;
    while ((msg = curl_multi_info_read(manager->multi_handle, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            CURL* easy_handle = msg->easy_handle;
            edge_http_request_t* request = NULL;

            curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, &request);

            if (request && request->response) {
                request->completed = 1;
                request->response->curl_code = msg->data.result;

                if (request->response->curl_code == CURLE_OK) {
                    curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &request->response->status_code);
                    curl_easy_getinfo(easy_handle, CURLINFO_TOTAL_TIME, &request->response->total_time);
                    curl_easy_getinfo(easy_handle, CURLINFO_SPEED_DOWNLOAD, &request->response->download_speed);
                }

                if (request->callback) {
                    request->callback(request->response, request->userdata);
                }
            }

            curl_multi_remove_handle(manager->multi_handle, easy_handle);
        }
    }

    return still_running;
}

int edge_http_async_manager_is_done(edge_http_async_manager_t* manager) {
    return edge_http_async_manager_poll(manager) == 0;
}

void edge_http_async_manager_wait(edge_http_async_manager_t* manager) {
    if (!manager || !manager->started) return;

    while (edge_http_async_manager_poll(manager) > 0) {
        curl_multi_poll(manager->multi_handle, NULL, 0, 100, NULL);
    }
}