// UI STUFF ############################################################
// C-w - focus next panel
// C-W - focus previous panel
// C-q - exit
// C-p - create new editor vsplit after focus
//
// FILE TREE ###########################################################
// C-R - expand all folders
// Esc - exit filetree
//
// EDITOR ##############################################################
//
// MOVEMENT AND SELECTION  ---------------------------------------------
//   j - select next group
//   k - select previous group
//
//   h - expand selection group
//   l - contract selection group
//
//   K - contract selection upwards (buggy)
// C-J - contract selection downwards (buggy)
// C-K - expand selection upwards
//
// Tab - toggle paragraph granularity
//   r - toggle subword granularity
//
//   f - select entire file
//   F - select from selection start to end of file
// C-F - select from start of file to selection end
//
//   p - vsplit, adding another editor to the right
//
//   / - enter search mode
//
// SEARCH MODE ---------------------------------------------------------
// C-j - go to next matched item
// C-k - go to previous matched item
// Enter - exit search mode and select current matched item
//
// EDIT MODE -----------------------------------------------------------
//   c - delete selection and enter edit mode
//
//   i - enter edit mode at start of selection
//   a - enter edit mode at end of selection
//
//   I - enter edit mode at first non-whitespace character of selection
//   A - enter edit mode at last non-whitespace character of selection
//
// COPY PASTE ----------------------------------------------------------
//   d - cut selection and select next group
//   y - copy selection
//   p - paste
//
// MISC ----------------------------------------------------------------
// C-s - save
//   m - trim whitespace from ends of selection
//   u - undo
// C-r - redo
//
//   q - close editor if saved
//   Q - close editor without saving
//
//   w - contract selection group and select first new group
//   W - expand selection group and select first new group
//   e - contract selection group and select last new group
//   E - expand selection group and select last new group
//
//   t - open file tree and recursively expand all folders
//   T - open file tree

UndoStack   undo_create(Arena *arena);
void        undo_clear(UndoStack *st);
UndoElem   *undo_record(UndoStack *st, I64 at, U8 *text, I64 text_length, UndoOp op);

void        editor_clear_file(Editor *ed);
Range       editor_group(Editor *ed, Group group, I64 byte);
Range       editor_group_next(Editor *ed, Group group, I64 current_group_end);
Range       editor_group_prev(Editor *ed, Group group, I64 current_group_start);
I64         editor_line_index(Editor *ed, I64 byte);
Rect        editor_line_rect(Editor *ed, FontAtlas *font_atlas, I64 a, I64 b, Rect *text_v);
void        editor_selection_trim(Editor *ed);

void        editor_undo(Editor *ed);
void        editor_redo(Editor *ed);
static inline U8 editor_text(Editor *ed, I64 byte);
void        editor_text_remove(Editor *ed, I64 start, I64 end);
void        editor_text_insert(Editor *ed, I64 at, U8 *text, I64 length);
// same as above, but does not add to the undo stack
void        editor_text_remove_raw(Editor *ed, I64 start, I64 end);
void        editor_text_insert_raw(Editor *ed, I64 at, U8 *text, I64 length);

static U64 int_to_string(Arena *arena, I64 n);

// EDITOR ####################################################################

Panel *editor_create(UI *ui, const U8 *filepath) { TRACE
    Panel *panel = panel_create(ui);
    panel->update_fn = editor_update;
    Arena *arena = panel_arena(panel);

    Editor *ed = arena_alloc(arena, sizeof(Editor), alignof(Editor));
    *ed = (Editor) {
        .undo_stack = undo_create(arena),
        .selection_group = Group_Line,
        .copied_text = arena_alloc(arena, COPY_MAX_LENGTH, 16),
        .mode_text = arena_alloc(arena, MODE_TEXT_MAX_LENGTH, 16),
        .text = arena_alloc(arena, TEXT_MAX_LENGTH, page_size()),
    };
    ed->arena = arena;
    panel->data = ed;
    panel->name = "editor";

    expect(editor_load_filepath(ed, filepath) == 0);

    return panel;
}

void editor_group_expand(Editor *ed) { TRACE
    static const Group lut[Group_Count] = {
       Group_Paragraph, // Group_Paragraph
       Group_Paragraph, // Group_Line
       Group_Line,      // Group_Word
       Group_Word,      // Group_SubWord
       Group_Word,      // Group_Character
    };
    ed->selection_group = lut[ed->selection_group];
}

void editor_group_contract(Editor *ed) { TRACE
    static const Group lut[Group_Count] = {
       Group_Line,      // Group_Paragraph
       Group_Word,      // Group_Line
       Group_Character, // Group_Word
       Group_Character, // Group_SubWord
       Group_Character, // Group_Character
    };
    ed->selection_group = lut[ed->selection_group];
}

