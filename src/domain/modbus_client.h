#ifndef MODBUS_CLIENT_H
#define MODBUS_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#include "i_clock.h"
#include "i_uart.h"
#include "modbus_frame.h"

typedef enum {
    MODBUS_CLIENT_OK = 0,
    MODBUS_CLIENT_ERR_TIMEOUT = -1,
    MODBUS_CLIENT_ERR_CRC = -2,
    MODBUS_CLIENT_ERR_RESPONSE = -3,
    MODBUS_CLIENT_ERR_UART = -4,
    MODBUS_CLIENT_ERR_PARAM = -5,
} modbus_client_status_t;

#define MODBUS_CLIENT_MAX_REGS 16u

typedef struct {
    uart_t* uart;
    clock_t* clock;
    uint64_t response_timeout_us;
    uint8_t rx_buf[MODBUS_FRAME_MAX];
} modbus_client_t;

void modbus_client_init(modbus_client_t* c, uart_t* uart, clock_t* clock,
                        uint64_t response_timeout_us);

modbus_client_status_t modbus_client_read_registers(modbus_client_t* c, uint8_t addr,
                                                    uint16_t start, uint16_t count, uint16_t* out,
                                                    size_t out_cap);

modbus_client_status_t modbus_client_write_single(modbus_client_t* c, uint8_t addr, uint16_t reg,
                                                  uint16_t value);

modbus_client_status_t modbus_client_write_multiple(modbus_client_t* c, uint8_t addr,
                                                    uint16_t start, const uint16_t* values,
                                                    size_t count);

#endif /* MODBUS_CLIENT_H */