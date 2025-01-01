#ifndef EDITOR_H_
#define EDITOR_H_

#include "common.h"
#include "font.h"

#define MODE_INPUT_TEXT_MAX 512
#define TEXT_MAX_LENGTH (1ull << 30)

#define UNDO_STACK_SIZE (1ul*GB)
#define UNDO_TEXT_SIZE (1ul*GB)
#define UNDO_MAX (UNDO_STACK_SIZE / sizeof(UndoElem))

typedef struct Range {
    I64 start;
    I64 end;
} Range;

typedef enum Group {
    // separated by empty lines
    Group_Paragraph,

    // separated by lines
    Group_Line,

    // separated by differing character types (symbol, alphanumeric, etc.)
    Group_Word,

    // separated by character
    Group_Character,

    Group_Count,
} Group;

typedef enum Mode {
    Mode_Normal,
    Mode_Insert,
} Mode;

typedef enum UndoOp {
    UndoOp_Insert = 0,
    UndoOp_Remove = 1,
} UndoOp;

typedef struct UndoElem {
    I64 at;
    U32 text_length;
    U8 op;
} UndoElem;

typedef struct UndoStack {
    U8 *text_stack;
    UndoElem *undo_stack;
    U32 text_stack_head;
    U32 undo_stack_head;
    U32 undo_count;
} UndoStack;


typedef struct Editor {
    Arena *arena;
    UndoStack undo_stack;
    U8 *filepath;
    U32 filepath_length;

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
editor_create(Arena *arena, const char *initial_filepath);

void
editor_destroy(Editor *ed);

GlyphSlice
editor_update(W *w, Editor *ed, FontAtlas *font_atlas, Rect viewport);

#endif
