#include <ft2build.h>
#include FT_FREETYPE_H

#include "font.h"

// NOT THREAD SAFE

struct FontBackend {
    W *w;
    Arena *arena;
    FT_Library library;
    FontAtlas *atlas;
};

FontBackend *font_backend_create(W *w, Arena *arena) {
    FontBackend *backend = ARENA_ALLOC(arena, *backend);
    *backend = (FontBackend) { .w = w, .arena = arena, };
    assert(FT_Init_FreeType(&backend->library) == 0);
    return backend;
}

void font_backend_destroy(FontBackend *backend) {
    assert(FT_Done_FreeType(backend->library) == 0);
}

FontAtlas *font_atlas_create(FontBackend *backend, FontAtlasConfig *cfg) {
    FontAtlas *atlas = ARENA_ALLOC(backend->arena, *atlas);

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
        VK_ASSERT(vkCreateImage(backend->w->device, &info, NULL, &atlas->atlas_image));
        VK_ASSERT(gpu_alloc_image(
            backend->w,
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
            backend->w->device,
            &view_info,
            NULL,
            &atlas->atlas_image_view
        ));

        staging_buffer_push_image_transition(
            &backend->w->staging_buffer,
            &backend->w->frame_arena,
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
        VK_ASSERT(vkCreateBuffer(backend->w->device, &info, NULL, &atlas->glyph_lookup_buffer));
        VK_ASSERT(gpu_alloc_buffer(
            backend->w,
            atlas->glyph_lookup_buffer,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &atlas->glyph_lookup_buffer_memory
        ));
    }

    // CREATE FONT FACE -------------------------------------------------------------

    FT_Face face;
    {
        assert(FT_New_Face(backend->library, cfg->ttf_path, 0, &face) == 0);

        // TODO: more robust scale factor calculation
        //assert(FT_Set_Char_Size(face, 0, (I64)(cfg->size/16.0f), 0, (U32)(cfg->scale_factor*72.0f)) == 0);
        assert(FT_Set_Char_Size(face, 0, 10 * 64, 96, 96) == 0);
    }

    // WRITE GLYPHS TO BUFFER --------------------------------------------------------

    {
        StagingBuffer *staging = &backend->w->staging_buffer;

        GlyphLookup* staging_glyph_lookup = (GlyphLookup*)staging_buffer_alloc(staging, GLYPH_LOOKUP_BUFFER_SIZE, 16);

        U64 image_suballocator_x = 0;
        U64 image_suballocator_y = 0;
        U64 image_suballocator_current_row_height = 0;

        for (U64 ch = 0; ch < 255; ++ch) {
            // create glyph bitmap ----------------
            FT_Bitmap *bitmap;
            {
                assert(FT_Load_Char(face, ch, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL) == 0);
                FT_GlyphSlot glyph = face->glyph;
                assert(glyph != NULL);
                //assert(glyph->format == FT_GLYPH_FORMAT_);

                bitmap = &glyph->bitmap;

                if (bitmap->width * bitmap->rows == 0) {
                    staging_glyph_lookup[ch] = (GlyphLookup) {0};
                    continue;
                }

                assert(bitmap->pitch > 0);
                //assert(glyph->bitmap.width > 1);
                //printf("width %u height %u\n", glyph->bitmap.width, glyph->bitmap.height);
            }

            // copy glyph to staging ----------------
            // idk the alignment to use, 16 should be fine.

            U64 glyph_size = (U64)bitmap->rows * (U64)bitmap->pitch;

            U8* staging_image = staging_buffer_alloc(staging, glyph_size, 16);
            memcpy(staging_image, bitmap->buffer, glyph_size);

            U64 extent_x, extent_y;
            {
                if (image_suballocator_x + (U64)bitmap->pitch > FONT_ATLAS_SIZE) {
                    image_suballocator_x = 0;
                    image_suballocator_y += image_suballocator_current_row_height;
                    image_suballocator_current_row_height = 0;
                }

                assert((U64)bitmap->pitch <= FONT_ATLAS_SIZE);
                assert(image_suballocator_y + bitmap->rows <= FONT_ATLAS_SIZE);

                extent_x = image_suballocator_x;
                extent_y = image_suballocator_y;

                if (bitmap->rows > image_suballocator_current_row_height)
                    image_suballocator_current_row_height = bitmap->rows;

                image_suballocator_x += (U64)bitmap->pitch;
            }

            staging_glyph_lookup[ch] = (GlyphLookup) {
                .x = (U32)extent_x,
                .y = (U32)extent_y,
                .width = bitmap->width,
                .height = bitmap->rows,
            };

            VkBufferImageCopy buffer_image_copy = {
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

            staging_buffer_push_copy_cmd_to_image(
                staging,
                &backend->w->frame_arena,
                atlas->atlas_image,
                &buffer_image_copy
            );
        }

        VkBufferCopy buffer_copy = {
            .srcOffset = (U64)((U8*)staging_glyph_lookup - staging->mapped_ptr),
            .dstOffset = 0,
            .size = GLYPH_LOOKUP_BUFFER_SIZE,
        };

        staging_buffer_push_copy_cmd_to_buffer(
            staging,
            &backend->w->frame_arena,
            atlas->glyph_lookup_buffer,
            &buffer_copy
        );
    }

    // END ----------------------------------------------------------------------------

    FT_Done_Face(face);

    return atlas;
}

void font_atlas_destroy(FontBackend *backend, FontAtlas *atlas) {
    gpu_free(backend->w, atlas->glyph_lookup_buffer_memory);
    gpu_free(backend->w, atlas->atlas_image_memory);
    vkDestroyBuffer(backend->w->device, atlas->glyph_lookup_buffer, NULL);
    vkDestroyImage(backend->w->device, atlas->atlas_image, NULL);
    vkDestroyImageView(backend->w->device, atlas->atlas_image_view, NULL);
}
