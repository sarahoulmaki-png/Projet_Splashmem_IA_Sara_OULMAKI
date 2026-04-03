#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "splash_game.h"

/* Engine state lives in this translation unit. */

int grid[GRID_SIZE][GRID_SIZE] = {0};
int turbo_mode = 5;
int player_count = 4;
Bomb bombs[MAX_BOMBS];

Uint8 clamp_u8(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (Uint8)value;
}

static int wrap_coord(int value) {
    int r = value % GRID_SIZE;
    if (r < 0) r += GRID_SIZE;
    return r;
}

static int torus_axis_dist(int a, int b) {
    int d = abs(a - b);
    if (d > GRID_SIZE / 2) d = GRID_SIZE - d;
    return d;
}

static int torus_dist(int x1, int y1, int x2, int y2) {
    return torus_axis_dist(x1, x2) + torus_axis_dist(y1, y2);
}

static int is_special_action(char action) {
    return action >= ACTION_DASH_L;
}

static int fork_action_cost(const Player *p) {
    return 10 + (p->score / 100);
}

static int base_action_cost(const Player *p, char action) {
    switch (action) {
        case ACTION_MOVE_L:
        case ACTION_MOVE_R:
        case ACTION_MOVE_U:
        case ACTION_MOVE_D:
        case ACTION_STILL:
            return 1;
        case ACTION_DASH_L:
        case ACTION_DASH_R:
        case ACTION_DASH_U:
        case ACTION_DASH_D:
            return 10;
        case ACTION_TELEPORT_L:
        case ACTION_TELEPORT_R:
        case ACTION_TELEPORT_U:
        case ACTION_TELEPORT_D:
            return 2;
        case ACTION_BOMB:
            return BOMB_COST;
        case ACTION_FORK:
            return fork_action_cost(p);
        case ACTION_CLEAN:
            return CLEAN_COST;
        case ACTION_MUTE:
            return MUTE_COST;
        case ACTION_SWAP:
            return SWAP_COST;
        default:
            return 1;
    }
}

static int compute_dx(char action) {
    switch (action) {
        case ACTION_MOVE_L: return -1;
        case ACTION_MOVE_R: return 1;
        case ACTION_DASH_L: return -8;
        case ACTION_DASH_R: return 8;
        case ACTION_TELEPORT_L: return -8;
        case ACTION_TELEPORT_R: return 8;
        default: return 0;
    }
}

static int compute_dy(char action) {
    switch (action) {
        case ACTION_MOVE_U: return -1;
        case ACTION_MOVE_D: return 1;
        case ACTION_DASH_U: return -8;
        case ACTION_DASH_D: return 8;
        case ACTION_TELEPORT_U: return -8;
        case ACTION_TELEPORT_D: return 8;
        default: return 0;
    }
}

typedef struct {
    const char *name;
    char action;
} ActionNameEntry;

static const ActionNameEntry kActionNames[] = {
    {"ACTION_STILL", ACTION_STILL},
    {"ACTION_MOVE_L", ACTION_MOVE_L},
    {"ACTION_MOVE_R", ACTION_MOVE_R},
    {"ACTION_MOVE_U", ACTION_MOVE_U},
    {"ACTION_MOVE_D", ACTION_MOVE_D},
    {"ACTION_DASH_L", ACTION_DASH_L},
    {"ACTION_DASH_R", ACTION_DASH_R},
    {"ACTION_DASH_U", ACTION_DASH_U},
    {"ACTION_DASH_D", ACTION_DASH_D},
    {"ACTION_TELEPORT_L", ACTION_TELEPORT_L},
    {"ACTION_TELEPORT_R", ACTION_TELEPORT_R},
    {"ACTION_TELEPORT_U", ACTION_TELEPORT_U},
    {"ACTION_TELEPORT_D", ACTION_TELEPORT_D},
    {"ACTION_BOMB", ACTION_BOMB},
    {"ACTION_FORK", ACTION_FORK},
    {"ACTION_CLEAN", ACTION_CLEAN},
    {"ACTION_MUTE", ACTION_MUTE},
    {"ACTION_SWAP", ACTION_SWAP}
};

static int parse_action_name(const char *token, char *out_action) {
    if (!token || !out_action) return 0;

    for (size_t i = 0; i < sizeof(kActionNames) / sizeof(kActionNames[0]); i++) {
        if (strcmp(token, kActionNames[i].name) == 0) {
            *out_action = kActionNames[i].action;
            return 1;
        }
    }

    char *endptr = NULL;
    long value = strtol(token, &endptr, 10);
    if (endptr == token || *endptr != '\0') return 0;
    if (value < 0 || value >= ACTION_NUMBER) return 0;
    *out_action = (char)value;
    return 1;
}

