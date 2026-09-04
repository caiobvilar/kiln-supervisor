/* src/domain/modbus_crc.c -- CRC-16/Modbus (poly 0xA001, init 0xFFFF, LSB-first).
 * From Ascon Tecnologic "Serial communication protocol ModBUS for Programmers
 * KM5/KR5/KX5" §3.5.1 (see docs/hardware/km5p-controller.md). */
#include "modbus_crc.h"

uint16_t modbus_crc16(const uint8_t* data, size_t n)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < n; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) ? (crc >> 1) ^ 0xA001u : (crc >> 1);
        }
    }
    return crc;
}
