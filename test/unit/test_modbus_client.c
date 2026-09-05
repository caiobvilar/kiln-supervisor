/* test/unit/test_modbus_client.c -- generic Modbus client request/response tests. */
/* @verifies KILN-FUN-004 */
#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "fake_clock.h"
#include "fake_uart.h"
#include "i_clock.h"
#include "modbus_client.h"

/* A clock that advances a fixed step on every read, so a bounded-timeout poll
 * loop terminates deterministically in tests. */
typedef struct {
    uint64_t now;
    uint64_t step_us;
} tick_clock_ctx_t;

static uint64_t tick_ticks(clock_t* c)
{
    tick_clock_ctx_t* k = (tick_clock_ctx_t*)c->ctx;
    uint64_t t = k->now;
    k->now += k->step_us;
    return t;
}

static const clock_vtable_t tick_clock_vt = {tick_ticks};

static void tick_clock_init(clock_t* c, tick_clock_ctx_t* k, uint64_t step_us)
{
    k->now = 0;
    k->step_us = step_us;
    c->vt = &tick_clock_vt;
    c->ctx = k;
}

void setUp(void) {}
void tearDown(void) {}

static void make_client(uart_t* u, fake_uart_ctx_t* uctx, clock_t* c, fake_clock_ctx_t* cctx,
                        modbus_client_t* cli)
{
    fake_uart_init(u, uctx);
    fake_clock_init(c, cctx);
    modbus_client_init(cli, u, c, 100000u);
}

void test_read_registers_success(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t rsp[] = {0x01, 0x03, 0x06, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0x67, 0x9F};
    fake_uart_enqueue_rx(&uctx, rsp, sizeof(rsp));

    const uint16_t expected[] = {0x0123, 0x4567, 0x89AB};
    uint16_t regs[3] = {0, 0, 0};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_OK,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 3, regs, 3));
    TEST_ASSERT_EQUAL_UINT16_ARRAY(expected, regs, 3);

    const uint8_t expect_tx[] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x03, 0x54, 0x0B};
    size_t n = 0;
    const uint8_t* tx = fake_uart_tx_bytes(&uctx, &n);
    TEST_ASSERT_EQUAL_UINT(8, n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect_tx, tx, 8);
}

void test_read_registers_timeout_when_no_response(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    tick_clock_ctx_t clkctx;
    modbus_client_t cli;
    fake_uart_init(&u, &uctx);
    tick_clock_init(&clk, &clkctx, 10000u);
    modbus_client_init(&cli, &u, &clk, 50000u);

    uint16_t regs[1] = {0xFFFF};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_TIMEOUT,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 1, regs, 1));

    size_t n = 0;
    (void)fake_uart_tx_bytes(&uctx, &n);
    TEST_ASSERT_EQUAL_UINT(8, n);
}

void test_read_registers_timeout_on_partial_frame(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    tick_clock_ctx_t clkctx;
    modbus_client_t cli;
    fake_uart_init(&u, &uctx);
    tick_clock_init(&clk, &clkctx, 10000u);
    modbus_client_init(&cli, &u, &clk, 50000u);

    const uint8_t partial[] = {0x01, 0x03};
    fake_uart_enqueue_rx(&uctx, partial, sizeof(partial));
    uint16_t regs[1] = {0};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_TIMEOUT,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 1, regs, 1));
}

void test_read_registers_rejects_bad_crc(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t bad[] = {0x01, 0x03, 0x06, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAC, 0x67, 0x9F};
    fake_uart_enqueue_rx(&uctx, bad, sizeof(bad));
    uint16_t regs[3] = {0xFFFF, 0xFFFF, 0xFFFF};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_CRC,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 3, regs, 3));
    const uint16_t untouched[] = {0xFFFF, 0xFFFF, 0xFFFF};
    TEST_ASSERT_EQUAL_UINT16_ARRAY(untouched, regs, 3);
}

