typedef struct Indices {
    U64 count;
    I64 *ptr;
} Indices;

typedef struct Insertion {
    U64 text_len;
    U8 *text;
} Insertion;

UndoStack   undo_create(Arena *arena);
void        undo_clear(UndoStack *st);
UndoElem   *undo_record(UndoStack *st, I64 at, U8 *text, I64 text_length, UndoOp op);

void        editor_on_focus(Panel *ed_panel);
void        editor_on_focus_lost(Panel *ed_panel);
void        editor_clear_file(Editor *ed);
Range       editor_group(Editor *ed, Group group, I64 byte);
Range       editor_group_next(Editor *ed, Group group, I64 current_group_end);
Range       editor_group_prev(Editor *ed, Group group, I64 current_group_start);
I64         editor_line_index(Editor *ed, I64 byte);
I64         editor_byte_index(Editor *ed, I64 line);
Rect        editor_line_rect(Editor *ed, FontAtlas *font_atlas, I64 a, I64 b, Rect *text_v);
void        editor_selection_trim(Editor *ed);
Range       editor_range_trim(Editor *ed, Range range);
Indices     editor_find_lines(Editor *ed, Arena *arena, I64 start, I64 end);
void        editor_comment_lines(Editor *ed, Indices lines);
void        editor_uncomment_lines(Editor *ed, Indices lines);
void        editor_set_selection(Editor *ed, I64 base, I64 head);

void        editor_undo(Editor *ed);
void        editor_redo(Editor *ed);
static inline U8 editor_text(Editor *ed, I64 byte);
void        editor_open_filetree(Panel *ed_panel, bool expand);
void        editor_open_jumplist(Panel *ed_panel);
void        editor_jumplist_add(Panel *ed_panel, JumpPoint point);
void        editor_text_remove(Editor *ed, I64 start, I64 end);
void        editor_text_insert(Editor *ed, I64 at, U8 *text, I64 length);
// void        editor_text_remove_bulk(Editor *ed, Range *ranges, U64 remove_count);
// void        editor_text_insert_bulk(Editor *ed, Insertion *insertions, U64 insert_count);
// same as above, but does not add to the undo stack
void        editor_text_remove_raw(Editor *ed, I64 start, I64 end);
void        editor_text_insert_raw(Editor *ed, I64 at, U8 *text, I64 length);

void        editor_remake_caches(Editor *ed);

static U64 int_to_string(Arena *arena, I64 n);

// EDITOR ####################################################################

#define SYNTAX_COMMENT_SLASHES      { {'/', '/'}, {'\n'}, 0, COLOUR_COMMENT }
#define SYNTAX_COMMENT_SLASH_STAR   { {'/', '*'}, {'*', '/'}, 0, COLOUR_COMMENT }
#define SYNTAX_COMMENT_HASHTAG      { {'#'}, {'\n'}, 0, COLOUR_COMMENT }  
#define SYNTAX_STRING_DOUBLE_QUOTES { {'"'}, {'"'}, '\\', COLOUR_STRING }
#define SYNTAX_STRING_SINGLE_QUOTES { {'\''}, {'\''}, '\\', COLOUR_STRING }

static SyntaxGroup syntax_c[] = {
    SYNTAX_COMMENT_SLASHES,
    SYNTAX_COMMENT_SLASH_STAR,
    SYNTAX_STRING_DOUBLE_QUOTES,
    SYNTAX_STRING_SINGLE_QUOTES,
};

static SyntaxGroup syntax_rs[] = {
    SYNTAX_COMMENT_SLASHES,
    SYNTAX_COMMENT_SLASH_STAR,
    SYNTAX_STRING_DOUBLE_QUOTES,
};

static SyntaxGroup syntax_odin[] = {
    SYNTAX_COMMENT_SLASHES,
    SYNTAX_COMMENT_SLASH_STAR,
    SYNTAX_STRING_DOUBLE_QUOTES,
    SYNTAX_STRING_SINGLE_QUOTES,
};

static SyntaxGroup syntax_sh[] = {
    SYNTAX_COMMENT_HASHTAG,
    SYNTAX_STRING_DOUBLE_QUOTES,
    SYNTAX_STRING_SINGLE_QUOTES,
};

static SyntaxGroup syntax_py[] = {
    SYNTAX_COMMENT_HASHTAG,
    SYNTAX_STRING_DOUBLE_QUOTES,
    SYNTAX_STRING_SINGLE_QUOTES,
};

static SyntaxGroup syntax_asm[] = {
    SYNTAX_COMMENT_HASHTAG,
    SYNTAX_STRING_DOUBLE_QUOTES,
    SYNTAX_STRING_SINGLE_QUOTES,
};

typedef struct SyntaxLookup {
    char extension[8];
    SyntaxHighlighting syntax;
} SyntaxLookup;

#define Highlighting(A) { countof(A), A, {0, 0} }
static SyntaxLookup syntax_lookup[] = {
    {"c",    Highlighting(syntax_c)},
    {"h",    Highlighting(syntax_c)},
    {"cpp",  Highlighting(syntax_c)},
    {"hpp",  Highlighting(syntax_c)},
    {"rs",   Highlighting(syntax_rs)},
    {"odin", Highlighting(syntax_odin)},
    {"sh",   Highlighting(syntax_sh)},
    {"py",   Highlighting(syntax_py)},
    {"glsl", Highlighting(syntax_c)},
    {"hlsl", Highlighting(syntax_c)},
    {"s",    Highlighting(syntax_asm)},
    {"asm",  Highlighting(syntax_asm)},
};

SyntaxHighlighting *syntax_for_path(const U8 *filepath, U32 filepath_len) {
    // find extension
    U32 ex_i = 0;
    {
        bool ex_found = false;
        for (U32 i = 0; i < filepath_len; ++i) {
            if (filepath[i] == '.') {
                ex_i = i;
                ex_found = true;
            }
        }
        if (ex_found == false) return NULL;
    }
    
    // copy extension to buffer
    U8 ex[8] = {0};
    {
        U32 ex_len = filepath_len - ex_i - 1;
        if (ex_len > sizeof(ex))
            return NULL;
            
        for (U32 i = 0; i < ex_len; ++i)
            ex[i] = filepath[ex_i + i + 1];
    }
    
    SyntaxHighlighting *syn = NULL;
    for (U64 i = 0; i < countof(syntax_lookup); ++i) {
        SyntaxLookup *lookup = &syntax_lookup[i];
        if (memcmp(ex, lookup->extension, sizeof(ex)) == 0) {
            syn = &lookup->syntax;
            break;
        }
    }
    
    // fill out `char_is_syntax_start` lookup bitfields
    if (syn) {
        for (U64 i = 0; i < syn->group_count; ++i) {
            U64 c = syn->groups[i].start_chars[0];
            
            U64 bit = 1ull << (c & 63);
            U64 arr = c >> 6ull;
            syn->char_is_syntax_start[arr] |= bit;
        }
    }
    
    return syn;
}

