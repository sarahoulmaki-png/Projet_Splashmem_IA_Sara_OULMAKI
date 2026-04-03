#include <stdio.h>
#include <string.h>

#include "splash_game.h"

static void cleanup_app(SDL_Renderer *ren, SDL_Window *win, TTF_Font *font, TTF_Font *big_font) {
    if (big_font != font) TTF_CloseFont(big_font);
    if (font) TTF_CloseFont(font);
    TTF_Quit();

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
}

static void reset_result_state(int *finished, int *winner_id, int *winner_score, char win_msg[64]) {
    *finished = 0;
    *winner_id = 0;
    *winner_score = 0;
    win_msg[0] = '\0';
}

static void finalize_match(Player players[MAX_PLAYERS], int *winner_id, int *winner_score, char win_msg[64]) {
    int best = -1;
    int winner = 0;
    for (int i = 0; i < player_count; i++) {
        if (players[i].score > best) {
            best = players[i].score;
            winner = i + 1;
        }
    }

    *winner_id = winner;
    *winner_score = best;
    snprintf(win_msg, 64, "VICTOIRE : JOUEUR %d (%d pts)", winner, best);
}

static void process_events(
    int *running,
    int *paused,
    int *finished,
    int *winner_id,
    int *winner_score,
    char win_msg[64],
    Player players[MAX_PLAYERS],
    const SDL_Rect *btnStart,
    const SDL_Rect *btnStop,
    const SDL_Rect *btnTurbo,
    const SDL_Rect *btnQuit
) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            *running = 0;
            continue;
        }

        if (e.type != SDL_MOUSEBUTTONDOWN) {
            continue;
        }

        SDL_Point m = {e.button.x, e.button.y};

        if (SDL_PointInRect(&m, btnStart)) {
            if (*finished) {
                reset_match(players);
                reset_result_state(finished, winner_id, winner_score, win_msg);
                turbo_mode = 5;
            }
            *paused = 0;
        }
        if (SDL_PointInRect(&m, btnStop)) *paused = 1;
        if (SDL_PointInRect(&m, btnQuit)) *running = 0;
        if (SDL_PointInRect(&m, btnTurbo)) turbo_mode = (turbo_mode == 5) ? 200 : 5;
    }
}

static int run_simulation_batch(Player players[MAX_PLAYERS]) {
    int active = 0;

    for (int step = 0; step < turbo_mode; step++) {
        active = 0;

        for (int i = 0; i < player_count; i++) {
            if (players[i].credits > 0) {
                update_player(players, i);
                active = 1;
            }
        }

        for (int i = 0; i < player_count; i++) {
            if (players[i].clone_pending_turns > 0 || players[i].clone_turns_left > 0) {
                update_clone(&players[i], i + 1);
                active = 1;
            }
        }

        if (has_active_bombs()) {
            process_bombs();
            active = 1;
        }

        tick_effects(players);

        if (!any_player_can_act(players) && !has_pending_clones(players) && !has_active_bombs()) {
            active = 0;
            break;
        }
    }

    return active;
}

static SDL_Rect render_header(
    SDL_Renderer *ren,
    TTF_Font *font,
    SDL_Rect btnStart,
    SDL_Rect btnStop,
    SDL_Rect btnTurbo,
    SDL_Rect btnQuit,
    SDL_Rect areaStatus,
    int finished
) {
    draw_button(ren, font, btnStart, (SDL_Color){22, 163, 74, 255}, "DEMARRER");
    draw_button(ren, font, btnStop, (SDL_Color){14, 165, 233, 255}, "PAUSE");
    draw_button(ren, font, btnTurbo, (SDL_Color){(Uint8)(turbo_mode > 5 ? 251 : 245), (Uint8)(turbo_mode > 5 ? 146 : 158), 11, 255}, "TURBO");
    draw_button(ren, font, btnQuit, (SDL_Color){239, 68, 68, 255}, "QUITTER");

    if (!finished && turbo_mode > 5) {
        draw_text(ren, font, "[MODE TURBO ACTIF]", areaStatus, (SDL_Color){255, 100, 0, 255});
    }

    int score_cols = (player_count > 4) ? 2 : 1;
    int score_panel_w = (score_cols == 2) ? 388 : 284;
    SDL_Rect scorePanel = {GRID_SIZE * CELL_SIZE - score_panel_w - 10, 14, score_panel_w, 96};

    SDL_SetRenderDrawColor(ren, 12, 12, 12, 255);
    SDL_RenderFillRect(ren, &scorePanel);
    SDL_SetRenderDrawColor(ren, 58, 58, 58, 255);
    SDL_RenderDrawRect(ren, &scorePanel);

    return scorePanel;
}

