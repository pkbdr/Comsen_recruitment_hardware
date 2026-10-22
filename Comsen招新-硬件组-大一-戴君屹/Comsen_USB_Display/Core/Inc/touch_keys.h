#ifndef TOUCH_KEYS_H
#define TOUCH_KEYS_H
#include <stdint.h>

#define TOUCH_KEYS_COUNT 30U
#define TOUCH_KEYS_DEBOUNCE_MS 30U

typedef struct {
    uint32_t candidate;
    uint32_t stable;
    uint32_t changed_at[TOUCH_KEYS_COUNT];
} touch_keys_t;

/* AI-written mapping/filter, using the upstream default channel tables.
 * Raw scans are still read by the student's GPIO routines in main.c.
 */
uint32_t touch_keys_map(uint16_t chip1, uint16_t chip2);
void touch_keys_init(touch_keys_t *keys);
/* Returns newly pressed physical T0..T29 bits after 30 ms of stable samples. */
uint32_t touch_keys_update(touch_keys_t *keys, uint16_t chip1, uint16_t chip2,
                           uint32_t now);
#endif
