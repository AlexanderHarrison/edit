#ifndef JUMPLIST_H_
#define JUMPLIST_H_

typedef struct JumpPoint {
    U8 *filepath;
    I64 line_idx;
    U8 *text; // may be null
    U32 filepath_len;
    U32 text_len;
} JumpPoint;

typedef struct JumpList {
    PanelHandle ed_handle;
    JumpPoint *points;
    U32 point_count;
    U32 point_idx;
} JumpList;

Panel *jumplist_create(UI *ui);
void jumplist_update(Panel *jl_panel);
 
// passed strings will be copied.
void jumppoint_add(Panel *jl_panel, JumpPoint point);
void jumppoint_remove(Panel *jl_panel, U32 point_idx);

#endif
