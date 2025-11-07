/*
 * libedgehttp - WebSocket Client
 *
 * WebSocket implementation using libcurl's WebSocket support
 * Requires libcurl 7.86.0 or later
 *
 * License: MIT
 */

#ifndef EDGE_HTTP_WEBSOCKET_H
#define EDGE_HTTP_WEBSOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <curl/curl.h>

typedef struct edge_allocator edge_allocator_t;

typedef enum {
    EDGE_HTTP_WS_FRAME_TEXT = 0x01,  /* Text frame */
    EDGE_HTTP_WS_FRAME_BINARY = 0x02,  /* Binary frame */
    EDGE_HTTP_WS_FRAME_CLOSE = 0x08,  /* Close frame */
    EDGE_HTTP_WS_FRAME_PING = 0x09,  /* Ping frame */
    EDGE_HTTP_WS_FRAME_PONG = 0x0A   /* Pong frame */
} edge_http_ws_frame_type_t;

typedef struct edge_http_websocket edge_http_websocket_t;

/* WebSocket message callback */
typedef void (*edge_http_ws_message_callback)(edge_http_websocket_t* ws, const void* data, size_t size, edge_http_ws_frame_type_t type, void* userdata);

/* WebSocket close callback */
typedef void (*edge_http_ws_close_callback)(edge_http_websocket_t* ws, int status_code, const char* reason, void* userdata);

/* WebSocket error callback */
typedef void (*edge_http_ws_error_callback)(
    edge_http_websocket_t* ws,
    const char* error,
    void* userdata
    );

/* WebSocket connect callback */
typedef void (*edge_http_ws_connect_callback)(
    edge_http_websocket_t* ws,
    void* userdata
    );

/**
 * Create a new WebSocket connection
 *
 * @param url WebSocket URL (ws:// or wss://)
 * @return WebSocket object, or NULL on failure
 */
edge_http_websocket_t* edge_http_websocket_create(const char* url, const edge_allocator_t* allocator);

/**
 * Free WebSocket and all associated resources
 *
 * @param ws WebSocket to free
 */
void edge_http_websocket_free(edge_http_websocket_t* ws);

/**
 * Set custom headers for the WebSocket handshake
 * Header should be in format "Name: Value"
 *
 * @param ws WebSocket object
 * @param header Header string
 */
void edge_http_websocket_add_header(edge_http_websocket_t* ws, const char* header);

/**
 * Set connection timeout
 *
 * @param ws WebSocket object
 * @param timeout_seconds Timeout in seconds
 */
void edge_http_websocket_set_timeout(edge_http_websocket_t* ws, long timeout_seconds);

/**
 * Set user agent
 *
 * @param ws WebSocket object
 * @param user_agent User agent string
 */
void edge_http_websocket_set_user_agent(edge_http_websocket_t* ws, const char* user_agent);

/**
 * Enable or disable verbose output
 *
 * @param ws WebSocket object
 * @param verbose 1 to enable, 0 to disable
 */
void edge_http_websocket_set_verbose(edge_http_websocket_t* ws, int verbose);

/**
 * Set subprotocols for WebSocket handshake
 *
 * @param ws WebSocket object
 * @param protocols Comma-separated list of protocols (e.g., "chat, superchat")
 */
void edge_http_websocket_set_protocols(edge_http_websocket_t* ws, const char* protocols);

/**
 * Set user data pointer
 *
 * @param ws WebSocket object
 * @param userdata User data pointer
 */
void edge_http_websocket_set_userdata(edge_http_websocket_t* ws, void* userdata);

/**
 * Get user data pointer
 *
 * @param ws WebSocket object
 * @return User data pointer
 */
void* edge_http_websocket_get_userdata(edge_http_websocket_t* ws);

/**
 * Set message received callback
 *
 * @param ws WebSocket object
 * @param callback Callback function
 */
void edge_http_websocket_set_message_callback(edge_http_websocket_t* ws, edge_http_ws_message_callback callback);

/**
 * Set connection established callback
 *
 * @param ws WebSocket object
 * @param callback Callback function
 */
void edge_http_websocket_set_connect_callback(edge_http_websocket_t* ws, edge_http_ws_connect_callback callback);

/**
 * Set close callback
 *
 * @param ws WebSocket object
 * @param callback Callback function
 */
