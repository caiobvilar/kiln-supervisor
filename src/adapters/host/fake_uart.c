/*
 * fake_uart.c -- host test double for the UART port.
 *
 * A scripted/recording fake: tests can queue expected tx bytes and program
 * rx bytes to read back. This is deliberately simple -- for complex fakes the
 * projects can switch to CMock-generated mocks -- but it covers the common
 * case with zero tooling.
 */
#include "i_uart.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t rx_buf[512];
    size_t rx_len;
    size_t rx_pos;
    uint8_t tx_buf[512];
    size_t tx_len;
} fake_uart_ctx_t;

static int fake_write(uart_t* u, const uint8_t* data, size_t n)
{
    fake_uart_ctx_t* c = (fake_uart_ctx_t*)u->ctx;
    if (c->tx_len + n > sizeof(c->tx_buf)) {
        return -1;
    }
    memcpy(&c->tx_buf[c->tx_len], data, n);
    c->tx_len += n;
    return (int)n;
}

static int fake_read(uart_t* u, uint8_t* data, size_t n)
{
    fake_uart_ctx_t* c = (fake_uart_ctx_t*)u->ctx;
    size_t avail = c->rx_len - c->rx_pos;
    size_t take = n < avail ? n : avail;
    if (take == 0) {
        return 0;
    }
    memcpy(data, &c->rx_buf[c->rx_pos], take);
    c->rx_pos += take;
    return (int)take;
}

static int fake_available(uart_t* u)
{
    fake_uart_ctx_t* c = (fake_uart_ctx_t*)u->ctx;
    return (int)(c->rx_len - c->rx_pos);
}

static const uart_vtable_t fake_uart_vt = {
    fake_write,
    fake_read,
    fake_available,
};

void fake_uart_init(uart_t* u, fake_uart_ctx_t* ctx)
{
    ctx->rx_len = 0;
    ctx->rx_pos = 0;
    ctx->tx_len = 0;
    u->vt = &fake_uart_vt;
    u->ctx = ctx;
}

/* Test-side helpers (declared in a fake header the tests include). */
void fake_uart_enqueue_rx(fake_uart_ctx_t* ctx, const uint8_t* data, size_t n)
{
    for (size_t i = 0; i < n && ctx->rx_len < sizeof(ctx->rx_buf); i++) {
        ctx->rx_buf[ctx->rx_len++] = data[i];
    }
}

const uint8_t* fake_uart_tx_bytes(fake_uart_ctx_t* ctx, size_t* n)
{
    *n = ctx->tx_len;
    return ctx->tx_buf;
}
