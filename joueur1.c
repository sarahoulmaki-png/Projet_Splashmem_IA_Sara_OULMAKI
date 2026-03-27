#include "actions.h"

// Va surtout a droite, puis descend de temps en temps
char get_action() {
    static int steps = 0;
    static int direction __attribute__((unused)) = 0;
    
    steps++;
    // Tous les 100 coups, on descend d'une case
    if (steps % 100 == 0) {
        return ACTION_MOVE_D;
    }
    return ACTION_MOVE_R;
}