void editor_update(Panel *panel) { TRACE
    Editor *ed = panel->data;
    Rect *viewport = &panel->viewport;
    UI *ui = panel->ui;
    W *w = ui->w;
    FontAtlas *font_atlas = ui->atlas;

    // UPDATE ---------------------------------------------------------------

    // state switch
    if (panel->flags & PanelFlag_Focused) {
        U64 special_pressed = w->inputs.key_special_pressed;
        U64 special_repeating = w->inputs.key_special_repeating;
        U64 pressed = w->inputs.key_pressed;
        U64 repeating = w->inputs.key_repeating;
        U64 modifiers = w->inputs.modifiers;

        bool ctrl = is(modifiers, GLFW_MOD_CONTROL);
        bool shift = is(modifiers, GLFW_MOD_SHIFT);

        switch (ed->mode) {
        case Mode_Normal: {
            if (!ctrl && !shift && is(pressed | repeating, key_mask(GLFW_KEY_U)))
                editor_undo(ed);

            if (ctrl && !shift && is(pressed | repeating, key_mask(GLFW_KEY_R)))
                editor_redo(ed);

            if (is(pressed, key_mask(GLFW_KEY_H))) {
                if (shift)
                    ed->selection_group = Group_Line;
                else
                    editor_group_expand(ed);
            }

            if (is(pressed, key_mask(GLFW_KEY_L))) {
                if (shift)
                    ed->selection_group = Group_Character;
                else
                    editor_group_contract(ed);
            }

            if (ctrl && is(pressed, key_mask(GLFW_KEY_S))) {
                if (ed->filepath && (ed->flags & EditorFlag_Unsaved) != 0) {
                    expect(ed->text_length >= 0);
                    expect(write_file((char*)ed->filepath, ed->text, (U64)ed->text_length) == 0);
                    ed->flags &= ~(U32)EditorFlag_Unsaved;
                    printf("wrote %li bytes to %s\n", ed->text_length, ed->filepath);
                }
            }

            if (is(pressed | repeating, key_mask(GLFW_KEY_J))) {
                if (!ctrl && !shift) {
                    Range next_group = editor_group_next(ed, ed->selection_group, ed->selection_b);
                    ed->selection_a = next_group.start;
                    ed->selection_b = next_group.end;
                } else if (!ctrl && shift) {
                    Range next_group = editor_group_next(ed, ed->selection_group, ed->selection_b);
                    ed->selection_b = next_group.end;
                } else if (ctrl && shift) {
                    Range a_group = editor_group(ed, ed->selection_group, ed->selection_a);
                    Range next_group = editor_group_next(ed, ed->selection_group, a_group.end);
                    ed->selection_a = next_group.start;
                }
            }

            if (is(pressed | repeating, key_mask(GLFW_KEY_K))) {
                if (!ctrl && !shift) {
                    Range prev_group = editor_group_prev(ed, ed->selection_group, ed->selection_a);
                    ed->selection_a = prev_group.start;
                    ed->selection_b = prev_group.end;
                } else if (!ctrl && shift) {
                    Range prev_group = editor_group_prev(ed, ed->selection_group, ed->selection_b);
                    ed->selection_b = prev_group.start;
                } else if (ctrl && shift) {
                    Range prev_group = editor_group_prev(ed, ed->selection_group, ed->selection_a);
                    ed->selection_a = prev_group.start;
                }
            }

            if (is(pressed, key_mask(GLFW_KEY_C))) {
                if (shift)
                    editor_selection_trim(ed);
                ed->mode = Mode_Insert;
                ed->insert_cursor = ed->selection_a;
                editor_text_remove(ed, ed->selection_a, ed->selection_b);
            }

            if (is(pressed, key_mask(GLFW_KEY_I))) {
                if (shift)
                    editor_selection_trim(ed);
                ed->mode = Mode_Insert;
                ed->insert_cursor = ed->selection_a;
            }

            if (is(pressed, key_mask(GLFW_KEY_A))) {
                if (shift)
                    editor_selection_trim(ed);
                ed->mode = Mode_Insert;
                ed->insert_cursor = ed->selection_b;
            }

            if (is(pressed, key_mask(GLFW_KEY_M))) {
                editor_selection_trim(ed);
            }

            if (!ctrl && is(pressed, key_mask(GLFW_KEY_W))) {
                editor_selection_trim(ed);
                if (shift)
                    editor_group_expand(ed);
                else
                    editor_group_contract(ed);
                Range range = editor_group(ed, ed->selection_group, ed->selection_a);
                ed->selection_a = range.start;
                ed->selection_b = range.end;
            }

            if (is(pressed, key_mask(GLFW_KEY_E))) {
                editor_selection_trim(ed);
                if (shift)
                    editor_group_expand(ed);
                else
                    editor_group_contract(ed);
                Range range = editor_group(ed, ed->selection_group, ed->selection_b);
                ed->selection_a = range.start;
                ed->selection_b = range.end;
            }

            if (!ctrl && is(pressed | repeating, key_mask(GLFW_KEY_D))) {
                editor_text_remove(ed, ed->selection_a, ed->selection_b);
                Range range = editor_group(ed, ed->selection_group, ed->selection_a);
                ed->selection_a = range.start;
                ed->selection_b = range.end;
            }

            if (is(pressed, key_mask(GLFW_KEY_Y))) {
                I64 copy_start = clamp(ed->selection_a, 0, ed->text_length);
                I64 copy_end = clamp(ed->selection_b, 0, ed->text_length);
                U32 copy_length = (U32)clamp(copy_end - copy_start, 0, COPY_MAX_LENGTH);

                ed->copied_text_length = copy_length;
                memcpy(ed->copied_text, &ed->text[copy_start], copy_length);
            }

            if (is(pressed, key_mask(GLFW_KEY_F))) {
                if (!ctrl)
                    ed->selection_b = ed->text_length;
                if (!shift)
                    ed->selection_a = 0;
            }

            if (is(pressed, key_mask(GLFW_KEY_R))) {
                if (ed->selection_group == Group_SubWord) {
                    ed->selection_group = Group_Word;
                    Range range = editor_group(ed, ed->selection_group, ed->selection_a);
                    ed->selection_a = range.start;
                    ed->selection_b = range.end;
                } else {
                    ed->selection_group = Group_SubWord;
                    Range range = editor_group(ed, ed->selection_group, ed->selection_a);
                    ed->selection_a = range.start;
                    ed->selection_b = range.end;
                }
            }

            if (is(pressed, key_mask(GLFW_KEY_SLASH))) {
                ed->mode = Mode_Search;
                ed->mode_text_length = 0;
            }

            if (!ctrl && is(pressed, key_mask(GLFW_KEY_P))) {
                I64 paste_idx;
                if (shift) {
                    paste_idx = ed->selection_a;
                } else {
                    paste_idx = ed->selection_b;
                }

                editor_text_insert(ed, paste_idx, ed->copied_text, ed->copied_text_length);
                ed->selection_a = paste_idx;
                ed->selection_b = paste_idx + ed->copied_text_length;
            }

            if (is(pressed, key_mask(GLFW_KEY_T))) {
                Panel *filetree_panel = filetree_create(panel->ui, panel, NULL);
                panel_insert_before_queued(panel, filetree_panel);
                panel_focus_queued(filetree_panel);

                if (!shift) {
                    FileTree *ft = filetree_panel->data;
                    filetree_dir_open_all(ft, &w->frame_arena, ft->dir_tree);
                }
            }

            //if (is(pressed, key_mask(GLFW_KEY_K))) ed->scroll_y -= 1.f;
            //else if (is(repeating, key_mask(GLFW_KEY_K))) ed->scroll_y -= CODE_SCROLL_SPEED_SLOW;

            //if (ctrl & is(held, key_mask(GLFW_KEY_D))) ed->scroll_y += CODE_SCROLL_SPEED_FAST;
            //if (ctrl & is(held, key_mask(GLFW_KEY_U))) ed->scroll_y -= CODE_SCROLL_SPEED_FAST;

            if (!ctrl && is(pressed, key_mask(GLFW_KEY_Q))) {
                if ((ed->flags & EditorFlag_Unsaved) == 0 || shift)
                    panel_destroy_queued(panel);
            }
            break;
        }
        case Mode_Insert: {
            bool esc = is(special_pressed, special_mask(GLFW_KEY_ESCAPE));
            bool caps = is(special_pressed, special_mask(GLFW_KEY_CAPS_LOCK));
            if (esc || caps) {
                ed->mode = Mode_Normal;
                Range range = editor_group(ed, ed->selection_group, ed->insert_cursor);
                ed->selection_a = range.start;
                ed->selection_b = range.end;
            }

            for (I64 i = 0; i < w->inputs.char_event_count; ++i) {
                U32 codepoint = w->inputs.char_events[i].codepoint;
                // enforce ascii for now
                expect(codepoint < 128);

                U8 codepoint_as_char = (U8)codepoint;
                editor_text_insert(ed, ed->insert_cursor, &codepoint_as_char, 1);
                ed->insert_cursor += 1;
            }

            if (ctrl && is(pressed | repeating, key_mask(GLFW_KEY_W))) {
                Range word = editor_group(ed, Group_SubWord, ed->insert_cursor);
                editor_text_remove(ed, word.start, ed->insert_cursor);
                ed->insert_cursor = word.start;
            }

            if (is(special_pressed, special_mask(GLFW_KEY_ENTER))) {
                Range line = editor_group(ed, Group_Line, ed->insert_cursor);
                I64 indent = 0;
                while (editor_text(ed, line.start++) == ' ')
                    indent++;

                U8 *text = ARENA_ALLOC_ARRAY(&w->frame_arena, U8, (U64)(indent+1));
                text[0] = '\n';
                for (I64 i = 1; i <= indent; ++i)
                    text[i] = ' ';

                editor_text_insert(ed, ed->insert_cursor, text, indent+1);
                ed->insert_cursor += indent+1;
            }

            if (is(special_pressed, special_mask(GLFW_KEY_TAB))) {
                Range line = editor_group(ed, Group_Line, ed->insert_cursor);
                I64 spaces = 1;
                while ((line.end+spaces) % 4 != 0)
                    spaces++;

                U8 *text = ARENA_ALLOC_ARRAY(&w->frame_arena, U8, (U64)spaces);
                for (I64 i = 0; i < spaces; ++i)
                    text[i] = ' ';

                editor_text_insert(ed, ed->insert_cursor, text, spaces);
                ed->insert_cursor += spaces;
            }

            if (is(special_pressed | special_repeating, special_mask(GLFW_KEY_BACKSPACE))) {
                ed->insert_cursor -= 1;
                editor_text_remove(ed, ed->insert_cursor, ed->insert_cursor+1);
            }

            break;
        }
        case Mode_Search: {
            for (I64 i = 0; i < w->inputs.char_event_count; ++i) {
                ed->search_cursor = 0;

                U32 codepoint = w->inputs.char_events[i].codepoint;
                // enforce ascii for now
                expect(codepoint < 128);

                U8 codepoint_as_char = (U8)codepoint;
                ed->mode_text[ed->mode_text_length++] = codepoint_as_char;
            }

            if (is(special_pressed | special_repeating, special_mask(GLFW_KEY_BACKSPACE))) {
                if (ed->mode_text_length > 0)
                    ed->mode_text_length--;
            }

            ed->search_matches = arena_prealign(&w->frame_arena, alignof(*ed->search_matches));
            ed->search_match_count = 0;
            if (ed->mode_text_length > 0) {
                I64 start = ed->selection_a;
                I64 end = ed->selection_b - ed->mode_text_length;
                for (I64 a = start; a < end; ++a) {
                    bool matches = true;
                    for (I64 i = 0; i < ed->mode_text_length; ++i) {
                        U8 search_char = ed->mode_text[i];
                        U8 text_char = ed->text[a+i];
                        if (search_char != text_char) {
                            matches = false; 
                            break;
                        }
                    }

                    if (matches) {
                        I64 *match_i = ARENA_ALLOC(&w->frame_arena, *match_i);
                        *match_i = a;
                        ed->search_match_count++;
                    }
                }
            }

            if (ctrl && is(pressed | repeating, key_mask(GLFW_KEY_J)))
                ed->search_cursor++;

            if (ctrl && is(pressed | repeating, key_mask(GLFW_KEY_K)))
                ed->search_cursor--;

            if (ed->search_cursor >= ed->search_match_count)
                ed->search_cursor = ed->search_match_count - 1;

            if (ed->search_cursor < 0)
                ed->search_cursor = 0;

            bool esc = is(special_pressed, special_mask(GLFW_KEY_ESCAPE));
            bool caps = is(special_pressed, special_mask(GLFW_KEY_CAPS_LOCK));
            if (esc || caps) {
                ed->mode = Mode_Normal;
            }

            if (is(special_pressed, special_mask(GLFW_KEY_ENTER))) { 
                if (ed->search_match_count > 0) {
                    I64 shown_match = ed->search_matches[ed->search_cursor];
                    ed->selection_a = shown_match;
                    ed->selection_b = shown_match + ed->mode_text_length;
                }
                ed->mode = Mode_Normal;
            }

            break;
        }
        //case (Mode_QuickSearch) {
        //    break;
        //}
        }
    }

    if (ed->selection_a > ed->selection_b) {
        fprintf(stderr, "Error: Cursor range out of order! Reversing %li <-> %li\n", ed->selection_a, ed->selection_b);
        I64 temp = ed->selection_a;
        ed->selection_a = ed->selection_b;
        ed->selection_b = temp;
    }

    // UPDATE ANIMATIONS ----------------------------------------------------

    // find new scroll y
    { 
        Mode mode = ed->mode;
        if (mode == Mode_Normal || mode == Mode_Insert) {
            I64 line_a = editor_line_index(ed, ed->selection_a);
            I64 line_b = editor_line_index(ed, ed->selection_b);
            ed->scroll_y = ((F32)line_a + (F32)line_b) / 2.f;
        } else if (mode == Mode_Search) {
            if (ed->search_match_count > 0) {
                I64 shown_match = ed->search_matches[ed->search_cursor];
                I64 line_a = editor_line_index(ed, shown_match);
                I64 line_b = editor_line_index(ed, shown_match + ed->mode_text_length);
                ed->scroll_y = ((F32)line_a + (F32)line_b) / 2.f;
            }
        }

        // animate scrolling
        F32 diff = ed->scroll_y - ed->scroll_y_visual;
        if (fabs(diff) < 0.05f) 
            ed->scroll_y_visual = ed->scroll_y;
        else
            ed->scroll_y_visual += diff * ANIM_EXP_FACTOR;
    }

    // START RENDER ----------------------------------------------------------

    Rect selection_bar_v = *viewport;
    selection_bar_v.w = BAR_SIZE;

    Rect text_v = selection_bar_v;
    text_v.x += selection_bar_v.w;
    text_v.w = viewport->w - text_v.x;

    // WRITE SPECIAL GLYPHS -------------------------------------------------

    // determine visible lines
    I64 byte_visible_start;
    I64 byte_visible_end;
    {
        F32 line_i = 0.f;
        I64 a = 0;
        while (1) {
            F32 line_offset_from_scroll = line_i - ed->scroll_y_visual;
            F32 line_y = line_offset_from_scroll * CODE_LINE_SPACING + text_v.h / 2.f;
            if (line_y + CODE_LINE_SPACING > 0.f) break;

            Range line = editor_group(ed, Group_Line, a);
            if (line.end >= ed->text_length)
                break;
            else
                a = line.end;
            line_i += 1.f;
        }
        byte_visible_start = a;

        while (1) {
            F32 line_offset_from_scroll = line_i - ed->scroll_y_visual;
            F32 line_y = line_offset_from_scroll * CODE_LINE_SPACING + text_v.h / 2.f;

            if (line_y > text_v.h) break;

            Range line = editor_group(ed, Group_Line, a);
            a = line.end;
            if (line.end >= ed->text_length)
                break;
            line_i += 1.f;
        }
        byte_visible_end = a;
    }

    // selection group bar
    {
        RGBA8 selection_bar_colour;

        static const RGBA8 selection_colours[Group_Count] = {
            {255, 0, 0, 255},       // Group_Paragraph      red
            {255, 100, 0, 255},     // Group_Line           orange
            {255, 255, 0, 255},     // Group_Word           yellow
            {100, 100, 255, 255},   // Group_SubWord        green
            {0, 255, 0, 255},       // Group_Character      blue
        };                      

        if (ed->mode == Mode_Normal) {
            selection_bar_colour = selection_colours[ed->selection_group];
        } else if (ed->mode == Mode_Insert) {
            selection_bar_colour = (RGBA8)COLOUR_FOREGROUND;
        } else if (ed->mode == Mode_Search) {
            selection_bar_colour = (RGBA8) { 0, 255, 100, 255 };
        } else {
            expect(0);
        }

        *ui_push_glyph(ui) = (Glyph) {
            .x = selection_bar_v.x,
            .y = selection_bar_v.y,
            .glyph_idx = special_glyph_rect((U32)selection_bar_v.w, (U32)selection_bar_v.h),
            .colour = selection_bar_colour,
        };
    }

    // selection rects
    if (ed->mode == Mode_Normal) {
        I64 a = ed->selection_a;
        I64 b = ed->selection_b;
        if (a < byte_visible_start) a = byte_visible_start;
        if (b > byte_visible_end) b = byte_visible_end;
        F32 line_i = (F32)editor_line_index(ed, a);

        while (1) {
            F32 line_offset_from_scroll = line_i - ed->scroll_y_visual;
            F32 line_y = line_offset_from_scroll * CODE_LINE_SPACING + text_v.h / 2.f;
            if (line_y > text_v.h) break;

            Rect rect = editor_line_rect(ed, font_atlas, a, b, &text_v);
            *ui_push_glyph(ui) = (Glyph) {
                .x = rect.x,
                .y = rect.y,
                .glyph_idx = special_glyph_rect((U32)rect.w, (U32)rect.h),
                .colour = COLOUR_SELECT,
            };

            Range line = editor_group(ed, Group_Line, a);
            if (line.end >= b)
                break;
            else
                a = line.end;
            line_i += 1.f;
        }
    }
    if (ed->mode == Mode_Insert) {
        I64 cursor = ed->insert_cursor;
        Rect rect = editor_line_rect(ed, font_atlas, cursor, cursor, &text_v);
        rect.w = 2.f;
        *ui_push_glyph(ui) = (Glyph) {
            .x = rect.x,
            .y = rect.y,
            .glyph_idx = special_glyph_rect((U32)rect.w, (U32)rect.h),
            .colour = COLOUR_FOREGROUND,
        };
    }
    if (ed->mode == Mode_Search) {
        for (I64 i = 0; i < ed->search_match_count; ++i) {
            I64 match_idx = ed->search_matches[i];

            if (match_idx < byte_visible_start) continue;
            if (match_idx > byte_visible_end) break;

            Rect rect = editor_line_rect(ed, font_atlas, match_idx, match_idx + ed->mode_text_length, &text_v);
            RGBA8 colour = i != ed->search_cursor ? (RGBA8)COLOUR_SEARCH : (RGBA8)COLOUR_SEARCH_SHOWN;

            if (rect.w == 0.f)
                rect.w = 2.f;

            *ui_push_glyph(ui) = (Glyph) {
                .x = rect.x,
                .y = rect.y,
                .glyph_idx = special_glyph_rect((U32)rect.w, (U32)rect.h),
                .colour = colour,
            };
        }
    }

    // WRITE TEXT GLYPHS ----------------------------------------------------

    I64 text_length = ed->text_length;
    F32 line = 0.f;
    F32 pen_x = 0.f;

    enum StateFlags {
        State_Normal,
        State_LineComment,
        State_BlockComment,
        State_String_DoubleQ,
        State_String_SingleQ,
        State_String_End,

        State_Count
    };

    static const RGBA8 state_colours[State_Count] = {
        COLOUR_FOREGROUND,
        COLOUR_COMMENT,
        COLOUR_COMMENT,
        COLOUR_STRING,
        COLOUR_STRING,
        COLOUR_STRING
    };

    U64 state = State_Normal;
    FontSize font_size = CODE_FONT_SIZE;
    F32 spacing = CODE_LINE_SPACING;
    for (I64 i = 0; i < text_length; ++i) {
        U8 ch_prev = editor_text(ed, i-1);
        U8 ch = editor_text(ed, i);
        U8 ch_next = editor_text(ed, i+1);

        if (ch == '\n') {
            line += 1.f;
            pen_x = 0.f;
            
            if (state == State_LineComment)
                state = State_Normal;

            continue;
        }

        if (state == State_String_End) {
            state = State_Normal;
        }

        if (state == State_Normal) {
            if (ch == '/' && ch_next == '/') {
                state = State_LineComment;
            } else if (ch == '/' && ch_next == '*') {
                state = State_BlockComment;
            } else if (ch == '\"') {
                state = State_String_DoubleQ;
            } else if (ch == '\'') {
                state = State_String_SingleQ;
            }
        } else if (state == State_BlockComment) {
            if (ch == '*' && ch_next == '/') {
                state = State_Normal;
            }
        } else if (state == State_String_DoubleQ) {
            U8 ch_prev2 = editor_text(ed, i-2);
            if (ch == '\"' && (ch_prev != '\\' || ch_prev2 == '\\')) {
                state = State_String_End;
            }
        } else if (state == State_String_SingleQ) {
            U8 ch_prev2 = editor_text(ed, i-2);
            if (ch == '\'' && (ch_prev != '\\' || ch_prev2 == '\\')) {
                state = State_String_End;
            }
        }

        // bounds checking -----

        F32 line_offset_from_scroll = line - ed->scroll_y_visual;
        F32 line_y = line_offset_from_scroll * CODE_LINE_SPACING + text_v.h / 2.f;

        if (line_y + spacing < 0.f) continue;
        if (line_y > text_v.h) break;

        // if is in text_v ----

        F32 pen_y = text_v.y + line_y + spacing;

        U32 glyph_idx = glyph_lookup_idx(font_size, ch);
        GlyphInfo info = font_atlas->glyph_info[glyph_idx];

        if (pen_x + info.advance_width <= text_v.x + text_v.w) {
            *ui_push_glyph(ui) = (Glyph) {
                .x = text_v.x + pen_x + info.offset_x,
                .y = text_v.y + pen_y + info.offset_y,
                .glyph_idx = glyph_idx,
                .colour = state_colours[state],
            };
        }
        pen_x += info.advance_width;
    }

    // WRITE MODE INFO GLYPHS -------------------------------------------------

    if (ed->mode == Mode_Search) {
        Rect mode_info_v = (Rect) {
            .x = text_v.x,
            .y = text_v.y + roundf(text_v.h / 2.f) + MODE_INFO_Y_OFFSET,
            .w = text_v.w,
            .h = MODE_INFO_HEIGHT,
        };

        *ui_push_glyph(ui) = (Glyph) {
            .x = mode_info_v.x,
            .y = mode_info_v.y,
            .glyph_idx = special_glyph_rect((U32)mode_info_v.w, (U32)mode_info_v.h),
            .colour = COLOUR_MODE_INFO,
        };
        
        // show selection index/count
        U8 *mode_info_text = arena_prealign(&w->frame_arena, 1);

        RGBA8 colour;
        if (ed->search_match_count > 0 && ed->mode_text_length > 0) {
            colour = (RGBA8) COLOUR_GREEN;
            int_to_string(&w->frame_arena, ed->search_cursor+1);
            *(U8*)ARENA_ALLOC(&w->frame_arena, U8) = '/';
            int_to_string(&w->frame_arena, ed->search_match_count);
        } else {
            colour = (RGBA8) COLOUR_RED;
        }

        do {
            *(U8*)ARENA_ALLOC(&w->frame_arena, U8) = ' ';
        } while (w->frame_arena.head - mode_info_text < 6);

        // show selection text
        U8 *copied_mode_text = ARENA_ALLOC_ARRAY(&w->frame_arena, U8, (U64)ed->mode_text_length);
        for (I64 i = 0; i < ed->mode_text_length; ++i)
            copied_mode_text[i] = ed->mode_text[i];

        // null terminator
        *(U8*)ARENA_ALLOC(&w->frame_arena, U8) = 0;

        F32 y = mode_info_v.y + mode_info_v.h - MODE_INFO_PADDING;
        F32 x = mode_info_v.x + MODE_INFO_PADDING;
        ui->glyph_count += write_string_terminated(
            &ui->glyphs[ui->glyph_count],
            mode_info_text,
            font_atlas,
            colour, MODE_FONT_SIZE,
            x, y, mode_info_v.w - mode_info_v.x - MODE_INFO_PADDING*2.f
        );
    } else if (ed->mode == Mode_Normal) {
        Rect mode_info_v = (Rect) {
            .x = text_v.x,
            .y = text_v.y + text_v.h - CODE_LINE_SPACING - BAR_SIZE,
            .w = text_v.w,
            .h = CODE_LINE_SPACING + BAR_SIZE,
        };

        *ui_push_glyph(ui) = (Glyph) {
            .x = mode_info_v.x,
            .y = mode_info_v.y,
            .glyph_idx = special_glyph_rect((U32)mode_info_v.w, (U32)mode_info_v.h),
            .colour = COLOUR_FILE_INFO,
        };

        if (ed->filepath) {
            U32 filepath_start = ed->filepath_length-1;
            U32 slash_count = 1;
            while (1) {
                if (filepath_start == 0) break;
                if (ed->filepath[filepath_start-1] == '/') {
                    if (slash_count == 0) break;
                    slash_count--;
                }
                filepath_start--;
            }

            F32 descent = font_atlas->descent[CODE_FONT_SIZE];

            if (ed->flags & EditorFlag_Unsaved) {
                ui->glyph_count += write_string_terminated(
                    &ui->glyphs[ui->glyph_count],
                    (const U8*)"[+]",
                    font_atlas,
                    (RGBA8) COLOUR_RED, CODE_FONT_SIZE,
                    mode_info_v.x, mode_info_v.y + CODE_LINE_SPACING + descent, mode_info_v.w
                );
            }

            ui->glyph_count += write_string(
                &ui->glyphs[ui->glyph_count],
                ed->filepath + filepath_start, ed->filepath_length - filepath_start,
                font_atlas,
                (RGBA8) COLOUR_FOREGROUND, CODE_FONT_SIZE,
                mode_info_v.x + 30.f, mode_info_v.y + CODE_LINE_SPACING + descent, mode_info_v.w
            );
        }
    }
}

