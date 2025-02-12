typedef struct FileTreeRow FileTreeRow;

void    filetree_clear              (FileTree *ft);
U8     *filetree_get_full_path      (FileTree *ft, Arena *arena, FileTreeRow *row);
U8     *filetree_get_full_path_dir  (FileTree *ft, Arena *arena, Dir *dir);

enum DirFlags {
    DirFlag_Open = (1u << 0),
    DirFlag_Loaded = (1u << 1),
};

// TODO just use pointers man
typedef struct Dir {
    struct Dir *parent;
    U32 name_offset;
    U32 file_names_offset;
    U16 child_index;
    U16 file_count;
    U16 child_count;
    U16 flags;
} Dir;

typedef enum EntryType {
    EntryType_File,
    EntryType_Dir,
} EntryType;

typedef struct FileTreeRow {
    U8 entry_type;
    U32 depth;
    Dir *parent;
    union {
        Dir *dir;
        U8 *filename;
    };
} FileTreeRow;

typedef struct FileTree {
    U8 *name_buffer;
    Dir *dir_tree;
    U32 text_buffer_head;
    U32 dir_count;
    FileTreeRow *rows;
    U32 row_count;
    I64 file_select_row;
    Panel *target_editor;

    U8 *search_buffer;
    U32 search_buffer_head;
} FileTree;

static void filetree_remake_rows(FileTree *ft);

Panel *filetree_create(UI *ui, Panel *target_editor, const U8 *working_dir) { TRACE
    Panel *panel = panel_create(ui);
    Arena *arena = panel_arena(panel);
    FileTree *ft = arena_alloc(arena, sizeof(FileTree), alignof(FileTree));
    Dir *dir_tree = arena_alloc(arena, FILETREE_MAX_ENTRY_SIZE, page_size());
    U8 *name_buffer = arena_alloc(arena, FILETREE_MAX_TEXT_SIZE, page_size());
    FileTreeRow *rows = arena_alloc(arena, FILETREE_MAX_ROW_SIZE, page_size());

    *ft = (FileTree) {
        .name_buffer = name_buffer,
        .dir_tree = dir_tree,
        .rows = rows,
        .target_editor = target_editor,
        .search_buffer = arena_alloc(arena, FILETREE_MAX_SEARCH_SIZE, page_size())
    };

    panel->data = ft;
    panel->static_w = 100.f;
    panel->dynamic_weight_w = 0.1f;
    panel->update_fn = filetree_update;

    Arena *frame_arena = &panel->ui->w->frame_arena;
    if (working_dir == NULL) {
        char buf[512];
        filetree_set_directory(ft, frame_arena, (U8*)getcwd(buf, sizeof(buf)));
    } else {
        filetree_set_directory(ft, frame_arena, working_dir);
    }

    return panel;
}

static bool str_contains(const U8 *a, const U8 *b) {
    while (*a) {
        const U8 *a_2 = a;
        const U8 *b_2 = b;

        while (*b_2) {
            if (*b_2 != *a_2)
                goto NEXT_CHAR;
            b_2++;
            a_2++;
        }

        return true;

NEXT_CHAR:
        a++;
    }

    return false;
}

