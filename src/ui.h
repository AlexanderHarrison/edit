typedef struct Panel Panel;
typedef struct UI UI;

enum PanelFlags {
    PanelMode_VSplit = (1u << 0),
    PanelMode_HSplit = (1u << 1),

    PanelFlag_InUse = (1u << 2),
    PanelFlag_Focused = (1u << 3),
};

typedef void (*PanelFn)(Panel *panel);

typedef struct PanelHandle {
    U32 idx;
    U32 generation;
} PanelHandle;

typedef struct Panel {
    // modify these
    void *data;
    F32 static_w;
    F32 static_h;
    F32 dynamic_weight_w;
    F32 dynamic_weight_h;
    PanelFn update_fn;
    PanelFn destroy_fn;
    const char *name;

    // please do not modify these
    Rect viewport;
    Arena *arena;
    Panel *parent;
    Panel *child;
    Panel *sibling_prev;
    Panel *sibling_next;
    U32 flags;
    U32 generation;
    UI *ui;
} Panel;

typedef struct UIOp {
    enum UIOp_Tag {
        UIOp_PanelDestroy,
        UIOp_PanelFocus,
        UIOp_PanelInsertBefore,
        UIOp_PanelInsertAfter,
        UIOp_PanelAddChild,
    } tag;

    Panel *panel;
    Panel *panel_source;
} UIOp;

typedef struct UI {
    Panel *root;
    W *w;
    FontAtlas *atlas;
    Panel *panel_store;
    Panel *free;
    Panel *focused;
    Glyph *glyphs;
    U64 glyph_count;
    UIOp *op_queue;
    U64 op_count;
} UI;

UI     *ui_create           (W *w, FontAtlas *atlas, Arena *arena);
void    ui_destroy          (UI *ui);
void    ui_update           (UI *ui, Rect *viewport);
UIOp   *ui_push_op          (UI *ui);
void    ui_flush_ops        (UI *ui);
Glyph  *ui_push_glyph       (UI *ui);
Panel  *panel_create        (UI *ui);
void    panel_focus         (Panel *panel);
void    panel_detach        (Panel *panel);
void    panel_destroy       (Panel *panel);
void    panel_add_child     (Panel *parent, Panel *new);
void    panel_insert_after  (Panel *old, Panel *new);
void    panel_insert_before (Panel *old, Panel *new);
void    panel_focus_queued         (Panel *panel);
void    panel_destroy_queued       (Panel *panel);
void    panel_add_child_queued     (Panel *parent, Panel *new);
void    panel_insert_after_queued  (Panel *old, Panel *new);
void    panel_insert_before_queued (Panel *old, Panel *new);

Panel      *panel_lookup(UI *ui, PanelHandle handle);
PanelHandle panel_handle(Panel *panel);

// creates an arena if it doesn't exist
Arena  *panel_arena     (Panel *panel);

// returns number of glyphs written
U64 write_string_terminated(
    Glyph *glyphs,
    const U8 *str,
    FontAtlas *font_atlas,
    RGBA8 colour, U64 font_size,
    F32 x, F32 y, F32 max_width
);

U64 write_string(
    Glyph *glyphs,
    const U8 *str, U64 length,
    FontAtlas *font_atlas,
    RGBA8 colour, U64 font_size,
    F32 x, F32 y, F32 max_width
);

