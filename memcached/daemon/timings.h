/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef TIMINIGS_H
#define TIMINIGS_H

#include <platform/platform.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_ATOMIC
    void initialize_timings(void);

#else
#define initialize_timings()
#endif

    hrtime_t collect_timing(uint8_t cmd, hrtime_t nsec);
    void generate_timings(uint8_t opcode, const void *cookie);

    bool binary_response_handler(const void *key, uint16_t keylen,
                                 const void *ext, uint8_t extlen,
                                 const void *body, uint32_t bodylen,
                                 uint8_t datatype, uint16_t status,
                                 uint64_t cas, const void *cookie);
#ifdef __cplusplus
}
#endif

#endif
