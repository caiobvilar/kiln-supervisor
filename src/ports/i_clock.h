/*
 * i_clock.h -- clock / time port interface.
 *
 * The abstraction a host test can fake without wall-clock assumptions: a
 * monotonic tick in microseconds, and a coarse ms tick for logging.
 */
#ifndef I_CLOCK_H
#define I_CLOCK_H

#include <stdint.h>

typedef struct clock_s clock_t;

typedef struct {
    uint64_t (*ticks_us)(clock_t* self);
} clock_vtable_t;

struct clock_s {
    const clock_vtable_t* vt;
    void* ctx;
};

static inline uint64_t clock_ticks_us(clock_t* c) { return c->vt->ticks_us(c); }

#endif /* I_CLOCK_H */
