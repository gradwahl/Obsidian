import re

print("Patching files...")

# 1. Update HomeView.tsx
with open('src/components/HomeView.tsx', 'r') as f:
    hv = f.read()

if 'tv-focus-pill' not in hv:
    cat_old = '                  <div\n                    className="tv-carousel tv-category-carousel"\n                    ref={(el) => {\n                      rowRefs.current[catIdx] = el;\n                    }}\n                  >'
    cat_new = '                  <div\n                    className="tv-carousel tv-category-carousel"\n                    ref={(el) => {\n                      rowRefs.current[catIdx] = el;\n                    }}\n                  >\n                    {isCatActive && !sidebarFocused && cat.games.length > 0 && (\n                      <div\n                        className="tv-focus-pill"\n                        style={{\n                          transform: `translate3d(${colIdx * (TV_LAYOUT.cardWidth + TV_LAYOUT.cardGap)}px, 0, 0)`,\n                        }}\n                      />\n                    )}'
    hv = hv.replace(cat_old, cat_new)

    sing_old = '{games.length ? (\n            <div className="tv-carousel" ref={carousel}>'
    sing_new = '{games.length ? (\n            <div className="tv-carousel" ref={carousel}>\n              {!sidebarFocused && games.length > 0 && (\n                <div\n                  className="tv-focus-pill"\n                  style={{\n                    transform: `translate3d(${selectedIndex * (TV_LAYOUT.cardWidth + TV_LAYOUT.cardGap)}px, 0, 0)`,\n                  }}\n                />\n              )}'
    hv = hv.replace(sing_old, sing_new)

    with open('src/components/HomeView.tsx', 'w') as f:
        f.write(hv)
    print("HomeView.tsx patched.")

# 2. Update wolodrive.h
with open('src/wolodrive.h', 'r') as f:
    wh = f.read()

if 'float focus_pill_x;' not in wh:
    wh = wh.replace('float scroll_velocity;\n} CarouselRow;', 'float scroll_velocity;\n    float focus_pill_x;\n} CarouselRow;')
    with open('src/wolodrive.h', 'w') as f:
        f.write(wh)
    print("wolodrive.h patched.")

# 3. Update ui_netflix.c
with open('src/ui_netflix.c', 'r') as f:
    un = f.read()

un = un.replace('int y = app->ui_theme == UI_THEME_UBIQUITY ? -500 : 470, h = 50;', 'int y = 470, h = 50;')

anim_old = '        row->target_scroll_x = row->selected_col * card_stride;'
anim_new = '        row->target_scroll_x = row->selected_col * card_stride;\n        float target_pill_x = CONTENT_X + row->selected_col * card_stride - row->scroll_x;\n        if (row->focus_pill_x == 0.0f) row->focus_pill_x = target_pill_x;\n        row->focus_pill_x += (target_pill_x - row->focus_pill_x) * (1.0f - expf(-16.0f * dt));\n        if (fabsf(target_pill_x - row->focus_pill_x) < 0.5f) row->focus_pill_x = target_pill_x;'

un = un.replace(anim_old, anim_new)

un = un.replace('SDL_Rect home_clip = {RAIL_WIDTH + 12, CARDS_Y - 50,\n                              SCREEN_WIDTH - RAIL_WIDTH - 24, 950 - (CARDS_Y - 50)};', 'SDL_Rect home_clip = {CONTENT_X - 28, CARDS_Y - 80,\n                              SCREEN_WIDTH - (CONTENT_X - 28), 980 - (CARDS_Y - 80)};')

un = un.replace('SDL_Rect row_clip = {RAIL_WIDTH + 20, SDL_max(home_clip.y, row_y - 14),\n                                 SCREEN_WIDTH - RAIL_WIDTH - 20, CARD_HEIGHT + CARD_TITLE + CARD_FOOTER + 28};', 'SDL_Rect row_clip = {CONTENT_X - 28, SDL_max(home_clip.y, row_y - 32),\n                                 SCREEN_WIDTH - (CONTENT_X - 28), CARD_HEIGHT + CARD_TITLE + CARD_FOOTER + 64};')

cat_loop_old = 'if (!cat_row->item_count) continue;\n            SDL_Rect row_clip = {CONTENT_X - 28, SDL_max(home_clip.y, row_y - 32),\n                                 SCREEN_WIDTH - (CONTENT_X - 28), CARD_HEIGHT + CARD_TITLE + CARD_FOOTER + 64};\n            SDL_RenderSetClipRect(r, &row_clip);'

cat_loop_new = cat_loop_old + '\n            if (is_active_cat && !app->sidebar_focus && cat_row->item_count > 0) {\n                int full_card_h = CARD_HEIGHT + CARD_TITLE + CARD_FOOTER;\n                int scaled_w = (int)lroundf((float)CARD_WIDTH * 1.08f);\n                int scaled_h = (int)lroundf((float)full_card_h * 1.08f);\n                int pill_x = (int)cat_row->focus_pill_x - 14;\n                int pill_y = row_y - 4 - (scaled_h - full_card_h) / 2 - 6;\n                int pill_w = scaled_w + 12;\n                int pill_h = scaled_h + 12;\n                draw_rounded_glow(r, pill_x, pill_y, pill_w, pill_h, 26, 12, 229, 9, 20, 140);\n                draw_rounded_border(r, pill_x, pill_y, pill_w, pill_h, 26, 2, 255, 56, 68, 255);\n                draw_rounded_border(r, pill_x + 1, pill_y + 1, pill_w - 2, pill_h - 2, 25, 1, 229, 9, 20, 240);\n                draw_rounded_border(r, pill_x + 2, pill_y + 2, pill_w - 4, pill_h - 4, 24, 1, 145, 4, 11, 220);\n            }'

