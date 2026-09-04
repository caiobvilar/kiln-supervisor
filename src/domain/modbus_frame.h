#ifndef MODBUS_FRAME_H
#define MODBUS_FRAME_H

#include <stddef.h>
#include <stdint.h>

#define MODBUS_FC_READ_MULTIPLE   0x03
#define MODBUS_FC_WRITE_SINGLE    0x06
#define MODBUS_FC_WRITE_MULTIPLE  0x16

#define MODBUS_FRAME_MAX          256u

int modbus_build_request(uint8_t out[], size_t cap, uint8_t addr,
                         uint8_t fc, uint16_t reg_or_addr, uint16_t value_or_count);

int modbus_frame_check(const uint8_t frame[], size_t len, uint8_t addr);

#endif /* MODBUS_FRAME_H */
