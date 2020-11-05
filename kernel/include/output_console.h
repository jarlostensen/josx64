#ifndef _JOS_KERNEL_OUTPUT_CONSOLE_H
#define _JOS_KERNEL_OUTPUT_CONSOLE_H

#include <wchar.h>

typedef struct _pos {

    size_t      x;
    size_t      y;

} pos_t;

jos_status_t output_console_initialise();
void output_console_set_colour(uint32_t colour);
void output_console_set_bg_colour(uint32_t bg_colour);
jos_status_t output_console_set_font(const uint8_t* font, size_t width, size_t height);
void output_console_output_string(const wchar_t* text);

#endif // _JOS_KERNEL_OUTPUT_CONSOLE_H