void filetree_update(Panel *panel) { TRACE
    FileTree *ft = panel->data;
    UI *ui = panel->ui;
    FontAtlas *font_atlas = ui->atlas;
    W *w = ui->w;

    // UPDATE ----------------------------------------------------

    bool search_buffer_changed = false;

    if (panel->flags & PanelFlag_Focused) {
        U64 special_pressed = w->inputs.key_special_pressed;
        U64 special_repeating = w->inputs.key_special_repeating;
        U64 pressed = w->inputs.key_pressed;
        U64 repeating = w->inputs.key_repeating;
        U64 modifiers = w->inputs.modifiers;

        bool ctrl = is(modifiers, GLFW_MOD_CONTROL);

        if (ctrl && is(pressed | repeating, key_mask(GLFW_KEY_R))) {
            filetree_dir_open_all(ft, &w->frame_arena, &ft->dir_tree[0]);
        }

        if (ctrl && is(pressed | repeating, key_mask(GLFW_KEY_J))) {
            ft->file_select_row++;
        }

        if (ctrl && is(pressed | repeating, key_mask(GLFW_KEY_K))) {
            if (ft->file_select_row > 0)
                ft->file_select_row--;
            else
                ft->file_select_row = 0;
        }

        if (is(special_pressed, special_mask(GLFW_KEY_ENTER))) {
            FileTreeRow *row = &ft->rows[ft->file_select_row];

            if (row->entry_type == EntryType_Dir) {
                if (row->dir->flags & DirFlag_Open) {
                    filetree_dir_close(ft, row->dir);
                } else {
                    filetree_dir_open(ft, &w->frame_arena, row->dir);
                }
            } else if (row->entry_type == EntryType_File) {
                U8 *filepath = filetree_get_full_path(ft, &w->frame_arena, row);
                Panel *target_editor = ft->target_editor;
                Editor *ed = target_editor->data;
                if (ed) {
                    editor_load_filepath(ed, filepath);
                    panel_focus_queued(target_editor);
                    panel_destroy_queued(panel);
                }
            }
        }

        if (ctrl && is(pressed, key_mask(GLFW_KEY_Q))) w->should_close = true;

        if (!ctrl) {
            for (I64 i = 0; i < w->inputs.char_event_count; ++i) {
                U32 codepoint = w->inputs.char_events[i].codepoint;
                // enforce ascii for now
                expect(codepoint < 128);

                U8 codepoint_as_char = (U8)codepoint;
                ft->search_buffer[ft->search_buffer_head++] = codepoint_as_char;
                ft->search_buffer[ft->search_buffer_head] = 0; // ensure null terminated
                search_buffer_changed = true;
            }

            if (is(special_pressed | special_repeating, special_mask(GLFW_KEY_BACKSPACE))) {
                if (ft->search_buffer_head > 0) {
                    search_buffer_changed = true;
                    ft->search_buffer_head--;
                }
            }

            if (search_buffer_changed) {
                ft->search_buffer[ft->search_buffer_head] = 0; // ensure null terminated
                filetree_remake_rows(ft);
            }
        }
    }

    // RENDER ----------------------------------------------------

    Rect filetree_v = panel->viewport;
    F32 y = filetree_v.y + CODE_LINE_SPACING;

    // write selection rect
    F32 descent = font_atlas->descent[CODE_FONT_SIZE];
    *ui_push_glyph(ui) = (Glyph) {
        .x = filetree_v.x,
        .y = filetree_v.y + (F32)(ft->file_select_row + 2) * CODE_LINE_SPACING - descent,
        .glyph_idx = special_glyph_rect((U32)filetree_v.w, (U32)CODE_LINE_SPACING),
        .colour = COLOUR_SELECT,
    };

    // write root dir
    Dir *root_dir = &ft->dir_tree[0];
    ui->glyph_count += write_string_terminated(
        &ui->glyphs[ui->glyph_count],
        ft->name_buffer + root_dir->name_offset,
        font_atlas,
        (RGBA8) COLOUR_RED, CODE_FONT_SIZE,
        filetree_v.x, y, filetree_v.x + filetree_v.w
    );
    y += CODE_LINE_SPACING;

    // write search buffer
    ui->glyph_count += write_string_terminated(
        &ui->glyphs[ui->glyph_count],
        ft->search_buffer,
        font_atlas,
        (RGBA8) COLOUR_GREEN, CODE_FONT_SIZE,
        filetree_v.x, y, filetree_v.x + filetree_v.w
    );
    y += CODE_LINE_SPACING;

    // write entry rows
    for (I64 row_i = 0; row_i < ft->row_count; ++row_i) {
        FileTreeRow *row = &ft->rows[row_i];
        
        F32 x = filetree_v.x + (F32)row->depth * FILETREE_INDENTATION_WIDTH;

        if (row->entry_type == EntryType_Dir) {
            Dir *dir = row->dir;
            U8 *dirname = ft->name_buffer + dir->name_offset;

            RGBA8 row_colour = (row->dir->flags & DirFlag_Open) ? (RGBA8) COLOUR_DIRECTORY_OPEN : (RGBA8) COLOUR_DIRECTORY_CLOSED;
            ui->glyph_count += write_string_terminated(
                &ui->glyphs[ui->glyph_count],
                dirname,
                font_atlas,
                row_colour, CODE_FONT_SIZE,
                x, y, filetree_v.x + filetree_v.w
            );
            y += CODE_LINE_SPACING;
        } else if (row->entry_type == EntryType_File) {
            ui->glyph_count += write_string_terminated(
                &ui->glyphs[ui->glyph_count],
                row->filename,
                font_atlas,
                (RGBA8) COLOUR_FOREGROUND, CODE_FONT_SIZE,
                x, y, filetree_v.x + filetree_v.w
            );
            y += CODE_LINE_SPACING;
        } else {
            expect(0);
        }

        if (y > filetree_v.y + filetree_v.h) break;
    }
}

