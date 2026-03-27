#include "actions.h"

// Parcours en serpent : une ligne a droite, la suivante a gauche

char get_action() {
    static int steps = 0;
    static int line = 0;

    steps++;

    // Fin de ligne : on descend
    if (steps % 100 == 0) {
        line++;
        return ACTION_MOVE_D;
    }

    // On alterne droite/gauche selon la ligne
    return (line % 2 == 0) ? ACTION_MOVE_R : ACTION_MOVE_L;
}