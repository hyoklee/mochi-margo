/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <stdio.h>
#include <margo.h>
#include <mercury_proc_string.h>
#include "munit/munit.h"
#include "munit/munit-goto.h"

struct call_info {
    uint64_t fn_start;
    uint64_t fn_end;
};

struct test_monitor_data {
    struct call_info call_count[MARGO_MONITOR_MAX];
};

#define X(__x__, __y__) \
    static void test_monitor_on_##__y__( \
        void* uargs, double timestamp,  margo_monitor_event_t event_type, \
        margo_monitor_##__y__##_args_t event_args) \
    { \
        struct test_monitor_data* monitor_data = (struct test_monitor_data*)uargs; \
        if(event_type == MARGO_MONITOR_FN_START) { \
            monitor_data->call_count[MARGO_MONITOR_ON_##__x__].fn_start += 1; \
        } else { \
            monitor_data->call_count[MARGO_MONITOR_ON_##__x__].fn_end += 1; \
        } \
    }

MARGO_EXPAND_MONITOR_MACROS
#undef X

static void* test_monitor_initialize(margo_instance_id mid,
                                      void*             uargs,
                                      const char*       config)
{
    (void)mid;
    (void)config;
    return uargs;
}

static void test_monitor_finalize(void* uargs)
{
    (void)uargs;
}

DECLARE_MARGO_RPC_HANDLER(echo_ult)
static void echo_ult(hg_handle_t handle)
{
    margo_instance_id mid = margo_hg_handle_get_instance(handle);
    munit_assert_not_null(mid);

    const struct hg_info* info = margo_get_info(handle);
    struct test_monitor_data* monitor_data =
        (struct test_monitor_data*)margo_registered_data(mid, info->id);
    munit_assert_not_null(mid);

    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_RPC_HANDLER].fn_start, ==, 1);
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_RPC_HANDLER].fn_end, ==, 1);
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_RPC_ULT].fn_start, ==, 1);
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_RPC_ULT].fn_end, ==, 0);

    char* input      = NULL;
    hg_return_t hret = HG_SUCCESS;

    hret = margo_get_input(handle, &input);
    munit_assert_int(hret, ==, HG_SUCCESS);
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_GET_INPUT].fn_start, ==, 1);
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_GET_INPUT].fn_end, ==, 1);

    hret = margo_respond(handle, &input);
    munit_assert_int(hret, ==, HG_SUCCESS);
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_RESPOND].fn_start, ==, 1);
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_RESPOND].fn_end, ==, 1);
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_SET_OUTPUT].fn_start, ==, 1);
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_SET_OUTPUT].fn_end, ==, 1);
    /* there is a wait that also started in the caller */
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_WAIT].fn_start, ==, 2);
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_WAIT].fn_end, ==, 1);
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_RESPOND_CB].fn_start, ==, 1);
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_RESPOND_CB].fn_end, ==, 1);

    hret = margo_free_input(handle, &input);
    munit_assert_int(hret, ==, HG_SUCCESS);
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_FREE_INPUT].fn_start, ==, 1);
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_FREE_INPUT].fn_end, ==, 1);

    hret = margo_destroy(handle);
    munit_assert_int(hret, ==, HG_SUCCESS);
    /* final destruction of the handle will actually happen
     * in the code generated by DEFINE_MARGO_RPC_HANDLER */
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_DESTROY].fn_start, ==, 0);
    munit_assert_int(monitor_data->call_count[MARGO_MONITOR_ON_DESTROY].fn_end, ==, 0);
}
DEFINE_MARGO_RPC_HANDLER(echo_ult)

static void* test_context_setup(const MunitParameter params[], void* user_data)
{
    (void)params;
    (void)user_data;
    return NULL;
}

static void test_context_tear_down(void* fixture)
{
    (void)fixture;
}

