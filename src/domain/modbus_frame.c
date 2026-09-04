/* src/domain/modbus_frame.c -- Modbus RTU ADU build/parse (fc 03/06/16).
 * Requests: 8 bytes (addr, fc, reg hi/lo, value/count hi/lo, CRC lo/hi).
 * Response validation computes CRC over all but the last 2 bytes and
 * cross-checks the address field (KM5P_r0 supports fc 03/06/16, docs/hardware). */
#include "modbus_frame.h"

#include "modbus_crc.h"

int modbus_build_request(uint8_t out[], size_t cap, uint8_t addr, uint8_t fc, uint16_t reg_or_addr,
                         uint16_t value_or_count)
{
    if ((fc == MODBUS_FC_READ_MULTIPLE || fc == MODBUS_FC_WRITE_MULTIPLE) &&
        (value_or_count < 1 || value_or_count > 16)) {
        return -1;
    }
    if (cap < 8) {
        return -1;
    }
    uint8_t pdu[8] = {
        addr,
        fc,
        (uint8_t)(reg_or_addr >> 8),
        (uint8_t)reg_or_addr,
        (uint8_t)(value_or_count >> 8),
        (uint8_t)value_or_count,
        0,
        0,
    };
    uint16_t crc = modbus_crc16(pdu, 6);
    pdu[6] = (uint8_t)crc;        /* LSB first */
    pdu[7] = (uint8_t)(crc >> 8); /* MSB */
    for (size_t i = 0; i < 8; ++i) {
        out[i] = pdu[i];
    }
    return 8;
}

int modbus_frame_check(const uint8_t frame[], size_t len, uint8_t addr)
{
    if (len < 5) { /* addr, fc, data, crc lo, crc hi */
        return -1;
    }
    if (frame[0] != addr) {
        return -1;
    }
    uint16_t crc = modbus_crc16(frame, len - 2);
    uint16_t recv =
        (uint16_t)((uint16_t)frame[len - 1] << 8) | frame[len - 2]; /* LSB first on wire */
    if (crc != recv) {
        return -1;
    }
    return (int)(len - 2); /* number of data bytes incl. addr+fc */
}
