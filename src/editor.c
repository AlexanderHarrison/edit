#include "editor.h"

typedef struct Range {
    I64 start;
    I64 end;
} Range;

int editor_load_filepath(Editor *ed, const char *filepath);
void editor_dealloc_file(Editor *ed);
Range editor_group(Editor *ed, Group group, I64 byte);
Range editor_group_next(Editor *ed, Group group, I64 current_group_end);
Range editor_group_prev(Editor *ed, Group group, I64 current_group_start);
I64 editor_line_index(Editor *ed, I64 byte);
Rect editor_line_rect(Editor *ed, FontAtlas *font_atlas, I64 a, I64 b, Rect *text_v);

U8 editor_text(Editor *ed, I64 byte);
void editor_text_remove(Editor *ed, I64 start, I64 end);
void editor_text_insert(Editor *ed, I64 at, U8 *text, I64 length);

Editor editor_create(Arena *arena, const char *initial_filepath) {
    Glyph *glyphs = ARENA_ALLOC_ARRAY(arena, *glyphs, MAX_GLYPHS);

    Editor ed = {
        .glyphs = glyphs,
        .selection_group = Group_Line,
        .mode_input_text = ARENA_ALLOC_ARRAY(arena, U8, MODE_INPUT_TEXT_MAX),
    };

    assert(editor_load_filepath(&ed, initial_filepath) == 0);

    ed.selection_b = editor_group(&ed, Group_Line, 0).end;

    return ed;
}

void editor_destroy(Editor *ed) {
    editor_dealloc_file(ed);
}

static inline bool is(U64 held, U64 mask) {
    return (held & mask) == mask;
}

#define SELECTION_GROUP_BAR_WIDTH 2.f

