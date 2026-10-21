/* Standalone full-screen detail page used only by (u)biquity.
   Keep Playnite-only playtime/achievements off-screen: the catalog lacks them. */
static void render_ubiquity_game_details(WolodriveApp *app) {
    SDL_Renderer *r = app->renderer;
    GameItem *item = app->detail_item;
    if (!item) return;
    SDL_Color white = {246, 246, 249, 255};
    SDL_Color muted = {188, 190, 200, 255};

    fill_rect(r, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 11, 13, 20, 255);
    draw_text(app, app->font_row_title, "Obsidian",
              SCREEN_WIDTH - 255, 45, muted, 210);
    draw_text(app, app->font_small, "X", 48, 47, white, 0);

    SDL_Rect title = {170, 285, 925, 162};
    draw_fitted_text(app, app->font_ubiquity_title ?
                     app->font_ubiquity_title : app->font_hero_title,
                     item->title, title, white, title, false);

    char metadata[320];
    char year[8] = "";
    if (item->release_date[0])
        snprintf(year, sizeof(year), "%.4s", item->release_date);
    else if (item->catalog_added_at >= 10000000000000ULL)
        snprintf(year, sizeof(year), "%04u",
                 (unsigned)((item->catalog_added_at / 10000000000ULL) % 10000ULL));
    snprintf(metadata, sizeof(metadata), "%s   %s%s%s   %s",
             item->source_name[0] ? item->source_name : "Obsidian",
             year, year[0] ? "   |" : "", year[0] ? "   " : "",
             item->platform[0] ? item->platform :
                 (strncmp(item->id_code, "PPSA", 4) == 0 ? "PS5" : "PS4"));
    SDL_Rect meta_rect = {170, 468, 840, 34};
    draw_fitted_text(app, app->font_small, metadata,
                     meta_rect, white, meta_rect, false);

    int x = 170;
    x = draw_hero_metadata_pill(app, sys_format_short_badge(item->format), x, 521);
    x = draw_hero_metadata_pill(app, item->id_code, x, 521);
    char version[64];
    snprintf(version, sizeof(version), "v%s", item->version);
    x = draw_hero_metadata_pill(app, version, x, 521);
    (void)x;

    char description[MAX_TRANSLATED_DESCRIPTION_BYTES];
    description_text(app, item, description, sizeof(description));
    SDL_Rect clip = {170, 579, 770, 179};
    UiTextEntry *entry = cached_text(app, app->font_body, description, white, clip.w);
    if (entry) {
        app->detail_scroll = SDL_max(0, SDL_min(app->detail_scroll,
                                                 SDL_max(0, entry->h - clip.h)));
        SDL_Rect dest = {clip.x, clip.y - app->detail_scroll, entry->w, entry->h};
        SDL_RenderSetClipRect(r, &clip);
        SDL_RenderCopy(r, entry->texture, NULL, &dest);
        SDL_RenderSetClipRect(r, NULL);
    }

    draw_text(app, app->font_small, "Install size", 170, 785, muted, 0);
    draw_text(app, app->font_small, "Version", 350, 785, muted, 0);
    draw_text(app, app->font_small, "Status", 540, 785, muted, 0);
    char size[64];
    game_list_size(item,size);
    draw_text(app, app->font_small, size, 170, 825, white, 0);
    draw_text(app, app->font_small, item->version, 350, 825, white, 0);
    draw_text(app, app->font_small,
              item->state == STATE_INSTALLED ? "Installed" :
                  item->state == STATE_DOWNLOADING ? "Downloading" : "Available",
              540, 825, white, 0);

    const SDL_Rect actions[3] = {
        {169, 902, 301, 66}, {485, 902, 74, 66}, {574, 902, 74, 66}
    };
    const char *labels[3] = {
        item->format == FMT_CATALOG ? TR(app, TXT_GAME_DETAILS) :
            (item->state == STATE_INSTALLED ? TR(app, TXT_MOUNT) :
             TR(app, TXT_GAME_INSTALL)),
        "...", "<"
    };
    for (int i = 0; i < 3; i++) {
        SDL_Rect box = actions[i];
        fill_rect(r, box.x, box.y, box.w, box.h, 15, 16, 20, 242);
        draw_border(r, box.x, box.y, box.w, box.h, 2,
                    app->detail_selection == i ? 255 : 155,
                    app->detail_selection == i ? 61 : 155,
                    app->detail_selection == i ? 173 : 165, 255);
        SDL_Rect text = {box.x + 12, box.y + 7, box.w - 24, box.h - 14};
        draw_fitted_text(app, app->font_small, labels[i], text, white, box, true);
    }
}
