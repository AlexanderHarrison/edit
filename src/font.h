#ifndef FONT_H_
#define FONT_H_

typedef struct Glyph {
    F32 x, y;
    U32 glyph_idx;
    RGBA8 colour;
} Glyph;

static inline U32 special_glyph_rect(U32 width, U32 height) {
    if (width >= 4096) width = 4095;
    if (height >= 4096) height = 4095;
    return (1u << 24) | (width << 12) | height;
}

typedef struct GlyphSlice {
    Glyph *ptr;
    U64 count;
} GlyphSlice;

typedef struct AtlasLocation {
    U32 x, y;
    U32 width;
    U32 height;
} AtlasLocation;

typedef struct GlyphInfo {
    F32 offset_x, offset_y;
    F32 advance_width;
    F32 unused;
} GlyphInfo;

typedef struct FontAtlas {
    VkImage atlas_image;
    VkDeviceMemory atlas_image_memory;
    VkImageView atlas_image_view;

    // gpu buffer of AtlasLocation[256][FontSize].
    VkBuffer glyph_lookup_buffer;
    VkDeviceMemory glyph_lookup_buffer_memory;

    GlyphInfo *glyph_info;

    // metrics
    F32 descent[FontSize_Count];
    F32 ascent[FontSize_Count];
} FontAtlas;

static inline U32
glyph_lookup_idx(FontSize font_size, U8 ch) {
    return (U32)font_size * 256u + (U32)ch;
}

FontAtlas *
font_atlas_create(W *w, Arena *arena, const char *ttf);

void
font_atlas_destroy(W *w, FontAtlas *atlas);

#endif