GlyphSlice editor_update(W *w, Editor *ed, FontAtlas *font_atlas, Rect viewport) {
    Rect text_v = viewport;
    text_v.x += SELECTION_GROUP_BAR_WIDTH;
    text_v.w -= SELECTION_GROUP_BAR_WIDTH;

    Rect selection_bar_v = viewport;
    selection_bar_v.w = SELECTION_GROUP_BAR_WIDTH;

    // UPDATE ---------------------------------------------------------------

    {
        switch (ed->mode) {
        case Mode_Normal: {
            U64 pressed = w->inputs.key_pressed;
            U64 repeating = w->inputs.key_repeating;
            U64 modifiers = w->inputs.modifiers;

            bool ctrl = is(modifiers, GLFW_MOD_CONTROL);
            bool shift = is(modifiers, GLFW_MOD_SHIFT);
            (void)shift;

            if (is(pressed, key_mask(GLFW_KEY_H))) {
                if (ed->selection_group == 0) {
                    ed->selection_group = Group_Count - 1;
                } else {
                    ed->selection_group--;
                }
            }

            if (is(pressed, key_mask(GLFW_KEY_L))) {
                if (ed->selection_group == Group_Count - 1) {
                    ed->selection_group = 0;
                } else {
                    ed->selection_group++;
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
                } else if (ctrl && !shift) {
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
                } else if (ctrl && !shift) {
                    Range prev_group = editor_group_prev(ed, ed->selection_group, ed->selection_b);
                    ed->selection_b = prev_group.start;
                }
            }

            if (is(pressed, key_mask(GLFW_KEY_C))) {
                ed->mode = Mode_Insert;
                ed->mode_data.insert_cursor = ed->selection_a;
                editor_text_remove(ed, ed->selection_a, ed->selection_b);
            }

            if (is(pressed | repeating, key_mask(GLFW_KEY_D))) {
                editor_text_remove(ed, ed->selection_a, ed->selection_b);
                Range range = editor_group(ed, ed->selection_group, ed->selection_a);
                ed->selection_a = range.start;
                ed->selection_b = range.end;
            }

            //if (is(pressed, key_mask(GLFW_KEY_K))) ed->scroll_y -= 1.f;
            //else if (is(repeating, key_mask(GLFW_KEY_K))) ed->scroll_y -= CODE_SCROLL_SPEED_SLOW;

            //if (ctrl & is(held, key_mask(GLFW_KEY_D))) ed->scroll_y += CODE_SCROLL_SPEED_FAST;
            //if (ctrl & is(held, key_mask(GLFW_KEY_U))) ed->scroll_y -= CODE_SCROLL_SPEED_FAST;

            if (is(pressed, key_mask(GLFW_KEY_Q))) w->should_close = true;
            break;
        }
        case Mode_Insert: {
            U64 special_pressed = w->inputs.key_special_pressed;

            bool esc = is(special_pressed, special_mask(GLFW_KEY_ESCAPE));
            bool caps = is(special_pressed, special_mask(GLFW_KEY_CAPS_LOCK));
            if (esc || caps) {
                ed->mode = Mode_Normal;
                Range range = editor_group(ed, ed->selection_group, ed->mode_data.insert_cursor);
                ed->selection_a = range.start;
                ed->selection_b = range.end;
            }

            for (I64 i = 0; i < w->inputs.char_event_count; ++i) {
                U32 codepoint = w->inputs.char_events[i].codepoint;
                // enforce ascii for now
                assert(codepoint < 128);

                U8 codepoint_as_char = (U8)codepoint;
                editor_text_insert(ed, ed->mode_data.insert_cursor, &codepoint_as_char, 1);
                ed->mode_data.insert_cursor += 1;
            }

            if (is(special_pressed, special_mask(GLFW_KEY_ENTER))) {
                U8 newline = '\n';
                editor_text_insert(ed, ed->mode_data.insert_cursor, &newline, 1);
                ed->mode_data.insert_cursor += 1;
            }

            break;
        }
        }
    }

    if (ed->selection_a > ed->selection_b) {
        fprintf(stderr, "Error: Cursor range out of order! Reversing %li <-> %li\n", ed->selection_a, ed->selection_b);
        I64 temp = ed->selection_a;
        ed->selection_a = ed->selection_b;
        ed->selection_b = temp;
    }

    // UPDATE ANIMATIONS ----------------------------------------------------

    {
        I64 line_a = editor_line_index(ed, ed->selection_a);
        I64 line_b = editor_line_index(ed, ed->selection_b);
        F32 scroll_y = ((F32)line_a + (F32)line_b) / 2.f;

        // scrolling
        F32 diff = scroll_y - ed->scroll_y_visual;
        if (fabs(diff) < 0.05f) 
            ed->scroll_y_visual = scroll_y;
        else
            ed->scroll_y_visual += diff * ANIM_EXP_FACTOR;
    }

    // WRITE SPECIAL GLYPHS -------------------------------------------------

    U64 glyph_count = 0;

    // selection group bar
    {
        RGBA8 selection_bar_colour;

        if (ed->mode == Mode_Normal) {
            static const RGBA8 selection_colours[Group_Count] = {
                {255, 0, 0, 255},
                {255, 130, 0, 255},
                {255, 255, 0, 255},
                {0, 255, 0, 255},
                {0, 255, 165, 255},
            };

            selection_bar_colour = selection_colours[ed->selection_group];
        } else if (ed->mode == Mode_Insert) {
            selection_bar_colour = (RGBA8)FOREGROUND;
        }

        ed->glyphs[glyph_count++] = (Glyph) {
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

        F32 line_i = (F32)(editor_line_index(ed, a));
        while (1) {
            Rect rect = editor_line_rect(ed, font_atlas, a, b, &text_v);

            ed->glyphs[glyph_count++] = (Glyph) {
                .x = rect.x,
                .y = rect.y,
                .glyph_idx = special_glyph_rect((U32)rect.w, (U32)rect.h),
                .colour = SELECT,
            };

            Range line = editor_group(ed, Group_Line, a);


            line_i += 1.f;
            if (line.end >= b) break;
            else a = line.end;
        }
    } else if (ed->mode == Mode_Insert) {
        I64 cursor = ed->mode_data.insert_cursor;
        Rect rect = editor_line_rect(ed, font_atlas, cursor, cursor, &text_v);
        rect.w = 3.f;
        ed->glyphs[glyph_count++] = (Glyph) {
            .x = rect.x,
            .y = rect.y,
            .glyph_idx = special_glyph_rect((U32)rect.w, (U32)rect.h),
            .colour = FOREGROUND,
        };
    }

    // WRITE TEXT GLYPHS ----------------------------------------------------

    I64 text_length = ed->text_length;
    F32 line = 0.f;
    F32 pen_x = text_v.x;

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
        FOREGROUND,
        COMMENT,
        COMMENT,
        STRING,
        STRING,
        STRING
    };

    U64 state = State_Normal;

    for (I64 i = 0; i < text_length; ++i) {
        U8 ch_prev = editor_text(ed, i-1);
        U8 ch = editor_text(ed, i);
        U8 ch_next = editor_text(ed, i+1);

        if (ch == '\n') {
            line += 1.f;
            pen_x = text_v.x;

            if (state == State_LineComment)
                state = State_Normal;

            continue;
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
            if (ch_prev != '\\' && ch == '\"') {
                state = State_String_End;
            }
        } else if (state == State_String_SingleQ) {
            if (ch_prev != '\\' && ch == '\'') {
                state = State_String_End;
            }
        } else if (state == State_String_End) {
            state = State_Normal;
        }

        // bounds checking -----

        F32 line_offset_from_scroll = line - ed->scroll_y_visual;
        F32 line_y = line_offset_from_scroll * CODE_LINE_SPACING + text_v.h / 2.f;

        if (line_y + CODE_LINE_SPACING < 0.f) {
            // skip entire line if before text_v
            //while (i < text_length && ed->text[i] != '\n') i++;
            continue;
        };
        if (line_y > text_v.h) break;

        // if is in text_v ----

        if (glyph_count == MAX_GLYPHS) break;
        F32 pen_y = text_v.y + line_y + CODE_LINE_SPACING;

        U32 glyph_idx = glyph_lookup_idx(CODE_FONT_SIZE, ch);
        GlyphInfo info = font_atlas->glyph_info[glyph_idx];

        ed->glyphs[glyph_count++] = (Glyph) {
            .x = pen_x + info.offset_x,
            .y = pen_y + info.offset_y,
            .glyph_idx = glyph_idx,
            .colour = state_colours[state],
        };
        pen_x += info.advance_width;
    }

    return (GlyphSlice) { ed->glyphs, glyph_count };
}

