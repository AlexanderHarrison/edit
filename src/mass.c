Panel *mass_create(UI *ui, const U8 *dirpath) { TRACE
    Panel *panel = panel_create(ui);
    panel->update_fn = mass_update;
    panel->name = "mass search replace";
    Arena *arena = panel_arena(panel);
    
    Mass *mass = arena_alloc(arena, sizeof(Mass), alignof(Mass));
    *mass = (Mass) {
        .arena = arena,
        .files = arena_alloc(arena, MASS_MAX_FILES_SIZE, page_size()),
        .prefix_matches = arena_alloc(arena, MASS_MAX_MATCHES_SIZE, page_size()),
        .matches = arena_alloc(arena, MASS_MAX_MATCHES_SIZE, page_size()),
        .search = arena_alloc(arena, MASS_TEXT_SIZE, page_size()),
        .replace = arena_alloc(arena, MASS_TEXT_SIZE, page_size()),
    };
    panel->data = mass;
    
    if (dirpath == NULL) {
        char buf[512];
        dirpath = (U8*)getcwd(buf, sizeof(buf));
    }
    
    mass->dirpath = copy_cstr(arena, dirpath);
    mass_read_files(mass, dirpath);
    
    return panel;
}

void mass_clear(Mass *mass) {
    arena_clear(mass->arena);
    mass->search_len = 0;
    mass->replace_len = 0;
    mass->file_count = 0;
    mass->match_count = 0;
    mass->prefix_match_count = 0;
    mass->mode = MassMode_EditSearch;
}

