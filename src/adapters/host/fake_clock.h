/*
 * fake_clock.h -- test-side declarations for the fake clock.
 */
#ifndef FAKE_CLOCK_H
#define FAKE_CLOCK_H

#include "i_clock.h"

typedef struct fake_clock_ctx_s fake_clock_ctx_t;

void fake_clock_init(clock_t* c, fake_clock_ctx_t* ctx);
void fake_clock_advance(fake_clock_ctx_t* ctx, uint64_t us);

#endif /* FAKE_CLOCK_H */
