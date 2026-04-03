#include "actions.h"

// Joueur de test: avance, pose des bombes regulierement, puis tente clean.
char get_action() {
    static int steps = 0;
    steps++;

    if (steps % 90 == 0) {
        return ACTION_CLEAN;
    }
    if (steps % 25 == 0) {
        return ACTION_BOMB;
    }
    if (steps % 40 == 0) {
        return ACTION_TELEPORT_D;
    }
    return ACTION_MOVE_R;
}