/*
 * i_spi.h -- SPI port interface (hexagonal architecture).
 *
 * See i_uart.h for the pattern. One vtable, one transfer call, an optional
 * CS control for boards where the peripheral does not own a CS line.
 */
#ifndef I_SPI_H
#define I_SPI_H

#include <stddef.h>
#include <stdint.h>

typedef struct spi_bus_s spi_bus_t;

typedef struct {
    int (*transfer)(spi_bus_t* self, const uint8_t* tx, uint8_t* rx, size_t n);
    int (*cs_assert)(spi_bus_t* self);
    int (*cs_release)(spi_bus_t* self);
} spi_vtable_t;

struct spi_bus_s {
    const spi_vtable_t* vt;
    void* ctx;
};

static inline int spi_transfer(spi_bus_t* b, const uint8_t* tx, uint8_t* rx, size_t n)
{
    return b->vt->transfer(b, tx, rx, n);
}

static inline int spi_cs_assert(spi_bus_t* b) { return b->vt->cs_assert(b); }

static inline int spi_cs_release(spi_bus_t* b) { return b->vt->cs_release(b); }

#endif /* I_SPI_H */