// the returned rect will run from a, until b or the end of the line, whichever is shortest.
Rect editor_line_rect(Editor *ed, FontAtlas *font_atlas, I64 a, I64 b, Rect *text_v) {
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

int editor_load_filepath(Editor *ed, const char *filepath) {
    if (filepath == NULL) return 0;

    editor_dealloc_file(ed);

    if (ed->text == NULL) {
        ed->text = vm_alloc(TEXT_MAX_LENGTH);
        assert(ed->text != NULL);
    }

    Bytes b = read_file_to(filepath, ed->text, TEXT_MAX_LENGTH);
    assert(b.ptr != NULL);
    ed->text_length = (I64)b.len;

    return 0;
}

void editor_dealloc_file(Editor *ed) {
    ed->text_length = 0;
    ed->text = NULL;
}

U8 editor_text(Editor *ed, I64 byte) {
    if (byte < 0) return '\n';
    if (byte >= ed->text_length) return '\n';
    return ed->text[byte];
}

Range editor_group_range_paragraph(Editor *ed, I64 byte) {
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

Range editor_group_range_line(Editor *ed, I64 byte) {
    if (byte < 0 || byte > ed->text_length)
        return (Range) { byte, byte+1 };

    I64 start = byte;
    while (editor_text(ed, start-1) != '\n')
        start--;

    I64 end = byte;
    while (editor_text(ed, end++) != '\n') {};

    return (Range) { start, end };
}

bool char_word_like(U8 c) {
    bool caps = ('A' <= c) & (c <= 'Z');
    bool lower = ('a' <= c) & (c <= 'z');
    bool num = ('0' <= c) & (c <= '9');
    bool other = c == '_';
    return caps | lower | num | other;
}

bool char_whitespace(U8 c) {
    return c == ' ' || c == '\t' || c == '\n';
}

bool char_symbolic(U8 c) {
    return !char_whitespace(c) && !char_word_like(c);
}

Range editor_group_range_word(Editor *ed, I64 byte) {
    // not much we can do here
    if (byte < 0) byte = 0;
    if (byte >= ed->text_length) byte = ed->text_length-1;

    I64 start = byte;
    while (char_whitespace(editor_text(ed, start)))
        start--;
    while (!char_whitespace(editor_text(ed, start-1)))
        start--;

    I64 end = byte;
    while (!char_whitespace(editor_text(ed, end)))
        end++;
    while (char_whitespace(editor_text(ed, end)))
        end++;

    return (Range) { start, end };
}

Range editor_group_range_type(Editor *ed, I64 byte) {
    // not much we can do here
    if (byte < 0) byte = 0;
    if (byte >= ed->text_length) byte = ed->text_length-1;

    I64 start = byte;
    while (char_whitespace(editor_text(ed, start)))
        start--;

    bool (*char_fn)(U8 c);
    if (char_word_like(editor_text(ed, start))) {
        char_fn = char_word_like;
    } else {
        char_fn = char_symbolic;
    }

    while (char_fn(editor_text(ed, start-1)))
        start--;

    I64 end = byte;
    while (char_fn(editor_text(ed, end)))
        end++;
    while (char_whitespace(editor_text(ed, end)))
        end++;

    return (Range) { start, end };
}

Range editor_group_range_char(Editor *ed, I64 byte) {
    (void)ed;
    return (Range) { byte, byte+1 };
}

Range editor_group(Editor *ed, Group group, I64 byte) {
    switch (group) {
        case Group_Paragraph:
            return editor_group_range_paragraph(ed, byte);
        case Group_Line:
            return editor_group_range_line(ed, byte);
        case Group_Word:
            return editor_group_range_word(ed, byte);
        case Group_Type:
            return editor_group_range_type(ed, byte);
        case Group_Character:
            return editor_group_range_char(ed, byte);
        default:
            assert(0);
    }
}

Range editor_group_next(Editor *ed, Group group, I64 current_group_end) {
    return editor_group(ed, group, current_group_end);
}

Range editor_group_prev(Editor *ed, Group group, I64 current_group_start) {
    return editor_group(ed, group, current_group_start-1);
}

I64 editor_line_index(Editor *ed, I64 byte) {
    I64 line_i = 0;
    for (I64 i = 0; i < byte; ++i) {
        if (editor_text(ed, i) == '\n')
            line_i++;
    }
    return line_i;
}

void editor_text_remove(Editor *ed, I64 start, I64 end) {
    if (start > end) {
        I64 temp = start;
        start = end;
        end = temp;
    }
    if (start < 0) start = 0;
    if (end > ed->text_length) end = ed->text_length;

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

void editor_text_insert(Editor *ed, I64 at, U8 *text, I64 length) {
    assert(length >= 0);
    if (length == 0) return;
    assert(ed->text_length + length <= (I64)TEXT_MAX_LENGTH);

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