static void render_scores(
    SDL_Renderer *ren,
    TTF_Font *font,
    SDL_Rect scorePanel,
    Player players[MAX_PLAYERS],
    SDL_Color p_colors[MAX_PLAYERS],
    int finished,
    int winner_id,
    int winner_score
) {
    Uint32 now = SDL_GetTicks();

    for (int i = 0; i < player_count; i++) {
        char info[96];
        int col = i / 4;
        int row = i % 4;
        SDL_Rect scoreArea = {scorePanel.x + 8 + (col * 190), scorePanel.y + 6 + (row * 22), 182, 20};

        if (now >= players[i].next_action_ui_update) {
            players[i].shown_action = players[i].last_action;
            players[i].next_action_ui_update = now + 250;
        }
        if (now < players[i].highlight_until) {
            players[i].shown_action = players[i].highlighted_action;
        }

        if (finished && winner_id == i + 1) {
            SDL_SetRenderDrawColor(ren, 255, 215, 0, 255);
            SDL_RenderFillRect(ren, &scoreArea);
            snprintf(info, sizeof(info), "P%d %d|%d|%s", i + 1, winner_score, players[i].credits, action_label(players[i].shown_action));
            draw_text(ren, font, info, scoreArea, (SDL_Color){20, 20, 20, 255});
        } else {
            snprintf(info, sizeof(info), "P%d %d|%d|%s", i + 1, players[i].score, players[i].credits, action_label(players[i].shown_action));
            draw_text(ren, font, info, scoreArea, p_colors[i]);
        }
    }
}

static void render_grid(SDL_Renderer *ren, SDL_Color p_colors[MAX_PLAYERS], int cur_scores[MAX_PLAYERS]) {
    for (int i = 0; i < MAX_PLAYERS; i++) cur_scores[i] = 0;

    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            if (grid[y][x] <= 0) continue;

            int owner = grid[y][x] - 1;
            if (owner < 0 || owner >= player_count) continue;

            cur_scores[owner]++;

            SDL_Color base = p_colors[owner];
            int checker = ((x + y) & 1) ? 14 : -10;
            Uint8 r1 = clamp_u8((base.r * 3) / 4 + checker);
            Uint8 g1 = clamp_u8((base.g * 3) / 4 + checker);
            Uint8 b1 = clamp_u8((base.b * 3) / 4 + checker);
            Uint8 r2 = clamp_u8(base.r + 26 + checker / 2);
            Uint8 g2 = clamp_u8(base.g + 26 + checker / 2);
            Uint8 b2 = clamp_u8(base.b + 26 + checker / 2);

            SDL_Rect r = {x * CELL_SIZE, y * CELL_SIZE + UI_HEIGHT, CELL_SIZE, CELL_SIZE};

            SDL_SetRenderDrawColor(ren, r1, g1, b1, 255);
            SDL_RenderFillRect(ren, &r);

            if (CELL_SIZE > 2) {
                SDL_Rect inner = {r.x + 1, r.y + 1, CELL_SIZE - 2, CELL_SIZE - 2};
                SDL_SetRenderDrawColor(ren, r2, g2, b2, 255);
                SDL_RenderFillRect(ren, &inner);
            }

            SDL_SetRenderDrawColor(ren, clamp_u8(r1 - 28), clamp_u8(g1 - 28), clamp_u8(b1 - 28), 255);
            SDL_RenderDrawRect(ren, &r);
        }
    }
}

static void update_scores(Player players[MAX_PLAYERS], int cur_scores[MAX_PLAYERS]) {
    for (int i = 0; i < player_count; i++) {
        players[i].score = cur_scores[i];
    }
}

static void render_player_markers(SDL_Renderer *ren, Player players[MAX_PLAYERS], SDL_Color p_colors[MAX_PLAYERS]) {
    for (int i = 0; i < player_count; i++) {
        int px = players[i].x * CELL_SIZE;
        int py = players[i].y * CELL_SIZE + UI_HEIGHT;
        int blink = ((SDL_GetTicks() / 180) % 2);

        SDL_Rect outer = {px - 2, py - 2, CELL_SIZE + 4, CELL_SIZE + 4};
        SDL_Rect inner = {px - 1, py - 1, CELL_SIZE + 2, CELL_SIZE + 2};

        if (blink) SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        else SDL_SetRenderDrawColor(ren, p_colors[i].r, p_colors[i].g, p_colors[i].b, 255);
        SDL_RenderDrawRect(ren, &outer);
        SDL_RenderDrawRect(ren, &inner);

        if (players[i].clone_pending_turns == 0 && players[i].clone_turns_left > 0) {
            int cx = players[i].clone_x * CELL_SIZE;
            int cy = players[i].clone_y * CELL_SIZE + UI_HEIGHT;
            SDL_Rect c = {cx + 2, cy + 2, CELL_SIZE - 4, CELL_SIZE - 4};
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 200);
            SDL_RenderDrawRect(ren, &c);
        }
    }
}

