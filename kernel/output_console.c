
#include <stdint.h>
#include <stdbool.h>

#include "jos.h"
#include "video.h"
#include "output_console.h"

static video_mode_info_t        _video_mode_info;
static pos_t                    _cursor_pos = {0,0};
static uint32_t                 _bg_colour = 0;
static uint32_t                 _colour = 0xffffffff;
static const uint8_t        *   _font = 0;

#define LINE_HEIGHT_PIXELS 10
#define CHAR_WIDTH_PIXELS  8

jos_status_t output_console_initialise() {

    jos_status_t status = _JOS_K_STATUS_SUCCESS;

    _video_mode_info = video_get_video_mode_info();

    return status;
}

void output_console_set_colour(uint32_t colour) {
    _colour = colour;
}

void output_console_set_bg_colour(uint32_t bg_colour) {
    _bg_colour = bg_colour;
}

jos_status_t output_console_set_font(const uint8_t* font, size_t width, size_t height) {
    if ( width != CHAR_WIDTH_PIXELS && height != LINE_HEIGHT_PIXELS) {
        return _JOS_K_STATUS_OUT_OF_RANGE;
    }

    _font = font;

    return _JOS_K_STATUS_SUCCESS;
}

void output_console_line_break(void) {
    //TODO: scroll
    _cursor_pos.y += LINE_HEIGHT_PIXELS;
    _cursor_pos.x = 0;
}

void output_console_output_string(const wchar_t* text) {

    size_t start = 0;
    size_t pos = 0;
    wchar_t c = *text;
    while(c)
    {
        if ( c == L'\n') {
            if( (pos - start)>1 ) {
                video_draw_text_segment(&(draw_text_segment_args_t){
                    .left = _cursor_pos.x,
                    .top = _cursor_pos.y,
                    .colour = _colour,
                    .bg_colour = _bg_colour,
                    .font_ptr = _font,
                    .seg_offs = start,
                    .seg_len = pos - start,
                },
                text);
            }
            ++pos;
            start = pos;
            //TODO: scroll
            _cursor_pos.y += LINE_HEIGHT_PIXELS;
            _cursor_pos.x = 0;
        }
        else {
            ++pos;
        }
        c = text[pos];
    }
    
    if(start!=pos) {
        video_draw_text_segment(&(draw_text_segment_args_t){
            .left = _cursor_pos.x,
            .top = _cursor_pos.y,
            .colour = _colour,
            .bg_colour = _bg_colour,
            .font_ptr = _font,
            .seg_offs = start,
            .seg_len = pos-start,
        },
        text);

        _cursor_pos.x += pos * CHAR_WIDTH_PIXELS;
    }
}