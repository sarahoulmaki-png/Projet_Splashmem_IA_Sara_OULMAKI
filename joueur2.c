#include "actions.h"

// Balayage en colonnes: bas, puis haut, etc
char get_action() {
    static int steps = 0;
    static int direction = 0; // 0 = bas, 1 = haut
    
    steps++;

    // Fin de colonne : on va a droite et on inverse le sens vertical
    if (steps % 100 == 0) {
        direction = !direction;
        return ACTION_MOVE_R;
    }

    // Sinon on continue verticalement
    if (direction == 0) {
        return ACTION_MOVE_D;
    } else {
        return ACTION_MOVE_U;
    }
}