static void render_winner_banner(
    SDL_Renderer *ren,
    TTF_Font *big_font,
    int finished,
    int winner_id,
    const char win_msg[64],
    SDL_Color p_colors[MAX_PLAYERS]
) {
    if (!finished) return;

    SDL_Rect centerBox = {80, (GRID_SIZE * CELL_SIZE + UI_HEIGHT) / 2 - 36, GRID_SIZE * CELL_SIZE - 160, 72};
    SDL_SetRenderDrawColor(ren, 245, 190, 40, 255);
    SDL_RenderFillRect(ren, &centerBox);

    SDL_Rect centerInner = {centerBox.x + 3, centerBox.y + 3, centerBox.w - 6, centerBox.h - 6};
    SDL_SetRenderDrawColor(ren, 35, 35, 35, 255);
    SDL_RenderFillRect(ren, &centerInner);

    if (winner_id > 0) draw_text(ren, big_font, win_msg, centerInner, p_colors[winner_id - 1]);
    else draw_text(ren, big_font, win_msg, centerInner, (SDL_Color){255, 255, 0, 255});
}

int main(int argc, char *argv[]) {
    if (argc < 5 || argc > MAX_PLAYERS + 1) {
        return printf("Usage: ./splash p1.so|p1.txt p2.so|p2.txt p3.so|p3.txt p4.so|p4.txt [p5..p8]\n"), 1;
    }

    player_count = argc - 1;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    TTF_Init();

    TTF_Font *font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14);
    if (!font) font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 14);
    TTF_Font *big_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 24);
    if (!big_font) big_font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial Bold.ttf", 24);
    if (!big_font) big_font = font;

    SDL_Window *win = SDL_CreateWindow(
        "Splashmem IA",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        GRID_SIZE * CELL_SIZE,
        GRID_SIZE * CELL_SIZE + UI_HEIGHT,
        0
    );
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    SDL_Rect btnStart = {10, 30, 95, 35};
    SDL_Rect btnStop = {110, 30, 95, 35};
    SDL_Rect btnTurbo = {210, 30, 95, 35};
    SDL_Rect btnQuit = {310, 30, 95, 35};
    SDL_Rect areaStatus = {10, 73, 350, 30};

    Player players[MAX_PLAYERS];
    memset(players, 0, sizeof(players));

    SDL_Color p_colors[MAX_PLAYERS] = {
        {255, 50, 50, 255},
        {50, 255, 50, 255},
        {50, 150, 255, 255},
        {255, 255, 50, 255},
        {255, 120, 40, 255},
        {180, 80, 255, 255},
        {40, 220, 200, 255},
        {255, 120, 180, 255}
    };

    for (int i = 0; i < player_count; i++) {
        char err[256] = {0};
        if (!load_player_source(&players[i], argv[i + 1], err, sizeof(err))) {
            fprintf(stderr, "Erreur chargement joueur %d (%s): %s\n", i + 1, argv[i + 1], err[0] ? err : "source invalide");
            release_players(players, i + 1);
            cleanup_app(ren, win, font, big_font);
            return 1;
        }
    }

    reset_match(players);

    int running = 1;
    int paused = 1;
    int finished = 0;
    char win_msg[64] = "";
    int winner_id = 0;
    int winner_score = 0;

    while (running) {
        process_events(
            &running,
            &paused,
            &finished,
            &winner_id,
            &winner_score,
            win_msg,
            players,
            &btnStart,
            &btnStop,
            &btnTurbo,
            &btnQuit
        );

        if (!paused && !finished) {
            if (!run_simulation_batch(players)) {
                finished = 1;
                paused = 1;
                finalize_match(players, &winner_id, &winner_score, win_msg);
            }
        }

        SDL_SetRenderDrawColor(ren, 20, 20, 20, 255);
        SDL_RenderClear(ren);

        SDL_Rect scorePanel = render_header(ren, font, btnStart, btnStop, btnTurbo, btnQuit, areaStatus, finished);
        int cur_scores[MAX_PLAYERS];

        render_scores(ren, font, scorePanel, players, p_colors, finished, winner_id, winner_score);
        render_grid(ren, p_colors, cur_scores);
        update_scores(players, cur_scores);
        render_player_markers(ren, players, p_colors);
        render_winner_banner(ren, big_font, finished, winner_id, win_msg, p_colors);

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    release_players(players, player_count);
    cleanup_app(ren, win, font, big_font);

    return 0;
}
