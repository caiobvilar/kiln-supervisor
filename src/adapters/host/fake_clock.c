/*
 * fake_clock.c -- host test double for the clock port.
 *
 * A simple monotonic counter the test advances explicitly.
 */
#include <stdint.h>

#include "i_clock.h"

typedef struct {
    uint64_t ticks;
} fake_clock_ctx_t;

static uint64_t fake_ticks(clock_t* c)
{
    fake_clock_ctx_t* ctx = (fake_clock_ctx_t*)c->ctx;
    return ctx->ticks;
}

static const clock_vtable_t fake_clock_vt = {
    fake_ticks,
};

void fake_clock_init(clock_t* c, fake_clock_ctx_t* ctx)
{
    ctx->ticks = 0;
    c->vt = &fake_clock_vt;
    c->ctx = ctx;
}

void fake_clock_advance(fake_clock_ctx_t* ctx, uint64_t us) { ctx->ticks += us; }