static int load_script_actions(const char *path, int **actions_out, int *count_out, char *err, size_t err_sz) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, err_sz, "impossible d'ouvrir le fichier texte");
        return 0;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        snprintf(err, err_sz, "fseek() a echoue");
        return 0;
    }

    long len = ftell(f);
    if (len < 0) {
        fclose(f);
        snprintf(err, err_sz, "ftell() a echoue");
        return 0;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        snprintf(err, err_sz, "fseek() (retour) a echoue");
        return 0;
    }

    char *content = (char *)malloc((size_t)len + 1);
    if (!content) {
        fclose(f);
        snprintf(err, err_sz, "memoire insuffisante");
        return 0;
    }

    size_t read_sz = fread(content, 1, (size_t)len, f);
    fclose(f);
    content[read_sz] = '\0';

    int cap = 32;
    int count = 0;
    int *actions = (int *)malloc((size_t)cap * sizeof(int));
    if (!actions) {
        free(content);
        snprintf(err, err_sz, "memoire insuffisante");
        return 0;
    }

    char *saveptr = NULL;
    char *tok = strtok_r(content, ", \t\r\n", &saveptr);
    while (tok) {
        char action = ACTION_STILL;
        if (!parse_action_name(tok, &action)) {
            free(actions);
            free(content);
            snprintf(err, err_sz, "action inconnue dans le script: %s", tok);
            return 0;
        }

        if (count >= cap) {
            cap *= 2;
            int *tmp = (int *)realloc(actions, (size_t)cap * sizeof(int));
            if (!tmp) {
                free(actions);
                free(content);
                snprintf(err, err_sz, "memoire insuffisante");
                return 0;
            }
            actions = tmp;
        }

        actions[count++] = (int)action;
        tok = strtok_r(NULL, ", \t\r\n", &saveptr);
    }

    free(content);

    if (count == 0) {
        free(actions);
        snprintf(err, err_sz, "fichier texte vide (aucune action)");
        return 0;
    }

    *actions_out = actions;
    *count_out = count;
    return 1;
}

static int load_player_from_so(Player *p, const char *requested_path, char *err, size_t err_sz) {
    const char *loaded_path = requested_path;

    p->handle = dlopen(requested_path, RTLD_LAZY);
    if (!p->handle) {
        const char *base = strrchr(requested_path, '/');
        if (base && *(base + 1) != '\0') {
            const char *local_name = base + 1;
            p->handle = dlopen(local_name, RTLD_LAZY);
            if (p->handle) {
                loaded_path = local_name;
            }
        }
    }

    if (!p->handle) {
        snprintf(err, err_sz, "%s", dlerror() ? dlerror() : "dlopen() a echoue");
        return 0;
    }

    dlerror();
    p->so_get_action = dlsym(p->handle, "get_action");
    {
        const char *sym_err = dlerror();
        if (sym_err || !p->so_get_action) {
            snprintf(err, err_sz, "symbole get_action invalide (%s)", sym_err ? sym_err : loaded_path);
            dlclose(p->handle);
            p->handle = NULL;
            return 0;
        }
    }

    p->source_type = PLAYER_SOURCE_SO;
    (void)loaded_path;
    return 1;
}

int load_player_source(Player *p, const char *requested_path, char *err, size_t err_sz) {
    const char *ext = strrchr(requested_path, '.');

    if (ext && strcmp(ext, ".txt") == 0) {
        p->source_type = PLAYER_SOURCE_SCRIPT;
        if (!load_script_actions(requested_path, &p->script_actions, &p->script_count, err, err_sz)) {
            return 0;
        }
        return 1;
    }

    if (load_player_from_so(p, requested_path, err, err_sz)) {
        return 1;
    }

    p->source_type = PLAYER_SOURCE_SCRIPT;
    if (load_script_actions(requested_path, &p->script_actions, &p->script_count, err, err_sz)) {
        return 1;
    }

    return 0;
}

static char next_action(Player *p) {
    if (p->source_type == PLAYER_SOURCE_SCRIPT) {
        if (p->script_count <= 0) return ACTION_STILL;
        char action = (char)p->script_actions[p->script_index];
        p->script_index = (p->script_index + 1) % p->script_count;
        return action;
    }

    if (!p->so_get_action) return ACTION_STILL;
    return p->so_get_action();
}

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
        case ACTION_BOMB:
            return "BOMB";
        case ACTION_FORK:
            return "FORK";
        case ACTION_CLEAN:
            return "CLEAN";
        case ACTION_MUTE:
            return "MUTE";
        case ACTION_SWAP:
            return "SWAP";
        default:
            return "STILL";
    }
}

