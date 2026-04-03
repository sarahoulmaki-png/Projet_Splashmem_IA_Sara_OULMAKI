#ifndef ACTIONS_H
#define ACTIONS_H

// On renvoi une de ces actions a chaque tour

// Deplacements simples (1 case).
#define ACTION_MOVE_L     1
#define ACTION_MOVE_R     2
#define ACTION_MOVE_U     3
#define ACTION_MOVE_D     4

// Dash (8 cases, plus cher).
#define ACTION_DASH_L     5
#define ACTION_DASH_R     6
#define ACTION_DASH_U     7
#define ACTION_DASH_D     8

// Teleport (8 cases, cout intermediaire).
#define ACTION_TELEPORT_L 9
#define ACTION_TELEPORT_R 10
#define ACTION_TELEPORT_U 11
#define ACTION_TELEPORT_D 12

// Ne rien faire ce tour.
#define ACTION_STILL      13

// Actions avancees.
#define ACTION_BOMB       14
#define ACTION_FORK       15
#define ACTION_CLEAN      16
#define ACTION_MUTE       17
#define ACTION_SWAP       18

// Nombre total d'actions (borne exclusive).
#define ACTION_NUMBER     19

#endif