void mass_update(Panel *panel) { TRACE
    // update -------------------------------------------------
    
    Mass *mass = panel->data;
    
    if (panel->flags & PanelFlag_Focused) {
        U64 special_pressed = w->inputs.key_special_pressed;
        U64 pressed = w->inputs.key_pressed;
        U64 modifiers = w->inputs.modifiers;

        bool ctrl = is(modifiers, GLFW_MOD_CONTROL);
        
        switch (mass->mode) {
            case MassMode_EditSearch: {
                U32 cursor = mass->search_len;
                if (write_inputs(mass->search, &mass->search_len, &cursor))
                    mass_search(mass);
                    
                bool esc = is(special_pressed, special_mask(GLFW_KEY_ESCAPE));
                bool caps = is(special_pressed, special_mask(GLFW_KEY_CAPS_LOCK));
                if (esc || caps)
                    panel_destroy_queued(panel);
                
                if (ctrl && is(pressed, key_mask(GLFW_KEY_R)))
                    mass->mode = MassMode_EditReplace;
                
                break;
            }
            case MassMode_EditReplace: {
                U32 cursor = mass->replace_len;
                write_inputs(mass->replace, &mass->replace_len, &cursor);
                
                bool esc = is(special_pressed, special_mask(GLFW_KEY_ESCAPE));
                bool caps = is(special_pressed, special_mask(GLFW_KEY_CAPS_LOCK));
                if (esc || caps)
                    mass->mode = MassMode_EditSearch;
                
                if (is(special_pressed, special_mask(GLFW_KEY_ENTER))) {
                    mass_execute(mass);
                    panel_destroy_queued(panel);
                }
                
                break;
            }
        }
    }
        
    // render -------------------------------------------------
    
    F32 font_height = font_height_px[CODE_FONT_SIZE];
    F32 y = font_height;
    for (U32 match_i = 0; match_i < mass->match_count; ++match_i) {
        F32 x = 0.f;
        
        Match *match = &mass->matches[match_i];
        File *file = &mass->files[match->file_idx];
        
        I64 line_start = match->match_offset;
        while (line_start > 0) {
            if (file->contents[line_start] == '\n') {
                line_start++;
                break;
            }
            line_start--;
        }
        
        I64 line_end = match->match_offset;
        while (line_end < file->contents_len) {
            if (file->contents[line_end] == '\n')
                break;
            line_end++;
        }
        
        // render before match
        for (I64 i = line_start; i < match->match_offset; ++i) {
            U8 ch = file->contents[i];
            U32 glyph_idx = glyph_lookup_idx(CODE_FONT_SIZE, ch);
            GlyphInfo info = panel->ui->atlas->glyph_info[glyph_idx];
            
            *ui_push_glyph(panel->ui) = (Glyph) {
                .x = panel->viewport.x + x + info.offset_x,
                .y = panel->viewport.y + y + info.offset_y,
                .glyph_idx = glyph_idx,
                .colour = (RGBA8)COLOUR_FOREGROUND,
            };
            x += info.advance_width;
            
            if (x > panel->viewport.w)
                break;
        }
        
        // render match
        U8 *replace;
        U32 replace_len;
        RGBA8 colour;
        if (mass->replace_len == 0) {
            replace = mass->search;
            replace_len = mass->search_len;
            colour = (RGBA8)COLOUR_RED;
        } else {
            replace = mass->replace;
            replace_len = mass->replace_len;
            colour = (RGBA8)COLOUR_GREEN;
        }
        for (I64 i = 0; i < replace_len; ++i) {
            U8 ch = replace[i];
            U32 glyph_idx = glyph_lookup_idx(CODE_FONT_SIZE, ch);
            GlyphInfo info = panel->ui->atlas->glyph_info[glyph_idx];
            
            *ui_push_glyph(panel->ui) = (Glyph) {
                .x = panel->viewport.x + x + info.offset_x,
                .y = panel->viewport.y + y + info.offset_y,
                .glyph_idx = glyph_idx,
                .colour = colour,
            };
            x += info.advance_width;
            
            if (x > panel->viewport.w)
                break;
        }
        
        // render after match
        for (I64 i = match->match_offset + mass->search_len; i < line_end; ++i) {
            U8 ch = file->contents[i];
            U32 glyph_idx = glyph_lookup_idx(CODE_FONT_SIZE, ch);
            GlyphInfo info = panel->ui->atlas->glyph_info[glyph_idx];
            
            *ui_push_glyph(panel->ui) = (Glyph) {
                .x = panel->viewport.x + x + info.offset_x,
                .y = panel->viewport.y + y + info.offset_y,
                .glyph_idx = glyph_idx,
                .colour = (RGBA8)COLOUR_FOREGROUND,
            };
            x += info.advance_width;
            
            if (x > panel->viewport.w)
                break;
        }
        
        y += font_height;
        if (y > panel->viewport.h)
            break;
    }
    
    // render mode info
    
    RGBA8 search_colour;
    RGBA8 replace_colour;
    if (mass->mode == MassMode_EditSearch) {
        search_colour = (RGBA8)COLOUR_GREEN;
        replace_colour = (RGBA8)COLOUR_RED;
    } else if (mass->mode == MassMode_EditReplace) {
        search_colour = (RGBA8)COLOUR_RED;
        replace_colour = (RGBA8)COLOUR_GREEN;
    } else {
        search_colour = (RGBA8)COLOUR_RED;
        replace_colour = (RGBA8)COLOUR_RED;
    }
    
    Rect mode_info_v = (Rect) {
        .x = panel->viewport.x,
        .y = panel->viewport.y + roundf(panel->viewport.h / 2.f) + MODE_INFO_Y_OFFSET,
        .w = panel->viewport.w,
        .h = MODE_INFO_HEIGHT,
    };

    *ui_push_glyph(panel->ui) = (Glyph) {
        .x = mode_info_v.x,
        .y = mode_info_v.y,
        .glyph_idx = special_glyph_rect((U32)mode_info_v.w, (U32)mode_info_v.h),
        .colour = COLOUR_MODE_INFO,
    };
    
    ui_push_string(
        panel->ui,
        mass->search, mass->search_len,
        panel->ui->atlas,
        search_colour, MODE_FONT_SIZE,
        mode_info_v.x, mode_info_v.y, mode_info_v.x + mode_info_v.w
    );
    
    mode_info_v.y += mode_info_v.h + 1;
    
    *ui_push_glyph(panel->ui) = (Glyph) {
        .x = mode_info_v.x,
        .y = mode_info_v.y,
        .glyph_idx = special_glyph_rect((U32)mode_info_v.w, (U32)mode_info_v.h),
        .colour = COLOUR_MODE_INFO,
    };
    
    ui_push_string(
        panel->ui,
        mass->replace, mass->replace_len,
        panel->ui->atlas,
        replace_colour, MODE_FONT_SIZE,
        mode_info_v.x, mode_info_v.y, mode_info_v.x + mode_info_v.w
    );
    
}

