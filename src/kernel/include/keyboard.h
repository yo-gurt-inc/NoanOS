#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

void keyboard_init(void);
int keyboard_getchar(void);
void keyboard_set_debug(int enable);
void keyboard_toggle_shift_lock(void);
void keyboard_toggle_symbol_mode(void);

#endif

