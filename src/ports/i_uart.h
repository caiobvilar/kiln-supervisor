/*
 * i_uart.h -- UART port interface (hexagonal architecture).
 *
 * Part of the embedded-template reusable fake HAL. The application talks to
 * the *port*, never to a concrete UART. Real implementations live in
 * src/adapters/stm32f4/, test doubles in src/adapters/host/.
 */
#ifndef I_UART_H
#define I_UART_H

#include <stddef.h>
#include <stdint.h>

typedef struct uart_s uart_t;

typedef struct {
    int (*write)(uart_t* self, const uint8_t* data, size_t n);
    int (*read)(uart_t* self, uint8_t* data, size_t n);
    int (*available)(uart_t* self);
} uart_vtable_t;

struct uart_s {
    const uart_vtable_t* vt;
    void* ctx;
};

static inline int uart_write(uart_t* u, const uint8_t* data, size_t n)
{
    return u->vt->write(u, data, n);
}

static inline int uart_read(uart_t* u, uint8_t* data, size_t n) { return u->vt->read(u, data, n); }

static inline int uart_available(uart_t* u) { return u->vt->available(u); }

#endif /* I_UART_H */
