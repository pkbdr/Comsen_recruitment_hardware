/* Student-written helpers, with AI guidance. Moved out of main.c for reuse
 * and testing; the three function bodies retain the student's behavior.
 * Callers must supply a 16-byte array and a valid length in the range 0..16.
 */
#include "input_text.h"
#include <string.h>

void clear_input(char *text, uint8_t *length)
{
    memset(text, ' ', 16);
    *length = 0;
}

void append_input(char *text, uint8_t *length, char ch)
{
    if (*length < 16) {
        text[*length] = ch;
        *length += 1;
    }
}

void delete_input(char *text, uint8_t *length)
{
    if (*length > 0) {
        *length -= 1;
        text[*length] = ' ';
    }
}