Panel *editor_create(UI *ui, const U8 *filepath) { TRACE
    Panel *panel = panel_create(ui);
    panel->update_fn = editor_update;
    panel->focus_fn = editor_on_focus;
    panel->focus_lost_fn = editor_on_focus_lost;
    Arena *arena = panel_arena(panel);

    Editor *ed = arena_alloc(arena, sizeof(Editor), alignof(Editor));
    *ed = (Editor) {
        .undo_stack = undo_create(arena),
        .selection_group = Group_Line,
        .mode_text = arena_alloc(arena, MODE_TEXT_MAX_LENGTH, 16),
        .mode_text_alt = arena_alloc(arena, MODE_TEXT_MAX_LENGTH, 16),
        .text = arena_alloc(arena, TEXT_MAX_LENGTH, page_size()),
        .search_matches = arena_alloc(arena, SEARCH_MAX_LENGTH, page_size()),
        .line_lookup = arena_alloc(arena, MAX_LINE_LOOKUP_SIZE, page_size()),
        .syntax_lookup = arena_alloc(arena, MAX_SYNTAX_LOOKUP_SIZE, page_size()),
    };
    ed->arena = arena;
    panel->data = ed;
    panel->name = "editor";
    
    if (filepath)
        expect(editor_load_filepath(ed, filepath, my_strlen(filepath)) == 0);

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

void editor_on_focus(Panel *ed_panel) {
    ed_panel->dynamic_weight_w = EDITOR_FOCUSED_PANEL_WEIGHT;
}

void editor_on_focus_lost(Panel *ed_panel) {
    ed_panel->dynamic_weight_w = 1.f;
}

void editor_update(Panel *panel) { TRACE
    Editor *ed = panel->data;
    Rect *viewport = &panel->viewport;
    UI *ui = panel->ui;
    FontAtlas *font_atlas = ui->atlas;
    
    // UPDATE ---------------------------------------------------------------
    
    // state switch
    if (panel->flags & PanelFlag_Focused) {
        U64 special_pressed = w->inputs.key_special_pressed;
        U64 special_repeating = w->inputs.key_special_repeating;
        U64 pressed = w->inputs.key_pressed;
        U64 held = w->inputs.key_held;
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
                
            if (!ctrl && !shift && is(pressed, key_mask(GLFW_KEY_O))) {
                // TODO run command
            }

            if (!ctrl && !shift && is(pressed, key_mask(GLFW_KEY_H)))
                editor_group_expand(ed);

            if (!ctrl && !shift && is(pressed, key_mask(GLFW_KEY_L)))
                editor_group_contract(ed);

            if (ctrl && is(pressed, key_mask(GLFW_KEY_S))) {
                if (ed->filepath && (ed->flags & EditorFlag_Unsaved) != 0) {
                    expect(ed->text_length >= 0);
                    expect(write_file((char*)ed->filepath, ed->text, (U64)ed->text_length) == 0);
                    ed->flags &= ~(U32)EditorFlag_Unsaved;
                }
            }

            if (is(pressed | repeating, key_mask(GLFW_KEY_J))) {
                if (!ctrl && !shift) {
                    Range next_group = editor_group_next(ed, ed->selection_group, ed->selection_head);
                    editor_set_selection(ed, next_group.start, next_group.end);
                } else if (!ctrl && shift) {
                    Range next_group = editor_group_next(ed, ed->selection_group, ed->selection_head);
                    editor_set_selection(ed, ed->selection_base, next_group.end);
                }
            }

            if (is(pressed | repeating, key_mask(GLFW_KEY_K))) {
                if (!ctrl && !shift) {
                    Range prev_group = editor_group_prev(ed, ed->selection_group, ed->selection_a);
                    editor_set_selection(ed, prev_group.start, prev_group.end);
                } else if (!ctrl && shift) {
                    Range prev_group = editor_group_prev(ed, ed->selection_group, ed->selection_head);
                    editor_set_selection(ed, ed->selection_base, prev_group.start);
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

            if (!ctrl && is(pressed, key_mask(GLFW_KEY_M))) {
                editor_selection_trim(ed);
            }

            if (!ctrl && is(pressed, key_mask(GLFW_KEY_W))) {
                editor_selection_trim(ed);
                if (shift)
                    editor_group_expand(ed);
                else
                    editor_group_contract(ed);
                Range range = editor_group(ed, ed->selection_group, ed->selection_a);
                editor_set_selection(ed, range.start, range.end);
            }

            if (is(pressed, key_mask(GLFW_KEY_E))) {
                editor_selection_trim(ed);
                if (shift)
                    editor_group_expand(ed);
                else
                    editor_group_contract(ed);
                Range range = editor_group(ed, ed->selection_group, ed->selection_b-1);
                editor_set_selection(ed, range.start, range.end);
            }

            if (!ctrl && is(pressed | repeating, key_mask(GLFW_KEY_D))) {
                editor_text_remove(ed, ed->selection_a, ed->selection_b);
                Range range = editor_group(ed, ed->selection_group, ed->selection_a);
                editor_set_selection(ed, range.start, range.end);
            }

            if (is(pressed, key_mask(GLFW_KEY_Y))) {
                I64 copy_start = clamp(ed->selection_a, 0, ed->text_length);
                I64 copy_end = clamp(ed->selection_b, 0, ed->text_length);
                I64 copy_length_signed = copy_end - copy_start;
                if (copy_length_signed > 0) {
                    U32 copy_length = (U32)copy_length_signed;
                    char *copied = arena_alloc(&w->frame_arena, copy_length+1, 1);
                    memcpy(copied, &ed->text[copy_start], copy_length);
                    copied[copy_length] = 0;
                    glfwSetClipboardString(NULL, copied);
                }
            }

            if (is(pressed, key_mask(GLFW_KEY_F))) {
                if (!ctrl)
                    editor_set_selection(ed, ed->selection_a, ed->text_length);
                if (!shift)
                    editor_set_selection(ed, 0, ed->selection_b);
            }

            if (!ctrl && !shift && is(pressed, key_mask(GLFW_KEY_R))) {
                editor_selection_trim(ed);
                ed->selection_group = Group_SubWord;
                Range range = editor_group(ed, ed->selection_group, ed->selection_a);
                editor_set_selection(ed, range.start, range.end);
            }

            if (is(pressed, key_mask(GLFW_KEY_SLASH))) {
                ed->mode = Mode_Search;
                ed->mode_text_length = 0;
                ed->search_match_count = 0;
                ed->search_cursor = 0;
                
                if (!shift) {
                    ed->search_a = 0;
                    ed->search_b = ed->text_length;
                } else {
                    ed->search_a = ed->selection_a;
                    ed->search_b = ed->selection_b;
                }
            }

            if (!ctrl && is(pressed, key_mask(GLFW_KEY_P))) {
                I64 paste_idx = shift ? ed->selection_a : ed->selection_b;
                const U8 *text = (const U8*) glfwGetClipboardString(NULL);
                if (text != NULL) {
                    U8 *text_owned = copy_cstr(&w->frame_arena, text);
                    U32 len = my_strlen(text);
                    editor_text_insert(ed, paste_idx, text_owned, len);
                    editor_set_selection(ed, paste_idx, paste_idx + len);
                }
            }

            if (is(pressed, key_mask(GLFW_KEY_T)))
                editor_open_filetree(panel, !shift);
                
            if (!ctrl && is(pressed, key_mask(GLFW_KEY_B))) {
                if (!shift) {
                    editor_open_jumplist(panel);
                } else {
                    Range para = editor_group(ed, Group_Paragraph, ed->selection_a);
                    if (para.start < 0) para.start = 0;
                    if (para.end > ed->text_length) para.end = ed->text_length;
                    if (para.end < para.start) {
                        I64 end = para.end;
                        para.end = para.start;
                        para.start = end;
                    }
                    
                    while (para.start < para.end && char_whitespace(editor_text(ed, para.end-1)))
                        para.end--;
                     
                    JumpPoint current_point = {
                        .filepath = ed->filepath,
                        .filepath_len = ed->filepath_length,
                        .text = &ed->text[para.start],
                        .text_len = (U32)(para.end - para.start),
                        .line_idx = editor_line_index(ed, ed->selection_a), 
                    };
                    editor_jumplist_add(panel, current_point);
                }
            }

            //if (is(pressed, key_mask(GLFW_KEY_K))) ed->scroll_y -= 1.f;
            //else if (is(repeating, key_mask(GLFW_KEY_K))) ed->scroll_y -= CODE_SCROLL_SPEED_SLOW;

            if (ctrl && !shift && is(pressed, key_mask(GLFW_KEY_J)))
                ed->mode = Mode_QuickMove;
            if (ctrl && !shift && is(pressed, key_mask(GLFW_KEY_K)))
                ed->mode = Mode_QuickMove;

            if (!ctrl && is(pressed, key_mask(GLFW_KEY_Q))) {
                if ((ed->flags & EditorFlag_Unsaved) == 0 || shift)
                    panel_destroy_queued(panel);
            }
            
            // '<' - dedent selected lines 
            if (!ctrl && shift && is(pressed, key_mask(GLFW_KEY_COMMA))) {
                Indices lines = editor_find_lines(ed, &w->frame_arena, ed->selection_a, ed->selection_b);
                for (U64 i = lines.count; i > 0; --i) {
                    I64 line_start = lines.ptr[i-1];
                    I64 text_start = line_start;
                    while (editor_text(ed, text_start) == ' ')
                        text_start++;
                        
                    if (text_start != line_start) {
                        I64 spaces_to_rm = 1;
                        while (((text_start - line_start - spaces_to_rm) & 3) != 0)
                            spaces_to_rm++;
                        editor_text_remove(ed, line_start, line_start+spaces_to_rm);
                    }
                }
            }
            
            // '>' - indent selected lines 
            if (!ctrl && shift && is(pressed, key_mask(GLFW_KEY_PERIOD))) {
                U8 *spaces = arena_alloc(&w->frame_arena, 4, 1);
                memset(spaces, ' ', 4);
                
                Indices lines = editor_find_lines(ed, &w->frame_arena, ed->selection_a, ed->selection_b);
                for (U64 i = lines.count; i > 0; --i) {
                    I64 line_start = lines.ptr[i-1];
                    I64 text_start = line_start;
                    while (editor_text(ed, text_start) == ' ')
                        text_start++;
                        
                    I64 spaces_to_add = 1;
                    while (((text_start - line_start + spaces_to_add) & 3) != 0)
                        spaces_to_add++;
                    expect(spaces_to_add <= 4);
                    editor_text_insert(ed, line_start, spaces, spaces_to_add);
                }
            }
            
            // 'v' - comment lines
            if (!ctrl && !shift && is(pressed, key_mask(GLFW_KEY_V))) {
                Indices lines = editor_find_lines(ed, &w->frame_arena, ed->selection_a, ed->selection_b);
                editor_comment_lines(ed, lines);
            }
            
            // 'V' - uncomment lines
            if (!ctrl && shift && is(pressed, key_mask(GLFW_KEY_V))) {
                Indices lines = editor_find_lines(ed, &w->frame_arena, ed->selection_a, ed->selection_b);
                editor_uncomment_lines(ed, lines);
            }
            
            break;
        }
        case Mode_Insert: {
            bool esc = is(special_pressed, special_mask(GLFW_KEY_ESCAPE));
            bool caps = is(special_pressed, special_mask(GLFW_KEY_CAPS_LOCK));
            if (esc || caps) {
                ed->mode = Mode_Normal;
                ed->selection_group = Group_Line;
                Range range = editor_group(ed, ed->selection_group, ed->insert_cursor);
                editor_set_selection(ed, range.start, range.end);
            }

            for (I64 i = 0; i < w->inputs.char_event_count; ++i) {
                U32 codepoint = w->inputs.char_events[i].codepoint;
                // enforce ascii for now
                expect(codepoint < 128);

                U8 codepoint_as_char = (U8)codepoint;
                editor_text_insert(ed, ed->insert_cursor, &codepoint_as_char, 1);
                ed->insert_cursor += 1;
            }

            bool up = is(special_pressed | special_repeating, special_mask(GLFW_KEY_UP));
            bool down = is(special_pressed | special_repeating, special_mask(GLFW_KEY_DOWN));
            if (up || down) {
                Range cur_line = editor_group(ed, Group_Line, ed->insert_cursor);
                I64 idx_in_line = ed->insert_cursor - cur_line.start;

                Range target_line = cur_line;
                if (up) target_line = editor_group(ed, Group_Line, cur_line.start-1);
                if (down) target_line = editor_group(ed, Group_Line, cur_line.end);

                ed->insert_cursor = target_line.start + idx_in_line;
                if (ed->insert_cursor > target_line.end)
                    ed->insert_cursor = target_line.end-1;
            }

            if (is(special_pressed | special_repeating, special_mask(GLFW_KEY_LEFT)))
                ed->insert_cursor--;
            if (is(special_pressed | special_repeating, special_mask(GLFW_KEY_RIGHT)))
                ed->insert_cursor++;

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
                I64 idx = ed->insert_cursor - line.start;
                while ((idx+spaces) % 4 != 0)
                    spaces++;

                U8 *text = ARENA_ALLOC_ARRAY(&w->frame_arena, U8, (U64)spaces);
                for (I64 i = 0; i < spaces; ++i)
                    text[i] = ' ';

                editor_text_insert(ed, ed->insert_cursor, text, spaces);
                ed->insert_cursor += spaces;
            }

            if (is(special_pressed | special_repeating, special_mask(GLFW_KEY_BACKSPACE))) {
                if (ctrl) {
                    Range word = editor_group(ed, Group_SubWord, ed->insert_cursor);
                    editor_text_remove(ed, word.start, ed->insert_cursor);
                    ed->insert_cursor = word.start;
                } else {
                    ed->insert_cursor -= 1;
                    editor_text_remove(ed, ed->insert_cursor, ed->insert_cursor+1);
                }
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
            
            if (ctrl && is(pressed, key_mask(GLFW_KEY_R))) {
                ed->mode = Mode_Replace;
                ed->mode_text_alt_length = 0;
            }

            ed->search_match_count = 0;
            if (ed->mode_text_length > 0) {
                I64 start = ed->search_a;
                I64 end = ed->search_b - ed->mode_text_length;
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

                    if (matches)
                        ed->search_matches[ed->search_match_count++] = a;
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
                ed->selection_group = Group_Line;
                ed->mode = Mode_Normal;
            }

            if (is(special_pressed, special_mask(GLFW_KEY_ENTER))) { 
                if (ed->search_match_count > 0) {
                    I64 shown_match = ed->search_matches[ed->search_cursor];
                    editor_set_selection(ed, shown_match, shown_match + ed->mode_text_length);
                }
                ed->mode = Mode_Normal;
            }

            break;
        }
        case Mode_Replace: {
            for (I64 i = 0; i < w->inputs.char_event_count; ++i) {
                ed->search_cursor = 0;

                U32 codepoint = w->inputs.char_events[i].codepoint;
                // enforce ascii for now
                expect(codepoint < 128);

                U8 codepoint_as_char = (U8)codepoint;
                ed->mode_text_alt[ed->mode_text_alt_length++] = codepoint_as_char;
            }
            
            if (is(special_pressed | special_repeating, special_mask(GLFW_KEY_BACKSPACE))) {
                if (ed->mode_text_alt_length > 0)
                    ed->mode_text_alt_length--;
            }
            
            bool esc = is(special_pressed, special_mask(GLFW_KEY_ESCAPE));
            bool caps = is(special_pressed, special_mask(GLFW_KEY_CAPS_LOCK));
            if (esc || caps)
                ed->mode = Mode_Search;
            
            if (is(special_pressed, special_mask(GLFW_KEY_ENTER))) {
                I64 i = ed->search_match_count;
                while (i != 0) {
                    i--;
                    I64 match_start = ed->search_matches[i];
                    I64 match_end = match_start + ed->mode_text_length;
                    editor_text_remove(ed, match_start, match_end);
                    editor_text_insert(ed, match_start, ed->mode_text_alt, ed->mode_text_alt_length);
                }
                
                ed->selection_group = Group_Line;
                ed->mode = Mode_Normal;
            }
            
            break;
        }

        case Mode_QuickMove: {
            float speed = shift ? CODE_SCROLL_SPEED_SLOW : CODE_SCROLL_SPEED_FAST;
            
            if (ctrl && is(held, key_mask(GLFW_KEY_J)))
                ed->scroll_y += speed / (F64)w->refresh_rate;
            if (ctrl && is(held, key_mask(GLFW_KEY_K)))
                ed->scroll_y -= speed / (F64)w->refresh_rate;
                 
            bool esc = is(special_pressed, special_mask(GLFW_KEY_ESCAPE));
            bool caps = is(special_pressed, special_mask(GLFW_KEY_CAPS_LOCK));
            if (esc || caps) {
                ed->mode = Mode_Normal;
            }
            
            if (is(special_pressed, special_mask(GLFW_KEY_ENTER))) {
                I64 line = (I64)round(ed->scroll_y);
                I64 byte = editor_byte_index(ed, line);
                Range range = editor_group(ed, Group_Line, byte);
                ed->selection_base = range.start;
                ed->selection_head = range.end;
                ed->mode = Mode_Normal;
            }
             
            break;
        }
        }
    }
    
    // UPDATE ANIMATIONS ----------------------------------------------------

    // find new scroll y
    {
        if (ed->mode == Mode_Search || ed->mode == Mode_Replace) {
            if (ed->search_match_count > 0) {
                I64 shown_match = ed->search_matches[ed->search_cursor];
                I64 line_a = editor_line_index(ed, shown_match);
                I64 line_b = editor_line_index(ed, shown_match + ed->mode_text_length);
                ed->scroll_y = ((F64)line_a + (F64)line_b) / 2.f;
            }
        } else if (ed->mode == Mode_QuickMove) {
            // do nothing, scroll y preserved across update
        } else {
            ed->scroll_y = (F64)editor_line_index(ed, ed->selection_head);
        }
    }
    
    // animate scrolling
    bool animation_playing = false;
    { 
        // animate scrolling
        if (ed->mode != Mode_QuickMove) {
            F64 diff = ed->scroll_y - ed->scroll_y_visual;
            if (fabs(diff) < 0.01f) { 
                ed->scroll_y_visual = ed->scroll_y;
            } else {
                ed->scroll_y_visual += diff * w->exp_factor;
                animation_playing = true;
            }
        } else {
            ed->scroll_y_visual = ed->scroll_y;
            
            // We need to update every frame or the scroll will only occur every default update delay,
            // which is very very slow.
            animation_playing = true;
        }
    }
    
    if (animation_playing) {
        w->force_update = true;
    }

    // START RENDER ----------------------------------------------------------

    Rect selection_bar_v = *viewport;
    selection_bar_v.w = BAR_SIZE;

    Rect text_v = selection_bar_v;
    text_v.x += selection_bar_v.w;
    text_v.w = viewport->x + viewport->w - text_v.x;

    // WRITE SPECIAL GLYPHS -------------------------------------------------
    
    F32 font_height = font_height_px[CODE_FONT_SIZE];

    // determine visible lines
    I64 byte_visible_start;
    I64 byte_visible_end;
    {
        F64 line_i = ed->scroll_y_visual;
        I64 a = editor_byte_index(ed, (I64)line_i);
        byte_visible_start = a;
        byte_visible_end = a;
        
        while (1) {
            F64 height_up = (ed->scroll_y_visual - line_i) * font_height;
            if (height_up + font_height > text_v.h / 2.f) break;
            
            Range line = editor_group(ed, Group_Line, byte_visible_start-1);
            byte_visible_start = line.start;
            line_i -= 1.f;
        }
        
        line_i = ed->scroll_y_visual;
        while (1) {
            F64 height_down = (line_i - ed->scroll_y_visual) * font_height;
            if (height_down > text_v.h / 2.f) break;

            Range line = editor_group(ed, Group_Line, byte_visible_end);
            byte_visible_end = line.end;
            line_i += 1.f;
        }
        
        if (byte_visible_start < 0)
            byte_visible_start = 0;
        if (byte_visible_end > ed->text_length)
            byte_visible_end = ed->text_length;
    }

    // mode/selection group colour bar
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
        } else if (ed->mode == Mode_QuickMove) {
            selection_bar_colour = (RGBA8) { 100, 255, 100, 255 };
        } else if (ed->mode == Mode_Replace) {
            selection_bar_colour = (RGBA8) { 255, 100, 100, 255 };
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
        F64 line_i = (F64)editor_line_index(ed, a);
        
        while (1) {
            F32 line_offset_from_scroll = (F32)(line_i - ed->scroll_y_visual);
            F32 line_y = line_offset_from_scroll * font_height + text_v.h / 2.f;
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
    if (ed->mode == Mode_Search || ed->mode == Mode_Replace) {
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
    
    F64 line_start = (F64)editor_line_index(ed, byte_visible_start);
    F32 line_y = (F32)(line_start - ed->scroll_y_visual) * font_height + text_v.h / 2.f;
    F32 pen_x = 0.f;
    U32 syntax_range_idx = 0;
    
    for (I64 i = byte_visible_start; i < byte_visible_end; ++i) {
        U8 ch = ed->text[i];
        if (ch == '\n') {
            line_y += font_height;
            pen_x = 0.f;
            continue;
        }
        
        RGBA8 text_colour = (RGBA8)COLOUR_FOREGROUND;
        while (syntax_range_idx != ed->syntax_range_count) {
            SyntaxRange *range = &ed->syntax_lookup[syntax_range_idx];
            if (range->end < i) {
                ++syntax_range_idx;
                continue;
            }
            
            if (range->start <= i)
                text_colour = range->colour;
            break;
        }

        F32 pen_y = line_y + font_height;

        U32 glyph_idx = glyph_lookup_idx(CODE_FONT_SIZE, ch);
        GlyphInfo info = font_atlas->glyph_info[glyph_idx];

        if (pen_x + info.advance_width <= text_v.w) {
            *ui_push_glyph(ui) = (Glyph) {
                .x = text_v.x + pen_x + info.offset_x,
                .y = text_v.y + pen_y + info.offset_y,
                .glyph_idx = glyph_idx,
                .colour = text_colour,
            };
        }
        pen_x += info.advance_width;
    }

    // WRITE MODE INFO GLYPHS -------------------------------------------------

    if (ed->mode == Mode_Search || ed->mode == Mode_Replace) {
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

        F32 y = mode_info_v.y;
        F32 x = mode_info_v.x + MODE_INFO_PADDING;
        ui_push_string_terminated(
            ui,
            mode_info_text,
            font_atlas,
            colour, MODE_FONT_SIZE,
            x, y, mode_info_v.x + mode_info_v.w
        );
        
        // also draw replacement text for replace mode
        if (ed->mode == Mode_Replace) {
            mode_info_v.y += mode_info_v.h + 1;
            
            *ui_push_glyph(ui) = (Glyph) {
                .x = mode_info_v.x,
                .y = mode_info_v.y,
                .glyph_idx = special_glyph_rect((U32)mode_info_v.w, (U32)mode_info_v.h),
                .colour = COLOUR_MODE_INFO,
            };
            
            U32 space_idx = glyph_lookup_idx(MODE_FONT_SIZE, ' ');
            F32 space_size = font_atlas->glyph_info[space_idx].advance_width;
            y = mode_info_v.y;
            x = mode_info_v.x + MODE_INFO_PADDING + space_size * 6.f;
            ui_push_string(
                ui,
                ed->mode_text_alt, (U64)ed->mode_text_alt_length,
                font_atlas,
                colour, MODE_FONT_SIZE,
                x, y, mode_info_v.x + mode_info_v.w
            );
        }
    }

    // draw status    
    {
        Rect mode_info_v = (Rect) {
            .x = text_v.x,
            .y = text_v.y + text_v.h - font_height - BAR_SIZE,
            .w = text_v.w,
            .h = font_height + BAR_SIZE,
        };
    
        *ui_push_glyph(ui) = (Glyph) {
            .x = mode_info_v.x,
            .y = mode_info_v.y,
            .glyph_idx = special_glyph_rect((U32)mode_info_v.w, (U32)mode_info_v.h),
            .colour = COLOUR_FILE_INFO,
        };
    
        F32 status_x = mode_info_v.x;
        F32 status_y = mode_info_v.y;
        F32 status_max_x = status_x + mode_info_v.w;
        
        // find line number
        I64 line_i = 0;
        switch (ed->mode) {
            case Mode_Normal: {
                line_i = editor_line_index(ed, ed->selection_b);
            } break;
            case Mode_Insert: {
                line_i = editor_line_index(ed, ed->insert_cursor);
            } break;
            case Mode_Replace:
            case Mode_Search: {
                if (ed->search_match_count) {
                    I64 search_byte = ed->search_matches[ed->search_cursor];  
                    line_i = editor_line_index(ed, search_byte);
                } else {
                    line_i = editor_line_index(ed, ed->selection_b);
                }
            } break;
            case Mode_QuickMove: {
                line_i = (I64)round(ed->scroll_y);
            } break;
        }
        
        // draw line number
        U8 *line_str = w->frame_arena.head;
        U64 line_str_len = int_to_string(&w->frame_arena, line_i);
        status_x += ui_push_string(
            ui,
            line_str, line_str_len,
            font_atlas,
            (RGBA8) COLOUR_FOREGROUND, CODE_FONT_SIZE,
            status_x, status_y, status_max_x
        );
        status_x += 10.f; 

        // unsaved [+] symbol 
        if (ed->flags & EditorFlag_Unsaved) {
            status_x += ui_push_string_terminated(
                ui,
                (const U8*)"[+]",
                font_atlas,
                (RGBA8) COLOUR_RED, CODE_FONT_SIZE,
                status_x, status_y, status_max_x
            );
            status_x += 10.f;
        } 
        
        // filename 
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
    
            status_x += ui_push_string(
                ui,
                ed->filepath + filepath_start, ed->filepath_length - filepath_start,
                font_atlas,
                (RGBA8) COLOUR_FOREGROUND, CODE_FONT_SIZE,
                status_x, status_y, status_max_x
            );
        }
    }
}

void editor_open_filetree(Panel *ed_panel, bool expand) {
    Panel *filetree_panel = filetree_create(ed_panel, NULL);
    panel_insert_before_queued(ed_panel, filetree_panel);
    panel_focus_queued(filetree_panel);
    if (expand) {
        FileTree *ft = filetree_panel->data;
        filetree_dir_open_all(ft, &w->frame_arena, ft->dir_tree);
    }
}

void editor_open_jumplist(Panel *ed_panel) {
    Panel *jl_panel = ui_find_panel(ed_panel->ui, "jumplist");
    if (jl_panel) {
        JumpList *jl = jl_panel->data;
        jl->ed_handle = panel_handle(ed_panel);
        if (jl->point_count)
            panel_focus_queued(jl_panel);
    }
}

void editor_jumplist_add(Panel *ed_panel, JumpPoint point) {
    Panel *jl_panel = ui_find_panel(ed_panel->ui, "jumplist");
    if (jl_panel)
        jumppoint_add(jl_panel, point);
}

// the returned rect will run from a, until b, the end of the line,
// or the end of the viewport, whichever is shortest.
Rect editor_line_rect(Editor *ed, FontAtlas *font_atlas, I64 a, I64 b, Rect *text_v) { TRACE
    Range line = editor_group(ed, Group_Line, a);
    I64 line_i = editor_line_index(ed, a);
    F32 max_x = text_v->x + text_v->w;

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
            
            if (x + width >= max_x) {
                width = max_x - x;
                break;
            }
        }
    }

    // get y position of selection rect on this line
    F32 y, height;
    {
        F32 scroll_diff = (F32)((F64)line_i - ed->scroll_y_visual);
        F32 font_height = font_height_px[CODE_FONT_SIZE];
        F32 descent = font_atlas->descent[CODE_FONT_SIZE];
        y = text_v->y + scroll_diff * font_height + text_v->h / 2.f - descent - 1;
        height = font_height + 1;
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

int editor_load_filepath(Editor *ed, const U8 *filepath, U32 filepath_length) { TRACE
    // TODO: this leaks - allocates for each opened file
    // Change to reusable staticly sized buffer
    U8 *arena_filepath = ARENA_ALLOC_ARRAY(ed->arena, U8, filepath_length+1);
    memcpy(arena_filepath, filepath, filepath_length);
    arena_filepath[filepath_length] = 0;

    editor_clear_file(ed);
    undo_clear(&ed->undo_stack);

    I64 size = read_file_to_buffer(ed->text, TEXT_MAX_LENGTH, arena_filepath);
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
    SyntaxHighlighting *syntax = syntax_for_path(arena_filepath, filepath_length);
    ed->syntax = syntax ? *syntax : (SyntaxHighlighting){0};

    ed->selection_group = Group_Line;
    editor_set_selection(ed, 0, editor_group(ed, Group_Line, 0).end);
    ed->flags &= ~(U32)EditorFlag_Unsaved;
    
    editor_remake_caches(ed);

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

bool range_all_whitespace(Editor *ed, Range range) {
    for (I64 i = range.start; i < range.end; ++i) {
        if (!char_whitespace(editor_text(ed, i)))
            return false;
    }
    return true;
}

Range editor_group_range_paragraph(Editor *ed, I64 byte) { TRACE
    // not much we can do here
    if (byte < 0) byte = 0;
    if (byte >= ed->text_length) byte = ed->text_length;

    I64 start = byte;
    if (range_all_whitespace(ed, editor_group(ed, Group_Line, start))) {
        while (start > 0) {
            Range line = editor_group(ed, Group_Line, start-1);
            if (!range_all_whitespace(ed, line))
                break;
            start = line.start;
        }
    }
    while (start > 0) {
        Range line = editor_group(ed, Group_Line, start-1);
        if (range_all_whitespace(ed, line))
            break;
        start = line.start;
    }

    I64 end = byte;
    while (end < ed->text_length) {
        Range line = editor_group(ed, Group_Line, end);
        end = line.end;
        if (range_all_whitespace(ed, line))
            break;
    }
    while (end < ed->text_length) {
        Range line = editor_group(ed, Group_Line, end);
        if (!range_all_whitespace(ed, line))
            break;
        end = line.end;
    }

    //I64 end = byte;
    //while (editor_text(ed, end) != '\n' || editor_text(ed, end-1) != '\n')
        //end++;
    //while (editor_text(ed, end) == '\n' && end < ed->text_length)
        //end++;
     
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

Range editor_group_range_word(Editor *ed, I64 byte) { TRACE
    // not much we can do here
    if (byte < 0) byte = 0;
    if (byte >= ed->text_length) byte = ed->text_length-1;

    I64 start = byte;
    while (start > 0 && char_whitespace(editor_text(ed, start)))
        start--;

    bool (*char_fn)(U8 c);
    U8 c = editor_text(ed, start);
    if (char_word_like(c)) {
        char_fn = char_word_like;
    } else if (char_mathematic(c)) {
        char_fn = char_mathematic;
    } else {
        char_fn = char_none;
    }

    while (start > 0 && char_fn(editor_text(ed, start-1)))
        start--;

    I64 end = start+1;
    while (end < ed->text_length && char_fn(editor_text(ed, end)))
        end++;
    while (end < ed->text_length && (char_whitespace(editor_text(ed, end))))
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
    U8 c = editor_text(ed, start);
    if (char_subword_like(c)) {
        char_fn = char_subword_like;
    } else if (char_mathematic(c)) {
        char_fn = char_mathematic;
    } else {
        char_fn = char_none;
    }

    while (start > 0 && char_fn(editor_text(ed, start-1)))
        start--;

    I64 end = start+1;
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
    if (byte >= ed->text_length)
        return (I64)ed->line_count + (byte - ed->text_length);
    if (ed->line_count == 0)
        return byte;
        
    I64 a = 0;
    I64 b = (I64)ed->line_count-1; 
    U32 *line_lookup = ed->line_lookup;
    U32 byte_u32 = (U32)byte;
    
    while (a <= b) {
        I64 mid = a + (b - a) / 2;
        U32 line_a = line_lookup[mid];
        U32 line_b = line_lookup[mid+1];
        
        if (byte_u32 < line_a) b = mid - 1;
        else if (byte_u32 >= line_b) a = mid + 1;
        else return mid;
    }
    
    return a;
}

I64 editor_byte_index(Editor *ed, I64 line) { TRACE
    if (line < 0)
        return line;
    if (line > ed->line_count)
        return ed->line_lookup[ed->line_count] + line - ed->line_count;
    return ed->line_lookup[line];
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
        editor_set_selection(ed, start, ed->selection_b);
    } else if (end <= ed->selection_a) {
        editor_set_selection(ed, ed->selection_a - (end - start), ed->selection_b);
    }

    if (start <= ed->selection_b && ed->selection_b < end) {
        editor_set_selection(ed, ed->selection_a, start);
    } else if (end <= ed->selection_b) {
        editor_set_selection(ed, ed->selection_a, ed->selection_b - (end - start));
    }

    U64 to_move = (U64)(ed->text_length - end);
    ed->text_length -= end - start;
    memmove(&ed->text[start], &ed->text[end], to_move);

    // force newline termination cuz it makes math a lot simpler
    if (ed->text[ed->text_length-1] != '\n') {
        ed->text[ed->text_length++] = '\n';
    }
    
    editor_remake_caches(ed);
}

void editor_text_insert(Editor *ed, I64 at, U8 *text, I64 length) { TRACE
    expect(length >= 0);
    expect(ed->text_length + length <= (I64)TEXT_MAX_LENGTH);

    if (length != 0) {
        undo_record(&ed->undo_stack, at, text, length, UndoOp_Insert);
        editor_text_insert_raw(ed, at, text, length);
        ed->flags |= EditorFlag_Unsaved;
    }
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
    
    editor_remake_caches(ed);
}

void editor_remake_caches(Editor *ed) {
    U32 syntax_range = 0;
    U32 text_length = (U32)ed->text_length;

    SyntaxGroup *current_syntax_group = NULL;
    SyntaxRange *current_range = NULL;
    
    U64 char_is_syntax_start[4];
    memcpy(char_is_syntax_start, ed->syntax.char_is_syntax_start, sizeof(char_is_syntax_start));
    
    U32 line_count = 0;
    ed->line_lookup[line_count++] = 0;
    
    for (U32 i = 0; i < text_length; ++i) {
        U8 ch = ed->text[i];
        
        if (ch == '\n')
            ed->line_lookup[line_count++] = i+1;
        
        if (current_syntax_group == NULL) {
            U64 bit = 1ull << ((U64)ch & 63ull);
            U64 arr = (U64)ch >> 6ull;
            U64 start_mask = char_is_syntax_start[arr];
            if ((start_mask & bit) == 0)
                continue;
        
            U8 ch_next = editor_text(ed, i+1);
            U64 group_count = ed->syntax.group_count;
             
            for (U64 j = 0; j < group_count; ++j) {
                SyntaxGroup *group = &ed->syntax.groups[j];
                U8 *start_chars = group->start_chars;
                bool match_0 = start_chars[0] == ch;
                bool match_1 = start_chars[1] == 0 || start_chars[1] == ch_next;
    
                if (match_0 & match_1) {
                    current_syntax_group = group;
                    current_range = &ed->syntax_lookup[syntax_range++];
                    current_range->start = i;
                    current_range->colour = group->colour;
                    break;
                }
            }
        } else {
            U8 ch_prev = editor_text(ed, i-1);
            U8 *end_chars = current_syntax_group->end_chars;
            bool end;
            I64 end_count;
            if (end_chars[1] == 0) {
                end = end_chars[0] == ch;
                end_count = 1;
            } else {
                end = end_chars[0] == ch_prev && end_chars[1] == ch;
                end_count = 2;
            }
            
            // If there are an even number of escape characters before
            // the end characters, then they all escape each other. Otherwise,
            // the first end character is escaped, and we do not end this group.
            U8 escape = current_syntax_group->escape;
            if (end && escape != 0) {
                I64 escape_count = 0;
                while (editor_text(ed, i-end_count-escape_count) == escape)
                    escape_count++;
                end &= (escape_count & 1) == 0; 
            }
            
            if (end) {
                current_range->end = i;
                current_range = NULL;
                current_syntax_group = NULL;
            }
        }
    }
    
    ed->line_lookup[line_count] = (U32)ed->text_length;
    ed->syntax_range_count = syntax_range;
    ed->line_count = line_count;
}

Range editor_range_trim(Editor *ed, Range range) {
    I64 a = range.start;
    I64 b = range.end;
    while (a < b && char_whitespace(editor_text(ed, a)))
        a++;
    while (a < b && char_whitespace(editor_text(ed, b-1)))
        b--;
    return (Range) { a, b };
}

void editor_selection_trim(Editor *ed) { TRACE
    Range selection = (Range) { ed->selection_a, ed->selection_b };
    Range new = editor_range_trim(ed, selection);
    editor_set_selection(ed, new.start, new.end);
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
            editor_set_selection(ed, elem.at, elem.at);
            break;
        case UndoOp_Remove:
            editor_text_insert_raw(ed, elem.at, text, (I64)elem.text_length);
            editor_set_selection(ed, elem.at, elem.at + elem.text_length);
            break;
    }
    
    ed->flags |= EditorFlag_Unsaved;
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
    
    ed->flags |= EditorFlag_Unsaved;
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
    U64 char_count = 0;
     
    if (n < 0) {
        *(U8*)ARENA_ALLOC(arena, U8) = '-';
        char_count++;
    }
    U64 n_abs = (U64)(n >= 0 ? n : -n);

    // count digits
    U64 digit_count = 0;
    U64 m = n_abs;
    do {
        m /= 10;
        digit_count++;
    } while (m != 0);

    U8 *digits = ARENA_ALLOC_ARRAY(arena, *digits, digit_count);
    char_count += digit_count;

    for (U64 i = 0; i < digit_count; ++i) {
        U64 digit = n_abs % 10;
        n_abs /= 10;
        digits[digit_count - i - 1] = (U8)digit + '0';
    }

    return char_count;
}

Indices editor_find_lines(Editor *ed, Arena *arena, I64 start, I64 end) {
    Indices indices = {
        .count = 0,
        .ptr = arena_prealign(arena, alignof(I64)),
    };
    
    while (editor_text(ed, start-1) != '\n')
        start--;
        
    while (1) {
        if (editor_text(ed, start-1) == '\n') {
            *(I64*)arena_alloc(arena, sizeof(I64), alignof(I64)) = start;
            indices.count++;
        }
            
        start++;
        if (start >= end)
            break;
    }
    
    return indices;
}

// returns true if line comment prefix was found
static bool editor_line_comment_prefix(Editor *ed, U8 *prefix) {
    for (U64 i = 0; i < ed->syntax.group_count; ++i) {
        if (ed->syntax.groups[i].end_chars[0] == '\n') {
            memcpy(prefix, ed->syntax.groups[i].start_chars, EDITOR_SYNTAX_GROUP_SIZE);
            return true;
        }
    }
    return false;
}

static I64 editor_min_indent(Editor *ed, Indices lines) {
    I64 indent = 99999;
    for (U64 i = lines.count; i != 0; --i) {
        I64 line_start = lines.ptr[i-1];
        Range line = editor_group(ed, Group_Line, line_start);
        Range trimmed = editor_range_trim(ed, line);
        if (trimmed.start == trimmed.end)
            continue;
        I64 cur_indent = trimmed.start - line_start;
        if (cur_indent < indent)
            indent = cur_indent;
    }
    
    if (indent == 99999)
        return 0;
    return indent;
}

void editor_comment_lines(Editor *ed, Indices lines) {
    Arena *frame_arena = &w->frame_arena;

    U8 prefix[EDITOR_SYNTAX_GROUP_SIZE];
    if (!editor_line_comment_prefix(ed, prefix))
        return;
        
    if (lines.count == 0)
        return;
    
    I64 indent = editor_min_indent(ed, lines);
    
    for (U64 i = lines.count; i != 0; --i) {
        Range line = editor_group(ed, Group_Line, lines.ptr[i-1]);
        
        bool all_whitespace = true;
        for (I64 j = line.start; j != line.end; ++j) {
            if (!char_whitespace(editor_text(ed, j))) {
                all_whitespace = false;
                break;
            }
        }
        if (all_whitespace)
            continue;
        
        U8 *comment_text = arena_prealign(frame_arena, 1);
        
        bool already_commented = true;
        for (I64 j = 0; j < EDITOR_SYNTAX_GROUP_SIZE; ++j) {
            U8 c = prefix[j];
            if (c == 0) break;
            if (c != editor_text(ed, line.start + indent + j))
                already_commented = false;
            *(U8*)ARENA_ALLOC(frame_arena, U8) = prefix[j];
        }
        *(U8*)ARENA_ALLOC(frame_arena, U8) = ' ';
        
        if (!already_commented) {
            I64 comment_length = (I64)(frame_arena->head - comment_text);
            editor_text_insert(ed, line.start + indent, comment_text, comment_length);
        }
    }
}

void editor_uncomment_lines(Editor *ed, Indices lines) {
    U8 prefix[EDITOR_SYNTAX_GROUP_SIZE];
    if (!editor_line_comment_prefix(ed, prefix))
        return;
        
    for (U64 i = lines.count; i != 0; --i) {
        Range line = editor_group(ed, Group_Line, lines.ptr[i-1]);
        
        while (line.start != line.end) {
            I64 j = 0;
            for (; j < EDITOR_SYNTAX_GROUP_SIZE; ++j) {
                U8 c = prefix[j];
                if (c == 0) break;
                if (c != editor_text(ed, line.start + j))
                    goto NEXT_CHAR;
            }
            
            if (editor_text(ed, line.start + j) == ' ')
                j++;
            
            editor_text_remove(ed, line.start, line.start + j);
            break;
                
            NEXT_CHAR:
            line.start++;
        }
    }
}

void editor_set_selection(Editor *ed, I64 base, I64 head) {
    ed->selection_base = base;
    ed->selection_head = head;
    
    if (base <= head) {
        ed->selection_a = base;
        ed->selection_b = head;
    } else {
        ed->selection_a = head;
        ed->selection_b = base;
    }
}

void editor_goto_line(Editor *ed, I64 line_idx) {
    I64 byte = editor_byte_index(ed, line_idx);
    Range line = editor_group(ed, Group_Line, byte);
    editor_set_selection(ed, line.start, line.end);
    ed->selection_group = Group_Line;
    ed->mode = Mode_Normal;
}