void mass_search_prefix_1(Mass *mass) {
    U8 c = mass->search[0];

    for (U32 file_i = 0; file_i < mass->file_count; ++file_i) {
        File *file = &mass->files[file_i];
        U32 end = file->contents_len;

        for (U32 i = 0; i + 8 < end; ++i) {
            if (file->contents[i] == c) {
                mass->prefix_matches[mass->prefix_match_count++] = (Match) {
                    .file_idx = file_i,
                    .match_offset = i,
                };
            }
        }
    }
}

void mass_search_prefix_2(Mass *mass) {
    U8 c[2];
    memcpy(c, mass->search, 2);

    for (U32 file_i = 0; file_i < mass->file_count; ++file_i) {
        File *file = &mass->files[file_i];
        U32 end = file->contents_len;

        for (U32 i = 0; i + 2 < end; ++i) {
            if (memcmp(c, &file->contents[i], 2) == 0) {
                mass->prefix_matches[mass->prefix_match_count++] = (Match) {
                    .file_idx = file_i,
                    .match_offset = i,
                };
            }
        }
    }
}

void mass_search_prefix_4(Mass *mass) {
    U8 c[4];
    memcpy(c, mass->search, 4);

    for (U32 file_i = 0; file_i < mass->file_count; ++file_i) {
        File *file = &mass->files[file_i];
        U32 end = file->contents_len;

        for (U32 i = 0; i + 4 < end; ++i) {
            if (memcmp(c, &file->contents[i], 4) == 0) {
                mass->prefix_matches[mass->prefix_match_count++] = (Match) {
                    .file_idx = file_i,
                    .match_offset = i,
                };
            }
        }
    }
}

void mass_search_prefix_8(Mass *mass) {
    U8 c[8];
    memcpy(c, mass->search, 8);

    for (U32 file_i = 0; file_i < mass->file_count; ++file_i) {
        File *file = &mass->files[file_i];
        U32 end = file->contents_len;

        for (U32 i = 0; i + 8 < end; ++i) {
            if (memcmp(c, &file->contents[i], 8) == 0) {
                mass->prefix_matches[mass->prefix_match_count++] = (Match) {
                    .file_idx = file_i,
                    .match_offset = i,
                };
            }
        }
    }
}

void mass_search(Mass *mass) {
    mass->match_count = 0;
    mass->prefix_match_count = 0;
    if (mass->search_len == 0) return;
    
    Timer t = timer_start();
    
    if (mass->search_len >= 8)
        mass_search_prefix_8(mass);
    else if (mass->search_len >= 4)
        mass_search_prefix_4(mass);
    else if (mass->search_len >= 2)
        mass_search_prefix_2(mass);
    else
        mass_search_prefix_1(mass);
        
    for (U32 prefix_match_i = 0; prefix_match_i < mass->prefix_match_count; ++prefix_match_i) {
        Match *match = &mass->prefix_matches[prefix_match_i];
        File *file = &mass->files[match->file_idx];
        
        int cmp = memcmp(&file->contents[match->match_offset], mass->search, mass->search_len);
        if (cmp == 0)
            mass->matches[mass->match_count++] = *match;
    }
    
    printf("search in %fms\n", timer_elapsed_ms(&t));
}

