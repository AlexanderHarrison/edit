#ifndef EDITOR_H_
#define EDITOR_H_

#include "common.h"
#include "font.h"

#define MODE_INPUT_TEXT_MAX 512
#define TEXT_MAX_LENGTH (1ull << 30)

typedef enum Group {
    // separated by empty lines
    Group_Paragraph,

    // separated by lines
    Group_Line,

    // separated by any whitespace
    Group_Word,

    // separated by differing character types (symbol, alphanumeric, etc.)
    Group_Type,

    // separated by character
    Group_Character,

    Group_Count,
} Group;

typedef enum Mode {
    Mode_Normal,
    Mode_Insert,
} Mode;

typedef struct Editor {
    U8 *filename;
    U32 filename_length;

    Glyph *glyphs; // cache to avoid reallocations
    U8 *text;
    I64 text_length;

    // a <= b always
    I64 selection_a;
    I64 selection_b;
    Group selection_group;

    Mode mode;
    U8 *mode_input_text;
    U32 mode_input_text_length;
    union {
        I64 insert_cursor;
    } mode_data;

    // animation values - do not set these directly
    F32 scroll_y_visual;
} Editor;

Editor
editor_create(Arena *arena, const char *initial_filename);

void
editor_destroy(Editor *ed);

GlyphSlice
editor_update(W *w, Editor *ed, FontAtlas *font_atlas, Rect viewport);

#endif
