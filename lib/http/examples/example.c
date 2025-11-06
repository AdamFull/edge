/*
 * libedgehttp - Example Usage
 * Demonstrates synchronous and async requests
 */

#include "httpc.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Callback for async requests */
static void on_request_complete(edge_http_response_t* response, void* userdata) {
    const char* label = (const char*)userdata;
    
    printf("\n[%s] Request complete!\n", label);
    if (response->curl_code == CURLE_OK) {
        printf("  Status: %ld\n", response->status_code);
        printf("  Size: %zu bytes\n", response->body_size);
        printf("  Time: %.3f seconds\n", response->total_time);
    } else {
        printf("  Error: %s\n", response->error_message);
    }
}

/* Simple sleep function for demonstration */
static void sleep_ms(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

int main(void) {
    /* Initialize library */
    if (!edge_http_global_init()) {
        fprintf(stderr, "Failed to initialize library\n");
        return 1;
    }
    
    printf("=== libedgehttp Example ===\n");
    printf("Version: %s\n\n", edge_http_version());
    
    /* ========================================================================
     * Example 1: Simple Synchronous GET Request
     * ======================================================================== */
    
    printf("\n>>> Example 1: Simple GET Request <<<\n");
    {
        edge_http_response_t* response = edge_http_get("https://httpbin.org/get");
        
        if (response) {
            if (response->curl_code == CURLE_OK && response->status_code == 200) {
                printf("Success! Got %zu bytes\n", response->body_size);
                printf("First 100 chars: %.100s\n", response->body);
            } else {
                printf("Failed: %s\n", response->error_message);
            }
            edge_http_response_free(response);
        }
    }
    
    /* ========================================================================
     * Example 2: POST Request with JSON
     * ======================================================================== */
    
    printf("\n>>> Example 2: POST with JSON <<<\n");
    {
        edge_http_request_t* request = edge_http_request_create("POST", "https://httpbin.org/post");
        
        /* Add headers */
        edge_http_request_add_header(request, "Content-Type: application/json");
        edge_http_request_add_header(request, "X-Custom-Header: Example");
        
        /* Set body */
        const char* json = "{\"name\":\"John Doe\",\"email\":\"john@example.com\"}";
        edge_http_request_set_body(request, json, strlen(json));
        
        /* Perform request */
        edge_http_response_t* response = edge_http_request_perform(request);
        
        if (response) {
            printf("Status: %ld\n", response->status_code);
            printf("Response size: %zu bytes\n", response->body_size);
            edge_http_response_free(response);
        }
        
        edge_http_request_free(request);
    }
    
    /* ========================================================================
     * Example 3: Multiple Methods
     * ======================================================================== */
    
    printf("\n>>> Example 3: Different HTTP Methods <<<\n");
    {
        const char* data = "{\"status\":\"updated\"}";
        
        /* PUT */
        edge_http_response_t* put_response = edge_http_put("https://httpbin.org/put", 
                                                    data, strlen(data));
        if (put_response) {
            printf("PUT Status: %ld\n", put_response->status_code);
            edge_http_response_free(put_response);
        }
        
        /* DELETE */
        edge_http_response_t* del_response = edge_http_delete("https://httpbin.org/delete");
        if (del_response) {
            printf("DELETE Status: %ld\n", del_response->status_code);
            edge_http_response_free(del_response);
        }
    }
    
    /* ========================================================================
     * Example 4: Custom Configuration
     * ======================================================================== */
    
    printf("\n>>> Example 4: Custom Configuration <<<\n");
    {
        edge_http_request_t* request = edge_http_request_create("GET", "https://httpbin.org/delay/2");
        
        edge_http_request_set_timeout(request, 5);
        edge_http_request_set_user_agent(request, "MyCustomApp/1.0");
        edge_http_request_set_follow_redirects(request, 1);
        edge_http_request_add_header(request, "Accept: application/json");
        
        edge_http_response_t* response = edge_http_request_perform(request);
        
        if (response) {
            printf("Status: %ld, Time: %.3fs\n", 
                   response->status_code, response->total_time);
            edge_http_response_free(response);
        }
        
        edge_http_request_free(request);
    }
    
    /* ========================================================================
     * Example 5: Truly Async Requests (Non-Blocking)
     * ======================================================================== */
    
    printf("\n>>> Example 5: Truly Async Requests (Non-Blocking) <<<\n");
    {
        edge_http_async_manager_t* manager = edge_http_async_manager_create();
        
        /* Add requests */
        for (int i = 1; i <= 3; i++) {
            char url[128];
            snprintf(url, sizeof(url), "https://httpbin.org/delay/1");
            
            edge_http_request_t* request = edge_http_request_create("GET", url);
            
            char* label = malloc(32);
            snprintf(label, 32, "Async-%d", i);
            edge_http_request_set_callback(request, on_request_complete, label);
            
            edge_http_async_manager_add_request(manager, request);
        }
        
        printf("Starting async requests (non-blocking)...\n");
        edge_http_async_manager_start(manager);
        printf("Started! Main thread continues...\n\n");
        
        /* Main thread continues and does other work */
        int iteration = 0;
        while (!edge_http_async_manager_is_done(manager)) {
            printf("Main thread: Doing work (iteration %d)\n", ++iteration);
            
            /* Poll for updates */
            int active = edge_http_async_manager_poll(manager);
            printf("Main thread: %d requests still active\n", active);
            
            /* Simulate work */
            sleep_ms(300);
        }
        
        printf("\nAll async requests complete!\n");
        printf("Main thread was NEVER blocked!\n");
        
        /* Free labels */
        for (int i = 0; i < 3; i++) {
            if (manager->requests[i]->userdata) {
                free(manager->requests[i]->userdata);
            }
        }
        
        edge_http_async_manager_free(manager);
    }
    
    /* ========================================================================
     * Example 6: Error Handling
     * ======================================================================== */
    
    printf("\n>>> Example 6: Error Handling <<<\n");
    {
        edge_http_response_t* response = edge_http_get("https://invalid-url-12345.com");
        
        if (response) {
            if (response->curl_code != CURLE_OK) {
                printf("Expected error occurred: %s\n", response->error_message);
            } else if (response->status_code >= 400) {
                printf("HTTP Error: %ld\n", response->status_code);
            }
            edge_http_response_free(response);
        }
    }
    
    /* Cleanup */
    edge_http_global_cleanup();
    
    printf("\n=== All examples completed! ===\n");
    
    return 0;
}
