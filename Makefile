# --- Variables de Compilation ---
CC = gcc

# Détection de l'OS (Darwin = macOS, Linux = Linux)
UNAME_S := $(shell uname -s)

# Détection automatique de sdl2-config selon l'environnement
ifeq ($(UNAME_S),Darwin)
    # Sur Mac (Homebrew ou standard)
    SDL2_CONFIG ?= $(shell if [ -x /opt/homebrew/bin/sdl2-config ]; then echo /opt/homebrew/bin/sdl2-config; else command -v sdl2-config 2>/dev/null; fi)
    DL_LIB := 
else
    # Sur Linux (Ubuntu)
    SDL2_CONFIG ?= $(shell command -v sdl2-config 2>/dev/null)
    # Linux nécessite explicitement -ldl pour charger des bibliothèques dynamiques (dlopen)
    DL_LIB := -ldl
endif

# Vérification si SDL2 est installé
ifeq ($(strip $(SDL2_CONFIG)),)
$(error SDL2 introuvable. Installez libsdl2-dev sur Ubuntu ou sdl2 sur Mac.)
endif

# --- Flags ---
# On récupère les flags de compilation et de lien via sdl2-config
CFLAGS = -Wall -Wextra -I. $(shell $(SDL2_CONFIG) --cflags)
LDFLAGS = $(DL_LIB) $(shell $(SDL2_CONFIG) --libs) -lSDL2_ttf -lSDL2_mixer

# --- Cibles ---
TARGET = splash
PLAYERS = p1.so p2.so p3.so p4.so

# Cibles "virtuelles" (ne correspondent pas à des fichiers)
.PHONY: all run clean

# 1. Cible par défaut : construit tout
all: $(TARGET) $(PLAYERS)

# 2. Compilation du moteur principal (le jeu)
$(TARGET): splash.c
	$(CC) $(CFLAGS) splash.c -o $(TARGET) $(LDFLAGS)

# 3. Compilation générique des joueurs en bibliothèques partagées (.so)
# % est un joker qui correspond au numéro du joueur
p%.so: joueur%.c
	$(CC) $(CFLAGS) -fPIC -shared $< -o $@

# 4. Lancement du jeu
# Utilisation de $(addprefix ./, $(PLAYERS)) pour transformer "p1.so" en "./p1.so"
# C'est indispensable sur Linux pour que le programme trouve les fichiers locaux.
run: all
	./$(TARGET) $(addprefix ./, $(PLAYERS))

# 5. Nettoyage
clean:
	rm -f $(TARGET) *.so