// the returned rect will run from a, until b or the end of the line, whichever is shortest.
Rect editor_line_rect(Editor *ed, FontAtlas *font_atlas, I64 a, I64 b, Rect *text_v) { TRACE
    Range line = editor_group(ed, Group_Line, a);
    I64 line_i = editor_line_index(ed, a);

    // get x position of selection rect on this line
    F32 x, width;
    {
        I64 i = line.start;
        x = text_v->x;
        for (; i < a; ++i) {
            U8 ch = editor_text(ed, i);
            U32 glyph_idx = glyph_lookup_idx(CODE_FONT_SIZE, ch);
            GlyphInfo info = font_atlas->glyph_info[glyph_idx];
            x += info.advance_width;
        }

        width = 0;
        I64 end = line.end < b ? line.end : b;
        for (; i < end; ++i) {
            U8 ch = editor_text(ed, i);
            U32 glyph_idx = glyph_lookup_idx(CODE_FONT_SIZE, ch);
            GlyphInfo info = font_atlas->glyph_info[glyph_idx];
            width += info.advance_width;
        }
    }

    // get y position of selection rect on this line
    F32 y, height;
    {
        F32 scroll_diff = (F32)line_i - ed->scroll_y_visual;
        F32 descent = font_atlas->descent[CODE_FONT_SIZE];
        y = text_v->y + scroll_diff * CODE_LINE_SPACING + text_v->h / 2.f - descent - 1;
        height = CODE_LINE_SPACING + 1;
    }

    return (Rect) { x, y, width, height };
}