static void filetree_remake_rows_inner(FileTree *ft, Dir *parent, U32 depth) {
    if ((parent->flags & DirFlag_Open) == 0) return;

    for (U16 child_dir_i = 0; child_dir_i < parent->child_count; ++child_dir_i) {
        Dir *child = &ft->dir_tree[parent->child_index + child_dir_i];
        U8 *filename = ft->name_buffer + child->name_offset;
        if (ft->search_buffer_head == 0 || str_contains(filename, ft->search_buffer)) {
            ft->rows[ft->row_count++] = (FileTreeRow) {
                .entry_type = EntryType_Dir,
                .parent = parent,
                .dir = child,
                .depth = depth,
            };
        }

        filetree_remake_rows_inner(ft, child, depth+1);
    }

    U8 *filename = ft->name_buffer + parent->file_names_offset;
    for (U16 file_i = 0; file_i < parent->file_count; ++file_i) {
        if (ft->search_buffer_head == 0 || str_contains(filename, ft->search_buffer)) {
            ft->rows[ft->row_count++] = (FileTreeRow) {
                .entry_type = EntryType_File,
                .parent = parent,
                .filename = filename,
                .depth = depth,
            };
        }

        filename += strlen((char*)filename) + 1;
    }
}

static void filetree_remake_rows(FileTree *ft) { TRACE
    if (ft->dir_count == 0) return;
    ft->row_count = 0;
    Dir *root_dir = &ft->dir_tree[0];
    filetree_remake_rows_inner(ft, root_dir, 0);
}

static U32 filetree_push_name(FileTree *ft, const U8 *name) { TRACE
    U32 name_offset = ft->text_buffer_head;
    U8 *null_term = (U8*)stpcpy((char*)ft->name_buffer + name_offset, (const char *)name);
    ft->text_buffer_head = (U32)(null_term - ft->name_buffer) + 1;
    return name_offset;
}

void filetree_clear(FileTree *ft) { TRACE
    ft->text_buffer_head = 0;
    ft->dir_count = 0;
    ft->row_count = 0;
}

static int filter_dir(const struct dirent *entry) {
    return entry->d_type == DT_DIR && entry->d_name[0] != '.';
}
static int filter_file(const struct dirent *entry) {
    return entry->d_type == DT_REG && entry->d_name[0] != '.';
}

