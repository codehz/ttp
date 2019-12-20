#include "../include/ttp.h"

#include "evilgcc.h"

static tb_byte_t hello[] = "Hello, world!\n";

static tb_void_t process_request(tb_socket_ref_t client) {
    tb_assert_and_check_return(client);
    defer_(client, tb_socket_exit);

    auto sin = tb_stream_init_from_sock_ref(client, TB_SOCKET_TYPE_TCP, tb_false);
    tb_assert_and_check_return(sin);
    defer_(sin, tb_stream_exit);
    tb_assert_and_check_return(tb_stream_open(sin));

    auto sout = tb_stream_init_from_sock_ref(client, TB_SOCKET_TYPE_TCP, tb_false);
    tb_assert_and_check_return(sout);
    defer_(sout, tb_stream_exit);
    tb_assert_and_check_return(tb_stream_open(sout));

    while (1) {
        tb_trace_d("starting parse request");
        ttp_request_t req = {};
        auto code         = ttp_request_parse(&req, sin);
        tb_trace_d("code: %d %d", code, tb_stream_state(sin));
        defer_ref(req, ttp_request_exit);

        if (code) {
            if (code == 414) return;
            ttp_response_t res = {code};
            ttp_response_dump(sout, &res);
            return;
        }

        tb_trace_d("%{ttp_request}", &req);
        tb_trace_d("starting writing response header");

        ttp_response_t res = {200};
        res.connection     = req.connection;
        ttp_response_set_content_length(&res, sizeof hello - 1);
        ttp_response_dump(sout, &res);

        tb_trace_d("starting writing response body");

        tb_stream_printf(sout, "%s", hello);
        tb_stream_bwrit(sout, hello, sizeof hello - 1);
        tb_stream_sync(sout, tb_false);

        if (req.connection != TTP_CONNECTION_KEEPALIVE) break;
    }
}

static tb_void_t accept_requests(tb_socket_ref_t server, tb_co_scheduler_ref_t sche) {
    tb_assert_and_check_return(server && sche);
    tb_socket_ref_t client;

    tb_ipaddr_t addr;
    while (1) {
        if ((client = tb_socket_accept(server, &addr)) != tb_null) {
            tb_assert_and_check_return(tb_coroutine_start(sche, vproxy(process_request), client, 0));
        } else if (tb_socket_wait(server, TB_SOCKET_EVENT_ACPT, -1) <= 0)
            break;
    }
}

tb_int_t main() {
    tb_assert_and_check_return_val(tb_init(tb_null, tb_null), 1);
    defer(tbox, tb_exit());
    ttp_init();

    auto sche = tb_co_scheduler_init();
    tb_assert_and_check_return_val(sche, 1);
    defer_(sche, tb_co_scheduler_exit);

    auto server = tb_socket_init(TB_SOCKET_TYPE_TCP, TB_IPADDR_FAMILY_IPV4);
    tb_assert_and_check_return_val(server, 1);
    defer_(server, tb_socket_exit);

    tb_ipaddr_t addr = {};
    tb_ipaddr_set(&addr, tb_null, 8848, TB_IPADDR_FAMILY_IPV4);
    tb_assertf_and_check_return_val(tb_socket_bind(server, &addr), 2, "Cannot bind %{ipaddr}", &addr);

    tb_assert_and_check_return_val(tb_socket_listen(server, 1000), 1);
    tb_assert_and_check_return_val(tb_coroutine_start(sche, vproxy(accept_requests, sche), server, 0), 1);

    tb_co_scheduler_loop(sche, tb_true);
    return 0;
}