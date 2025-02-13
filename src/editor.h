#ifndef EDITOR_H_
#define EDITOR_H_
#include <dirent.h>
#include <sys/types.h>
#include <libgen.h>

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

    // separated by differing character types or underscores
    Group_SubWord,

    // separated by character
    Group_Character,

    Group_Count,
} Group;

typedef enum Mode {
    Mode_Normal,
    Mode_Insert,
    Mode_Search,
    //Mode_QuickMove,
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
    U8 *copied_text;
    U8 *filepath;
    U32 copied_text_length;
    U32 filepath_length;
    F32 scroll_y;

    U8 *text;
    I64 text_length;

    // a <= b always
    I64 selection_a;
    I64 selection_b;
    Group selection_group;

    Mode mode;
    U8 *mode_text;
    I64 mode_text_length;
    I64 insert_cursor;
    I64 *search_matches;
    I64 search_match_count;
    I64 search_cursor;

    // animation values - do not set these directly
    F32 scroll_y_visual;
} Editor;

// if working_dir is NULL, then will get it from getcwd
Panel *
editor_create(UI *ui, const U8 *filepath);

void
editor_update(Panel *panel);

int
editor_load_filepath(Editor *ed, const U8 *filepath);

static inline I64 clamp(I64 n, I64 low, I64 high) {
    if (n < low) return low;
    if (n > high) return high;
    return n;
}

static inline bool char_word_like(U8 c) {
    bool caps = ('A' <= c) & (c <= 'Z');
    bool lower = ('a' <= c) & (c <= 'z');
    bool num = ('0' <= c) & (c <= '9');
    bool other = c == '_';
    return caps | lower | num | other;
}

static inline bool char_subword_like(U8 c) {
    bool caps = ('A' <= c) & (c <= 'Z');
    bool lower = ('a' <= c) & (c <= 'z');
    bool num = ('0' <= c) & (c <= '9');
    return caps | lower | num;
}

static inline bool char_whitespace(U8 c) {
    return c == ' ' || c == '\t' || c == '\n';
}

static inline bool char_symbolic(U8 c) {
    return !char_whitespace(c) && !char_word_like(c);
}

static inline bool char_underscore(U8 c) { return c == '_'; }

#endif
