/* src/domain/modbus_client.c -- generic Modbus RTU client over the UART port.
 * One-shot request/response with a bounded timeout: build a request, transmit
 * it, wait for a complete expected-length response, validate, then parse. */
#include "modbus_client.h"

#include "modbus_crc.h"
#include "modbus_frame.h"

static modbus_client_status_t tx_request(modbus_client_t* c, const uint8_t* frame, size_t n)
{
    if (uart_write(c->uart, frame, n) != (int)n) {
        return MODBUS_CLIENT_ERR_UART;
    }
    return MODBUS_CLIENT_OK;
}

static modbus_client_status_t recv_frame(modbus_client_t* c, size_t expected, size_t* out_len)
{
    uint64_t deadline = clock_ticks_us(c->clock) + c->response_timeout_us;
    size_t len = 0;
    for (;;) {
        if (uart_available(c->uart) > 0) {
            int n = uart_read(c->uart, &c->rx_buf[len], expected - len);
            if (n < 0) {
                return MODBUS_CLIENT_ERR_UART;
            }
            len += (size_t)n;
        }
        /* No expected reply can have 0x80 set in its fc byte (0x03/0x06/0x16),
         * so this is unambiguously a slave exception reply. */
        if (len >= 5 && (c->rx_buf[1] & 0x80u)) {
            return MODBUS_CLIENT_ERR_RESPONSE;
        }
        if (len >= expected) {
            *out_len = len;
            return MODBUS_CLIENT_OK;
        }
        if (clock_ticks_us(c->clock) >= deadline) {
            return MODBUS_CLIENT_ERR_TIMEOUT;
        }
    }
}

static modbus_client_status_t validate_frame(modbus_client_t* c, size_t len, uint8_t addr,
                                             uint8_t fc, uint8_t byte_count)
{
    if (modbus_frame_check(c->rx_buf, len, addr) < 0) {
        uint16_t crc = modbus_crc16(c->rx_buf, len - 2);
        uint16_t embedded =
            (uint16_t)((uint16_t)c->rx_buf[len - 2] | ((uint16_t)c->rx_buf[len - 1] << 8));
        return crc == embedded ? MODBUS_CLIENT_ERR_RESPONSE : MODBUS_CLIENT_ERR_CRC;
    }
    if (c->rx_buf[1] != fc) {
        return MODBUS_CLIENT_ERR_RESPONSE;
    }
    if (fc == MODBUS_FC_READ_MULTIPLE && c->rx_buf[2] != byte_count) {
        return MODBUS_CLIENT_ERR_RESPONSE;
    }
    return MODBUS_CLIENT_OK;
}

void modbus_client_init(modbus_client_t* c, uart_t* uart, clock_t* clock,
                        uint64_t response_timeout_us)
{
    c->uart = uart;
    c->clock = clock;
    c->response_timeout_us = response_timeout_us;
}

modbus_client_status_t modbus_client_read_registers(modbus_client_t* c, uint8_t addr,
                                                    uint16_t start, uint16_t count, uint16_t* out,
                                                    size_t out_cap)
{
    if (c == NULL || c->uart == NULL || c->clock == NULL || out == NULL) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    if (count < 1 || count > MODBUS_CLIENT_MAX_REGS) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    if ((size_t)count > out_cap) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    uint8_t req[MODBUS_FRAME_MAX];
    if (modbus_build_request(req, sizeof(req), addr, MODBUS_FC_READ_MULTIPLE, start, count) < 0) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    modbus_client_status_t st = tx_request(c, req, 8u);
    if (st != MODBUS_CLIENT_OK) {
        return st;
    }
    size_t len = 0;
    st = recv_frame(c, (size_t)(5 + 2 * count), &len);
    if (st != MODBUS_CLIENT_OK) {
        return st;
    }
    st = validate_frame(c, len, addr, MODBUS_FC_READ_MULTIPLE, (uint8_t)(2 * count));
    if (st != MODBUS_CLIENT_OK) {
        return st;
    }
    for (size_t i = 0; i < (size_t)count; i++) {
        out[i] = (uint16_t)(((uint16_t)c->rx_buf[3 + 2 * i] << 8) | (uint16_t)c->rx_buf[4 + 2 * i]);
    }
    return MODBUS_CLIENT_OK;
}

static modbus_client_status_t write_multiple_frame(modbus_client_t* c, uint8_t addr, uint16_t start,
                                                   const uint16_t* values, size_t count)
{
    uint8_t req[MODBUS_FRAME_MAX];
    size_t n = 9 + 2 * count;
    req[0] = addr;
    req[1] = MODBUS_FC_WRITE_MULTIPLE;
    req[2] = (uint8_t)(start >> 8);
    req[3] = (uint8_t)start;
    req[4] = (uint8_t)(count >> 8);
    req[5] = (uint8_t)count;
    req[6] = (uint8_t)(2 * count);
    for (size_t i = 0; i < count; i++) {
        req[7 + 2 * i] = (uint8_t)(values[i] >> 8);
        req[8 + 2 * i] = (uint8_t)values[i];
    }
    uint16_t crc = modbus_crc16(req, n - 2);
    req[n - 2] = (uint8_t)crc;
    req[n - 1] = (uint8_t)(crc >> 8);
    return tx_request(c, req, n);
}

modbus_client_status_t modbus_client_write_single(modbus_client_t* c, uint8_t addr, uint16_t reg,
                                                  uint16_t value)
{
    if (c == NULL || c->uart == NULL || c->clock == NULL) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    uint8_t req[MODBUS_FRAME_MAX];
    if (modbus_build_request(req, sizeof(req), addr, MODBUS_FC_WRITE_SINGLE, reg, value) < 0) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    modbus_client_status_t st = tx_request(c, req, 8u);
    if (st != MODBUS_CLIENT_OK) {
        return st;
    }
    size_t len = 0;
    st = recv_frame(c, 8u, &len);
    if (st != MODBUS_CLIENT_OK) {
        return st;
    }
    return validate_frame(c, len, addr, MODBUS_FC_WRITE_SINGLE, 0u);
}

modbus_client_status_t modbus_client_write_multiple(modbus_client_t* c, uint8_t addr,
                                                    uint16_t start, const uint16_t* values,
                                                    size_t count)
{
    if (c == NULL || c->uart == NULL || c->clock == NULL || values == NULL) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    if (count < 1 || count > MODBUS_CLIENT_MAX_REGS) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    modbus_client_status_t st = write_multiple_frame(c, addr, start, values, count);
    if (st != MODBUS_CLIENT_OK) {
        return st;
    }
    size_t len = 0;
    st = recv_frame(c, 8u, &len);
    if (st != MODBUS_CLIENT_OK) {
        return st;
    }
    return validate_frame(c, len, addr, MODBUS_FC_WRITE_MULTIPLE, 0u);
}