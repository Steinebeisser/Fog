#ifndef FOG_OUTPUT_H
#define FOG_OUTPUT_H

#include <stdbool.h>

bool is_fancy();

bool enable_raw_mode();
bool enable_ansi();
bool enable_utf8();

void disable_raw_mode();
void disable_ansi();
void disable_utf8();

bool setup_in_and_output();

// #ifdef __WIN32
// #define WIN32_LEAN_AND_MEAN
// #include <windows.h>
// HANDLE get_input_handle();
// #endif

#endif // FOG_OUTPUT_H
