#include "../include/ttp.h"

// #include "evilgcc.h"
#include "status_code.h"

static inline tb_bool_t
compare(tb_char_t const *restrict a, tb_size_t a_len, tb_char_t const *restrict b, tb_size_t b_len) {
    return a_len == b_len && !tb_strnicmp(a, b, a_len);
}

#define do_compare(start, end, target)                                                                                 \
    compare((tb_char_t const *) start, end - start, (tb_char_t const *) target, sizeof target - 1)

#define make_string(name, cstr)                                                                                        \
    tb_string_ref_t name = tb_malloc0(sizeof(tb_string_t));                                                            \
    tb_assert(name);                                                                                                   \
    tb_string_init(name);                                                                                              \
    tb_string_cstrcpy(name, cstr)

#define make_string_(cstr)                                                                                             \
    ({                                                                                                                 \
        make_string(_, cstr);                                                                                          \
        _;                                                                                                             \
    })

inline static tb_char_t const *get_connection_str(ttp_connection_t conn) {
    switch (conn) {
    case TTP_CONNECTION_CLOSE: return "closed";
    case TTP_CONNECTION_KEEPALIVE: return "keep-alive";
    case TTP_CONNECTION_UNSET: return tb_null;
    }
    return tb_null;
}

static tb_char_t const *get_method_name(ttp_request_method_t method) {
    switch (method) {
    case TTP_REQUEST_GET: return "GET";
    case TTP_REQUEST_HEAD: return "HEAD";
    case TTP_REQUEST_PUT: return "PUT";
    case TTP_REQUEST_POST: return "POST";
    case TTP_REQUEST_PATCH: return "PATCH";
    case TTP_REQUEST_DELETE: return "DELETE";
    case TTP_REQUEST_CONNECT: return "CONNECT";
    case TTP_REQUEST_OPTIONS: return "OPTIONS";
    default: return "UNKNOWN";
    }
}

static tb_char_t const *dump_str_maybe(tb_string_ref_t str) {
    if (str) return tb_string_cstr(str);
    return "";
}

static tb_long_t print_request(tb_cpointer_t object, tb_char_t *cstr, tb_size_t maxn) {
    tb_assert_and_check_return_val(object, -1);
    ttp_request_ref_t req = (ttp_request_ref_t) object;

    tb_string_t buf = {};
    tb_string_init(&buf);
    tb_string_cstrfcat(&buf, "%s [%s]@%s\n", get_method_name(req->method), dump_str_maybe(req->path), dump_str_maybe(req->host));
    if (req->connection) tb_string_cstrfcat(&buf, "Connection: %s\n", get_connection_str(req->connection));
    if (req->content_type) tb_string_cstrfcat(&buf, "Content-Type: %llu\n", tb_string_cstr(req->content_type));
    if (req->has_content_length) tb_string_cstrfcat(&buf, "Content-Length: %llu\n", req->content_length);
    if (req->range) tb_string_cstrfcat(&buf, "Range: bytes=%lld-%lld\n", req->range->begin, req->range->end);
    if (req->user_agent) tb_string_cstrfcat(&buf, "User-Agent: %s\n", tb_string_cstr(req->user_agent));
    if (req->addition) {
        tb_for_all(tb_hash_map_item_ref_t, a, req->addition) {
            tb_string_cstrfcat(&buf, "%s: %s\n", a->name, a->data);
        }
    }
    cstr = tb_strncpy(cstr, tb_string_cstr(&buf), maxn);
    tb_string_exit(&buf);
    return cstr? tb_strlen(cstr) : -1;
}

tb_void_t ttp_init() { tb_printf_object_register("ttp_request", print_request); }

