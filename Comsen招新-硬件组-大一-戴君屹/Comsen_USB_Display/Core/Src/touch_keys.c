#include "touch_keys.h"
#include <string.h>

static const uint8_t chip1_to_key[16] = {
    7, 11, 6, 1, 0, 5, 10, 2, 255, 14, 9, 4, 3, 8, 13, 12
};
static const uint8_t chip2_to_key[16] = {
    27, 15, 20, 25, 16, 21, 26, 17, 22, 28, 23, 18, 29, 24, 19, 255
};

uint32_t touch_keys_map(uint16_t chip1, uint16_t chip2)
{
    uint32_t mapped = 0;
    for (unsigned i = 0; i < 16; ++i) {
        if ((chip1 & (1U << i)) && chip1_to_key[i] < TOUCH_KEYS_COUNT) {
            mapped |= 1UL << chip1_to_key[i];
        }
        if ((chip2 & (1U << i)) && chip2_to_key[i] < TOUCH_KEYS_COUNT) {
            mapped |= 1UL << chip2_to_key[i];
        }
    }
    return mapped;
}

void touch_keys_init(touch_keys_t *keys)
{
    memset(keys, 0, sizeof(*keys));
}

uint32_t touch_keys_update(touch_keys_t *keys, uint16_t chip1, uint16_t chip2,
                           uint32_t now)
{
    uint32_t raw = touch_keys_map(chip1, chip2);
    uint32_t pressed = 0;
    for (unsigned i = 0; i < TOUCH_KEYS_COUNT; ++i) {
        uint32_t bit = 1UL << i;
        if ((raw ^ keys->candidate) & bit) {
            keys->candidate ^= bit;
            keys->changed_at[i] = now;
        }
        if (((keys->candidate ^ keys->stable) & bit) &&
            (uint32_t)(now - keys->changed_at[i]) >= TOUCH_KEYS_DEBOUNCE_MS) {
            keys->stable ^= bit;
            if (keys->stable & bit) pressed |= bit;
        }
    }
    /* Reject ambiguous multi-key presses instead of appending several keys. */
    if (keys->stable && (keys->stable & (keys->stable - 1U))) return 0;
    return pressed;
}
