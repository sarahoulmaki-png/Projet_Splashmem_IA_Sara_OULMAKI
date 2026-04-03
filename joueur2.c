#include "actions.h"

// Joueur de test: construit de l'historique puis declenche fork.
char get_action() {
    static int steps = 0;
    steps++;

    if (steps % 70 == 0) {
        return ACTION_FORK;
    }
    if (steps % 18 == 0) {
        return ACTION_DASH_D;
    }
    if (steps % 9 == 0) {
        return ACTION_MOVE_U;
    }
    return ACTION_MOVE_D;
}