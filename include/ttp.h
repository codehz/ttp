#pragma once

#include <tbox/tbox.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TTP_REQUEST_UNKNOWN,
    TTP_REQUEST_GET,
    TTP_REQUEST_HEAD,
    TTP_REQUEST_POST,
    TTP_REQUEST_PUT,
    TTP_REQUEST_DELETE,
    TTP_REQUEST_CONNECT,
    TTP_REQUEST_OPTIONS,
    TTP_REQUEST_PATCH,
} ttp_request_method_t;

typedef struct {
    tb_hize_t begin, end;
} ttp_request_range_t, *ttp_request_range_ref_t;

typedef struct {
    tb_hize_t begin, end, full;
} ttp_response_content_range_t, *ttp_response_content_range_ref_t;

typedef enum {
    TTP_RESPONSE_ACCEPT_RANGES_UNDEF,
    TTP_RESPONSE_ACCEPT_RANGES_NONE,
    TTP_RESPONSE_ACCEPT_RANGES_BYTES,
} ttp_response_accept_ranges_t;

typedef enum {
    TTP_CONNECTION_UNSET,
    TTP_CONNECTION_CLOSE,
    TTP_CONNECTION_KEEPALIVE,
} ttp_connection_t;

typedef struct {
    ttp_request_method_t method;
    ttp_connection_t connection;
    tb_bool_t has_content_length;
    tb_hize_t content_length;
    ttp_request_range_ref_t range;
    tb_string_ref_t content_type;
    tb_string_ref_t path;
    tb_string_ref_t host;
    tb_string_ref_t user_agent;
    tb_hash_map_ref_t addition;
} ttp_request_t, *ttp_request_ref_t;

typedef struct {
    tb_ushort_t status_code;
    ttp_connection_t connection;
    tb_bool_t has_content_length;
    tb_hize_t content_length;
    ttp_response_content_range_ref_t content_range;
    ttp_response_accept_ranges_t accept_ranges;
    tb_string_ref_t content_type;
    tb_string_ref_t location;
    tb_string_ref_t upgrade;
    tb_hash_map_ref_t addition;
} ttp_response_t, *ttp_response_ref_t;

tb_void_t ttp_init();

tb_ushort_t ttp_request_parse(ttp_request_ref_t req, tb_stream_ref_t stream);
tb_void_t ttp_request_exit(ttp_request_ref_t req);

tb_void_t ttp_response_init(ttp_response_ref_t res);
tb_void_t ttp_response_dump(tb_stream_ref_t stream, ttp_response_ref_t res);
tb_void_t ttp_response_exit(ttp_response_ref_t res);

tb_void_t ttp_response_set_content_length(ttp_response_ref_t res, tb_hize_t length);
tb_void_t ttp_response_set_content_range(ttp_response_ref_t res, tb_hize_t begin, tb_hize_t end, tb_hize_t full);
tb_void_t ttp_response_append(ttp_response_ref_t res, tb_char_t const *key, tb_char_t const *value);
tb_void_t ttp_response_appendf(ttp_response_ref_t res, tb_char_t const *key, tb_char_t const *format, ...);

#ifdef __cplusplus
}
#endif
