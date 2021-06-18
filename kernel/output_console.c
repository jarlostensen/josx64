
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "jos.h"
#include "video.h"
#include "output_console.h"

typedef struct _console_context {

    pos_t           _cursor_pos;
    uint32_t        _bg_colour;
    uint32_t        _colour;
    const uint8_t* _font;
    rect_t          _rect;

} console_context_t;

static video_mode_info_t        _video_mode_info;
#define MAX_CONSOLE_CONTEXTS 8
static console_context_t        _contexts[MAX_CONSOLE_CONTEXTS];
static console_context_t* _active_context = _contexts;
static size_t                   _context_count = 1;

#define LINE_HEIGHT_PIXELS 10
#define CHAR_WIDTH_PIXELS  8

jo_status_t output_console_initialise(void) {

    jo_status_t status = _JO_STATUS_SUCCESS;

    memset((void*)_contexts, 0, sizeof(_contexts));
    _video_mode_info = video_get_video_mode_info();
    _active_context->_bg_colour = 0;
    _active_context->_colour = 0xffffffff;
    _active_context->_rect.bottom = _video_mode_info.vertical_resolution;
    _active_context->_rect.right = _video_mode_info.horisontal_resolution;

    return status;
}

jo_status_t output_console_create_region(rect_t* rect, region_handle_t* outHandle) {
    if (_context_count == MAX_CONSOLE_CONTEXTS) {
        return _JO_STATUS_RESOURCE_EXHAUSTED;
    }

    //TODO: check valid
    _contexts[_context_count]._rect = *rect;
    _contexts[_context_count]._cursor_pos.x = rect->left;
    _contexts[_context_count]._cursor_pos.y = rect->top;
    *outHandle = (void*)_context_count++;
    
    return _JO_STATUS_SUCCESS;
}

void output_console_activate_region(region_handle_t handle) {
    if (!handle) {
        _active_context = _contexts;
    }
    else {
        size_t context = (size_t)handle;
        assert(context < _context_count);
        _active_context = _contexts + context;
    }
}

void output_console_set_colour(uint32_t colour) {
    _active_context->_colour = colour;
}

uint32_t output_console_get_colour(void) {
    return _active_context->_colour;
}

void output_console_set_bg_colour(uint32_t bg_colour) {
    _active_context->_bg_colour = bg_colour;
}

jo_status_t output_console_set_font(const uint8_t* font, size_t width, size_t height) {
    if (width != CHAR_WIDTH_PIXELS && height != LINE_HEIGHT_PIXELS) {
        return _JO_STATUS_OUT_OF_RANGE;
    }

    _active_context->_font = font;

    return _JO_STATUS_SUCCESS;
}

void output_console_clear_screen(void) {
    video_clear_screen(_active_context->_bg_colour);
    _active_context->_cursor_pos.x = _active_context->_rect.left;
    _active_context->_cursor_pos.y = _active_context->_rect.top;

    video_present();
}

void output_console_line_break(void) {
    _active_context->_cursor_pos.y += LINE_HEIGHT_PIXELS;
    _active_context->_cursor_pos.x = 0;
    if (_active_context->_cursor_pos.y > _active_context->_rect.bottom - LINE_HEIGHT_PIXELS) {
        // scroll
        _active_context->_cursor_pos.y -= LINE_HEIGHT_PIXELS;
        video_scroll_up_region_full_width(_active_context->_rect.top, _active_context->_cursor_pos.y, LINE_HEIGHT_PIXELS);
    }

    video_present();
}

void output_console_output_string(const wchar_t* text) {

    size_t start = 0;
    size_t pos = 0;
    wchar_t c = *text;
    while (c)
    {
        if (c == L'\n') {
            if ((pos - start) > 1) {
                video_draw_text_segment(&(draw_text_segment_args_t) {
                    .left = _active_context->_cursor_pos.x,
                        .top = _active_context->_cursor_pos.y,
                        .colour = _active_context->_colour,
                        .bg_colour = _active_context->_bg_colour,
                        .font_ptr = _active_context->_font,
                        .seg_offs = start,
                        .seg_len = pos - start,
                },
                    text);
            }
            ++pos;
            start = pos;
            output_console_line_break();
        }
        else {
            ++pos;
        }
        c = text[pos];
    }

    if (start != pos) {
        video_draw_text_segment(&(draw_text_segment_args_t) {
            .left = _active_context->_cursor_pos.x,
                .top = _active_context->_cursor_pos.y,
                .colour = _active_context->_colour,
                .bg_colour = _active_context->_bg_colour,
                .font_ptr = _active_context->_font,
                .seg_offs = start,
                .seg_len = pos - start,
        },
            text);

        _active_context->_cursor_pos.x += pos * CHAR_WIDTH_PIXELS;
    }

    //TODO: not sure if this is the best place to do this, but it may be...?
    //      for substantial updates a "region" concept is obviously ideal
    video_present();
}