un = un.replace(cat_loop_old, cat_loop_new)

un = un.replace('SDL_Rect clip = {RAIL_WIDTH + 20, CARDS_Y - 14, SCREEN_WIDTH - RAIL_WIDTH - 32,\n                     950 - (CARDS_Y - 14)};', 'SDL_Rect clip = {CONTENT_X - 28, CARDS_Y - 32, SCREEN_WIDTH - (CONTENT_X - 28),\n                     980 - (CARDS_Y - 32)};')

sing_loop_old = 'if (!row->item_count) {\n        return;\n    }\n    SDL_Rect clip = {CONTENT_X - 28, CARDS_Y - 32, SCREEN_WIDTH - (CONTENT_X - 28),\n                     980 - (CARDS_Y - 32)};\n    SDL_RenderSetClipRect(r, &clip);'

sing_loop_new = sing_loop_old + '\n    if (!app->sidebar_focus && row->item_count > 0) {\n        int full_card_h = CARD_HEIGHT + CARD_TITLE + CARD_FOOTER;\n        int scaled_w = (int)lroundf((float)CARD_WIDTH * 1.08f);\n        int scaled_h = (int)lroundf((float)full_card_h * 1.08f);\n        int pill_x = (int)row->focus_pill_x - 14;\n        int pill_y = CARDS_Y - 4 - (scaled_h - full_card_h) / 2 - 6;\n        int pill_w = scaled_w + 12;\n        int pill_h = scaled_h + 12;\n        draw_rounded_glow(r, pill_x, pill_y, pill_w, pill_h, 26, 12, 229, 9, 20, 140);\n        draw_rounded_border(r, pill_x, pill_y, pill_w, pill_h, 26, 2, 255, 56, 68, 255);\n        draw_rounded_border(r, pill_x + 1, pill_y + 1, pill_w - 2, pill_h - 2, 25, 1, 229, 9, 20, 240);\n        draw_rounded_border(r, pill_x + 2, pill_y + 2, pill_w - 4, pill_h - 4, 24, 1, 145, 4, 11, 220);\n    }'

un = un.replace(sing_loop_old, sing_loop_new)

old_scale = '''    if (selected && !app->sidebar_focus) {
        float scale = app->card_focus_scale > 1.0f ? app->card_focus_scale : 1.05f;
        int new_w = (int)lroundf((float)base_card.w * scale);
        int new_h = (int)lroundf((float)CARD_HEIGHT * scale);
        card.x = base_card.x - (new_w - base_card.w) / 2;
        card.y = base_card.y - (new_h - CARD_HEIGHT) / 2 - 6;
        card.w = new_w;
        card.h = new_h;
    } else {
        card.h = CARD_HEIGHT;
    }'''

new_scale = '''    int full_card_h = CARD_HEIGHT + CARD_TITLE + CARD_FOOTER;
    if (selected && !app->sidebar_focus) {
        float scale = 1.08f;
        int new_w = (int)lroundf((float)base_card.w * scale);
        int new_h = (int)lroundf((float)CARD_HEIGHT * scale);
        card.x = base_card.x - (new_w - base_card.w) / 2;
        card.y = base_card.y - 4 - (new_h - CARD_HEIGHT) / 2;
        card.w = new_w;
        card.h = new_h;
    } else {
        float scale = 0.92f;
        int new_w = (int)lroundf((float)base_card.w * scale);
        int new_h = (int)lroundf((float)CARD_HEIGHT * scale);
        card.x = base_card.x + (base_card.w - new_w) / 2;
        card.y = base_card.y + (CARD_HEIGHT - new_h) / 2;
        card.w = new_w;
        card.h = new_h;
    }'''

un = un.replace(old_scale, new_scale)

old_out = '''        if (selected && !app->sidebar_focus) {
            draw_rounded_glow(r, card.x - 3, card.y - 3, card.w + 6, full_h + 6,
                              radius + 3, 7, 229, 9, 20, 110);
            draw_rounded_border(r, card.x - 3, card.y - 3, card.w + 6, full_h + 6,
                                radius + 3, 3, 245, 245, 250, 255);
            draw_rounded_border(r, card.x, card.y, card.w, full_h,
                                radius, 1, 229, 9, 20, 215);
        }'''

new_out = '''        if (selected && !app->sidebar_focus) {
            /* Handled by smooth floating focus pill */
        }'''

un = un.replace(old_out, new_out)

with open('src/ui_netflix.c', 'w') as f:
    f.write(un)
print("ui_netflix.c patched.")
