#pragma once
#ifndef _JOS_KERNEL_OUTPUT_CONSOLE_H
#define _JOS_KERNEL_OUTPUT_CONSOLE_H

#include <wchar.h>

typedef void* region_handle_t;

jo_status_t output_console_initialise(void);
void output_console_clear_screen(void);
uint32_t output_console_get_colour(void);
void output_console_set_colour(uint32_t colour);
void output_console_set_bg_colour(uint32_t bg_colour);
jo_status_t output_console_set_font(const uint8_t* font, size_t width, size_t height);
void output_console_output_string_w(const wchar_t* text);
void output_console_output_string_a(const char* text);
void output_console_line_break(void);

jo_status_t output_console_create_region(rect_t* rect, region_handle_t* outHandle);
void output_console_activate_region(region_handle_t handle);

#endif // _JOS_KERNEL_OUTPUT_CONSOLE_H
