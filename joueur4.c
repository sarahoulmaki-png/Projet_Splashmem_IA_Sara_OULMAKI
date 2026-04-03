#include "actions.h"

// Joueur de test: produit des mouvements plus agressifs pour croiser les autres.
char get_action() {
    static int steps = 0;
    steps++;

    if (steps % 45 == 0) {
        return ACTION_BOMB;
    }
    if (steps % 20 == 0) {
        return ACTION_TELEPORT_U;
    }
    if (steps % 10 == 0) {
        return ACTION_DASH_L;
    }
    return ACTION_MOVE_L;
}