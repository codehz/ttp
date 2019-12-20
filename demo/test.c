#include "../include/ttp.h"

#include "evilgcc.h"

#define CRLF "\r\n"

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

static tb_byte_t const test01[] =
    "GET /test HTTP/1.1" CRLF "Host: localhost" CRLF "Content-Length:    3  " CRLF CRLF "c:t";

static tb_byte_t const test02[] = "GET /test HTTP/1.1" CRLF "test" CRLF CRLF "c:t";

static tb_byte_t const test03[] = "GET /test HTTP/1.1" CRLF "Custom: test" CRLF CRLF;

int main() {
    tb_assert_and_check_return_val(tb_init(tb_null, tb_null), 1);
    defer(tbox, tb_exit());

    {
        tb_trace_d("test 01:\n%s", test01);
        auto stream = tb_stream_init_from_data(test01, sizeof test01);
        defer_(stream, tb_stream_exit);
        tb_assert_and_check_return_val(tb_stream_open(stream), 1);
        ttp_request_t req = {};
        tb_assert(ttp_request_parse(&req, stream) == 0);
        defer_ref(req, ttp_request_exit);
        tb_assert_and_check_return_val(!tb_string_cstrcmp(req.path, "/test"), 2);
        tb_assert_and_check_return_val(!tb_string_cstrcmp(req.host, "localhost"), 2);
        tb_assert_and_check_return_val(req.content_length == 3, 2);
    }

    {
        tb_trace_d("test 02:\n%s", test02);
        auto stream = tb_stream_init_from_data(test02, sizeof test02);
        defer_(stream, tb_stream_exit);
        tb_assert_and_check_return_val(tb_stream_open(stream), 1);
        ttp_request_t req = {};
        tb_assert(ttp_request_parse(&req, stream) == 400);
        defer_ref(req, ttp_request_exit);
    }

    {
        tb_trace_d("test 03:\n%s", test03);
        auto stream = tb_stream_init_from_data(test03, sizeof test03);
        defer_(stream, tb_stream_exit);
        tb_assert_and_check_return_val(tb_stream_open(stream), 1);
        ttp_request_t req = {};
        tb_assert(ttp_request_parse(&req, stream) == 0);
        defer_ref(req, ttp_request_exit);
        tb_assert_and_check_return_val(!tb_string_cstrcmp(req.path, "/test"), 2);
        tb_assert(req.addition);
        tb_char_t *ret = tb_hash_map_get(req.addition, "custom");
        tb_assert_and_check_return_val(ret, 3);
        tb_assert(!tb_strcmp(ret, "test"));
    }

    {
        tb_trace_d("response01");
        ttp_response_t res = {200};
        ttp_response_init(&res);
        defer_ref(res, ttp_response_exit);
        res.content_type = make_string_("text/html");
        ttp_response_set_content_length(&res, 1024);
        ttp_response_set_content_range(&res, 0, 1023, 1244);
        ttp_response_append(&res, "Test", "True");
        ttp_response_appendf(&res, "TestNumber", "%d", 256);
        auto stream = tb_stream_init_from_file("/dev/stdout", TB_FILE_MODE_WO);
        defer_(stream, tb_stream_exit);
        tb_assert_and_check_return_val(tb_stream_open(stream), 1);
        ttp_response_dump(stream, &res);
    }

    {
        tb_trace_d("response02");
        ttp_response_t res = {404};
        auto stream = tb_stream_init_from_file("/dev/stdout", TB_FILE_MODE_WO);
        defer_(stream, tb_stream_exit);
        tb_assert_and_check_return_val(tb_stream_open(stream), 1);
        ttp_response_dump(stream, &res);
    }
}