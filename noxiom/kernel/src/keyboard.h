#pragma once

void keyboard_init(void);
void keyboard_irq_handler(void);
char keyboard_getchar(void);    /* blocks until a key is available */
