#ifndef DRIVING_INPUT224_H
#define DRIVING_INPUT224_H
#include <windows.h>
#include <stdint.h>
/* Called at the seven original XPP function entries, before their prologues. */
int driving_input224(uint32_t va);
/* Called from the existing Driving preview window procedure. */
void driving_input224_window(HWND window, UINT message, WPARAM w, LPARAM l);
const char *driving_input224_hint(void);
#endif