static const char *read_file_to_buffer_err(I64 f) {
    if (f == -1) return "File does not exist, or you have insufficient permissions";
    if (f == -2) return "Could not read file size";
    if (f == -3) return "File is too large";
    if (f == -4) return "Could not read file";
    if (f == -5) return "Could not close file";
    return "Error message not implemented";
}

static I64 read_file_to_buffer(U8 *buffer, U64 buffer_size, const U8 *filepath) {
    FILE *f = fopen((const char*)filepath, "rb");
    if (f == NULL) return -1;
    
    I64 fsize = file_size(f);
    if (fsize < 0) {
        fclose(f);
        return -2;
    }

    if ((U64)fsize > buffer_size) return -3;

    if (fsize != 0 && fread(buffer, (U64)fsize, 1, f) != 1) {
        fclose(f);
        return -4;
    }

    if (fclose(f) != 0) return -5;

    return fsize;
}

int editor_load_filepath(Editor *ed, const U8 *filepath) { TRACE
    if (filepath == NULL) return 0;

    // TODO: this leaks - allocates for each opened file
    // Change to reusable staticly sized buffer
    U32 filepath_length = (U32)strlen((const char*)filepath);
    U8 *arena_filepath = ARENA_ALLOC_ARRAY(ed->arena, U8, filepath_length+1);
    memcpy(arena_filepath, filepath, filepath_length+1);

    editor_clear_file(ed);

    I64 size = read_file_to_buffer(ed->text, TEXT_MAX_LENGTH, filepath);
    if (size >= 0) {
        ed->text_length = size;
        ed->filepath = arena_filepath;
        ed->filepath_length = filepath_length;
    } else {
        const char *err = read_file_to_buffer_err(size);
        fprintf(stderr, "Error reading file: %s\n", err);

        ed->text_length = 0;
        ed->filepath = NULL;
        ed->filepath_length = 0;
    }

    ed->selection_group = Group_Line;
    ed->selection_a = 0;
    ed->selection_b = editor_group(ed, Group_Line, 0).end;

    return 0;
}

