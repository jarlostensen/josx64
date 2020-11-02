#ifndef _JOS_KERNEL_VIDEO_H
#define _JOS_KERNEL_VIDEO_H

CEfiStatus video_initialise();
uint32_t video_make_color(uint8_t r, uint8_t g, uint8_t b);
void video_clear_screen(uint32_t colour);

typedef struct _draw_text_segment_args {
    size_t      left;
    size_t      top;
    uint32_t    colour;
    uint32_t    bg_colour;
    // expected to be 128x8
    uint8_t*    font_ptr;
    size_t      seg_offs;
    size_t      seg_len;

} draw_text_segment_args_t;

void video_draw_text_segment(draw_text_segment_args_t* args, const wchar_t* text);

#endif // _JOS_KERNEL_VIDEO_H