void jumplist_on_focus(Panel *jl_panel);
void jumplist_on_focus_lost(Panel *jl_panel);

Panel *jumplist_create(UI *ui) {
    Panel *jl_panel = panel_create(ui);
    Arena *arena = panel_arena(jl_panel);
    
    JumpList *jl = ARENA_ALLOC(arena, JumpList);
    *jl = (JumpList) {
        .ed_handle = PANEL_HANDLE_NULL,
        .points = ARENA_ALLOC_ARRAY(arena, JumpPoint, JUMPLIST_MAX_POINT_COUNT),
        .point_count = 0, 
        .point_idx = 0, 
    };
    
    jl_panel->data = jl;
    jl_panel->update_fn = jumplist_update;
    jl_panel->focus_fn = jumplist_on_focus;
    jl_panel->focus_lost_fn = jumplist_on_focus_lost;
    jl_panel->name = "jumplist";
    jl_panel->static_w = 100.f;
    jl_panel->dynamic_weight_w = 1.f;
    jl_panel->flags |= PanelFlag_Hidden;
    
    return jl_panel;
}

void jumplist_on_focus(Panel *jl_panel) {
    jl_panel->flags &= ~(U32)PanelFlag_Hidden;
}

void jumplist_on_focus_lost(Panel *jl_panel) {
    jl_panel->flags |= PanelFlag_Hidden;
}

void jumplist_update(Panel *jl_panel) {
    JumpList *jl = jl_panel->data;
    
    // UPDATE ------------------------------------------------------
    if (jl_panel->flags & PanelFlag_Focused) {
        U64 special_pressed = w->inputs.key_special_pressed;
        U64 pressed = w->inputs.key_pressed;
        U64 modifiers = w->inputs.modifiers;
        
        bool ctrl = is(modifiers, GLFW_MOD_CONTROL);
        bool shift = is(modifiers, GLFW_MOD_SHIFT);
        
        bool escape = is(special_pressed, special_mask(GLFW_KEY_ESCAPE));
        bool caps = is(special_pressed, special_mask(GLFW_KEY_CAPS_LOCK));
        if (escape || caps) {
            Panel *ed_panel = panel_lookup(jl_panel->ui, jl->ed_handle);
            if (ed_panel)
                panel_focus_queued(ed_panel);
        }
        
        if (is(special_pressed, special_mask(GLFW_KEY_ENTER))) {
            Panel *ed_panel = panel_lookup(jl_panel->ui, jl->ed_handle);
            if (ed_panel) {
                panel_focus_queued(ed_panel);
                
                if (jl->point_idx < jl->point_count) {
                    JumpPoint *jp = &jl->points[jl->point_idx]; 
                    Editor *ed = ed_panel->data;
                    
                    if (editor_load_filepath(ed, jp->filepath, jp->filepath_len) == 0) {
                        editor_goto_line(ed, jp->line_idx);
                    } else {
                        printf("path '%.*s' doesn't exist!\n", jp->filepath_len, jp->filepath);
                    }
                }
            }
        }
        
        if (ctrl && !shift && is(pressed, key_mask(GLFW_KEY_J))) {
            jl->point_idx++;
            if (jl->point_idx >= jl->point_count)
                jl->point_idx = 0;
        }
        
        if (ctrl && !shift && is(pressed, key_mask(GLFW_KEY_K))) {
            if (jl->point_idx == 0) {
                if (jl->point_count != 0)
                    jl->point_idx = jl->point_count - 1;
            } else {
                jl->point_idx--;
            }
        }
    }
    
    // RENDER ------------------------------------------------------
    Rect *viewport = &jl_panel->viewport;
    UI *ui = jl_panel->ui;
    F32 x = viewport->x;
    F32 y = viewport->y;
    F32 max_x = x + viewport->w;
    F32 max_y = y + viewport->h;
    F32 font_height = font_height_px[CODE_SMALL_FONT_SIZE];
    for (U32 i = 0; i < jl->point_count; ++i) {
        JumpPoint *jp = &jl->points[i];
        
        // Alloc selection background rect.
        Glyph *selection_background = NULL; 
        if (i == jl->point_idx) {
            selection_background = ui_push_glyph(ui);
            
            // We don't know the height yet, we will fill glyph_idx in later.
            *selection_background = (Glyph) {
                .x = x, .y = y,
                .colour = COLOUR_SELECT,
            };
        }
        
        // write filename
        F32 filename_width = ui_push_string(
            ui,
            jp->filepath, jp->filepath_len,
            ui->atlas,
            (RGBA8) COLOUR_RED, CODE_SMALL_FONT_SIZE,
            x, y, max_x
        );
        
        // write line number
        U8 *line_idx_str = w->frame_arena.head; 
        U64 line_idx_len = int_to_string(&w->frame_arena, jp->line_idx);
        ui_push_string(
            ui,
            line_idx_str, line_idx_len,
            ui->atlas,
            (RGBA8) COLOUR_RED, CODE_SMALL_FONT_SIZE,
            x + filename_width + 10.f, y, max_x
        );
        y += font_height;
        
        // write text
        Dims dims = ui_push_string_multiline(
            ui,
            jp->text, jp->text_len,
            ui->atlas,
            (RGBA8) COLOUR_FOREGROUND, CODE_SMALL_FONT_SIZE,
            x, y,
            max_x, max_y
        );
        y += dims.h;
        
        // write selection rect
        if (selection_background) {
            selection_background->glyph_idx = special_glyph_rect(
                (U32)viewport->w,
                (U32)(y - selection_background->y)
            );
        }
        
        // write separator bar
        *ui_push_glyph(ui) = (Glyph) {
            .x = x, .y = y,
            .glyph_idx = special_glyph_rect(
                (U32)viewport->w,
                (U32)BAR_SIZE
             ),
            .colour = COLOUR_MODE_INFO,
        };
        y += BAR_SIZE; 
    } 
}

void jumppoint_add(Panel *jl_panel, JumpPoint point) {
    JumpList *jl = jl_panel->data;
    if (jl->point_count == JUMPLIST_MAX_POINT_COUNT)
        return;
        
    // TODO: fix string leaks
    Arena *arena = jl_panel->arena;
    if (point.filepath != NULL)
        point.filepath = copy_str(arena, point.filepath, point.filepath_len);
    if (point.text != NULL)
        point.text = copy_str(arena, point.text, point.text_len);
        
    jl->points[jl->point_count++] = point;
}

void jumppoint_remove(Panel *jl_panel, U32 point_idx) {
    JumpList *jl = jl_panel->data;
    
    if (point_idx >= jl->point_count)
         return;
    
    JumpPoint *dst = &jl->points[point_idx];  
    JumpPoint *src = &jl->points[point_idx+1];  
    U32 after_count = jl->point_count - point_idx - 1;
    memmove(dst, src, after_count*sizeof(JumpPoint));
    jl->point_count--;
}