static int paint_owner_for_player(const Player *p, int default_id) {
    if (p->paint_mode == PAINT_MUTE && p->paint_turns_left > 0) return 0;
    if (p->paint_mode == PAINT_SWAP && p->paint_turns_left > 0) return p->swap_owner_id;
    return default_id;
}

static void push_history(Player *p, char action) {
    if (p->action_history_count < FORK_HISTORY_CAP) {
        p->action_history[p->action_history_count++] = action;
        return;
    }

    memmove(p->action_history, p->action_history + 1, FORK_HISTORY_CAP - 1);
    p->action_history[FORK_HISTORY_CAP - 1] = action;
}

static char history_at_offset(const Player *p, int offset) {
    if (offset < 0 || offset >= p->action_history_count) {
        return ACTION_STILL;
    }
    return p->action_history[offset];
}

static void apply_movement(int *x, int *y, char action) {
    int dx = compute_dx(action);
    int dy = compute_dy(action);
    *x = wrap_coord(*x + dx);
    *y = wrap_coord(*y + dy);
}

static void mark_player_cell(int x, int y, int owner_id) {
    if (owner_id < 0 || owner_id > MAX_PLAYERS) return;
    grid[y][x] = owner_id;
}

static void add_bomb(int owner_id, int x, int y) {
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].active) {
            bombs[i].active = 1;
            bombs[i].owner_id = owner_id;
            bombs[i].x = x;
            bombs[i].y = y;
            bombs[i].turns_left = BOMB_DELAY_TURNS + 1;
            return;
        }
    }
}

static void explode_bomb(const Bomb *b) {
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nx = wrap_coord(b->x + dx);
            int ny = wrap_coord(b->y + dy);
            grid[ny][nx] = b->owner_id;
        }
    }
}

void process_bombs(void) {
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].active) continue;
        bombs[i].turns_left--;
        if (bombs[i].turns_left <= 0) {
            explode_bomb(&bombs[i]);
            bombs[i].active = 0;
        }
    }
}

int has_active_bombs(void) {
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active) return 1;
    }
    return 0;
}

static void clean_around_player(const Player *p) {
    for (int dy = -3; dy <= 3; dy++) {
        for (int dx = -3; dx <= 3; dx++) {
            int nx = wrap_coord(p->x + dx);
            int ny = wrap_coord(p->y + dy);
            grid[ny][nx] = 0;
        }
    }
}

static int find_closest_enemy(Player players[MAX_PLAYERS], int self_idx) {
    int best_idx = -1;
    int best_dist = 1 << 30;

    for (int i = 0; i < player_count; i++) {
        if (i == self_idx) continue;

        int d = torus_dist(players[self_idx].x, players[self_idx].y, players[i].x, players[i].y);
        if (d < best_dist) {
            best_dist = d;
            best_idx = i;
        }
    }

    return best_idx;
}

static void apply_status_effect(Player *target, int attacker_id, int effect_mode) {
    switch (effect_mode) {
        case PAINT_MUTE:
            target->paint_mode = PAINT_MUTE;
            target->paint_turns_left = MUTE_TURNS;
            target->swap_owner_id = 0;
            break;
        case PAINT_SWAP:
            target->paint_mode = PAINT_SWAP;
            target->paint_turns_left = SWAP_TURNS;
            target->swap_owner_id = attacker_id;
            break;
        default:
            break;
    }
}

static void start_fork(Player *p) {
    p->fork_penalty_turns_left = FORK_DURATION_TURNS;
    p->clone_pending_turns = FORK_DELAY_TURNS + 1;
    p->clone_turns_left = FORK_DURATION_TURNS;
    p->fork_anchor_x = p->x;
    p->fork_anchor_y = p->y;
    p->clone_history_offset = p->action_history_count;
}

