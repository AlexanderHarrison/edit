#ifndef EDITOR_H_
#define EDITOR_H_
#include <dirent.h>
#include <sys/types.h>
#include <libgen.h>

#include "common.h"
#include "font.h"

#define MODE_INPUT_TEXT_MAX 512
#define COPY_MAX_LENGTH 8096
#define TEXT_MAX_LENGTH (1ull << 28)

#define UNDO_STACK_SIZE (64ul*MB)
#define UNDO_TEXT_SIZE (64ul*MB)
#define UNDO_MAX (UNDO_STACK_SIZE / sizeof(UndoElem))

#define FILETREE_MAX_ENTRY_SIZE (64ul*MB)
#define FILETREE_MAX_TEXT_SIZE (64ul*MB)
#define FILETREE_MAX_ENTRY_COUNT (FILETREE_MAX_ENTRY_SIZE / sizeof(Entry))

enum DirFlags {
    DirFlag_Open = (1u << 0),
    DirFlag_Loaded = (1u << 1),
};

typedef struct Dir {
    U32 name_offset;
    U32 file_names_offset;
    U16 child_index;
    U16 file_count;
    U16 child_count;
    U16 flags;
} Dir;

typedef struct FileTree {
    U8 *name_buffer;
    Dir *dir_tree;
    U8 *dirpath;
    U32 text_buffer_head;
    U32 dir_count;
} FileTree;

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

static const RGBA8 selection_colours[Group_Count] = {
    {255, 0, 0, 255},
    {255, 100, 0, 255},
    {255, 255, 0, 255},
    {0, 255, 0, 255},
};

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
    FileTree filetree;
    U8 *copied_text;
    U8 *filepath;
    U32 copied_text_length;
    U32 filepath_length;

    Glyph *glyphs; // cache to avoid reallocations
    U8 *text;
    I64 text_length;

    // a <= b always
    I64 selection_a;
    I64 selection_b;
    Group selection_group;

    Mode mode;
    union {
        I64 insert_cursor;
    } mode_data;

    // animation values - do not set these directly
    F32 scroll_y_visual;
} Editor;

// if working_dir is NULL, then will get it from getcwd
Editor
editor_create(Arena *arena, const char *working_dir, const char *filepath);

void
editor_destroy(Editor *ed);

GlyphSlice
editor_update(W *w, Editor *ed, FontAtlas *font_atlas, Rect viewport);

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

static inline bool char_whitespace(U8 c) {
    return c == ' ' || c == '\t' || c == '\n';
}

static inline bool char_symbolic(U8 c) {
    return !char_whitespace(c) && !char_word_like(c);
}

#endif
