/*
 * libedgehttp - WebSocket Client Implementation
 */

#include "edge_http_websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

 /* Check if libcurl has WebSocket support */
#if LIBCURL_VERSION_NUM < 0x075600
#error "libcurl 7.86.0 or later is required for WebSocket support"
#endif

/* Forward declarations for allocator functions from edge_http.c */
/* These will be linked from the main library */
extern void* edge_http_malloc(size_t size);
extern void edge_http_free(void* ptr);
extern void* edge_http_realloc(void* ptr, size_t size);
extern void* edge_http_calloc(size_t nmemb, size_t size);
extern char* edge_http_strdup(const char* str);

struct edge_http_websocket {
    char* url;
    CURL* curl;
    struct curl_slist* headers;
    char* protocols;
    char* user_agent;
    long timeout;
    int verbose;
    int connected;
    int should_close;

    /* Callbacks */
    edge_http_ws_message_callback message_callback;
    edge_http_ws_connect_callback connect_callback;
    edge_http_ws_close_callback close_callback;
    edge_http_ws_error_callback error_callback;
    void* userdata;

    /* Receive buffer */
    unsigned char* recv_buffer;
    size_t recv_buffer_size;
    size_t recv_buffer_capacity;

    /* Error message */
    char error_buffer[CURL_ERROR_SIZE];
};

static void edge_http_websocket_handle_error(edge_http_websocket_t* ws, const char* error) {
    if (ws->error_callback) {
        ws->error_callback(ws, error, ws->userdata);
    }
}

static void edge_http_websocket_handle_close(edge_http_websocket_t* ws, int status_code, const char* reason) {
    ws->connected = 0;
    if (ws->close_callback) {
        ws->close_callback(ws, status_code, reason, ws->userdata);
    }
}

static void edge_http_websocket_handle_message(edge_http_websocket_t* ws, const void* data, size_t size, edge_http_ws_frame_type_t type) {
    if (ws->message_callback) {
        ws->message_callback(ws, data, size, type, ws->userdata);
    }
}

edge_http_websocket_t* edge_http_websocket_create(const char* url) {
    if (!url) return NULL;

    /* Check URL scheme */
    if (strncmp(url, "ws://", 5) != 0 && strncmp(url, "wss://", 6) != 0) {
        fprintf(stderr, "WebSocket URL must start with ws:// or wss://\n");
        return NULL;
    }

    edge_http_websocket_t* ws = (edge_http_websocket_t*)edge_http_calloc(1, sizeof(edge_http_websocket_t));
    if (!ws) return NULL;

    ws->url = edge_http_strdup(url);
    ws->headers = NULL;
    ws->protocols = NULL;
    ws->user_agent = edge_http_strdup("libedgehttp-websocket/1.0");
    ws->timeout = 30L;
    ws->verbose = 0;
    ws->connected = 0;
    ws->should_close = 0;

    ws->message_callback = NULL;
    ws->connect_callback = NULL;
    ws->close_callback = NULL;
    ws->error_callback = NULL;
    ws->userdata = NULL;

    ws->recv_buffer_capacity = 8192;
    ws->recv_buffer = (unsigned char*)edge_http_malloc(ws->recv_buffer_capacity);
    ws->recv_buffer_size = 0;

    ws->curl = NULL;
    ws->error_buffer[0] = '\0';

    if (!ws->url || !ws->user_agent || !ws->recv_buffer) {
        edge_http_websocket_free(ws);
        return NULL;
    }

    return ws;
}

void edge_http_websocket_free(edge_http_websocket_t* ws) {
    if (!ws) return;

    if (ws->curl) {
        curl_easy_cleanup(ws->curl);
    }

    edge_http_free(ws->url);
    edge_http_free(ws->protocols);
    edge_http_free(ws->user_agent);
    edge_http_free(ws->recv_buffer);

    if (ws->headers) {
        curl_slist_free_all(ws->headers);
    }

    edge_http_free(ws);
}

void edge_http_websocket_add_header(edge_http_websocket_t* ws, const char* header) {
    if (!ws || !header) return;
    ws->headers = curl_slist_append(ws->headers, header);
}

void edge_http_websocket_set_timeout(edge_http_websocket_t* ws, long timeout_seconds) {
    if (!ws) return;
    ws->timeout = timeout_seconds;
}

void edge_http_websocket_set_user_agent(edge_http_websocket_t* ws, const char* user_agent) {
    if (!ws || !user_agent) return;
    edge_http_free(ws->user_agent);
    ws->user_agent = edge_http_strdup(user_agent);
}

void edge_http_websocket_set_verbose(edge_http_websocket_t* ws, int verbose) {
    if (!ws) return;
    ws->verbose = verbose;
}

void edge_http_websocket_set_protocols(edge_http_websocket_t* ws, const char* protocols) {
    if (!ws || !protocols) return;
    edge_http_free(ws->protocols);
    ws->protocols = edge_http_strdup(protocols);
}

void edge_http_websocket_set_userdata(edge_http_websocket_t* ws, void* userdata) {
    if (!ws) return;
    ws->userdata = userdata;
}

