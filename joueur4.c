#include "actions.h"

// Avance a droite, avec un teleport vers le bas de temps en temps
char get_action() {
    static int steps = 0;
    steps++;

    // Tous les 100 coups, on saute vers le bas
    if (steps % 100 == 0) {
        return ACTION_TELEPORT_D;
    }
    
    // Le reste du temps, on avance case par case
    return ACTION_MOVE_R;
}