#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "actions.h"

// Reglages generaux du jeu
#define GRID_SIZE 100
#define CELL_SIZE 8
#define MAX_PLAYERS 4
#define INITIAL_CREDITS 9000
#define UI_HEIGHT 120 

// Infos qu'on garde pour chaque joueur.
typedef struct {
    void *handle;
    char (*get_action)();
    int x, y;
    int credits;
    int score;
    char last_action;
    char shown_action;
    Uint32 next_action_ui_update;
    char highlighted_action;
    Uint32 highlight_until;
} Player;

int grid[GRID_SIZE][GRID_SIZE] = {0};
int turbo_mode = 5;

static Uint8 clamp_u8(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (Uint8)value;
}

// Convertit une action en texte lisible dans l'UI.
const char *action_label(char action) {
    switch (action) {
        case ACTION_MOVE_L:
        case ACTION_MOVE_R:
        case ACTION_MOVE_U:
        case ACTION_MOVE_D:
            return "MOVE";
        case ACTION_DASH_L:
        case ACTION_DASH_R:
        case ACTION_DASH_U:
        case ACTION_DASH_D:
            return "DASH";
        case ACTION_TELEPORT_L:
        case ACTION_TELEPORT_R:
        case ACTION_TELEPORT_U:
        case ACTION_TELEPORT_D:
            return "TELEPORT";
        default:
            return "STILL";
    }
}

// Applique l'action du joueur, puis met a jour sa case sur la grille.
void update_player(Player *p, int id) {
    if (p->credits <= 0) {
        p->last_action = ACTION_STILL;
        return;
    }

    char act = p->get_action();
    p->last_action = act;

    if ((act >= ACTION_DASH_L && act <= ACTION_DASH_D) ||
        (act >= ACTION_TELEPORT_L && act <= ACTION_TELEPORT_D)) {
        p->highlighted_action = act;
        p->highlight_until = SDL_GetTicks() + 900;
    }

    int dx = 0, dy = 0, cost = 1;
    switch (act) {
        case ACTION_MOVE_L: dx = -1; break;
        case ACTION_MOVE_R: dx = 1;  break;
        case ACTION_MOVE_U: dy = -1; break;
        case ACTION_MOVE_D: dy = 1;  break;
        case ACTION_STILL: break;
        case ACTION_DASH_L: dx = -8; cost = 10; break;
        case ACTION_DASH_R: dx = 8;  cost = 10; break;
        case ACTION_DASH_U: dy = -8; cost = 10; break;
        case ACTION_DASH_D: dy = 8;  cost = 10; break;
        case ACTION_TELEPORT_L: dx = -8; cost = 2; break;
        case ACTION_TELEPORT_R: dx = 8;  cost = 2; break;
        case ACTION_TELEPORT_U: dy = -8; cost = 2; break;
        case ACTION_TELEPORT_D: dy = 8;  cost = 2; break;
        default: cost = 1; break;
    }
    if (p->credits >= cost) {
        p->credits -= cost;
        p->x = (p->x + dx + GRID_SIZE) % GRID_SIZE;
        p->y = (p->y + dy + GRID_SIZE) % GRID_SIZE;
        grid[p->y][p->x] = id;
    }
}

// Affiche un texte centre dans une zone.
void draw_text(SDL_Renderer *ren, TTF_Font *font, const char *txt, SDL_Rect area, SDL_Color color) {
    if (!font || !txt) return;
    SDL_Surface *s = TTF_RenderText_Blended(font, txt, color);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    SDL_Rect dst = { area.x + (area.w - s->w)/2, area.y + (area.h - s->h)/2, s->w, s->h };
    SDL_RenderCopy(ren, t, NULL, &dst);
    SDL_FreeSurface(s); SDL_DestroyTexture(t);
}

