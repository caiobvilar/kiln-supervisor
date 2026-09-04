#ifndef MODBUS_CRC_H
#define MODBUS_CRC_H

#include <stddef.h>
#include <stdint.h>

uint16_t modbus_crc16(const uint8_t* data, size_t n);

#endif /* MODBUS_CRC_H */
