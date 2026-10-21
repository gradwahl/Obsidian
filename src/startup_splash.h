#ifndef OBSIDIAN_STARTUP_SPLASH_H
#define OBSIDIAN_STARTUP_SPLASH_H

/* Lightweight splash that can be drawn as soon as the SDL renderer exists.
   It deliberately uses no fonts, images, file access, catalogue or threads.
   SDL rendering is kept on the window's owning thread. */
#include <SDL.h>
#include <stdint.h>
#include <string.h>

static void startup_splash_letters(SDL_Renderer *renderer, const char *label,
                                   int x, int y, int pixel, SDL_Color color) {
    /* 5x7 pixel glyphs for "OBSIDIAN" and "LOADING" (no TTF dependency). */
    static const uint8_t glyphs[][7] = {
        {30,17,17,30,16,16,16}, /* P */
        {14,17,17,17,17,17,14}, /* O */
        {16,16,16,16,16,16,31}, /* L */
        {30,17,17,17,17,17,30}, /* D */
        {30,17,17,30,20,18,17}, /* R */
        {31,4,4,4,4,4,31},       /* I */
        {17,17,17,17,17,10,4},  /* V */
        {31,16,16,30,16,16,31}, /* E */
        {14,17,17,31,17,17,17}, /* A */
        {17,25,21,19,17,17,17}, /* N */
        {14,17,16,23,17,17,14}  /* G */
    };
    const char *alphabet = "OBSIDIANANG";
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (const char *ch = label; *ch; ++ch, x += 6 * pixel) {
        const char *matched = strchr(alphabet, *ch);
        if (!matched) continue;
        const uint8_t *glyph = glyphs[matched - alphabet];
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (!(glyph[row] & (1u << (4 - col)))) continue;
                SDL_Rect dot = {x + col * pixel, y + row * pixel, pixel, pixel};
                SDL_RenderFillRect(renderer, &dot);
            }
        }
    }
}

static void startup_splash_draw(SDL_Renderer *renderer, unsigned stage) {
    if (!renderer) return;
    if (stage > 7u) stage = 7u;
    int w = 1920, h = 1080;
    SDL_RenderGetLogicalSize(renderer, &w, &h);
    if (w <= 0 || h <= 0) SDL_GetRendererOutputSize(renderer, &w, &h);
    if (w <= 0 || h <= 0) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 9, 11, 18, 255);
    SDL_RenderClear(renderer);

    /* Brand mark, wordmark and a visibly changing spinner/progress indicator. */
    const int cx = w / 2, cy = h / 2;
    const int glyph_px = SDL_max(2, w / 640);
    int text_width = 8 * 6 * glyph_px;
    startup_splash_letters(renderer, "OBSIDIAN", cx - text_width / 2,
                           cy - 130, glyph_px, (SDL_Color){246, 247, 251, 255});
    SDL_Rect accent = {cx - text_width / 2, cy - 130 + 8 * glyph_px,
                       SDL_max(44, text_width / 4), SDL_max(3, glyph_px)};
    SDL_SetRenderDrawColor(renderer, 232, 31, 59, 255);
    SDL_RenderFillRect(renderer, &accent);

    static const int directions[][2] = {
        {0,-100}, {50,-87}, {87,-50}, {100,0}, {87,50}, {50,87},
        {0,100}, {-50,87}, {-87,50}, {-100,0}, {-87,-50}, {-50,-87}
    };
    const int radius = SDL_max(24, w / 35);
    const int phase = ((int)(SDL_GetTicks() / 80u) + (int)stage * 2) % 12;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 12; ++i) {
        const int distance = (i - phase + 12) % 12;
        Uint8 alpha = (Uint8)(46 + (11 - distance) * 17);
        SDL_SetRenderDrawColor(renderer, 243, 45, 74, alpha);
        const int x1 = cx + radius * directions[i][0] / 100;
        const int y1 = cy + radius * directions[i][1] / 100 + 65;
        const int x2 = cx + (radius + 14) * directions[i][0] / 100;
        const int y2 = cy + (radius + 14) * directions[i][1] / 100 + 65;
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }

    int loading_px = SDL_max(1, glyph_px - 1);
    startup_splash_letters(renderer, "LOADING", cx - 7 * 6 * loading_px / 2,
                           cy + radius + 120, loading_px,
                           (SDL_Color){167, 174, 190, 255});

    int bar_width = SDL_min(w - 100, 380);
    SDL_Rect background = {cx - bar_width / 2, cy + radius + 175, bar_width, 4};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 50, 55, 66, 255);
    SDL_RenderFillRect(renderer, &background);
    SDL_Rect progress = background;
    progress.w = bar_width * (int)(stage + 1) / 8;
    SDL_SetRenderDrawColor(renderer, 238, 34, 64, 255);
    SDL_RenderFillRect(renderer, &progress);
    SDL_RenderPresent(renderer);
}
#endif