void test_read_registers_rejects_wrong_slave_address(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t other[] = {0x02, 0x03, 0x06, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0x73, 0x6F};
    fake_uart_enqueue_rx(&uctx, other, sizeof(other));
    uint16_t regs[3] = {0};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_RESPONSE,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 3, regs, 3));
}

void test_read_registers_rejects_byte_count_mismatch(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t miscnt[] = {0x01, 0x03, 0x05, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0x54, 0x9F};
    fake_uart_enqueue_rx(&uctx, miscnt, sizeof(miscnt));
    uint16_t regs[3] = {0};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_RESPONSE,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 3, regs, 3));
}

void test_read_registers_exception_reply_is_response_error(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t exc[] = {0x01, 0x83, 0x02, 0xC0, 0xF1};
    fake_uart_enqueue_rx(&uctx, exc, sizeof(exc));
    uint16_t regs[1] = {0};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_RESPONSE,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 1, regs, 1));
}

void test_read_registers_params(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    uint16_t regs[2] = {0};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_PARAM,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 0, regs, 2));
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_PARAM,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 17, regs, 2));
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_PARAM,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 3, regs, 2));
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_PARAM,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 1, NULL, 0));

    size_t n = 99;
    (void)fake_uart_tx_bytes(&uctx, &n);
    TEST_ASSERT_EQUAL_UINT(0, n);
}

void test_read_registers_overlength_is_response_error(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t rsp[] = {0x01, 0x03, 0x06, 0x01, 0x23, 0x45, 0x67,
                           0x89, 0xAB, 0x67, 0x9F, 0xAA, 0xBB};
    fake_uart_enqueue_rx(&uctx, rsp, sizeof(rsp));

    uint16_t regs[3] = {0};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_RESPONSE,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 3, regs, 3));
    TEST_ASSERT_EQUAL_UINT(uctx.rx_len, uctx.rx_pos);

    const uint8_t expect_tx[] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x03, 0x54, 0x0B};
    size_t n = 0;
    const uint8_t* tx = fake_uart_tx_bytes(&uctx, &n);
    TEST_ASSERT_EQUAL_UINT(8, n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect_tx, tx, 8);
}

void test_read_registers_clean_after_overlength(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t noisy[] = {0x01, 0x03, 0x06, 0x01, 0x23, 0x45, 0x67,
                             0x89, 0xAB, 0x67, 0x9F, 0xAA, 0xBB};
    fake_uart_enqueue_rx(&uctx, noisy, sizeof(noisy));
    uint16_t regs[3] = {0};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_RESPONSE,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 3, regs, 3));

    const uint8_t rsp[] = {0x01, 0x03, 0x06, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0x67, 0x9F};
    fake_uart_enqueue_rx(&uctx, rsp, sizeof(rsp));

    const uint16_t expected[] = {0x0123, 0x4567, 0x89AB};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_OK,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 3, regs, 3));
    TEST_ASSERT_EQUAL_UINT16_ARRAY(expected, regs, 3);

    const uint8_t expect_tx[] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x03, 0x54, 0x0B};
    size_t n = 0;
    const uint8_t* tx = fake_uart_tx_bytes(&uctx, &n);
    TEST_ASSERT_EQUAL_UINT(16, n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect_tx, tx + 8, 8);
}

void test_read_registers_exception_with_trailing_bytes(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t exc[] = {0x01, 0x83, 0x02, 0xC0, 0xF1, 0xAA, 0xBB};
    fake_uart_enqueue_rx(&uctx, exc, sizeof(exc));
    uint16_t regs[1] = {0};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_RESPONSE,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 1, regs, 1));
    TEST_ASSERT_EQUAL_UINT(uctx.rx_len, uctx.rx_pos);
}

/* A UART whose write always fails, to exercise ERR_UART. */
typedef struct {
    int unused;
} fail_uart_ctx_t;

static int fail_write(uart_t* u, const uint8_t* data, size_t n)
{
    (void)u;
    (void)data;
    (void)n;
    return -1;
}

