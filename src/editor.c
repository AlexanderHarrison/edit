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
//   a - enter edit mode at end of selection
//
// SEARCH MODE ---------------------------------------------------------
// C-j - go to next matched item
// C-k - go to previous matched item
// Enter - exit search mode and select current matched item
//
// EDIT MODE -----------------------------------------------------------
//   c - delete selection and enter edit mode
//   C - trim, delete selection, and enter edit mode
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
//   u - undo
// C-r - redo
//
// C-p - vsplit, adding another editor to the right
//
//   / - enter search mode
//
//   < - unindent lines
//   > - indent lines
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

typedef struct Indices {
    U64 count;
    I64 *ptr;
} Indices;

UndoStack   undo_create(Arena *arena);
void        undo_clear(UndoStack *st);
UndoElem   *undo_record(UndoStack *st, I64 at, U8 *text, I64 text_length, UndoOp op);

void        editor_clear_file(Editor *ed);
Range       editor_group(Editor *ed, Group group, I64 byte);
Range       editor_group_next(Editor *ed, Group group, I64 current_group_end);
Range       editor_group_prev(Editor *ed, Group group, I64 current_group_start);
I64         editor_line_index(Editor *ed, I64 byte);
I64         editor_byte_index(Editor *ed, I64 line);
Rect        editor_line_rect(Editor *ed, FontAtlas *font_atlas, I64 a, I64 b, Rect *text_v);
void        editor_selection_trim(Editor *ed);
Indices     editor_find_lines(Editor *ed, Arena *arena, I64 start, I64 end);

void        editor_undo(Editor *ed);
void        editor_redo(Editor *ed);
static inline U8 editor_text(Editor *ed, I64 byte);
void        editor_open_filetree(Panel *ed_panel, bool expand);
void        editor_open_jumplist(Panel *ed_panel);
void        editor_jumplist_add(Panel *ed_panel, JumpPoint point);
void        editor_text_remove(Editor *ed, I64 start, I64 end);
void        editor_text_insert(Editor *ed, I64 at, U8 *text, I64 length);
// same as above, but does not add to the undo stack
void        editor_text_remove_raw(Editor *ed, I64 start, I64 end);
void        editor_text_insert_raw(Editor *ed, I64 at, U8 *text, I64 length);

static U64 int_to_string(Arena *arena, I64 n);

// EDITOR ####################################################################

#define SYNTAX_COMMENT_SLASHES { {'/', '/'}, {'\n'}, 0, COLOUR_COMMENT }
#define SYNTAX_COMMENT_HASHTAG { {'#'}, {'\n'}, 0, COLOUR_COMMENT }
#define SYNTAX_STRING_DOUBLE_QUOTES { {'"'}, {'"'}, '\\', COLOUR_STRING }
#define SYNTAX_STRING_SINGLE_QUOTES { {'\''}, {'\''}, '\\', COLOUR_STRING }

static SyntaxGroup syntax_c[] = {
    SYNTAX_COMMENT_SLASHES,
    SYNTAX_STRING_DOUBLE_QUOTES,
    SYNTAX_STRING_SINGLE_QUOTES,
};

