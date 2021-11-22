
#include "scroller.h"
#include <video.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum _scroller_tile_type {

    kScTile_Ground1,
    kScTile_PipeBody1,
    kScTile_Cloud,
    kScTile_Sky,

    kScTile_NumberOfTiles

} scroller_tile_t;

enum _scroller_constants {

    kScLayer_Height = 8,

    kScLayer_VisibleFieldWidth = 32,
    kScLayer_LayerFieldWidth,   //< NOTE: kScLayer_VisibleLayerWidth+1

    // pixels
    kScTile_Height = 8,
    kScTile_Width = 8,
};

static uint32_t _scroller_palette[] = {

    // black
    0,
    // dark brown
    0x663300,
    // light brown
    0x99c400,
    // dark green
    0x193300,
    // grass green
    0x006600,
    // pipe body silver 
    0xc0c0c0,
    // pipe body dark 
    0x808080,
    // sky blue
    0x00ffff,
    // cloud white
    0x99ffff,
    // white
    0xffffff,
};

enum _scroller_palette_colour {

    kScColour_Black,
    kScColour_DarkBrown,
    kScColour_LightBrown,
    kScColour_DarkGreen,
    kScColour_GrassGreen,
    kScColour_PipeBodySilver,
    kScColour_PipeBodyDark,
    kScColour_SkyBlue,
    kScColour_CloudWhite,
    kScColour_White
};

// top down
static const uint8_t _scroller_sprites[kScTile_NumberOfTiles][kScTile_Height][kScTile_Width] = {

    // ground
    {
        { kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen,kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen },
        { kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen,kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen },
        { kScColour_GrassGreen, kScColour_DarkGreen, kScColour_DarkGreen, kScColour_GrassGreen, kScColour_DarkGreen, kScColour_GrassGreen, kScColour_GrassGreen, kScColour_DarkGreen },
        { kScColour_DarkGreen, kScColour_DarkGreen, kScColour_GrassGreen, kScColour_DarkGreen, kScColour_DarkGreen, kScColour_GrassGreen, kScColour_GrassGreen, kScColour_DarkGreen },
        { kScColour_DarkGreen, kScColour_DarkGreen, kScColour_DarkGreen, kScColour_DarkGreen, kScColour_DarkGreen, kScColour_GrassGreen, kScColour_DarkGreen, kScColour_DarkGreen },
        { kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown },
        { kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown },
        { kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown },
    },

    // pipe body
    {
        { kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite,kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite },
        { kScColour_CloudWhite, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver,kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodyDark },
        { kScColour_CloudWhite, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver,kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodyDark },
        { kScColour_CloudWhite, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver,kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodyDark },
        { kScColour_CloudWhite, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver,kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodyDark },
        { kScColour_CloudWhite, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver,kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodyDark },
        { kScColour_CloudWhite, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver,kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodyDark },
        { kScColour_CloudWhite, kScColour_PipeBodyDark, kScColour_PipeBodyDark, kScColour_PipeBodyDark, kScColour_PipeBodyDark, kScColour_PipeBodyDark, kScColour_PipeBodyDark, kScColour_PipeBodyDark },
    },

    // cloud 
    {
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_CloudWhite, kScColour_CloudWhite,kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue},
        { kScColour_SkyBlue, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite,kScColour_SkyBlue, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_SkyBlue },
        { kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite,kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite},
        { kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite,kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite},
        { kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite,kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_SkyBlue,kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
    },

    // sky
    {
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
    }
};

//NOTE: a multiple of kScTile_Height and Width
static rect_t _scroller_window;
// the tile field which we'll be rendering
static scroller_tile_t _scroller_layers[kScLayer_Height][kScLayer_LayerFieldWidth];
static uint32_t _scroller_bm[kScLayer_Height * kScTile_Height][kScLayer_LayerFieldWidth * kScTile_Width];