tb_ushort_t ttp_request_parse(ttp_request_ref_t req, tb_stream_ref_t stream) {
    tb_assert(req && stream);
    // parse method & path
    {
        tb_char_t buf[4096], *cur = buf, *path_start, *http_start;
        tb_long_t size = tb_stream_bread_line(stream, buf, sizeof buf);
        tb_check_return_val(size > 0 && size < sizeof buf, 414);

        tb_trace_d("REQ: [%s]", buf);

        enum { STATE_METHOD, STATE_SEP0, STATE_PATH, STATE_SEP1, STATE_HTTP, STATE_DONE } state = STATE_METHOD;

        while ((cur - buf) < size) {
            switch (state) {
            case STATE_METHOD:
                switch (*cur) {
                case 'A' ... 'Z': break;
                default:
                    switch (cur - buf) {
                    case 3:
                        if (tb_strncmp(buf, "GET", 3) == 0)
                            req->method = TTP_REQUEST_GET;
                        else if (tb_strncmp(buf, "PUT", 3) == 0)
                            req->method = TTP_REQUEST_PUT;
                        else
                            return 405;
                        break;
                    case 4:
                        if (tb_strncmp(buf, "POST", 4) == 0)
                            req->method = TTP_REQUEST_POST;
                        else if (tb_strncmp(buf, "HEAD", 4) == 0)
                            req->method = TTP_REQUEST_HEAD;
                        else
                            return 405;
                        break;
                    case 5:
                        if (tb_strncmp(buf, "PATCH", 5) == 0)
                            req->method = TTP_REQUEST_PATCH;
                        else
                            return 405;
                        break;
                    case 6:
                        if (tb_strncmp(buf, "DELETE", 6) == 0)
                            req->method = TTP_REQUEST_DELETE;
                        else
                            return 405;
                        break;
                    case 7:
                        if (tb_strncmp(buf, "OPTIONS", 7) == 0)
                            req->method = TTP_REQUEST_OPTIONS;
                        else if (tb_strncmp(buf, "CONNECT", 7) == 0)
                            req->method = TTP_REQUEST_CONNECT;
                        else
                            return 405;
                        break;
                    default: return 405;
                    }
                    state = STATE_SEP0;
                    break;
                }
                break;
            case STATE_SEP0:
                if (*cur != ' ') {
                    path_start = cur;
                    state      = STATE_PATH;
                }
                break;
            case STATE_PATH:
                if (*cur == ' ') {
                    *cur      = 0;
                    req->path = make_string_(path_start);
                    state     = STATE_SEP1;
                }
                break;
            case STATE_SEP1:
                if (*cur != ' ') {
                    if (*cur != 'H') { return 400; }
                    http_start = cur;
                    state      = STATE_HTTP;
                }
                break;
            case STATE_HTTP:
                switch (cur - http_start) {
                case 0 ... 6: tb_check_return_val(*cur == "HTTP/1."[cur - http_start], 400); break;
                case 7:
                    tb_check_return_val(*cur == '1' || *cur == '0', 400);
                    state = STATE_DONE;
                    break;
                default: return 400;
                }
                break;
            case STATE_DONE: return 400;
            }
            cur++;
        }

        if (state != STATE_DONE) return 400;
    }
    tb_trace_d("parsing key-value");
    // parse kv
    {
        tb_char_t buf[4096];
        tb_long_t size = 0;
        while ((size = tb_stream_bread_line(stream, buf, sizeof buf)) != 0) {
            tb_check_return_val(size > 0 && size < sizeof buf, 400);
            tb_trace_d("LIN: [%s]", buf);

            enum { STATE_START, STATE_SEP, STATE_VALUE, STATE_VALUE_TAIL } state = STATE_START;
            tb_char_t *cur = buf, *key_end = tb_null, *value_start = tb_null, *value_end = tb_null;

            for (; *cur; cur++) {
                switch (state) {
                case STATE_START:
                    if (*cur == ':') {
                        *cur    = 0;
                        key_end = cur;
                        state   = STATE_SEP;
                    }
                    break;
                case STATE_SEP:
                    if (*cur != ' ') {
                        value_start = cur;
                        state       = STATE_VALUE;
                    }
                    break;
                case STATE_VALUE:
                    if (*cur == ' ') {
                        value_end = cur;
                        state     = STATE_VALUE_TAIL;
                    }
                    break;
                case STATE_VALUE_TAIL:
                    if (*cur != ' ') {
                        value_end = tb_null;
                        state     = STATE_VALUE;
                    }
                    break;
                }
            }
            if (state < STATE_VALUE) return 400;
            if (!value_end)
                value_end = cur;
            else
                *value_end = 0;

            tb_trace_d("MAP: [%s] -> [%s]", buf, value_start);

            // parse kv
            if (do_compare(buf, key_end, "Content-Length")) {
                tb_hize_t len           = tb_atoll(value_start);
                req->has_content_length = tb_true;
                req->content_length     = len;
            } else if (do_compare(buf, key_end, "Connection")) {
                if (do_compare(value_start, value_end, "close"))
                    req->connection = TTP_CONNECTION_CLOSE;
                else if (do_compare(value_start, value_end, "keep-alive"))
                    req->connection = TTP_CONNECTION_KEEPALIVE;
            } else if (do_compare(buf, key_end, "Range")) {
                enum { RANGE_BYTES, RANGE_BEGIN, RANGE_END } rstate = RANGE_BYTES;
                tb_char_t *xcur = value_start, *range_begin = tb_null, *range_end = tb_null;

                for (; *xcur; xcur++) {
                    switch (rstate) {
                    case RANGE_BYTES:
                        if (xcur - value_start == sizeof "bytes") {
                            if (*xcur != '=') return 400;
                            range_begin = xcur + 1;
                            rstate      = RANGE_BEGIN;
                        } else if (*xcur != "bytes"[xcur - value_start])
                            return 400;
                        break;
                    case RANGE_BEGIN:
                        switch (*xcur) {
                        case '-':
                            *xcur     = 0;
                            range_end = xcur + 1;
                            rstate    = RANGE_END;
                            break;
                        case '0' ... '9': break;
                        default: return 400;
                        }
                        break;
                    case RANGE_END:
                        switch (*xcur) {
                        case '0' ... '9': break;
                        default: return 400;
                        }
                        break;
                    } // switch rstate
                }     // for xcur
                if (rstate != RANGE_END || xcur == range_end) return 400;
                req->range = tb_malloc0(sizeof(ttp_request_range_t));
                tb_assert(req->range);
                req->range->begin = tb_s10tou64(range_begin);
                req->range->end   = tb_s10tou64(range_end);
            } else if (do_compare(buf, key_end, "Content-Type")) {
                req->content_type = make_string_(value_start);
            } else if (do_compare(buf, key_end, "Host")) {
                req->host = make_string_(value_start);
            } else if (do_compare(buf, key_end, "User-Agent")) {
                req->user_agent = make_string_(value_start);
            } else {
                if (!req->addition)
                    req->addition = tb_hash_map_init(8, tb_element_str(tb_false), tb_element_str(tb_true));
                tb_hash_map_insert(req->addition, buf, value_start);
            }
        } // while line
    }
    tb_trace_d("parsed");
    return 0;
}