void editor_clear_file(Editor *ed) { TRACE
    ed->filepath_length = 0;
    ed->filepath = NULL;
    ed->text_length = 0;
}

static inline U8 editor_text(Editor *ed, I64 byte) {
    if (byte < 0) return '\n';
    if (byte >= ed->text_length) return '\n';
    return ed->text[byte];
}

Range editor_group_range_paragraph(Editor *ed, I64 byte) { TRACE
    // not much we can do here
    if (byte < 0) byte = 0;
    if (byte >= ed->text_length) byte = ed->text_length;

    I64 start = byte;
    while (editor_text(ed, start-1) != '\n' || editor_text(ed, start-2) != '\n')
        start--;

    I64 end = byte;
    while (editor_text(ed, end) != '\n' || editor_text(ed, end-1) != '\n')
        end++;
    while (editor_text(ed, end) == '\n' && end < ed->text_length)
        end++;

    return (Range) { start, end };
}

Range editor_group_range_line(Editor *ed, I64 byte) { TRACE
    if (byte < 0 || byte > ed->text_length)
        return (Range) { byte, byte+1 };

    I64 start = byte;
    while (editor_text(ed, start-1) != '\n')
        start--;

    I64 end = byte;
    while (editor_text(ed, end++) != '\n') {};

    return (Range) { start, end };
}

