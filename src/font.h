#ifndef FONT_H_
#define FONT_H_

#include "common.h"

typedef enum FontSize {
    FontSize_10, FontSize_12, FontSize_14, FontSize_16,
    FontSize_20, FontSize_24, FontSize_28, FontSize_32,
    FontSize_40, FontSize_48, FontSize_56, FontSize_64,
    FontSize_96, FontSize_128,

    FontSize_Count,
} FontSize;

static const F32 font_size_px[FontSize_Count] = {
    10.f, 12.f, 14.f, 16.f,
    20.f, 24.f, 28.f, 32.f,
    40.f, 48.f, 56.f, 64.f,
    96.f, 128.f
};

typedef struct Glyph {
    F32 x, y;
    U32 glyph_idx;
    RGBA8 colour;
} Glyph;

typedef struct {
    U32 x, y;
    U32 width;
    U32 height;
} AtlasLocation;

typedef struct {
    I32 offset_x, offset_y;
    I32 advance_width;
    I32 unused;
} GlyphKerning;

typedef struct {
    VkImage atlas_image;
    VkDeviceMemory atlas_image_memory;
    VkImageView atlas_image_view;

    // TODO
    //GlyphKerning *(kerning[256][FontSize_Count]);

    // gpu buffer of AtlasLocation[256][FontSize].
    VkBuffer glyph_lookup_buffer;
    VkDeviceMemory glyph_lookup_buffer_memory;
} FontAtlas;

static inline U32
glyph_lookup_idx(FontSize font_size, char ch) {
    return (U32)font_size * 256u + (U32)ch;
}

FontAtlas *
font_atlas_create(W *w, Arena *arena, const char *ttf);

void
font_atlas_destroy(W *w, FontAtlas *atlas);

#endif
