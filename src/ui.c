typedef struct Panel Panel;
typedef struct UI UI;

// TODO switch to handles

enum PanelFlags {
    PanelMode_VSplit = (1u << 0),
    PanelMode_HSplit = (1u << 1),

    PanelFlag_InUse = (1u << 2),
    PanelFlag_Focused = (1u << 3),
};

typedef void (*PanelFn)(Panel *panel);

typedef struct Panel {
    // modify these
    void *data;
    F32 static_w;
    F32 static_h;
    F32 dynamic_weight_w;
    F32 dynamic_weight_h;
    PanelFn update_fn;
    PanelFn destroy_fn;

    // please do not modify these
    Arena *arena;
    Rect viewport;
    Panel *parent;
    Panel *child;
    Panel *sibling_prev;
    Panel *sibling_next;
    U32 flags;
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

// external
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
Arena  *panel_arena         (Panel *panel);

// returns number of glyphs written
U64 write_string_terminated(
    Glyph *glyphs,
    U8 *str,
    FontAtlas *font_atlas,
    RGBA8 colour, U64 font_size,
    F32 x, F32 y, F32 max_width
);

U64 write_string(
    Glyph *glyphs,
    U8 *str, U64 length,
    FontAtlas *font_atlas,
    RGBA8 colour, U64 font_size,
    F32 x, F32 y, F32 max_width
);

// internal
void    panel_set_viewport  (Panel *panel, Rect *viewport);
void    panel_update        (Panel *panel);
void    panel_destroy_inner (Panel *panel);
void    panel_destroy_single(Panel *panel);

UI *ui_create(W *w, FontAtlas *atlas, Arena *arena) { TRACE
    UI *ui = arena_alloc(arena, sizeof(UI), alignof(UI));
    arena_alloc(arena, 8ul*KB - 1, 4096);
    arena_alloc(arena, 300ul*KB, 4096);
    Panel *panel_store = arena_alloc(arena, UI_MAX_PANEL_SIZE, page_size());
    UIOp *op_queue = arena_alloc(arena, UI_MAX_OP_QUEUE_SIZE, page_size());
    Glyph *glyphs = ARENA_ALLOC_ARRAY(arena, *glyphs, MAX_GLYPHS);

    *ui = (UI) {
        .w = w,
        .atlas = atlas,
        .panel_store = panel_store,
        .free = panel_store,
        .glyphs = glyphs,
        .glyph_count = 0,
        .op_queue = op_queue,
        .op_count = 0,
    };
    return ui;
}

void ui_destroy(UI *ui) { TRACE
    return;
    Panel *end = ui->free;
    while (end->sibling_next)
        end = end->sibling_next;

    U64 panel_count = (U64)(end - ui->panel_store);

    for (U64 i = 0; i < panel_count; ++i) {
        Panel *panel = &ui->panel_store[i];
        if ((panel->flags & PanelFlag_InUse) != 0)
            panel_destroy_single(panel);
    }

    memset(ui->panel_store, 0, panel_count * sizeof(Panel));
    ui->free = ui->panel_store;
}

void ui_update(UI *ui, Rect *viewport) { TRACE
    ui->glyph_count = 0;
    if (ui->root) {
        panel_set_viewport(ui->root, viewport);
        panel_update(ui->root);
    }
}

UIOp *ui_push_op(UI *ui) { TRACE
    return &ui->op_queue[ui->op_count++];
}

void ui_flush_ops(UI *ui) { TRACE
    for (U64 i = 0; i < ui->op_count; ++i) {
        UIOp *op = &ui->op_queue[i];

        switch (op->tag) {
            case UIOp_PanelDestroy:
                panel_destroy(op->panel);
                break;
            case UIOp_PanelFocus:
                panel_focus(op->panel);
                break;
            case UIOp_PanelInsertBefore:
                panel_insert_before(op->panel_source, op->panel);
                break;
            case UIOp_PanelInsertAfter:
                panel_insert_after(op->panel_source, op->panel);
                break;
            case UIOp_PanelAddChild:
                panel_add_child(op->panel_source, op->panel);
                break;
        }
    }
    ui->op_count = 0;
}

void panel_destroy_queued(Panel *panel) { TRACE
    *ui_push_op(panel->ui) = (UIOp) {
        .tag = UIOp_PanelDestroy,
        .panel = panel,
    };
}

Glyph *ui_push_glyph(UI *ui) {
    return &ui->glyphs[ui->glyph_count++];
}

Arena *panel_arena(Panel *panel) {
    if (panel->arena == NULL) {
        Arena arena = arena_create_sized(1ull * GB);
        Arena *arena_ptr = arena_alloc(&arena, sizeof(Arena), alignof(Arena));
        *arena_ptr = arena;
        panel->arena = arena_ptr;
    }

    return panel->arena;
}

void panel_set_viewport(Panel *panel, Rect *viewport) { TRACE
    panel->viewport = *viewport;

    if (panel->flags & PanelMode_VSplit) {
        F32 dynamic_size = panel->viewport.w;
        F32 dynamic_children = 0.f;
        for (Panel *child = panel->child; child; child = child->sibling_next) {
            dynamic_children += child->dynamic_weight_w;
            dynamic_size -= child->static_w;
        }

        F32 dynamic_width = dynamic_size / dynamic_children;
        Rect child_viewport = panel->viewport;
        for (Panel *child = panel->child; child; child = child->sibling_next) {
            F32 width = dynamic_width * child->dynamic_weight_w + child->static_w;
            child_viewport.w = width;
            panel_set_viewport(child, &child_viewport);
            child_viewport.x += width;
        }
    } else if (panel->flags & PanelMode_HSplit) {
        F32 dynamic_size = panel->viewport.h;
        F32 dynamic_children = 0.f;
        for (Panel *child = panel->child; child; child = child->sibling_next) {
            dynamic_children += child->dynamic_weight_h;
            dynamic_size -= child->static_h;
        }

        F32 dynamic_height = dynamic_size / dynamic_children;
        Rect child_viewport = panel->viewport;
        for (Panel *child = panel->child; child; child = child->sibling_next) {
            F32 height = dynamic_height * child->dynamic_weight_h + child->static_h;
            child_viewport.h = height;
            panel_set_viewport(child, &child_viewport);
            child_viewport.y += height;
        }
    }
}

void panel_focus(Panel *panel) { TRACE
    expect(panel->flags & PanelFlag_InUse);

    UI *ui = panel->ui;
    if (ui->focused)
        ui->focused->flags &= ~(U32)PanelFlag_Focused;
    panel->flags |= PanelFlag_Focused;
    ui->focused = panel;
}

void panel_update(Panel *panel) { TRACE
    if (panel->update_fn)
        (panel->update_fn)(panel);

    for (Panel *child = panel->child; child; child = child->sibling_next)
        panel_update(child);
}

Panel *panel_create(UI *ui) { TRACE
    Panel *panel = ui->free;
    ui->free = panel->sibling_next ? panel->sibling_next : panel + 1;
    *panel = (Panel) {
        .dynamic_weight_w = 1.f,
        .dynamic_weight_h = 1.f,
        .ui = ui,
        .flags = PanelFlag_InUse,
    };
    return panel;
}

// Calls destructors for this single panel and does not detach
void panel_destroy_single(Panel *panel) { TRACE
    if (panel->destroy_fn)
        (panel->destroy_fn)(panel);

    if (panel->arena)
        arena_destroy(panel->arena);

    UI *ui = panel->ui;
    *panel = (Panel) { .sibling_next = ui->free };
    ui->free = panel;
    
    if (ui->focused == panel)
        ui->focused = NULL;
}

// Calls destructors recursively and does not detach
void panel_destroy_inner(Panel *panel) { TRACE
    for (Panel *child = panel->child; child; child = child->sibling_next)
        panel_destroy_inner(child);
    panel_destroy_single(panel);
}

// Detaches and calls destructors recursively
void panel_destroy(Panel *panel) { TRACE
    panel_detach(panel);
    panel_destroy_inner(panel);
}

void panel_detach(Panel *panel) { TRACE
    Panel *prev = panel->sibling_prev;
    Panel *next = panel->sibling_next;
    Panel *parent = panel->parent;

    if (next)
        next->sibling_prev = prev;
    if (prev)
        prev->sibling_next = next;
    else if (parent)
        parent->child = next;

    panel->parent = NULL;
    panel->sibling_next = NULL;
    panel->sibling_prev = NULL;
}

void panel_add_child(Panel *parent, Panel *new) { TRACE
    if (parent->child == NULL) {
        parent->child = new;
    } else {
        Panel *last_child = parent->child;
        while (last_child->sibling_next)
            last_child = last_child->sibling_next;
        panel_insert_after(last_child, new);
    }
    new->parent = parent;
}

void panel_insert_after(Panel *old, Panel *new) { TRACE
    expect(old->ui == new->ui);
    expect(new->parent == NULL);
    expect(old->parent != NULL);

    if (old->sibling_next)
        old->sibling_next->sibling_prev = new;
    new->sibling_next = old->sibling_next;
    old->sibling_next = new;
    new->sibling_prev = old;
    new->parent = old->parent;
}

void panel_insert_before(Panel *old, Panel *new) { TRACE
    expect(old->ui == new->ui);
    expect(new->parent == NULL);
    expect(old->parent != NULL);

    if (old->sibling_prev)
        old->sibling_prev->sibling_next = new;
    else if (old->parent)
        old->parent->child = new;
    new->sibling_prev = old->sibling_prev;
    old->sibling_prev = new;
    new->sibling_next = old;
    new->parent = old->parent;
}

void panel_focus_queued(Panel *panel) { TRACE
    *ui_push_op(panel->ui) = (UIOp) {
        .tag = UIOp_PanelFocus,
        .panel = panel,
    };
}

void panel_insert_after_queued(Panel *old, Panel *new) { TRACE
    *ui_push_op(old->ui) = (UIOp) {
        .tag = UIOp_PanelInsertAfter,
        .panel = new,
        .panel_source = old,
    };
}

void panel_insert_before_queued(Panel *old, Panel *new) { TRACE
    *ui_push_op(old->ui) = (UIOp) {
        .tag = UIOp_PanelInsertBefore,
        .panel = new,
        .panel_source = old,
    };
}

void panel_add_child_queued(Panel *parent, Panel *new) { TRACE
    *ui_push_op(parent->ui) = (UIOp) {
        .tag = UIOp_PanelAddChild,
        .panel = new,
        .panel_source = parent,
    };
}

// returns number of glyphs written
U64 write_string_terminated(
    Glyph *glyphs,
    U8 *str,
    FontAtlas *font_atlas,
    RGBA8 colour, U64 font_size,
    F32 x, F32 y, F32 max_x
) { TRACE
    U64 glyphs_written = 0;
    while (1) {
        U8 ch = *str;
        if (ch == 0) break;

        U32 glyph_idx = glyph_lookup_idx(font_size, ch);
        GlyphInfo info = font_atlas->glyph_info[glyph_idx];

        if (x + info.advance_width > max_x) break;

        glyphs[glyphs_written++] = (Glyph) {
            .x = x + info.offset_x,
            .y = y + info.offset_y,
            .glyph_idx = glyph_idx,
            .colour = colour,
        };
        x += info.advance_width;

        str++;
    }
    return glyphs_written;
}

U64 write_string(
    Glyph *glyphs,
    U8 *str, U64 length,
    FontAtlas *font_atlas,
    RGBA8 colour, U64 font_size,
    F32 x, F32 y, F32 max_width
) { TRACE
    U64 glyphs_written = 0;
    for (U64 i = 0; i < length; ++i) {
        U32 glyph_idx = glyph_lookup_idx(font_size, str[i]);
        GlyphInfo info = font_atlas->glyph_info[glyph_idx];

        if (x + info.advance_width > max_width) break;

        glyphs[glyphs_written++] = (Glyph) {
            .x = x + info.offset_x,
            .y = y + info.offset_y,
            .glyph_idx = glyph_idx,
            .colour = colour,
        };
        x += info.advance_width;
    }
    return glyphs_written;
}