static SyntaxGroup syntax_rs[] = {
    SYNTAX_COMMENT_SLASHES,
    SYNTAX_STRING_DOUBLE_QUOTES,
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

static SyntaxLookup syntax_lookup[] = {
    {"c",   {countof(syntax_c), syntax_c}},
    {"h",   {countof(syntax_c), syntax_c}},
    {"cpp", {countof(syntax_c), syntax_c}},
    {"hpp", {countof(syntax_c), syntax_c}},
    {"rs",  {countof(syntax_rs), syntax_rs}},
    {"sh",  {countof(syntax_sh), syntax_sh}},
    {"py",  {countof(syntax_py), syntax_py}},
    {"glsl",{countof(syntax_c), syntax_c}},
    {"hlsl",{countof(syntax_c), syntax_c}},
    {"s",   {countof(syntax_asm), syntax_asm}},
    {"asm", {countof(syntax_asm), syntax_asm}},
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

    for (U64 i = 0; i < countof(syntax_lookup); ++i) {
        SyntaxLookup *lookup = &syntax_lookup[i];
        if (memcmp(ex, lookup->extension, sizeof(ex)) == 0)
            return &lookup->syntax;
    }
    
    return NULL;
}

Panel *editor_create(UI *ui, const U8 *filepath) { TRACE
    Panel *panel = panel_create(ui);
    panel->update_fn = editor_update;
    Arena *arena = panel_arena(panel);

    Editor *ed = arena_alloc(arena, sizeof(Editor), alignof(Editor));
    *ed = (Editor) {
        .undo_stack = undo_create(arena),
        .selection_group = Group_Line,
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
                    Range prev_group = editor_group_prev(ed, ed->selection_group, ed->selection_a);
                    ed->selection_a = prev_group.start;
                } else if (ctrl && shift) {
                    Range prev_group = editor_group_prev(ed, ed->selection_group, ed->selection_b);
                    ed->selection_b = prev_group.start;
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
                Range range = editor_group(ed, ed->selection_group, ed->selection_b-1);
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
                I64 copy_length_signed = copy_end - copy_start;
                if (copy_length_signed > 0) {
                    U32 copy_length = (U32)copy_length_signed;
                    char *copied = arena_alloc(&panel->ui->w->frame_arena, copy_length+1, 1);
                    memcpy(copied, &ed->text[copy_start], copy_length);
                    copied[copy_length] = 0;
                    glfwSetClipboardString(NULL, copied);
                }
            }

            if (is(pressed, key_mask(GLFW_KEY_F))) {
                if (!ctrl)
                    ed->selection_b = ed->text_length;
                if (!shift)
                    ed->selection_a = 0;
            }

            if (!ctrl && !shift && is(pressed, key_mask(GLFW_KEY_R))) {
                editor_selection_trim(ed);
                ed->selection_group = Group_SubWord;
                Range range = editor_group(ed, ed->selection_group, ed->selection_a);
                ed->selection_a = range.start;
                ed->selection_b = range.end;
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
                const char *text = glfwGetClipboardString(NULL);
                if (text != NULL) {
                    U8 *text_owned = copy_cstr(&panel->ui->w->frame_arena, text);
                    U32 len = (U32)strlen(text);
                    editor_text_insert(ed, paste_idx, text_owned, len);
                    ed->selection_a = paste_idx;
                    ed->selection_b = paste_idx + len;
                }
            }

            if (is(pressed, key_mask(GLFW_KEY_T)))
                editor_open_filetree(panel, !shift);
                
            if (!ctrl && is(pressed, key_mask(GLFW_KEY_B))) {
                if (!shift) {
                    editor_open_jumplist(panel);
                } else {
                    Range paragraph = editor_group(ed, Group_Paragraph, ed->selection_a);
                    if (paragraph.start < 0) paragraph.start = 0;
                    if (paragraph.end > ed->text_length) paragraph.end = ed->text_length;
                    if (paragraph.end < paragraph.start) {
                        I64 end = paragraph.end;
                        paragraph.end = paragraph.start;
                        paragraph.start = end;
                    }
                     
                    JumpPoint current_point = {
                        .filepath = ed->filepath,
                        .filepath_len = ed->filepath_length,
                        .text = &ed->text[paragraph.start],
                        .text_len = (U32)(paragraph.end - paragraph.start),
                        .line_idx = editor_line_index(ed, paragraph.start), 
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
                U8 *spaces = arena_alloc(&panel->ui->w->frame_arena, 4, 1);
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
            
            break;
        }
        case Mode_Insert: {
            bool esc = is(special_pressed, special_mask(GLFW_KEY_ESCAPE));
            bool caps = is(special_pressed, special_mask(GLFW_KEY_CAPS_LOCK));
            if (esc || caps) {
                ed->mode = Mode_Normal;
                ed->selection_group = Group_Line;
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

            ed->search_matches = arena_prealign(&w->frame_arena, alignof(*ed->search_matches));
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
                ed->selection_group = Group_Line;
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
        case Mode_QuickMove: {
            float speed = shift ? CODE_SCROLL_SPEED_SLOW : CODE_SCROLL_SPEED_FAST;
            
            if (ctrl && is(held, key_mask(GLFW_KEY_J)))
                ed->scroll_y += speed / (F32)w->refresh_rate;
            if (ctrl && is(held, key_mask(GLFW_KEY_K)))
                ed->scroll_y -= speed / (F32)w->refresh_rate;
                 
            bool esc = is(special_pressed, special_mask(GLFW_KEY_ESCAPE));
            bool caps = is(special_pressed, special_mask(GLFW_KEY_CAPS_LOCK));
            if (esc || caps) {
                ed->mode = Mode_Normal;
            }
            
            if (is(special_pressed, special_mask(GLFW_KEY_ENTER))) {
                I64 line = (I64)roundf(ed->scroll_y);
                I64 byte = editor_byte_index(ed, line);
                Range range = editor_group(ed, Group_Line, byte);
                ed->selection_a = range.start;
                ed->selection_b = range.end;
                ed->mode = Mode_Normal;
            }
             
            break;
        }
        }
    }
    
    // Reverse selection
    if (ed->selection_a > ed->selection_b) {
        I64 temp = ed->selection_a;
        ed->selection_a = ed->selection_b;
        ed->selection_b = temp;
    }

    // UPDATE ANIMATIONS ----------------------------------------------------

    // find new scroll y
    {
        if (ed->mode == Mode_Search) {
            if (ed->search_match_count > 0) {
                I64 shown_match = ed->search_matches[ed->search_cursor];
                I64 line_a = editor_line_index(ed, shown_match);
                I64 line_b = editor_line_index(ed, shown_match + ed->mode_text_length);
                ed->scroll_y = ((F32)line_a + (F32)line_b) / 2.f;
            }
        } else if (ed->mode == Mode_QuickMove) {
            // do nothing, scroll y preserved across update
        } else {
            I64 line_a = editor_line_index(ed, ed->selection_a);
            I64 line_b = editor_line_index(ed, ed->selection_b);
            ed->scroll_y = ((F32)line_a + (F32)line_b) / 2.f;
        }
    }
    
    // animate scrolling
    bool animation_playing = false;
    { 
        // animate scrolling
        if (ed->mode != Mode_QuickMove) {
            F32 diff = ed->scroll_y - ed->scroll_y_visual;
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
    
    SyntaxGroup *current_syntax_group = NULL;
    
    FontSize font_size = CODE_FONT_SIZE;
    F32 spacing = CODE_LINE_SPACING;
    
    for (I64 i = 0; i < text_length; ++i) {
        U8 ch = ed->text[i];

        // compute syntax highlighting
        RGBA8 text_colour;
        if (current_syntax_group == NULL) {
            U8 ch_next = editor_text(ed, i+1);

            U64 group_count = ed->syntax.group_count; 
            for (U64 j = 0; j < group_count; ++j) {
                SyntaxGroup *group = &ed->syntax.groups[j];
                U8 *start_chars = group->start_chars;
                bool match_0 = start_chars[0] == ch;
                bool match_1 = start_chars[1] == 0 || start_chars[1] == ch_next;

                if (match_0 & match_1) {
                    current_syntax_group = group;
                    break;
                }
            }
            
            text_colour = current_syntax_group ? current_syntax_group->colour : (RGBA8) COLOUR_FOREGROUND;
        } else {
            text_colour = current_syntax_group ? current_syntax_group->colour : (RGBA8) COLOUR_FOREGROUND;
            
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
            
            if (end)
                current_syntax_group = NULL;
        }

        if (ch == '\n') {
            line += 1.f;
            pen_x = 0.f;
            continue;
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
                .colour = text_colour,
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
        ui_push_string_terminated(
            ui,
            mode_info_text,
            font_atlas,
            colour, MODE_FONT_SIZE,
            x, y, mode_info_v.x + mode_info_v.w
        );
    }

    // draw status    
    {
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
    
        F32 descent = font_atlas->descent[CODE_FONT_SIZE];
        F32 status_x = mode_info_v.x;
        F32 status_y = mode_info_v.y + descent + CODE_LINE_SPACING;
        F32 status_max_x = status_x + mode_info_v.w;
        
        // find line number
        I64 line_i;
        switch (ed->mode) {
            case Mode_Normal: {
                line_i = editor_line_index(ed, ed->selection_b);
            } break;
            case Mode_Insert: {
                line_i = editor_line_index(ed, ed->insert_cursor);
            } break;
            case Mode_Search: {
                if (ed->search_match_count) {
                    I64 search_byte = ed->search_matches[ed->search_cursor];  
                    line_i = editor_line_index(ed, search_byte);
                } else {
                    line_i = editor_line_index(ed, ed->selection_b);
                }
            } break;
            case Mode_QuickMove: {
                line_i = (I64)roundf(ed->scroll_y);
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
        filetree_dir_open_all(ft, &ed_panel->ui->w->frame_arena, ft->dir_tree);
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
    undo_clear(&ed->undo_stack);

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
    ed->flags &= ~(U32)EditorFlag_Unsaved;
    
    SyntaxHighlighting *syntax = syntax_for_path(arena_filepath, filepath_length);
    ed->syntax = syntax ? *syntax : (SyntaxHighlighting){0};

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

    U8 c = editor_text(ed, start);
    bool (*char_fn)(U8 c);
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
    while (char_fn(editor_text(ed, end)))
        end++;
    while (char_whitespace(editor_text(ed, end)))
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
    I64 line_i = 0;
    for (I64 i = 0; i < byte; ++i) {
        if (editor_text(ed, i) == '\n')
            line_i++;
    }
    return line_i;
}

I64 editor_byte_index(Editor *ed, I64 line) { TRACE
    if (line < 0) return line;
    I64 byte = 0;
    while (line != 0) {
        if (editor_text(ed, byte) == '\n')
            line--;
        byte++;
    }
    return byte;
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
