/*
 * fake_uart.h -- test-side declarations for the fake UART.
 *
 * The port header (i_uart.h) is the interface the application sees; this
 * header is what tests include to script the fake.
 */
#ifndef FAKE_UART_H
#define FAKE_UART_H

#include "i_uart.h"

typedef struct {
    uint8_t rx_buf[512];
    size_t rx_len;
    size_t rx_pos;
    uint8_t tx_buf[512];
    size_t tx_len;
} fake_uart_ctx_t;

void fake_uart_init(uart_t* u, fake_uart_ctx_t* ctx);
void fake_uart_enqueue_rx(fake_uart_ctx_t* ctx, const uint8_t* data, size_t n);
const uint8_t* fake_uart_tx_bytes(fake_uart_ctx_t* ctx, size_t* n);

#endif /* FAKE_UART_H */
