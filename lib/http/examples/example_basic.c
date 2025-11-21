#include <edge_http.h>
#include <edge_allocator.h>

int main(void) {
    /* Initialize */
    edge_allocator_t allocator = edge_allocator_create_default();

    edge_http_initialize_global_context(&allocator);

    /* Simple GET request */
    edge_http_response_t* response = edge_http_get("http://api.example.com/data", &allocator);

    if (response && response->curl_code == CURLE_OK) {
        printf("Status: %ld\n", response->status_code);
        printf("Body: %s\n", response->body);
    }

    edge_http_response_free(response);
    edge_http_gleanup_global();

    return 0;
}
