/*
 * i_i2c.h -- I2C port interface (hexagonal architecture).
 *
 * One vtable, one combined-write-then-read call (the common embedded idiom),
 * plus a write-only variant for simple register poke.
 */
#ifndef I_I2C_H
#define I_I2C_H

#include <stddef.h>
#include <stdint.h>

typedef struct i2c_bus_s i2c_bus_t;

typedef struct {
    int (*write)(i2c_bus_t* self, uint8_t addr, const uint8_t* data, size_t n);
    int (*write_then_read)(i2c_bus_t* self, uint8_t addr, const uint8_t* wdata, size_t wn,
                           uint8_t* rdata, size_t rn);
} i2c_vtable_t;

struct i2c_bus_s {
    const i2c_vtable_t* vt;
    void* ctx;
};

static inline int i2c_write(i2c_bus_t* b, uint8_t addr, const uint8_t* data, size_t n)
{
    return b->vt->write(b, addr, data, n);
}

static inline int i2c_write_then_read(i2c_bus_t* b, uint8_t addr, const uint8_t* wdata, size_t wn,
                                      uint8_t* rdata, size_t rn)
{
    return b->vt->write_then_read(b, addr, wdata, wn, rdata, rn);
}

#endif /* I_I2C_H */