void update_player(Player players[MAX_PLAYERS], int idx) {
    Player *p = &players[idx];
    int player_id = idx + 1;

    if (p->credits <= 0) {
        p->last_action = ACTION_STILL;
        return;
    }

    char act = next_action(p);
    p->last_action = act;

    if (is_special_action(act)) {
        p->highlighted_action = act;
        p->highlight_until = SDL_GetTicks() + 900;
    }

    int cost = base_action_cost(p, act);
    if (p->fork_penalty_turns_left > 0) {
        cost *= 2;
    }

    if (p->credits < cost) {
        p->last_action = ACTION_STILL;
        push_history(p, ACTION_STILL);
        return;
    }

    p->credits -= cost;

    switch (act) {
        case ACTION_BOMB:
            add_bomb(player_id, p->x, p->y);
            break;
        case ACTION_CLEAN:
            clean_around_player(p);
            break;
        case ACTION_MUTE:
        case ACTION_SWAP: {
            int target_idx = find_closest_enemy(players, idx);
            if (target_idx >= 0) {
                apply_status_effect(&players[target_idx], player_id, (act == ACTION_MUTE) ? PAINT_MUTE : PAINT_SWAP);
            }
            break;
        }
        case ACTION_FORK:
            start_fork(p);
            break;
        default:
            break;
    }

    apply_movement(&p->x, &p->y, act);
    mark_player_cell(p->x, p->y, paint_owner_for_player(p, player_id));

    push_history(p, act);
}

void update_clone(Player *p, int player_id) {
    if (p->clone_pending_turns > 0) {
        p->clone_pending_turns--;
        if (p->clone_pending_turns == 0) {
            p->clone_x = p->x;
            p->clone_y = p->y;
            mark_player_cell(p->clone_x, p->clone_y, player_id);
        }
    }

    if (p->clone_turns_left <= 0 || p->clone_pending_turns > 0) {
        return;
    }

    char clone_action = history_at_offset(p, p->clone_history_offset);
    p->clone_history_offset++;

    apply_movement(&p->clone_x, &p->clone_y, clone_action);
    mark_player_cell(p->clone_x, p->clone_y, player_id);
    p->clone_turns_left--;
}

int has_pending_clones(Player players[MAX_PLAYERS]) {
    for (int i = 0; i < player_count; i++) {
        if (players[i].clone_pending_turns > 0 || players[i].clone_turns_left > 0) {
            return 1;
        }
    }
    return 0;
}

int any_player_can_act(Player players[MAX_PLAYERS]) {
    for (int i = 0; i < player_count; i++) {
        if (players[i].credits > 0) return 1;
    }
    return 0;
}

void tick_effects(Player players[MAX_PLAYERS]) {
    for (int i = 0; i < player_count; i++) {
        if (players[i].paint_turns_left > 0) {
            players[i].paint_turns_left--;
            if (players[i].paint_turns_left == 0) {
                players[i].paint_mode = PAINT_NORMAL;
                players[i].swap_owner_id = 0;
            }
        }

        if (players[i].fork_penalty_turns_left > 0) {
            players[i].fork_penalty_turns_left--;
        }
    }
}

void draw_text(SDL_Renderer *ren, TTF_Font *font, const char *txt, SDL_Rect area, SDL_Color color) {
    if (!font || !txt) return;
    SDL_Surface *s = TTF_RenderText_Blended(font, txt, color);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    if (!t) {
        SDL_FreeSurface(s);
        return;
    }
    SDL_Rect dst = { area.x + (area.w - s->w) / 2, area.y + (area.h - s->h) / 2, s->w, s->h };
    SDL_RenderCopy(ren, t, NULL, &dst);
    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
}

void draw_button(SDL_Renderer *ren, TTF_Font *font, SDL_Rect rect, SDL_Color bg, const char *label) {
    SDL_SetRenderDrawColor(ren, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderFillRect(ren, &rect);
    draw_text(ren, font, label, rect, (SDL_Color){255, 255, 255, 255});
}

static void reset_player_state(Player *p, int idx) {
    p->credits = INITIAL_CREDITS;
    p->score = 0;

    p->last_action = ACTION_STILL;
    p->shown_action = ACTION_STILL;
    p->next_action_ui_update = 0;
    p->highlighted_action = ACTION_STILL;
    p->highlight_until = 0;

    p->paint_mode = PAINT_NORMAL;
    p->paint_turns_left = 0;
    p->swap_owner_id = 0;

    p->fork_penalty_turns_left = 0;
    p->clone_pending_turns = 0;
    p->clone_turns_left = 0;
    p->clone_x = 0;
    p->clone_y = 0;
    p->clone_history_offset = 0;
    p->fork_anchor_x = 0;
    p->fork_anchor_y = 0;
    p->action_history_count = 0;

    p->x = idx * 25;
    p->y = 50;
}

void release_players(Player players[MAX_PLAYERS], int count) {
    for (int i = 0; i < count; i++) {
        if (players[i].handle) dlclose(players[i].handle);
        free(players[i].script_actions);
    }
}

void reset_match(Player players[MAX_PLAYERS]) {
    memset(grid, 0, sizeof(grid));
    memset(bombs, 0, sizeof(bombs));

    for (int i = 0; i < player_count; i++) {
        reset_player_state(&players[i], i);
    }
}
