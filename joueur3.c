#include "actions.h"

// Joueur de test: alterne mute et swap pour verifier les effets de couleur.
char get_action() {
    static int steps = 0;
    steps++;

    if (steps % 60 == 0) {
        return ACTION_SWAP;
    }
    if (steps % 30 == 0) {
        return ACTION_MUTE;
    }
    if (steps % 12 == 0) {
        return ACTION_TELEPORT_R;
    }
    return ACTION_MOVE_U;
}