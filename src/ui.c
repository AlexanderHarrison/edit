void    panel_set_viewport  (Panel *panel, Rect *viewport);
void    panel_update        (Panel *panel);
void    panel_destroy_inner (Panel *panel);
void    panel_destroy_single(Panel *panel);

UI *ui_create(FontAtlas *atlas, Arena *arena) { TRACE
    UI *ui = arena_alloc(arena, sizeof(UI), alignof(UI));
    arena_alloc(arena, 8ul*KB - 1, 4096);
    arena_alloc(arena, 300ul*KB, 4096);
    Panel *panel_store = arena_alloc(arena, UI_MAX_PANEL_SIZE, page_size());
    UIOp *op_queue = arena_alloc(arena, UI_MAX_OP_QUEUE_SIZE, page_size());
    Glyph *glyphs = ARENA_ALLOC_ARRAY(arena, *glyphs, MAX_GLYPHS);

    *ui = (UI) {
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

Panel *panel_next(Panel *panel) {
    Panel *next = panel->sibling_next;
    while (next) {
        if ((next->flags & PanelFlag_Hidden) == 0)
            return next;
        next = next->sibling_next;
    }
    
    Panel *prev = panel->sibling_prev;
    Panel *last_valid = NULL;
    while (prev) {
        if ((prev->flags & PanelFlag_Hidden) == 0)
            last_valid = prev;
        prev = prev->sibling_prev;
    }
    return last_valid;
}

Panel *panel_prev(Panel *panel) {
    Panel *prev = panel->sibling_prev;
    while (prev) {
        if ((prev->flags & PanelFlag_Hidden) == 0)
            return prev;
        prev = prev->sibling_prev;
    }
    
    Panel *next = panel->sibling_next;
    Panel *last_valid = NULL;
    while (next) {
        if ((next->flags & PanelFlag_Hidden) == 0)
            last_valid = next;
        next = next->sibling_next;
    }
    return last_valid;
}

void ui_update(UI *ui, Rect *viewport) { TRACE
    ui->glyph_count = 0;

    U64 pressed = w->inputs.key_pressed;
    U64 modifiers = w->inputs.modifiers;
    bool ctrl = is(modifiers, GLFW_MOD_CONTROL);

    if (ctrl && is(pressed, key_mask(GLFW_KEY_W))) {
        if (ui->focused) {
            if (!is(modifiers, GLFW_MOD_SHIFT))
                panel_focus(panel_next(ui->focused));
            else 
                panel_focus(panel_prev(ui->focused));
        }
    }

    if (ctrl && is(pressed, key_mask(GLFW_KEY_Q)))
        w->should_close = true;

    if (ctrl && is(pressed, key_mask(GLFW_KEY_P))) {
        Panel *target = ui->focused ? ui->focused : ui->root;
        Panel *new_editor = editor_create(ui, NULL);

        if ((target->flags & PanelMode_VSplit) || (target->flags & PanelMode_HSplit)) {
            // is layout panel
            panel_add_child(target, new_editor);
        } else {
            // is data panel
            panel_insert_after(target, new_editor);
        }

        panel_focus_queued(new_editor);
    }
    
    if (ctrl && is(pressed, key_mask(GLFW_KEY_M))) {
        Panel *target = ui->focused ? ui->focused : ui->root;
        Panel *mass = mass_create(ui, NULL);

        if ((target->flags & PanelMode_VSplit) || (target->flags & PanelMode_HSplit)) {
            // is layout panel
            panel_add_child(target, mass);
        } else {
            // is data panel
            panel_insert_after(target, mass);
        }

        panel_focus_queued(mass);
    }

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
            if (child->flags & PanelFlag_Hidden)
                continue;
            dynamic_children += child->dynamic_weight_w;
            dynamic_size -= child->static_w;
        }

        F32 dynamic_width = dynamic_children != 0.f ? dynamic_size / dynamic_children : 0.f;
        Rect child_viewport = panel->viewport;
        for (Panel *child = panel->child; child; child = child->sibling_next) {
            if (child->flags & PanelFlag_Hidden) {
                child_viewport.w = 0.f;
            } else {
                F32 dynamic_weight = child->dynamic_weight_w;  
                F32 width = dynamic_width * dynamic_weight + child->static_w;
                child_viewport.w = width;
            }
            panel_set_viewport(child, &child_viewport);
            child_viewport.x += child_viewport.w;
        }
    } else if (panel->flags & PanelMode_HSplit) {
        F32 dynamic_size = panel->viewport.h;
        F32 dynamic_children = 0.f;
        for (Panel *child = panel->child; child; child = child->sibling_next) {
            if (child->flags & PanelFlag_Hidden)
                continue;
            dynamic_children += child->dynamic_weight_h;
            dynamic_size -= child->static_h;
        }

        F32 dynamic_height = dynamic_children != 0.f ? dynamic_size / dynamic_children : 0.f;
        Rect child_viewport = panel->viewport;
        for (Panel *child = panel->child; child; child = child->sibling_next) {
            if (child->flags & PanelFlag_Hidden) {
                child_viewport.h = 0.f;
            } else {
                F32 dynamic_weight = child->dynamic_weight_h;  
                F32 height = dynamic_height * dynamic_weight + child->static_h;
                child_viewport.h = height;
            }
            panel_set_viewport(child, &child_viewport);
            child_viewport.y += child_viewport.h;
        }
    }
}