void edge_http_websocket_set_close_callback(edge_http_websocket_t* ws, edge_http_ws_close_callback callback);

/**
 * Set error callback
 *
 * @param ws WebSocket object
 * @param callback Callback function
 */
void edge_http_websocket_set_error_callback(edge_http_websocket_t* ws, edge_http_ws_error_callback callback);

/**
 * Start WebSocket connection (non-blocking)
 * Returns immediately after initiating connection
 *
 * @param ws WebSocket object
 * @return 0 on success, -1 on error
 */
int edge_http_websocket_connect(edge_http_websocket_t* ws);

/**
 * Poll for WebSocket events (non-blocking)
 * Call this periodically to process messages
 *
 * @param ws WebSocket object
 * @param timeout_ms Timeout in milliseconds (0 for immediate return)
 * @return 1 if still connected, 0 if closed, -1 on error
 */
int edge_http_websocket_poll(edge_http_websocket_t* ws, int timeout_ms);

/**
 * Check if WebSocket is connected
 *
 * @param ws WebSocket object
 * @return 1 if connected, 0 if not
 */
int edge_http_websocket_is_connected(edge_http_websocket_t* ws);

/**
 * Send text message
 *
 * @param ws WebSocket object
 * @param text Text to send (null-terminated)
 * @return Number of bytes sent, or -1 on error
 */
int edge_http_websocket_send_text(edge_http_websocket_t* ws, const char* text);

/**
 * Send binary message
 *
 * @param ws WebSocket object
 * @param data Binary data
 * @param size Size of data
 * @return Number of bytes sent, or -1 on error
 */
int edge_http_websocket_send_binary(edge_http_websocket_t* ws, const void* data, size_t size);

/**
 * Send ping
 *
 * @param ws WebSocket object
 * @param payload Optional ping payload
 * @param size Size of payload (can be 0)
 * @return 0 on success, -1 on error
 */
int edge_http_websocket_send_ping(edge_http_websocket_t* ws, const void* payload, size_t size);

/**
 * Send pong (usually in response to ping)
 *
 * @param ws WebSocket object
 * @param payload Optional pong payload
 * @param size Size of payload (can be 0)
 * @return 0 on success, -1 on error
 */
int edge_http_websocket_send_pong(edge_http_websocket_t* ws, const void* payload, size_t size);

/**
 * Send close frame
 *
 * @param ws WebSocket object
 * @param status_code Close status code (e.g., 1000 for normal)
 * @param reason Optional close reason
 * @return 0 on success, -1 on error
 */
int edge_http_websocket_send_close(edge_http_websocket_t* ws, int status_code, const char* reason);

#define EDGE_HTTP_WS_CLOSE_NORMAL           1000  /* Normal closure */
#define EDGE_HTTP_WS_CLOSE_GOING_AWAY       1001  /* Endpoint going away */
#define EDGE_HTTP_WS_CLOSE_PROTOCOL_ERROR   1002  /* Protocol error */
#define EDGE_HTTP_WS_CLOSE_UNSUPPORTED      1003  /* Unsupported data */
#define EDGE_HTTP_WS_CLOSE_NO_STATUS        1005  /* No status code */
#define EDGE_HTTP_WS_CLOSE_ABNORMAL         1006  /* Abnormal closure */
#define EDGE_HTTP_WS_CLOSE_INVALID_DATA     1007  /* Invalid frame data */
#define EDGE_HTTP_WS_CLOSE_POLICY           1008  /* Policy violation */
#define EDGE_HTTP_WS_CLOSE_TOO_LARGE        1009  /* Message too large */
#define EDGE_HTTP_WS_CLOSE_MANDATORY_EXT    1010  /* Mandatory extension */
#define EDGE_HTTP_WS_CLOSE_INTERNAL_ERROR   1011  /* Internal error */
#define EDGE_HTTP_WS_CLOSE_TLS_HANDSHAKE    1015  /* TLS handshake failure */

/**
 * Simple blocking WebSocket client
 * Connects, runs until closed, blocking the calling thread
 *
 * @param url WebSocket URL
 * @param on_message Message callback
 * @param userdata User data for callbacks
 * @return 0 on success, -1 on error
 */
int edge_http_websocket_run(const char* url, edge_http_ws_message_callback on_message, void* userdata, const edge_allocator_t* allocator);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_HTTP_WEBSOCKET_H */