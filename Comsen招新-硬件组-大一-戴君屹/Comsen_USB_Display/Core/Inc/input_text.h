#ifndef INPUT_TEXT_H
#define INPUT_TEXT_H
#include <stdint.h>

/* Fixed 16-character LCD input, with a separate length (0..16). */
void clear_input(char *text, uint8_t *length);
void append_input(char *text, uint8_t *length, char ch);
void delete_input(char *text, uint8_t *length);
#endif
