#ifndef FONT_H_
#define FONT_H_

#include "common.h"

#define GLYPH_LOOKUP_BUFFER_SIZE (256*sizeof(GlyphLookup))
#define FONT_ATLAS_SIZE 1024

// all abstracted away - truetype used on unix, something else will be on windows

typedef struct FontBackend FontBackend;

typedef struct {
    F32 size;
    F32 scale_factor;
    const char *ttf_path;
} FontAtlasConfig;

typedef struct {
    U32 x;      // offset into lookup_buffer
    U32 y;      // offset into lookup_buffer
    U32 width;  // width of glyph row
    U32 height; // height of glyph (number of rows)
} GlyphLookup;

// 8x8
typedef struct {
    VkImage atlas_image;
    VkDeviceMemory atlas_image_memory;
    VkImageView atlas_image_view;

    VkBuffer glyph_lookup_buffer;
    VkDeviceMemory glyph_lookup_buffer_memory;
} FontAtlas;

FontBackend *
font_backend_create(W *w, Arena *arena);

void
font_backend_destroy(FontBackend *backend);

FontAtlas *
font_atlas_create(FontBackend *backend, FontAtlasConfig *cfg);

void
font_atlas_destroy(FontBackend *backend, FontAtlas *atlas);

#endif