static MunitResult test_monitoring(const MunitParameter params[],
                                   void*                data)
{
    hg_return_t hret                      = HG_SUCCESS;
    struct test_monitor_data monitor_data = {0};

    struct margo_monitor test_monitor = {
        .uargs      = (void*)&monitor_data,
        .initialize = test_monitor_initialize,
        .finalize   = test_monitor_finalize,
#define X(__x__, __y__) .on_##__y__ = test_monitor_on_##__y__,
       MARGO_EXPAND_MONITOR_MACROS
#undef X
    };

    const char* protocol = munit_parameters_get(params, "protocol");
    struct margo_init_info init_info = {
        .json_config   = NULL,
        .progress_pool = ABT_POOL_NULL,
        .rpc_pool      = ABT_POOL_NULL,
        .hg_class      = NULL,
        .hg_context    = NULL,
        .hg_init_info  = NULL,
        .logger        = NULL,
        .monitor       = &test_monitor
    };
    margo_instance_id mid = margo_init_ext(protocol, MARGO_SERVER_MODE, &init_info);
    munit_assert_not_null(mid);

    hg_id_t echo_id = MARGO_REGISTER(mid, "echo", hg_string_t, hg_string_t, echo_ult);
    munit_assert_int(echo_id, !=, 0);

    hret = margo_register_data(mid, echo_id, &monitor_data, NULL);
    munit_assert_int(hret, ==, HG_SUCCESS);

    /* note: because of the __shutdown__ RPC, the count will be at 2 */
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_REGISTER].fn_start, ==, 2);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_REGISTER].fn_end, ==, 2);

    const char* input  = "hello world";
    hg_addr_t addr     = HG_ADDR_NULL;
    hg_handle_t handle = HG_HANDLE_NULL;

    hret = margo_addr_self(mid, &addr);
    munit_assert_int(hret, ==, HG_SUCCESS);
    /* margo_addr_self triggers an on_lookup in the monitoring system,
     * and there is one done in margo_init_ext already */
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_LOOKUP].fn_start, ==, 2);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_LOOKUP].fn_end, ==, 2);

    hret = margo_create(mid, addr, echo_id, &handle);
    munit_assert_int(hret, ==, HG_SUCCESS);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_CREATE].fn_start, ==, 1);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_CREATE].fn_end, ==, 1);

    hret = margo_forward(handle, &input);
    munit_assert_int(hret, ==, HG_SUCCESS);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_FORWARD].fn_start, ==, 1);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_FORWARD].fn_end, ==, 1);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_SET_INPUT].fn_start, ==, 1);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_SET_INPUT].fn_end, ==, 1);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_WAIT].fn_start, ==, 2);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_WAIT].fn_end, ==, 2);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_FORWARD_CB].fn_start, ==, 1);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_FORWARD_CB].fn_end, ==, 1);

    char* output = NULL;
    hret = margo_get_output(handle, &output);
    munit_assert_int(hret, ==, HG_SUCCESS);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_GET_OUTPUT].fn_start, ==, 1);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_GET_OUTPUT].fn_end, ==, 1);

    hret = margo_free_output(handle, &output);
    munit_assert_int(hret, ==, HG_SUCCESS);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_FREE_OUTPUT].fn_start, ==, 1);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_FREE_OUTPUT].fn_end, ==, 1);

    hret = margo_destroy(handle);
    munit_assert_int(hret, ==, HG_SUCCESS);
    /* note: because the server is on the same process, the same handle is passed to
     * the RPC ULT, so there is only 1 handle alive in this program */
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_DESTROY].fn_start, ==, 1);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_DESTROY].fn_end, ==, 1);

    hret = margo_addr_free(mid, addr);
    munit_assert_int(hret, ==, HG_SUCCESS);

    hret = margo_deregister(mid, echo_id);
    munit_assert_int(hret, ==, HG_SUCCESS);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_DEREGISTER].fn_start, ==, 1);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_DEREGISTER].fn_end, ==, 1);

    margo_finalize(mid);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_PREFINALIZE].fn_start, ==, 1);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_PREFINALIZE].fn_end, ==, 1);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_FINALIZE].fn_start, ==, 1);
    munit_assert_int(monitor_data.call_count[MARGO_MONITOR_ON_FINALIZE].fn_end, ==, 1);

    return MUNIT_OK;
}

static char* protocol_params[] = {"na+sm", NULL};

static MunitParameterEnum test_params[]
    = {{"protocol", protocol_params}, {NULL, NULL}};

static MunitTest test_suite_tests[] = {
    {(char*)"/monitoring", test_monitoring, test_context_setup,
     test_context_tear_down, MUNIT_TEST_OPTION_NONE, test_params},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite
    = {(char*)"/margo", test_suite_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)])
{
    return munit_suite_main(&test_suite, NULL, argc, argv);
}