/*void mass_search(Mass *mass) {
    mass->match_count = 0;
    if (mass->search_len == 0) return;
    
    Timer t = timer_start();
    
    for (U32 file_i = 0; file_i < mass->file_count; ++file_i) {
        File *file = &mass->files[file_i];
        
        for (U32 i = 0; i + mass->search_len < file->contents_len; ++i) {
            if (memcmp(&file->contents[i], mass->search, mass->search_len) == 0) {
                mass->matches[mass->match_count++] = (Match) {
                    .file_idx = file_i,
                    .match_offset = i,
                };
            }
        }
    }
    
    printf("search in %fms\n", timer_elapsed_ms(&t));
}*/

static int mass_filter_dir(const struct dirent *entry) {
    return entry->d_type == DT_DIR && entry->d_name[0] != '.';
}
static int mass_filter_file(const struct dirent *entry) {
    return entry->d_type == DT_REG && entry->d_name[0] != '.';
}

static bool probably_utf8(U8 *f, U64 length) {
    for (U64 i = 0; i < length; ++i) {
        U8 c = f[i];
        if (c == 0)
            return false;
        if (c >= 0xF5)
            return false;
        if (c == 0xC0 || c == 0xC1)
            return false;
    }
    return true;
}

void mass_read_files(Mass *mass, const U8 *dirpath) { TRACE
    struct dirent **entries;

    {// iter child files
        int filenum = scandir((const char*)dirpath, &entries, mass_filter_file, alphasort);
        expect(filenum >= 0);

        for (int file = 0; file < filenum; ++file) {
            const char *filename = entries[file]->d_name;
            
            ArenaResetPoint reset = arena_reset_point(mass->arena);
            U8 *path = path_join(mass->arena, dirpath, (const U8*)filename);
            Bytes f = read_file_in((const char*)path, mass->arena);
            
            if (probably_utf8(f.ptr, f.len)) {
                mass->files[mass->file_count++] = (File) {
                    .path = path,
                    .contents_len = (U32)f.len,
                    .contents = f.ptr,
                };
            } else {
                printf("skipping %s\n", (char *)path);
                arena_reset(mass->arena, &reset);
            }
        }

        free(entries);
    }
    
    {// iter child directories
        int dirnum = scandir((const char*)dirpath, &entries, mass_filter_dir, alphasort);
        expect(dirnum >= 0);

        for (int dir = 0; dir < dirnum; ++dir) {
            const U8 *dirname = (const U8*)entries[dir]->d_name;
            U8 *subpath = path_join(mass->arena, dirpath, dirname);
            mass_read_files(mass, subpath);
        }

        free(entries);
    }
}

void mass_execute(Mass *mass) { TRACE
    U32 match_i = 0;
    for (U32 file_i = 0; file_i < mass->file_count; ++file_i) {
        File *file = &mass->files[file_i];
        FILE *f = fopen((const char*)file->path, "wb");
        if (f == NULL) {
            fprintf(stderr, "Could not open %s\n", (const char*)file->path);
            continue;
        }
        
        U32 contents_i = 0;
        while (match_i < mass->match_count) {
            Match *match = &mass->matches[match_i];
            if (match->file_idx > file_i)
                break;
            expect(match->match_offset >= contents_i);
                
            U32 until_match = match->match_offset - contents_i;
            size_t written = fwrite(&file->contents[contents_i], 1, until_match, f);
            expect(written == until_match);
            
            written = fwrite(mass->replace, 1, mass->replace_len, f);
            expect(written == mass->replace_len);
            
            contents_i += until_match + mass->search_len;
            
            match_i++;
        }
        
        U32 until_end = file->contents_len - contents_i;
        size_t written = fwrite(&file->contents[contents_i], 1, until_end, f);
        expect(written == until_end);
        
        fclose(f);
    }
}
