# Projet Splashmem IA

## Description
Ce projet implémente un jeu de grille avec interface SDL2.

- `splash.c` gère l'interface, les événements et la boucle principale.
- `splash_game.c` contient la logique de jeu (déplacements, score, bombes, fork, effets).
- Les joueurs peuvent être chargés depuis des bibliothèques partagées (`.so`) ou des scripts texte (`.txt`).

## Compilation
Compilez le projet avec :

```bash
make
```

La compilation génère :

- l'exécutable `splash`
- les IA compilées `p1.so`, `p2.so`, `p3.so`, `p4.so`

## Exécution
Le programme attend entre 4 et 8 joueurs en argument.

Format :

```bash
./splash p1.so|p1.txt p2.so|p2.txt p3.so|p3.txt p4.so|p4.txt [p5..p8]
```

Exemple avec les IA compilées :

```bash
./splash ./p1.so ./p2.so ./p3.so ./p4.so
```

Ou simplement :

```bash
make run
```

## Actions disponibles
Les actions définies dans `actions.h` sont :

- `ACTION_MOVE_L`, `ACTION_MOVE_R`, `ACTION_MOVE_U`, `ACTION_MOVE_D`
- `ACTION_DASH_L`, `ACTION_DASH_R`, `ACTION_DASH_U`, `ACTION_DASH_D`
- `ACTION_TELEPORT_L`, `ACTION_TELEPORT_R`, `ACTION_TELEPORT_U`, `ACTION_TELEPORT_D`
- `ACTION_STILL`
- `ACTION_BOMB`
- `ACTION_FORK`
- `ACTION_CLEAN`
- `ACTION_MUTE`
- `ACTION_SWAP`

## Dépendances
- SDL2
- SDL2_ttf

Sous Ubuntu :

```bash
sudo apt-get install -y build-essential libsdl2-dev libsdl2-ttf-dev
```

## Structure du projet
- `splash.c` : point d'entrée et rendu UI
- `splash_game.c` : logique de simulation
- `splash_game.h` : types, constantes et API partagée
- `joueur1.c` à `joueur4.c` : IA exemples
- `actions.h` : identifiants d'actions
- `Makefile` : compilation et exécution

## Nettoyage
Supprime les binaires et bibliothèques générés :

```bash
make clean
```