//Range editor_group_range_word(Editor *ed, I64 byte) {
//    // not much we can do here
//    if (byte < 0) byte = 0;
//    if (byte >= ed->text_length) byte = ed->text_length-1;
//
//    I64 start = byte;
//    while (char_whitespace(editor_text(ed, start)))
//        start--;
//    while (!char_whitespace(editor_text(ed, start-1)))
//        start--;
//
//    I64 end = byte;
//    while (!char_whitespace(editor_text(ed, end)))
//        end++;
//    while (char_whitespace(editor_text(ed, end)))
//        end++;
//
//    return (Range) { start, end };
//}

Range editor_group_range_word(Editor *ed, I64 byte) { TRACE
    // not much we can do here
    if (byte < 0) byte = 0;
    if (byte >= ed->text_length) byte = ed->text_length-1;

    I64 start = byte;
    while (start > 0 && char_whitespace(editor_text(ed, start)))
        start--;

    bool (*char_fn)(U8 c);
    if (char_word_like(editor_text(ed, start))) {
        char_fn = char_word_like;
    } else {
        char_fn = char_symbolic;
    }

    while (start > 0 && char_fn(editor_text(ed, start-1)))
        start--;

    I64 end = byte;
    while (end < ed->text_length && char_fn(editor_text(ed, end)))
        end++;
    while (end < ed->text_length && char_whitespace(editor_text(ed, end)))
        end++;

    return (Range) { start, end };
}