static int fail_read(uart_t* u, uint8_t* data, size_t n)
{
    (void)u;
    (void)data;
    (void)n;
    return -1;
}

static int fail_available(uart_t* u)
{
    (void)u;
    return 0;
}

static const uart_vtable_t fail_uart_vt = {fail_write, fail_read, fail_available};

static void fail_uart_init(uart_t* u)
{
    static fail_uart_ctx_t ctx;
    ctx.unused = 0;
    u->vt = &fail_uart_vt;
    u->ctx = &ctx;
}

void test_write_single_success(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t echo[] = {0x01, 0x06, 0x00, 0x01, 0x00, 0x0A, 0x58, 0x0D};
    fake_uart_enqueue_rx(&uctx, echo, sizeof(echo));

    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_OK, modbus_client_write_single(&cli, 0x01, 0x0001, 0x000A));

    const uint8_t expect_tx[] = {0x01, 0x06, 0x00, 0x01, 0x00, 0x0A, 0x58, 0x0D};
    size_t n = 0;
    const uint8_t* tx = fake_uart_tx_bytes(&uctx, &n);
    TEST_ASSERT_EQUAL_UINT(8, n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect_tx, tx, 8);
}

void test_write_multiple_success(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t echo[] = {0x01, 0x16, 0x00, 0x01, 0x00, 0x02, 0x98, 0x08};
    fake_uart_enqueue_rx(&uctx, echo, sizeof(echo));
    const uint16_t values[] = {0x0064, 0x00C8};

    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_OK,
                          modbus_client_write_multiple(&cli, 0x01, 0x0001, values, 2));

    const uint8_t expect_tx[] = {0x01, 0x16, 0x00, 0x01, 0x00, 0x02, 0x04,
                                 0x00, 0x64, 0x00, 0xC8, 0x92, 0x35};
    size_t n = 0;
    const uint8_t* tx = fake_uart_tx_bytes(&uctx, &n);
    TEST_ASSERT_EQUAL_UINT(13, n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect_tx, tx, 13);
}

void test_write_single_uart_failure(void)
{
    uart_t u;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    fail_uart_init(&u);
    fake_clock_init(&clk, &clkctx);
    modbus_client_init(&cli, &u, &clk, 100000u);

    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_UART,
                          modbus_client_write_single(&cli, 0x01, 0x0001, 0x000A));
}

void test_write_multiple_params(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint16_t values[] = {0x0001, 0x0002};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_PARAM,
                          modbus_client_write_multiple(&cli, 0x01, 0x0001, NULL, 2));
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_PARAM,
                          modbus_client_write_multiple(&cli, 0x01, 0x0001, values, 0));
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_PARAM,
                          modbus_client_write_multiple(&cli, 0x01, 0x0001, values, 17));

    size_t n = 99;
    (void)fake_uart_tx_bytes(&uctx, &n);
    TEST_ASSERT_EQUAL_UINT(0, n);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_read_registers_success);
    RUN_TEST(test_read_registers_timeout_when_no_response);
    RUN_TEST(test_read_registers_timeout_on_partial_frame);
    RUN_TEST(test_read_registers_rejects_bad_crc);
    RUN_TEST(test_read_registers_rejects_wrong_slave_address);
    RUN_TEST(test_read_registers_rejects_byte_count_mismatch);
    RUN_TEST(test_read_registers_exception_reply_is_response_error);
    RUN_TEST(test_read_registers_params);
    RUN_TEST(test_read_registers_overlength_is_response_error);
    RUN_TEST(test_read_registers_clean_after_overlength);
    RUN_TEST(test_read_registers_exception_with_trailing_bytes);
    RUN_TEST(test_write_single_success);
    RUN_TEST(test_write_multiple_success);
    RUN_TEST(test_write_single_uart_failure);
    RUN_TEST(test_write_multiple_params);
    return UNITY_END();
}
