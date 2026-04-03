#ifndef ACTIONS_H
#define ACTIONS_H

// Actions possibles a chaque tour.
typedef enum {
	ACTION_MOVE_L = 1,
	ACTION_MOVE_R = 2,
	ACTION_MOVE_U = 3,
	ACTION_MOVE_D = 4,

	ACTION_DASH_L = 5,
	ACTION_DASH_R = 6,
	ACTION_DASH_U = 7,
	ACTION_DASH_D = 8,

	ACTION_TELEPORT_L = 9,
	ACTION_TELEPORT_R = 10,
	ACTION_TELEPORT_U = 11,
	ACTION_TELEPORT_D = 12,

	ACTION_STILL = 13,

	ACTION_BOMB = 14,
	ACTION_FORK = 15,
	ACTION_CLEAN = 16,
	ACTION_MUTE = 17,
	ACTION_SWAP = 18,

	// Nombre total d'actions (borne exclusive).
	ACTION_NUMBER = 19
} Action;

#endif