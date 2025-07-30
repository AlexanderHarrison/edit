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
    Mode_Replace,
    Mode_QuickMove,
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

enum EditorFlags {
    EditorFlag_Unsaved = (1ul << 0ul),
};

typedef struct SyntaxGroup {
    U8 start_chars[EDITOR_SYNTAX_GROUP_SIZE];
    U8 end_chars[EDITOR_SYNTAX_GROUP_SIZE];
    U8 escape;
    RGBA8 colour;
} SyntaxGroup;

typedef struct SyntaxHighlighting {
    U64 group_count;
    SyntaxGroup *groups;
    U64 char_is_syntax_start[4];
} SyntaxHighlighting;

typedef struct SyntaxRange {
    U32 start, end;
    RGBA8 colour;
} SyntaxRange;

typedef struct Editor {
    Arena *arena;
    UndoStack undo_stack;
    U8 *filepath;
    U32 filepath_length;
    SyntaxHighlighting syntax;
    F32 scroll_y;
    U32 flags;

    U8 *text;
    I64 text_length;
    
    U32 *line_lookup;
    SyntaxRange *syntax_lookup;
    U32 line_count;
    U32 syntax_range_count;

    // a <= b always
    I64 selection_a;
    I64 selection_b;
    Group selection_group;

    Mode mode;
    U8 *mode_text;
    I64 mode_text_length;
    U8 *mode_text_alt;
    I64 mode_text_alt_length;
    I64 insert_cursor;
    I64 search_a;
    I64 search_b;
    I64 *search_matches;
    I64 search_match_count;
    I64 search_cursor;

    // animation values - do not set these directly
    F32 scroll_y_visual;
} Editor;

Panel *
editor_create(UI *ui, const U8 *filepath);

void
editor_update(Panel *panel);

int
editor_load_filepath(Editor *ed, const U8 *filepath, U32 filepath_length);

void
editor_goto_line(Editor *ed, I64 line_idx);

static inline I64 clamp(I64 n, I64 low, I64 high) {
    if (n < low) return low;
    if (n > high) return high;
    return n;
}

typedef enum CharType {
    Char_None = 0,
    Char_Whitespace,
    Char_Alphabetic,
    Char_Numeric,
    Char_Mathematic,
    Char_Symbolic,
    Char_Underscore,
} CharType;

static const U8 char_lookup[128] = {
#include "ascii.c"
};

static inline bool char_word_like(U8 c) {
    if (c >= 128) return false;
    CharType type = char_lookup[c];
    return type == Char_Alphabetic 
        || type == Char_Numeric
        || type == Char_Underscore;
}

static inline bool char_subword_like(U8 c) {
    if (c >= 128) return false;
    CharType type = char_lookup[c];
    return type == Char_Alphabetic || type == Char_Numeric;
}

static inline bool char_whitespace(U8 c) {
    if (c >= 128) return false;
    CharType type = char_lookup[c];
    return type == Char_Whitespace;
}

static inline bool char_mathematic(U8 c) {
    if (c >= 128) return false;
    CharType type = char_lookup[c];
    return type == Char_Mathematic;
}

static inline bool char_underscore(U8 c) {
    if (c >= 128) return false;
    return c == '_';
}

static inline bool char_none(U8 c) {
    (void)c;
    return false;
}

#endif