static void filetree_load_dir(FileTree *ft, Arena *scratch, Dir *dir_entry) { TRACE
    U8 *dirpath = filetree_get_full_path_dir(ft, scratch, dir_entry);

    struct dirent **sorted_entries;

    {// iter child directories
        int dirnum = scandir((char*)dirpath, &sorted_entries, filter_dir, alphasort);
        expect(dirnum >= 0);
        U16 child_index = (U16)ft->dir_count;

        for (int dir = 0; dir < dirnum; ++dir) {
            U32 name_offset = filetree_push_name(ft, (const U8*)sorted_entries[dir]->d_name);
            //free(sorted_entries[dir]->d_name);
            ft->dir_tree[ft->dir_count++] = (Dir) {
                .parent = dir_entry,
                .name_offset = name_offset,
            };
        }

        free(sorted_entries);

        dir_entry->child_index = child_index;
        dir_entry->child_count = (U16)dirnum;
    }

    {// iter child files
        int filenum = scandir((char*)dirpath, &sorted_entries, filter_file, alphasort);
        expect(filenum >= 0);
        dir_entry->file_names_offset = ft->text_buffer_head;

        for (int file = 0; file < filenum; ++file) {
            filetree_push_name(ft, (const U8*)sorted_entries[file]->d_name);
            //free(sorted_entries[file]->d_name);
        }

        dir_entry->file_count = (U16)filenum;

        free(sorted_entries);
    }

    dir_entry->flags |= DirFlag_Loaded;
}

static void filetree_dir_open_all_inner(FileTree *ft, Arena *scratch, Dir *dir) {
    if ((dir->flags & DirFlag_Loaded) == 0)
        filetree_load_dir(ft, scratch, dir);
    dir->flags |= DirFlag_Open;

    for (U16 child_dir_i = 0; child_dir_i < dir->child_count; ++child_dir_i) {
        Dir *child = &ft->dir_tree[dir->child_index + child_dir_i];
        filetree_dir_open_all_inner(ft, scratch, child);
    }
}

void filetree_dir_open_all(FileTree *ft, Arena *scratch, Dir *dir) { TRACE
    filetree_dir_open_all_inner(ft, scratch, dir);
    filetree_remake_rows(ft);
}

void filetree_dir_open(FileTree *ft, Arena *scratch, Dir *dir) { TRACE
    if ((dir->flags & DirFlag_Loaded) == 0)
        filetree_load_dir(ft, scratch, dir);
    dir->flags |= DirFlag_Open;

    filetree_remake_rows(ft);
}

void filetree_dir_close(FileTree *ft, Dir *dir) { TRACE
    dir->flags &= (U16)(~DirFlag_Open);
    filetree_remake_rows(ft);
}

static void filetree_get_full_path_inner(FileTree *ft, Arena *arena, Dir *dir) {
    if (dir->parent) {
        filetree_get_full_path_dir(ft, arena, dir->parent);
        *(arena->head-1) = '/'; // replace null with separator
    }
    arena_copy_string_terminated(arena, ft->name_buffer + dir->name_offset);
}

U8 *filetree_get_full_path_dir(FileTree *ft, Arena *arena, Dir *dir) { TRACE
    U8 *full_path = arena->head;
    filetree_get_full_path_inner(ft, arena, dir);
    return full_path;
}

U8 *filetree_get_full_path(FileTree *ft, Arena *arena, FileTreeRow *row) { TRACE
    if (ft->dir_count == 0) return NULL;
    U8 *full_path = arena->head;

    if (row->entry_type == EntryType_Dir) {
        filetree_get_full_path_dir(ft, arena, row->dir);
    } else {
        filetree_get_full_path_dir(ft, arena, row->parent);
        *(arena->head-1) = '/'; // replace null with separator
        arena_copy_string_terminated(arena, row->filename);
    }

    return full_path;
}

void filetree_set_directory(FileTree *ft, Arena *scratch, const U8 *dirpath) { TRACE
    filetree_clear(ft);

    DIR *dir = opendir((const char *)dirpath);
    if (dir == NULL) return;

    U32 name_offset = filetree_push_name(ft, dirpath);
    ft->dir_count = 1;
    ft->dir_tree[0] = (Dir) {
        .parent = NULL,
        .name_offset = name_offset,
    };
    closedir(dir);
    filetree_dir_open(ft, scratch, &ft->dir_tree[0]);

    Dir *d = &ft->dir_tree[0];
    U8 *f = ft->name_buffer + d->file_names_offset;
    for (int i = 0; i < d->file_count; ++i) {
        f += strlen((char*)f) + 1;
    }
}