Panel *ui_panel_find_inner(Panel *panel, const char *name) {
    if (panel->name && strcmp(panel->name, name) == 0)
        return panel;
    
    if (panel->child) {
        Panel *ret = ui_panel_find_inner(panel->child, name);
        if (ret)
            return ret; 
    }
    
    if (panel->sibling_next) {
        Panel *ret = ui_panel_find_inner(panel->sibling_next, name);
        if (ret)
            return ret; 
    }
    
    return NULL;
}

Panel *ui_find_panel(UI *ui, const char *name) {
    return ui_panel_find_inner(ui->root, name);
}

void panel_focus(Panel *panel) { TRACE
    expect(panel->flags & PanelFlag_InUse);
    UI *ui = panel->ui;
    Panel *prev_focused = ui->focused;
    if (prev_focused)
        prev_focused->flags &= ~(U32)PanelFlag_Focused;
    if (panel)
        panel->flags |= PanelFlag_Focused;
    ui->focused = panel;
    
    if (prev_focused && prev_focused->focus_lost_fn)
        prev_focused->focus_lost_fn(prev_focused);
    if (panel && panel->focus_fn)
        panel->focus_fn(panel);
}

void panel_update(Panel *panel) { TRACE
    if (panel->update_fn)
        (panel->update_fn)(panel);

    if (panel->flags & PanelFlag_Focused) {
        *ui_push_glyph(panel->ui) = (Glyph) {
            .x = panel->viewport.x,
            .y = panel->viewport.y + panel->viewport.h - BAR_SIZE,
            .glyph_idx = special_glyph_rect((U32)panel->viewport.w, BAR_SIZE),
            .colour = COLOUR_WHITE,
        };
    }

    for (Panel *child = panel->child; child; child = child->sibling_next)
        panel_update(child);
}

Panel *panel_create(UI *ui) { TRACE
    Panel *panel = ui->free;
    ui->free = panel->sibling_next ? panel->sibling_next : panel + 1;
    U32 generation = panel->generation + 1;
    *panel = (Panel) {
        .dynamic_weight_w = 1.f,
        .dynamic_weight_h = 1.f,
        .ui = ui,
        .flags = PanelFlag_InUse,
        .generation = generation, 
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
    U32 generation = panel->generation + 1;
    *panel = (Panel) { .sibling_next = ui->free, .generation = generation };
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
    if (panel->flags & PanelFlag_Focused) {
        Panel *focus = panel_next(panel);
        if (focus == NULL) focus = panel->parent;
        if (focus == NULL) focus = panel->ui->root;
        panel_focus(focus);
    }

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

Panel *panel_lookup(UI *ui, PanelHandle handle) {
    Panel *panel = &ui->panel_store[handle.idx];
    if (panel->generation != handle.generation)
        return NULL;
    if ((panel->flags & PanelFlag_InUse) == 0)
        return NULL;
    return panel;
}

PanelHandle panel_handle(Panel *panel) {
    U32 idx = (U32)(panel - panel->ui->panel_store);
    return (PanelHandle) { idx, panel->generation }; 
}

F32 ui_push_string_terminated(
    UI *ui,
    const U8 *str,
    FontAtlas *font_atlas,
    RGBA8 colour, U64 font_size,
    F32 x, F32 y, F32 max_x
) { TRACE
    y += font_height_px[font_size] + font_atlas->descent[font_size];
    F32 width = 0.f;
    while (1) {
        U8 ch = *str;
        if (ch == 0) break;

        U32 glyph_idx = glyph_lookup_idx(font_size, ch);
        GlyphInfo info = font_atlas->glyph_info[glyph_idx];

        if (x + width + info.advance_width <= max_x) {
            ui->glyphs[ui->glyph_count++] = (Glyph) {
                .x = x + width + info.offset_x,
                .y = y + info.offset_y,
                .glyph_idx = glyph_idx,
                .colour = colour,
            };
        }
        width += info.advance_width;

        str++;
    }
    return width;
}

F32 ui_push_string(
    UI *ui,
    const U8 *str, U64 length,
    FontAtlas *font_atlas,
    RGBA8 colour, U64 font_size,
    F32 x, F32 y, F32 max_x
) { TRACE
    y += font_height_px[font_size] + font_atlas->descent[font_size];
    F32 width = 0.f;
    for (U64 i = 0; i < length; ++i) {
        U8 c = str[i];
        if (c == '\n') break; 
        U32 glyph_idx = glyph_lookup_idx(font_size, c);
        GlyphInfo info = font_atlas->glyph_info[glyph_idx];

        if (x + width + info.advance_width <= max_x) {
            *ui_push_glyph(ui) = (Glyph) {
                .x = x + width + info.offset_x,
                .y = y + info.offset_y,
                .glyph_idx = glyph_idx,
                .colour = colour,
            };
        }
        width += info.advance_width;
    }
    return width;
}

Dims ui_push_string_multiline(
    UI *ui,
    const U8 *str, U64 length,
    FontAtlas *font_atlas,
    RGBA8 colour, U64 font_size,
    F32 x, F32 y,
    F32 max_x, F32 max_y
) {
    F32 y_increment = font_height_px[font_size];

    Dims dims = {0.f, 0.f};
    while (y <= max_y) {
        F32 width = ui_push_string(
            ui, 
            str, length,
            font_atlas,
            colour, font_size,
            x, y, max_x
        );
        if (width > dims.w)
            dims.w = width;
        y += y_increment;
        dims.h += y_increment;
        
        while (1) {
            if (length == 0) return dims;
            U8 c = *str;
            str++;
            length--;
            if (c == '\n') break; 
        }
    }
    
    return dims;
}