Range editor_group_range_subword(Editor *ed, I64 byte) { TRACE
    // not much we can do here
    if (byte < 0) byte = 0;
    if (byte >= ed->text_length) byte = ed->text_length-1;

    I64 start = byte;
    while (start > 0 && (char_whitespace(editor_text(ed, start)) || editor_text(ed, start) == '_'))
        start--;

    bool (*char_fn)(U8 c);
    U8 ch = editor_text(ed, start);

    if (char_subword_like(ch)) {
        char_fn = char_subword_like;
    } else {
        char_fn = char_symbolic;
    }

    while (start > 0 && char_fn(editor_text(ed, start-1)))
        start--;

    I64 end = byte;
    while (end < ed->text_length && char_fn(editor_text(ed, end)))
        end++;
    while (end < ed->text_length && (char_whitespace(editor_text(ed, end)) || editor_text(ed, end) == '_'))
        end++;

    return (Range) { start, end };
}

Range editor_group(Editor *ed, Group group, I64 byte) { TRACE
    switch (group) {
        case Group_Paragraph:
            return editor_group_range_paragraph(ed, byte);
        case Group_Line:
            return editor_group_range_line(ed, byte);
        case Group_Word:
            return editor_group_range_word(ed, byte);
        case Group_SubWord:
            return editor_group_range_subword(ed, byte);
        case Group_Character:
            return (Range) { byte, byte+1 };
        default:
            expect(0);
            return (Range) {0};
    }
}

Range editor_group_next(Editor *ed, Group group, I64 current_group_end) { TRACE
    return editor_group(ed, group, current_group_end);
}

Range editor_group_prev(Editor *ed, Group group, I64 current_group_start) { TRACE
    return editor_group(ed, group, current_group_start-1);
}

I64 editor_line_index(Editor *ed, I64 byte) { TRACE
    if (byte < 0) return byte;
    I64 line_i = 0;
    for (I64 i = 0; i < byte; ++i) {
        if (editor_text(ed, i) == '\n')
            line_i++;
    }
    return line_i;
}

void editor_text_remove(Editor *ed, I64 start, I64 end) { TRACE
    if (start > end) {
        I64 temp = start;
        start = end;
        end = temp;
    }
    if (start < 0) start = 0;
    if (end < 0) end = 0;
    if (start > ed->text_length) start = ed->text_length;
    if (end > ed->text_length) end = ed->text_length;

    undo_record(&ed->undo_stack, start, &ed->text[start], end - start, UndoOp_Remove);
    editor_text_remove_raw(ed, start, end);
    if (start != end)
        ed->flags |= EditorFlag_Unsaved;
}

