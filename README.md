# Projet 2

## Description
Ce projet contient un programme principal (`splash.c`) et 4 modules de joueurs (`joueur1.c`, `joueur2.c`, `joueur3.c`, `joueur4.c`). Il utilise la bibliothèque SDL2 pour l'affichage et la gestion des polices.

## Compilation
Pour compiler le projet, utilisez la commande :

```
make
```

Cela génère l'exécutable `splash` et les bibliothèques partagées pour chaque joueur (`p1.so`, `p2.so`, `p3.so`, `p4.so`).

## Exécution
Lancez le programme principal avec :

```
./splash
```

## Dépendances
- SDL2
- SDL2_ttf
- SDL2_mixer

Assurez-vous que ces bibliothèques sont installées sur votre système.

## Structure du projet
- `splash.c` : Programme principal
- `joueur1.c` à `joueur4.c` : Modules de joueurs
- `actions.h` : Fichier d'en-tête commun
- `Makefile` : Script de compilation

## Nettoyage
Pour supprimer les fichiers générés :

```
make clean
```
# Projet_Splashmem_IA_Sara_OULMAKI
