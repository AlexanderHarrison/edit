#ifndef MASS_H_
#define MASS_H_

typedef struct Match {
    U32 file_idx;
    U32 match_offset;
} Match;

typedef struct File {
    U8 *path;
    U32 contents_len;
    U8 *contents;
} File;

typedef enum MassMode {
    MassMode_EditSearch,
    MassMode_EditReplace,
} MassMode;

typedef struct Mass {
    Arena *arena;
    U8 *dirpath;
    U8 *search;
    U8 *replace;
    U32 search_len;
    U32 replace_len;
    U32 file_count;
    U32 match_count;
    File *files;
    Match *matches;
    MassMode mode;
} Mass;

Panel *mass_create(UI *ui, const U8 *dirpath);
void mass_update(Panel *panel);

#endif
