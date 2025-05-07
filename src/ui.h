typedef struct Panel Panel;
typedef struct UI UI;

enum PanelFlags {
    PanelMode_VSplit = (1u << 0),
    PanelMode_HSplit = (1u << 1),

    PanelFlag_InUse = (1u << 2),
    PanelFlag_Focused = (1u << 3),
    PanelFlag_Hidden = (1u << 4),
};

typedef void (*PanelFn)(Panel *panel);

typedef struct PanelHandle {
    U32 idx;
    U32 generation;
} PanelHandle;

// The first generation of panels will always have > 0 generation,
// so this fails either from a wrong generation (if in use),
// or because the panel is not marked as in use.
#define PANEL_HANDLE_NULL ((PanelHandle){0,0})

typedef struct Panel {
    // modify these
    void *data;
    
    // The minimum size of the panel.
    F32 static_w;
    F32 static_h;
    // How much the panel grows to fill dynamic space.
    F32 dynamic_weight_w;
    F32 dynamic_weight_h;
    PanelFn update_fn;
    PanelFn destroy_fn;
    PanelFn focus_fn;
    PanelFn focus_lost_fn;
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
Panel  *ui_find_panel       (UI *ui, const char *name);
Panel  *panel_create        (UI *ui);
Panel  *panel_next          (Panel *panel);
Panel  *panel_prev          (Panel *panel);
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

F32 ui_push_string_terminated(
    UI *ui,
    const U8 *str,
    FontAtlas *font_atlas,
    RGBA8 colour, U64 font_size,
    F32 x, F32 y, F32 max_width
);

F32 ui_push_string(
    UI *ui,
    const U8 *str, U64 length,
    FontAtlas *font_atlas,
    RGBA8 colour, U64 font_size,
    F32 x, F32 y, F32 max_width
);

typedef struct Dims {
    F32 w, h;
} Dims;

Dims ui_push_string_multiline(
    UI *ui,
    const U8 *str, U64 length,
    FontAtlas *font_atlas,
    RGBA8 colour, U64 font_size,
    F32 x, F32 y,
    F32 max_x, F32 max_y
);
