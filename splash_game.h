#ifndef SPLASH_GAME_H
#define SPLASH_GAME_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "actions.h"

/* Shared constants, state, and APIs for the game engine/UI split. */

#define GRID_SIZE 100
#define CELL_SIZE 8
#define MAX_PLAYERS 8
#define INITIAL_CREDITS 9000
#define UI_HEIGHT 120

#define BOMB_COST 9
#define BOMB_DELAY_TURNS 5
#define CLEAN_COST 40
#define MUTE_COST 30
#define MUTE_TURNS 10
#define SWAP_COST 35
#define SWAP_TURNS 5
#define FORK_DELAY_TURNS 5
#define FORK_DURATION_TURNS 20
#define FORK_HISTORY_CAP 128
#define MAX_BOMBS 2048

#define PLAYER_SOURCE_SO 0
#define PLAYER_SOURCE_SCRIPT 1

#define PAINT_NORMAL 0
#define PAINT_MUTE 1
#define PAINT_SWAP 2

typedef struct {
    int owner_id;
    int x;
    int y;
    int turns_left;
    int active;
} Bomb;

typedef struct {
    void *handle;
    char (*so_get_action)(void);
    int source_type;

    int *script_actions;
    int script_count;
    int script_index;

    int x, y;
    int credits;
    int score;

    char last_action;
    char shown_action;
    Uint32 next_action_ui_update;
    char highlighted_action;
    Uint32 highlight_until;

    int paint_mode;
    int paint_turns_left;
    int swap_owner_id;

    int fork_penalty_turns_left;
    int clone_pending_turns;
    int clone_turns_left;
    int clone_x;
    int clone_y;
    int clone_history_offset;
    int fork_anchor_x;
    int fork_anchor_y;
    char action_history[FORK_HISTORY_CAP];
    int action_history_count;
} Player;

extern int grid[GRID_SIZE][GRID_SIZE];
extern int turbo_mode;
extern int player_count;
extern Bomb bombs[MAX_BOMBS];

Uint8 clamp_u8(int value);
int load_player_source(Player *p, const char *requested_path, char *err, size_t err_sz);
const char *action_label(char action);
void update_player(Player players[MAX_PLAYERS], int idx);
void update_clone(Player *p, int player_id);
int has_pending_clones(Player players[MAX_PLAYERS]);
int any_player_can_act(Player players[MAX_PLAYERS]);
void tick_effects(Player players[MAX_PLAYERS]);
void draw_text(SDL_Renderer *ren, TTF_Font *font, const char *txt, SDL_Rect area, SDL_Color color);
void draw_button(SDL_Renderer *ren, TTF_Font *font, SDL_Rect rect, SDL_Color bg, const char *label);
void reset_match(Player players[MAX_PLAYERS]);
void release_players(Player players[MAX_PLAYERS], int count);
int has_active_bombs(void);
void process_bombs(void);

#endif
