#include <ft2build.h>
#include FT_FREETYPE_H

#include "font.h"

#define GLYPH_LOOKUP_BUFFER_SIZE (256*sizeof(AtlasLocation)*FontSize_Count)
#define FONT_ATLAS_SIZE 512

FontAtlas *font_atlas_create(W *w, Arena *arena, const char *ttf_path) { TRACE
    FT_Library library;
    expect(FT_Init_FreeType(&library) == 0);

    FT_Face face;
    expect(FT_New_Face(library, ttf_path, 0, &face) == 0);

    FontAtlas *atlas = ARENA_ALLOC(arena, *atlas);

    // CREATE VK GLYPH ATLAS IMAGE ----------------------------------------------------
    {
        VkImageCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .extent = {
                .width = FONT_ATLAS_SIZE,
                .height = FONT_ATLAS_SIZE,
                .depth = 1,
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .format = VK_FORMAT_R8_UNORM,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .flags = 0,
        };
        VK_ASSERT(vkCreateImage(w->device, &info, NULL, &atlas->atlas_image));
        VK_ASSERT(gpu_alloc_image(
            w,
            atlas->atlas_image,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &atlas->atlas_image_memory
        ));

        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = atlas->atlas_image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R8_UNORM,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        VK_ASSERT(vkCreateImageView(
            w->device,
            &view_info,
            NULL,
            &atlas->atlas_image_view
        ));

        staging_buffer_push_image_transition(
            &w->staging_buffer,
            &w->frame_arena,
            atlas->atlas_image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL
        );
    }

    // CREATE VK GLYPH LOOKUP BUFFER -------------------------------------------------
    {
        VkBufferCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = GLYPH_LOOKUP_BUFFER_SIZE,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        };
        VK_ASSERT(vkCreateBuffer(w->device, &info, NULL, &atlas->glyph_lookup_buffer));
        VK_ASSERT(gpu_alloc_buffer(
            w,
            atlas->glyph_lookup_buffer,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &atlas->glyph_lookup_buffer_memory
        ));
    }

    atlas->glyph_info = ARENA_ALLOC_ARRAY(arena, *atlas->glyph_info, 256 * FontSize_Count);

    // CREATE FONT FACE -------------------------------------------------------------

    {
        StagingBuffer *staging = &w->staging_buffer;
        AtlasLocation* staging_glyph_lookup = (AtlasLocation*)staging_buffer_alloc(staging, GLYPH_LOOKUP_BUFFER_SIZE, 64);

        U32 copies = 0;
        VkBufferImageCopy* buffer_image_copies = ARENA_ALLOC_ARRAY(&w->frame_arena, *buffer_image_copies, 256*FontSize_Count);

        U64 image_suballocator_x = 0;
        U64 image_suballocator_y = 0;
        U64 image_suballocator_current_row_height = 0;

        for (U64 font_size_i = 0; font_size_i < FontSize_Count; ++font_size_i) {
            // TODO: scale factor calculation
            expect(FT_Set_Char_Size(face, 0, (I64)font_size_px[font_size_i] * 64, 0, 72.0f) == 0);

            atlas->descent[font_size_i] = (F32)face->size->metrics.descender / 64.f;
            atlas->ascent[font_size_i] = (F32)face->size->metrics.ascender / 64.f;

            // WRITE GLYPHS TO BUFFER --------------------------------------------------------

            for (U64 ch = 0; ch < 255; ++ch) {
                U64 ch_lookup_idx = font_size_i*256 + ch;

                // create glyph bitmap ----------------
                FT_Bitmap *bitmap;
                FT_GlyphSlot glyph;
                {
                    expect(FT_Load_Char(face, ch, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL) == 0);

                    glyph = face->glyph;
                    expect(glyph != NULL);

                    bitmap = &glyph->bitmap;
                    if (bitmap->width * bitmap->rows == 0) {
                        staging_glyph_lookup[ch_lookup_idx] = (AtlasLocation) {0};

                        atlas->glyph_info[ch_lookup_idx] = (GlyphInfo) {
                            .offset_x = (F32)glyph->bitmap_left,
                            .offset_y = -(F32)glyph->bitmap_top,
                            .advance_width = (F32)glyph->linearHoriAdvance / 65536.f,
                        };

                        continue;
                    }
                    expect(bitmap->pitch > 0);
                }

                // copy glyph details -----------------

                atlas->glyph_info[ch_lookup_idx] = (GlyphInfo) {
                    .offset_x = (F32)glyph->bitmap_left,
                    .offset_y = -(F32)glyph->bitmap_top,
                    .advance_width = (F32)glyph->linearHoriAdvance / 65536.f,
                };

                // copy glyph to staging ----------------
                // idk the alignment to use, 16 should be fine.

                U64 glyph_size = (U64)bitmap->rows * (U64)bitmap->pitch;

                U8* staging_image = staging_buffer_alloc(staging, glyph_size, 16);
                memcpy(staging_image, bitmap->buffer, glyph_size);

                U64 extent_x, extent_y;
                {
                    if (image_suballocator_x + (U64)bitmap->pitch > FONT_ATLAS_SIZE) {
                        image_suballocator_y += image_suballocator_current_row_height;
                        image_suballocator_current_row_height = 0;
                        image_suballocator_x = 0;
                    }

                    expect((U64)bitmap->pitch <= FONT_ATLAS_SIZE);
                    expect(image_suballocator_y + bitmap->rows <= FONT_ATLAS_SIZE);

                    extent_x = image_suballocator_x;
                    extent_y = image_suballocator_y;

                    if (bitmap->rows >= image_suballocator_current_row_height)
                        image_suballocator_current_row_height = bitmap->rows;

                    image_suballocator_x += (U64)bitmap->pitch;
                }

                staging_glyph_lookup[ch_lookup_idx] = (AtlasLocation) {
                    .x = (U32)extent_x,
                    .y = (U32)extent_y,
                    .width = bitmap->width,
                    .height = bitmap->rows,
                };

                buffer_image_copies[copies++] = (VkBufferImageCopy) {
                    .bufferOffset = (U64)((U8*)staging_image - staging->mapped_ptr),
                    .bufferRowLength = (U32)bitmap->pitch,
                    .bufferImageHeight = bitmap->rows,
                    .imageSubresource = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                    .imageOffset = {
                        .x = (I32)extent_x,
                        .y = (I32)extent_y,
                        .z = 0,
                    },
                    .imageExtent = {
                        .width = bitmap->width,
                        .height = bitmap->rows,
                        .depth = 1,
                    },
                };
            }

            staging_buffer_cmd_copy_to_image(
                staging,
                &w->frame_arena,
                atlas->atlas_image,
                copies,
                buffer_image_copies
            );

            VkBufferCopy *buffer_copy = ARENA_ALLOC(&w->frame_arena, *buffer_copy);
            *buffer_copy = (VkBufferCopy) {
                .srcOffset = (U64)((U8*)staging_glyph_lookup - staging->mapped_ptr),
                .dstOffset = 0,
                .size = GLYPH_LOOKUP_BUFFER_SIZE,
            };

            staging_buffer_cmd_copy_to_buffer(
                staging,
                &w->frame_arena,
                atlas->glyph_lookup_buffer,
                1,
                buffer_copy
            );
        }
    }

    // END ----------------------------------------------------------------------------

    FT_Done_Face(face);
    expect(FT_Done_FreeType(library) == 0);

    return atlas;
}

void font_atlas_destroy(W *w, FontAtlas *atlas) { TRACE
    gpu_free(w, atlas->glyph_lookup_buffer_memory);
    gpu_free(w, atlas->atlas_image_memory);
    vkDestroyBuffer(w->device, atlas->glyph_lookup_buffer, NULL);
    vkDestroyImage(w->device, atlas->atlas_image, NULL);
    vkDestroyImageView(w->device, atlas->atlas_image_view, NULL);
}