// Reinitialise toute la partie pour rejouer sans relancer l'application.
void reset_match(Player players[MAX_PLAYERS]) {
    memset(grid, 0, sizeof(grid));
    for (int i = 0; i < MAX_PLAYERS; i++) {
        players[i].credits = INITIAL_CREDITS;
        players[i].score = 0;
        players[i].last_action = ACTION_STILL;
        players[i].shown_action = ACTION_STILL;
        players[i].next_action_ui_update = 0;
        players[i].highlighted_action = ACTION_STILL;
        players[i].highlight_until = 0;
        players[i].x = i * 25;
        players[i].y = 50;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 5) return printf("Usage: ./splash p1.so p2.so p3.so p4.so\n"), 1;

    // On initialise SDL (video + audio).
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    TTF_Init();
    
    // ...gestion musique supprimée...

    TTF_Font *font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14);
    if(!font) font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 14);
    TTF_Font *big_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 24);
    if(!big_font) big_font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial Bold.ttf", 24);
    if(!big_font) big_font = font;

    SDL_Window *win = SDL_CreateWindow("Splashmem IA", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                        GRID_SIZE * CELL_SIZE, GRID_SIZE * CELL_SIZE + UI_HEIGHT, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    // Zones cliquables des boutons.
    SDL_Rect btnStart = { 10, 30, 95, 35 }, btnStop = { 110, 30, 95, 35 }, 
             btnTurbo = { 210, 30, 95, 35 }, btnQuit = { 310, 30, 95, 35 };
    SDL_Rect areaStatus = { 10, 73, 350, 30 }; 

    Player players[MAX_PLAYERS];
    SDL_Color p_colors[] = {{255,50,50,255}, {50,255,50,255}, {50,150,255,255}, {255,255,50,255}};
    
    // On charge les IA et on place les joueurs au depart.
    for (int i = 0; i < MAX_PLAYERS; i++) {
        const char *requested_path = argv[i + 1];
        const char *loaded_path = requested_path;
        const char *load_err = NULL;

        players[i].handle = dlopen(requested_path, RTLD_LAZY);
        if (!players[i].handle) {
            load_err = dlerror();

            // Fallback utile si le chemin fourni est fictif mais que le .so est present localement.
            const char *base = strrchr(requested_path, '/');
            if (base && *(base + 1) != '\0') {
                const char *local_name = base + 1;
                players[i].handle = dlopen(local_name, RTLD_LAZY);
                if (players[i].handle) {
                    loaded_path = local_name;
                }
            }
        }

        if (!players[i].handle) {
            fprintf(stderr, "Erreur chargement joueur %d (%s): %s\n", i + 1, requested_path, load_err ? load_err : dlerror());
            for (int j = 0; j < i; j++) {
                if (players[j].handle) dlclose(players[j].handle);
            }
            // ...libération musique supprimée...
            if (big_font != font) TTF_CloseFont(big_font);
            if (font) TTF_CloseFont(font);
            TTF_Quit();
            SDL_DestroyRenderer(ren);
            SDL_DestroyWindow(win);
            SDL_Quit();
            return 1;
        }

        dlerror();
        players[i].get_action = dlsym(players[i].handle, "get_action");
        {
            const char *sym_err = dlerror();
            if (sym_err || !players[i].get_action) {
                fprintf(stderr, "Erreur symbole get_action joueur %d (%s): %s\n", i + 1, loaded_path, sym_err ? sym_err : "symbole manquant");
                for (int j = 0; j <= i; j++) {
                    if (players[j].handle) dlclose(players[j].handle);
                }
                // ...libération musique supprimée...
                if (big_font != font) TTF_CloseFont(big_font);
                if (font) TTF_CloseFont(font);
                TTF_Quit();
                SDL_DestroyRenderer(ren);
                SDL_DestroyWindow(win);
                SDL_Quit();
                return 1;
            }
        }

        players[i].credits = INITIAL_CREDITS;
        players[i].score = 0;
        players[i].last_action = ACTION_STILL;
        players[i].shown_action = ACTION_STILL;
        players[i].next_action_ui_update = 0;
        players[i].highlighted_action = ACTION_STILL;
        players[i].highlight_until = 0;
        players[i].x = i * 25; players[i].y = 50;
    }

    int running = 1, paused = 1, finished = 0;
    char win_msg[64] = "";
    int winner_id = 0;
    int winner_score = 0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                SDL_Point m = { e.button.x, e.button.y };
                
                // Demarrer: lance ou reprend la partie.
                if (SDL_PointInRect(&m, &btnStart)) {
                    if (finished) {
                        reset_match(players);
                        finished = 0;
                        turbo_mode = 5;
                        winner_id = 0;
                        winner_score = 0;
                        win_msg[0] = '\0';
                    }
                    paused = 0;
                }
                
                // Pause: arrete temporairement la partie.
                if (SDL_PointInRect(&m, &btnStop)) {
                    paused = 1;
                }
                
                // Quitter ferme le jeu, Turbo accelere la simulation.
                if (SDL_PointInRect(&m, &btnQuit)) running = 0;
                if (SDL_PointInRect(&m, &btnTurbo)) turbo_mode = (turbo_mode == 5) ? 200 : 5;
            }
        }

        // On fait avancer la simulation.
        if (!paused && !finished) {
            int active = 0;
            for (int step = 0; step < turbo_mode; step++) { 
                active = 0;
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (players[i].credits > 0) {
                        update_player(&players[i], i + 1);
                        active = 1;
                    }
                }
                if (!active) break;
            }
            if (!active) { 
                finished = 1; paused = 1; 
                // ...musique supprimée...
                int best = 0, winner = 0;
                for(int i=0; i<4; i++) if(players[i].score > best) { best = players[i].score; winner = i+1; }
                winner_id = winner;
                winner_score = best;
                sprintf(win_msg, "VICTOIRE : JOUEUR %d (%d pts)", winner, best);
            }
        }

        // Affichage de l'interface puis de la grille.
        SDL_SetRenderDrawColor(ren, 20, 20, 20, 255);
        SDL_RenderClear(ren);

        SDL_SetRenderDrawColor(ren, 22, 163, 74, 255); SDL_RenderFillRect(ren, &btnStart);
        draw_text(ren, font, "DEMARRER", btnStart, (SDL_Color){255,255,255,255});
        SDL_SetRenderDrawColor(ren, 14, 165, 233, 255); SDL_RenderFillRect(ren, &btnStop);
        draw_text(ren, font, "PAUSE", btnStop, (SDL_Color){255,255,255,255});
        SDL_SetRenderDrawColor(ren, (turbo_mode > 5 ? 251 : 245), (turbo_mode > 5 ? 146 : 158), 11, 255);
        SDL_RenderFillRect(ren, &btnTurbo);
        draw_text(ren, font, "TURBO", btnTurbo, (SDL_Color){255,255,255,255});
        SDL_SetRenderDrawColor(ren, 239, 68, 68, 255); SDL_RenderFillRect(ren, &btnQuit);
        draw_text(ren, font, "QUITTER", btnQuit, (SDL_Color){255,255,255,255});

        if (!finished && turbo_mode > 5) {
            draw_text(ren, font, "[MODE TURBO ACTIF]", areaStatus, (SDL_Color){255, 100, 0, 255});
        }

        SDL_Rect scorePanel = { 454, 14, 258, 96 };
        SDL_SetRenderDrawColor(ren, 12, 12, 12, 255);
        SDL_RenderFillRect(ren, &scorePanel);
        SDL_SetRenderDrawColor(ren, 58, 58, 58, 255);
        SDL_RenderDrawRect(ren, &scorePanel);

        int cur_scores[4] = {0};
        Uint32 now = SDL_GetTicks();
        for (int i = 0; i < MAX_PLAYERS; i++) {
            char info[64];
            SDL_Rect scoreArea = {458, 20 + (i*22), 250, 20};

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
                sprintf(info, "P%d: %d pts | %d crd | %s", i+1, winner_score, players[i].credits, action_label(players[i].shown_action));
                draw_text(ren, font, info, scoreArea, (SDL_Color){20, 20, 20, 255});
            } else {
                sprintf(info, "P%d: %d pts | %d crd | %s", i+1, players[i].score, players[i].credits, action_label(players[i].shown_action));
                draw_text(ren, font, info, scoreArea, p_colors[i]);
            }
        }

        for (int y = 0; y < GRID_SIZE; y++) {
            for (int x = 0; x < GRID_SIZE; x++) {
                if (grid[y][x] > 0) {
                    // Le score = nombre de cases controlees.
                    cur_scores[grid[y][x]-1]++;

                    SDL_Color base = p_colors[grid[y][x]-1];
                    int checker = ((x + y) & 1) ? 14 : -10;
                    Uint8 r1 = clamp_u8((base.r * 3) / 4 + checker);
                    Uint8 g1 = clamp_u8((base.g * 3) / 4 + checker);
                    Uint8 b1 = clamp_u8((base.b * 3) / 4 + checker);
                    Uint8 r2 = clamp_u8(base.r + 26 + checker / 2);
                    Uint8 g2 = clamp_u8(base.g + 26 + checker / 2);
                    Uint8 b2 = clamp_u8(base.b + 26 + checker / 2);

                    SDL_Rect r = { x * CELL_SIZE, y * CELL_SIZE + UI_HEIGHT, CELL_SIZE, CELL_SIZE };

                    // Fond avec ombrage leger.
                    SDL_SetRenderDrawColor(ren, r1, g1, b1, 255);
                    SDL_RenderFillRect(ren, &r);

                    // Interieur un peu plus lumineux pour donner du relief.
                    if (CELL_SIZE > 2) {
                        SDL_Rect inner = { r.x + 1, r.y + 1, CELL_SIZE - 2, CELL_SIZE - 2 };
                        SDL_SetRenderDrawColor(ren, r2, g2, b2, 255);
                        SDL_RenderFillRect(ren, &inner);
                    }

                    // Bordure fine pour mieux separer les cases.
                    SDL_SetRenderDrawColor(ren, clamp_u8(r1 - 28), clamp_u8(g1 - 28), clamp_u8(b1 - 28), 255);
                    SDL_RenderDrawRect(ren, &r);
                }
            }
        }
        for(int i=0; i<4; i++) players[i].score = cur_scores[i];

        // Marqueur visuel pour reperer rapidement la position actuelle de chaque joueur.
        for (int i = 0; i < MAX_PLAYERS; i++) {
            int px = players[i].x * CELL_SIZE;
            int py = players[i].y * CELL_SIZE + UI_HEIGHT;
            int blink = ((SDL_GetTicks() / 180) % 2);

            SDL_Rect outer = { px - 2, py - 2, CELL_SIZE + 4, CELL_SIZE + 4 };
            SDL_Rect inner = { px - 1, py - 1, CELL_SIZE + 2, CELL_SIZE + 2 };

            if (blink) {
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            } else {
                SDL_SetRenderDrawColor(ren, p_colors[i].r, p_colors[i].g, p_colors[i].b, 255);
            }
            SDL_RenderDrawRect(ren, &outer);
            SDL_RenderDrawRect(ren, &inner);
        }

        if (finished) {
            SDL_Rect centerBox = { 80, (GRID_SIZE * CELL_SIZE + UI_HEIGHT) / 2 - 36, GRID_SIZE * CELL_SIZE - 160, 72 };
            SDL_SetRenderDrawColor(ren, 245, 190, 40, 255);
            SDL_RenderFillRect(ren, &centerBox);

            SDL_Rect centerInner = { centerBox.x + 3, centerBox.y + 3, centerBox.w - 6, centerBox.h - 6 };
            SDL_SetRenderDrawColor(ren, 35, 35, 35, 255);
            SDL_RenderFillRect(ren, &centerInner);

            if (winner_id > 0) {
                draw_text(ren, big_font, win_msg, centerInner, p_colors[winner_id - 1]);
            } else {
                draw_text(ren, big_font, win_msg, centerInner, (SDL_Color){255, 255, 0, 255});
            }
        }

        SDL_RenderPresent(ren);
        SDL_Delay(16); 
    }

    // Nettoyage avant de quitter.
    for (int i = 0; i < MAX_PLAYERS; i++) dlclose(players[i].handle);
    if (big_font != font) TTF_CloseFont(big_font);
    TTF_CloseFont(font); TTF_Quit(); SDL_Quit();
    return 0;
}