void editor_text_remove_raw(Editor *ed, I64 start, I64 end) { TRACE
    if (start <= ed->selection_a && ed->selection_a < end) {
        ed->selection_a = start;
    } else if (end <= ed->selection_a) {
        ed->selection_a -= end - start;
    }

    if (start <= ed->selection_b && ed->selection_b < end) {
        ed->selection_b = start;
    } else if (end <= ed->selection_b) {
        ed->selection_b -= end - start;
    }

    U64 to_move = (U64)(ed->text_length - end);
    ed->text_length -= end - start;
    memmove(&ed->text[start], &ed->text[end], to_move);

    // force newline termination cuz it makes math a lot simpler
    if (ed->text[ed->text_length-1] != '\n') {
        ed->text[ed->text_length++] = '\n';
    }
}

void editor_text_insert(Editor *ed, I64 at, U8 *text, I64 length) { TRACE
    expect(length >= 0);
    expect(ed->text_length + length <= (I64)TEXT_MAX_LENGTH);

    undo_record(&ed->undo_stack, at, text, length, UndoOp_Insert);
    editor_text_insert_raw(ed, at, text, length);
    if (length != 0)
        ed->flags |= EditorFlag_Unsaved;
}

void editor_text_insert_raw(Editor *ed, I64 at, U8 *text, I64 length) { TRACE
    if (length == 0) return;

    if (at <= ed->selection_a)
        ed->selection_a += length;
    if (at <= ed->selection_b)
        ed->selection_b += length;

    if (at < 0) {
        I64 created = -(at + length);
        if (created < 0) created = 0;
        ed->selection_a += created;
        ed->selection_b += created;
        ed->text_length += created;
        memmove(&ed->text[created+length], ed->text, (U64)ed->text_length);
        memset(&ed->text[length], '\n', (U64)created);
        at = 0;
    } else if (at > ed->text_length) {
        I64 created = at - ed->text_length;
        ed->text_length += created;
        memset(&ed->text[ed->text_length], '\n', (U64)(at - ed->text_length));
    } else {
        memmove(&ed->text[at+length], &ed->text[at], (U64)(ed->text_length - at));
    }

    ed->text_length += length;
    memcpy(&ed->text[at], text, (U64)length);

    // force newline termination cuz it makes math a lot simpler
    if (ed->text[ed->text_length-1] != '\n') {
        ed->text[ed->text_length++] = '\n';
    }
}

void editor_selection_trim(Editor *ed) { TRACE
    I64 a = ed->selection_a;
    I64 b = ed->selection_b;
    while (a < b && char_whitespace(editor_text(ed, a)))
        a++;
    while (a < b && char_whitespace(editor_text(ed, b-1)))
        b--;
    ed->selection_a = a;
    ed->selection_b = b;
}

// UNDO REDO ####################################################################

void editor_undo(Editor *ed) { TRACE
    UndoStack *st = &ed->undo_stack;
    if (st->undo_stack_head == 0) return;

    UndoElem elem = st->undo_stack[--st->undo_stack_head];
    st->text_stack_head -= elem.text_length;
    U8 *text = st->text_stack + st->text_stack_head;

    switch (elem.op) {
        case UndoOp_Insert:
            editor_text_remove_raw(ed, elem.at, elem.at + (I64)elem.text_length);
            ed->selection_a = elem.at;
            ed->selection_b = elem.at;
            break;
        case UndoOp_Remove:
            editor_text_insert_raw(ed, elem.at, text, (I64)elem.text_length);
            ed->selection_a = elem.at;
            ed->selection_b = elem.at + elem.text_length;
            break;
    }
}

void editor_redo(Editor *ed) { TRACE
    UndoStack *st = &ed->undo_stack;
    if (st->undo_stack_head == st->undo_count) return;

    UndoElem elem = st->undo_stack[st->undo_stack_head++];
    U8 *text = st->text_stack + st->text_stack_head;
    st->text_stack_head += elem.text_length;

    switch (elem.op) {
        case UndoOp_Insert:
            editor_text_insert_raw(ed, elem.at, text, (I64)elem.text_length);
            break;
        case UndoOp_Remove:
            editor_text_remove_raw(ed, elem.at, elem.at + (I64)elem.text_length);
            break;
    }
}

UndoElem *undo_record(UndoStack *st, I64 at, U8 *text, I64 text_length, UndoOp op) { TRACE
    if (text_length == 0) return NULL;

    expect(text_length <= UINT32_MAX);
    expect(st->undo_stack_head < UNDO_MAX && st->text_stack_head + text_length < (I64)UNDO_TEXT_SIZE);

    UndoElem *new_elem = &st->undo_stack[st->undo_stack_head++];
    *new_elem = (UndoElem) { at, (U32)text_length, op };
    memcpy(st->text_stack + st->text_stack_head, text, (U32)text_length);
    st->text_stack_head += (U32)text_length;
    st->undo_count = st->undo_stack_head;
    return new_elem;
}

UndoStack undo_create(Arena *arena) { TRACE
    U8 *text_stack = arena_alloc(arena, UNDO_TEXT_SIZE, page_size());
    UndoElem *undo_stack = arena_alloc(arena, UNDO_STACK_SIZE, page_size());

    return (UndoStack) {
        .text_stack = text_stack,
        .undo_stack = undo_stack,
    };
}

void undo_clear(UndoStack *st) { TRACE
    st->text_stack_head = 0;
    st->undo_stack_head = 0;
    st->undo_count = 0;
}

// returns number of digits
static U64 int_to_string(Arena *arena, I64 n) {
    if (n < 0)
        *(U8*)ARENA_ALLOC(arena, U8) = '-';
    U64 n_abs = (U64)(n >= 0 ? n : -n);

    // count digits
    U64 digit_count = 0;
    U64 m = n_abs;
    do {
        m /= 10;
        digit_count++;
    } while (m != 0);

    U8 *digits = ARENA_ALLOC_ARRAY(arena, *digits, digit_count);

    for (U64 i = 0; i < digit_count; ++i) {
        U64 digit = n_abs % 10;
        n_abs /= 10;
        digits[digit_count - i - 1] = (U8)digit + '0';
    }

    return digit_count;
}