void* edge_http_websocket_get_userdata(edge_http_websocket_t* ws) {
    return ws ? ws->userdata : NULL;
}

void edge_http_websocket_set_message_callback(edge_http_websocket_t* ws,
    edge_http_ws_message_callback callback) {
    if (!ws) return;
    ws->message_callback = callback;
}

void edge_http_websocket_set_connect_callback(edge_http_websocket_t* ws,
    edge_http_ws_connect_callback callback) {
    if (!ws) return;
    ws->connect_callback = callback;
}

void edge_http_websocket_set_close_callback(edge_http_websocket_t* ws,
    edge_http_ws_close_callback callback) {
    if (!ws) return;
    ws->close_callback = callback;
}

void edge_http_websocket_set_error_callback(edge_http_websocket_t* ws,
    edge_http_ws_error_callback callback) {
    if (!ws) return;
    ws->error_callback = callback;
}

static int edge_http_websocket_setup_curl(edge_http_websocket_t* ws) {
    if (!ws) return -1;

    if (ws->curl) {
        curl_easy_cleanup(ws->curl);
    }

    ws->curl = curl_easy_init();
    if (!ws->curl) {
        edge_http_websocket_handle_error(ws, "Failed to initialize CURL");
        return -1;
    }

    /* Set URL */
    curl_easy_setopt(ws->curl, CURLOPT_URL, ws->url);

    /* Enable WebSocket */
    curl_easy_setopt(ws->curl, CURLOPT_CONNECT_ONLY, 2L); /* 2 = WebSocket */

    /* Set headers */
    if (ws->headers) {
        curl_easy_setopt(ws->curl, CURLOPT_HTTPHEADER, ws->headers);
    }

    /* Set protocols if specified */
    if (ws->protocols) {
        char* protocol_header = (char*)edge_http_malloc(strlen(ws->protocols) + 32);
        if (protocol_header) {
            sprintf(protocol_header, "Sec-WebSocket-Protocol: %s", ws->protocols);
            ws->headers = curl_slist_append(ws->headers, protocol_header);
            curl_easy_setopt(ws->curl, CURLOPT_HTTPHEADER, ws->headers);
            edge_http_free(protocol_header);
        }
    }

    /* Set options */
    curl_easy_setopt(ws->curl, CURLOPT_TIMEOUT, ws->timeout);
    curl_easy_setopt(ws->curl, CURLOPT_USERAGENT, ws->user_agent);
    curl_easy_setopt(ws->curl, CURLOPT_ERRORBUFFER, ws->error_buffer);
    curl_easy_setopt(ws->curl, CURLOPT_VERBOSE, (long)ws->verbose);

    /* SSL options */
    curl_easy_setopt(ws->curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(ws->curl, CURLOPT_SSL_VERIFYHOST, 2L);

    return 0;
}

int edge_http_websocket_connect(edge_http_websocket_t* ws) {
    if (!ws) return -1;

    if (edge_http_websocket_setup_curl(ws) != 0) {
        return -1;
    }

    /* Perform connection handshake */
    CURLcode res = curl_easy_perform(ws->curl);

    if (res != CURLE_OK) {
        edge_http_websocket_handle_error(ws, ws->error_buffer[0] ?
            ws->error_buffer : curl_easy_strerror(res));
        return -1;
    }

    ws->connected = 1;

    /* Call connect callback */
    if (ws->connect_callback) {
        ws->connect_callback(ws, ws->userdata);
    }

    return 0;
}

int edge_http_websocket_is_connected(edge_http_websocket_t* ws) {
    return ws ? ws->connected : 0;
}

int edge_http_websocket_poll(edge_http_websocket_t* ws, int timeout_ms) {
    if (!ws || !ws->connected || !ws->curl) return -1;

    /* Receive data */
    size_t received;
    const struct curl_ws_frame* meta;

    CURLcode res = curl_ws_recv(ws->curl, ws->recv_buffer,
        ws->recv_buffer_capacity, &received, &meta);

    if (res == CURLE_AGAIN) {
        /* No data available, not an error */
        return 1;
    }

    if (res != CURLE_OK) {
        if (res == CURLE_GOT_NOTHING) {
            /* Connection closed */
            edge_http_websocket_handle_close(ws, EDGE_HTTP_WS_CLOSE_NORMAL, "Connection closed");
            return 0;
        }

        edge_http_websocket_handle_error(ws, ws->error_buffer[0] ? ws->error_buffer : curl_easy_strerror(res));
        ws->connected = 0;
        return -1;
    }

    if (received > 0 && meta) {
        /* Handle frame based on type */
        if (meta->flags & CURLWS_CLOSE) {
            /* Close frame */
            int status_code = EDGE_HTTP_WS_CLOSE_NORMAL;
            const char* reason = "";

            if (received >= 2) {
                status_code = (ws->recv_buffer[0] << 8) | ws->recv_buffer[1];
                if (received > 2) {
                    reason = (const char*)(ws->recv_buffer + 2);
                }
            }

            edge_http_websocket_handle_close(ws, status_code, reason);
            return 0;
        }
        else if (meta->flags & CURLWS_PING) {
            /* Ping - auto respond with pong */
            edge_http_websocket_send_pong(ws, ws->recv_buffer, received);
        }
        else if (meta->flags & CURLWS_PONG) {
            /* Pong - notify via message callback */
            edge_http_websocket_handle_message(ws, ws->recv_buffer, received, EDGE_HTTP_WS_FRAME_PONG);
        }
        else if (meta->flags & CURLWS_TEXT) {
            /* Text frame */
            edge_http_websocket_handle_message(ws, ws->recv_buffer, received, EDGE_HTTP_WS_FRAME_TEXT);
        }
        else if (meta->flags & CURLWS_BINARY) {
            /* Binary frame */
            edge_http_websocket_handle_message(ws, ws->recv_buffer, received, EDGE_HTTP_WS_FRAME_BINARY);
        }
    }

    return ws->connected ? 1 : 0;
}

int edge_http_websocket_send_text(edge_http_websocket_t* ws, const char* text) {
    if (!ws || !ws->connected || !ws->curl || !text) return -1;

    size_t sent;
    CURLcode res = curl_ws_send(ws->curl, text, strlen(text), &sent, 0, CURLWS_TEXT);

    if (res != CURLE_OK) {
        edge_http_websocket_handle_error(ws, ws->error_buffer[0] ? ws->error_buffer : curl_easy_strerror(res));
        return -1;
    }

    return (int)sent;
}

int edge_http_websocket_send_binary(edge_http_websocket_t* ws, const void* data, size_t size) {
    if (!ws || !ws->connected || !ws->curl || !data) return -1;

    size_t sent;
    CURLcode res = curl_ws_send(ws->curl, data, size, &sent, 0, CURLWS_BINARY);

    if (res != CURLE_OK) {
        edge_http_websocket_handle_error(ws, ws->error_buffer[0] ? ws->error_buffer : curl_easy_strerror(res));
        return -1;
    }

    return (int)sent;
}

int edge_http_websocket_send_ping(edge_http_websocket_t* ws, const void* payload, size_t size) {
    if (!ws || !ws->connected || !ws->curl) return -1;

    size_t sent;
    CURLcode res = curl_ws_send(ws->curl, payload, size, &sent, 0, CURLWS_PING);

    if (res != CURLE_OK) {
        edge_http_websocket_handle_error(ws, ws->error_buffer[0] ? ws->error_buffer : curl_easy_strerror(res));
        return -1;
    }

    return 0;
}

int edge_http_websocket_send_pong(edge_http_websocket_t* ws, const void* payload, size_t size) {
    if (!ws || !ws->connected || !ws->curl) return -1;

    size_t sent;
    CURLcode res = curl_ws_send(ws->curl, payload, size, &sent, 0, CURLWS_PONG);

    if (res != CURLE_OK) {
        edge_http_websocket_handle_error(ws, ws->error_buffer[0] ? ws->error_buffer : curl_easy_strerror(res));
        return -1;
    }

    return 0;
}

int edge_http_websocket_send_close(edge_http_websocket_t* ws, int status_code, const char* reason) {
    if (!ws || !ws->curl) return -1;

    /* Prepare close frame payload */
    size_t reason_len = reason ? strlen(reason) : 0;
    size_t payload_size = 2 + reason_len;
    unsigned char* payload = (unsigned char*)edge_http_malloc(payload_size);

    if (!payload) return -1;

    /* Status code (2 bytes, big-endian) */
    payload[0] = (status_code >> 8) & 0xFF;
    payload[1] = status_code & 0xFF;

    /* Reason */
    if (reason_len > 0) {
        memcpy(payload + 2, reason, reason_len);
    }

    size_t sent;
    CURLcode res = curl_ws_send(ws->curl, payload, payload_size, &sent, 0, CURLWS_CLOSE);

    edge_http_free(payload);

    if (res != CURLE_OK) {
        return -1;
    }

    ws->should_close = 1;
    ws->connected = 0;

    return 0;
}

int edge_http_websocket_run(const char* url, edge_http_ws_message_callback on_message, void* userdata) {
    edge_http_websocket_t* ws = edge_http_websocket_create(url);
    if (!ws) return -1;

    edge_http_websocket_set_message_callback(ws, on_message);
    edge_http_websocket_set_userdata(ws, userdata);

    /* Connect */
    if (edge_http_websocket_connect(ws) != 0) {
        return -1;
    }

    /* Event loop - poll until disconnected */
    while (ws->connected && !ws->should_close) {
        int result = edge_http_websocket_poll(ws, 100);

        if (result <= 0) {
            break;
        }

        /* Small sleep to avoid busy-waiting */
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 10000000; /* 10ms */
        nanosleep(&ts, NULL);
    }

    /* Send close if we're initiating the close */
    if (ws->should_close && ws->connected) {
        edge_http_websocket_send_close(ws, EDGE_HTTP_WS_CLOSE_NORMAL, "Client closing");
    }

    edge_http_websocket_free(ws);

    return 0;
}