void scroller_initialise(rect_t* dest_rect) {

    _scroller_window = *dest_rect;

    for (size_t row = 0; row < kScLayer_Height; ++row) {
        for (size_t col = 0; col < kScLayer_LayerFieldWidth; ++col) {
            _scroller_layers[row][col] = kScTile_Sky;
        }
    }
    // ground
    for (size_t col = 0; col < kScLayer_LayerFieldWidth; ++col) {
        _scroller_layers[kScLayer_Height - 1][col] = kScTile_Ground1;
    }

    // a few clouds 
    for (size_t col = 0; col < kScLayer_LayerFieldWidth; ++col) {
        if (!(rand() % 4)) {
            _scroller_layers[rand() % 3][col] = kScTile_Cloud;
        }
    }

    // a few intermittent tiles to step on 
    for (size_t col = 0; col < kScLayer_LayerFieldWidth; ++col) {
        if (!(rand() % 6)) {
            _scroller_layers[kScLayer_Height - 2][col] = kScTile_PipeBody1;
        }
    }

    size_t x = 0;
    size_t y = 0;
    for (size_t row = 0; row < kScLayer_Height; ++row) {
        for (size_t col = 0; col < kScLayer_LayerFieldWidth; ++col) {

            const uint8_t* sprite = (const uint8_t*)&_scroller_sprites[_scroller_layers[row][col]];

            for (size_t i = 0; i < kScTile_Height; ++i) {
                for (size_t j = 0; j < kScTile_Width; ++j) {

                    _scroller_bm[y + i][x + j] = _scroller_palette[*sprite++];
                }
            }
            x += kScTile_Width;
        }
        x = 0;
        y += kScTile_Height;
    }
}

//TEST:
static void scroller_refresh(void) {

    for (size_t row = 0; row < kScLayer_Height; ++row) {
        for (size_t col = kScLayer_VisibleFieldWidth - 1; col < kScLayer_LayerFieldWidth; ++col) {
            _scroller_layers[row][col] = kScTile_Sky;
        }
    }

    // ground
    for (size_t col = kScLayer_VisibleFieldWidth - 1; col < kScLayer_LayerFieldWidth; ++col) {
        _scroller_layers[kScLayer_Height - 1][col] = kScTile_Ground1;
    }

    // a few clouds 
    for (size_t col = kScLayer_VisibleFieldWidth - 1; col < kScLayer_LayerFieldWidth; ++col) {
        if (!(rand() % 4)) {
            _scroller_layers[rand() % 3][col] = kScTile_Cloud;
        }
    }

    // a few intermittent tiles to step on 
    for (size_t col = kScLayer_VisibleFieldWidth - 1; col < kScLayer_LayerFieldWidth; ++col) {
        if (!(rand() % 6)) {
            _scroller_layers[kScLayer_Height - 2][col] = kScTile_PipeBody1;
        }
    }
}

void scroller_render_field(void) {

    static size_t _scroll_pos = 0;
    static const size_t bm_height = (kScLayer_Height * kScTile_Height);
    static const size_t bm_width = (kScLayer_LayerFieldWidth * kScTile_Width);
    static const size_t bm_vis_width = (kScLayer_VisibleFieldWidth * kScTile_Width);

    if (_scroll_pos) {
        // scroll bitmap horisontally, feed in from rightmost column
        uint32_t* bm_row = (uint32_t*)_scroller_bm;
        for (size_t row = 0; row < bm_height; ++row) {
            for (size_t j = 0; j < (bm_width - 1); ++j) {
                bm_row[j] = bm_row[j + 1];
            }
            bm_row += bm_width;
        }
    }

    video_scale_draw_bitmap((const uint32_t*)_scroller_bm, bm_vis_width, bm_height, bm_width,
        _scroller_window.top, _scroller_window.left, _scroller_window.right - _scroller_window.left, _scroller_window.bottom - _scroller_window.top, kVideo_Filter_None);
    video_present();

    ++_scroll_pos;
    if ((_scroll_pos % kScTile_Width) == 0) {
        // wrap; generate a new rigthmost column
        scroller_refresh();

        // fill the bitmap rightmost column with fresh pixels
        size_t y = 0;
        for (size_t row = 0; row < kScLayer_Height; ++row) {
            for (size_t col = kScLayer_VisibleFieldWidth - 1; col < kScLayer_LayerFieldWidth; ++col) {
                const uint8_t* sprite = (const uint8_t * )&_scroller_sprites[_scroller_layers[row][col]];
                for (size_t i = 0; i < kScTile_Height; ++i) {
                    for (size_t j = 0; j < kScTile_Width; ++j) {
                        _scroller_bm[y + i][bm_vis_width + j] = _scroller_palette[*sprite++];
                    }
                }
            }
            y += kScTile_Height;
        }
    }
}