tb_void_t ttp_response_init(ttp_response_ref_t res) {
    res->addition = tb_hash_map_init(8, tb_element_str(tb_true), tb_element_str(tb_true));
}

#define CRLF "\r\n"

inline static tb_void_t write_content_range(tb_stream_ref_t out, ttp_response_content_range_ref_t range) {
    tb_stream_printf(out, "Content-Range: bytes %llu-%llu/%llu" CRLF, range->begin, range->end, range->full);
}

inline static tb_char_t const *get_accept_ranges_str(ttp_response_accept_ranges_t r) {
    if (r == TTP_RESPONSE_ACCEPT_RANGES_BYTES)
        return "bytes";
    else
        return "none";
}

tb_void_t ttp_response_dump(tb_stream_ref_t out, ttp_response_ref_t res) {
    tb_assert(res);
    tb_stream_printf(out, "HTTP/1.1 %u %s" CRLF, res->status_code, get_http_status(res->status_code));
    if (res->content_type) tb_stream_printf(out, "Content-Type: %s" CRLF, tb_string_cstr(res->content_type));
    if (res->has_content_length) tb_stream_printf(out, "Content-Length: %llu" CRLF, res->content_length);
    if (res->content_range) write_content_range(out, res->content_range);
    if (res->accept_ranges) tb_stream_printf(out, "Accept-Ranges: %s" CRLF, get_accept_ranges_str(res->accept_ranges));
    if (res->connection) tb_stream_printf(out, "Connection: %s" CRLF, get_connection_str(res->connection));
    if (res->location) tb_stream_printf(out, "Location: %s" CRLF, tb_string_cstr(res->location));
    if (res->upgrade) tb_stream_printf(out, "Upgrade: %s" CRLF, tb_string_cstr(res->upgrade));
    if (res->addition) {
        tb_for_all(tb_hash_map_item_ref_t, item, res->addition) {
            tb_stream_printf(out, "%s: %s" CRLF, item->name, item->data);
        }
    }
    tb_stream_printf(out, CRLF);
}

tb_void_t ttp_response_set_content_length(ttp_response_ref_t res, tb_hize_t length) {
    tb_assert_and_check_return(res);
    res->has_content_length = tb_true;
    res->content_length     = length;
}
tb_void_t ttp_response_set_content_range(ttp_response_ref_t res, tb_hize_t begin, tb_hize_t end, tb_hize_t full) {
    tb_assert_and_check_return(res);
    if (!res->content_range) res->content_range = tb_malloc0(sizeof(ttp_response_content_range_t));
    res->content_range->begin = begin;
    res->content_range->end   = end;
    res->content_range->full  = full;
}

tb_void_t ttp_response_append(ttp_response_ref_t res, tb_char_t const *key, tb_char_t const *value) {
    tb_assert_and_check_return(res && res->addition);

    tb_hash_map_insert(res->addition, key, value);
}

tb_void_t ttp_response_appendf(ttp_response_ref_t res, tb_char_t const *key, tb_char_t const *format, ...) {
    tb_assert_and_check_return(res && res->addition);

    tb_char_t buffer[4096];
    {
        tb_va_list_t args;
        tb_va_start(args, format);
        tb_vsnprintf(buffer, sizeof buffer, format, args);
        tb_va_end(args);
    }
    tb_hash_map_insert(res->addition, key, buffer);
}

#define do_free_simple(item)                                                                                           \
    if (item) tb_free(item)

#define do_free_thing_(item, fn)                                                                                       \
    if (item) { fn(item); }

#define do_free_thing(item, fn)                                                                                        \
    if (item) {                                                                                                        \
        fn(item);                                                                                                      \
        tb_free(item);                                                                                                 \
    }

#define do_free_str(item) do_free_thing(item, tb_string_exit)

tb_void_t ttp_request_exit(ttp_request_ref_t req) {
    tb_assert_and_check_return(req);

    do_free_simple(req->range);
    do_free_str(req->content_type);
    do_free_str(req->path);
    do_free_str(req->host);
    do_free_str(req->user_agent);
    do_free_thing_(req->addition, tb_hash_map_exit);
}

tb_void_t ttp_response_exit(ttp_response_ref_t res) {
    tb_assert_and_check_return(res);

    do_free_simple(res->content_range);
    do_free_simple(res->accept_ranges);
    do_free_str(res->content_type);
    do_free_str(res->location);
    do_free_str(res->upgrade);
    do_free_thing_(res->addition, tb_hash_map_exit);
}
