// Gravity Shift - Version con 3 niveles, menu y gravedad corregida
// Compilar (ejemplo):
//   gcc main.c -o GravityShift -lraylib -lm -lpthread -ldl -lrt -lX11
//
// Controles:
//   A / D  -> mover izquierda / derecha
//   F      -> invertir gravedad (solo tocando una superficie)
//   R      -> reiniciar nivel actual
//   ENTER  -> iniciar juego (en la pantalla de inicio) / reiniciar desde victoria
//   ESC    -> salir

#include "raylib.h"
#include <math.h>
/*
typedef struct Platform {
    Rectangle rect;
} Platform;

typedef struct Level {
    Platform *platforms;
    int platformCount;
    Rectangle goal;
    Vector2 playerStart;
} Level;

typedef struct Player {
    Vector2 position;
    Vector2 velocity;
    int width;
    int height;
    bool onGround;
    int gravityDir; // 1 = hacia abajo, -1 = hacia arriba
} Player;

typedef enum {
    GAME_MENU,
    GAME_PLAYING,
    GAME_VICTORY
} GameState;*/

// Gravity Shift - 3 niveles, menu, gravedad y pinchos
// Compilar (ejemplo):
//   gcc main.c -o GravityShift -lraylib -lm -lpthread -ldl -lrt -lX11
//
// Controles:
//   A / D  -> mover izquierda / derecha
//   F      -> invertir gravedad (solo tocando una superficie)
//   R      -> reiniciar nivel actual
//   ENTER  -> iniciar juego / reiniciar desde victoria
//   ESC    -> salir

#include "raylib.h"

typedef struct Platform {
    Rectangle rect;
} Platform;

typedef struct Spike {
    Rectangle rect;
} Spike;

typedef struct Level {
    Platform *platforms;
    int platformCount;
    Spike *spikes;
    int spikeCount;
    Rectangle goal;
    Vector2 playerStart;
} Level;

typedef struct Player {
    Vector2 position;
    Vector2 velocity;
    int width;
    int height;
    bool onGround;
    int gravityDir; // 1 = hacia abajo, -1 = hacia arriba
} Player;

typedef enum {
    GAME_MENU,
    GAME_PLAYING,
    GAME_VICTORY
} GameState;

// ------------ Declaración de niveles -------------

// Nivel 1: sencillo, intro
Platform level1Platforms[] = {
    { (Rectangle){ 0, 400, 800, 50 } },   // piso
    { (Rectangle){ 200, 320, 120, 20 } },
    { (Rectangle){ 420, 280, 140, 20 } },
    { (Rectangle){ 0, 50, 800, 20 } }     // techo
};

Spike level1Spikes[] = {
    // Algunos pinchos en el piso y bajo una plataforma
    { (Rectangle){ 320, 380, 40, 20 } },
    { (Rectangle){ 580, 380, 40, 20 } }
};

// Nivel 2: más huecos, más pinchos
Platform level2Platforms[] = {
    { (Rectangle){ 0,   400, 260, 50 } },
    { (Rectangle){ 320, 400, 160, 50 } },
    { (Rectangle){ 540, 400, 260, 50 } },
    { (Rectangle){ 150, 320, 120, 20 } },
    { (Rectangle){ 360, 270, 120, 20 } },
    { (Rectangle){ 580, 230, 120, 20 } },
    { (Rectangle){ 0,   60, 300, 20 } },
    { (Rectangle){ 500, 60, 300, 20 } }
};

Spike level2Spikes[] = {
    // Entre bloques de piso
    { (Rectangle){ 260, 380, 60, 20 } },
    { (Rectangle){ 480, 380, 60, 20 } },
    // Debajo de plataformas intermedias
    { (Rectangle){ 180, 340, 60, 20 } },
    { (Rectangle){ 390, 290, 60, 20 } },
    // Bajo la plataforma alta del lado derecho
    { (Rectangle){ 600, 250, 60, 20 } }
};

// Nivel 3: vertical y con muchos pinchos
Platform level3Platforms[] = {
    // piso fragmentado
    { (Rectangle){ 0,   400, 180, 50 } },
    { (Rectangle){ 260, 400, 120, 50 } },
    { (Rectangle){ 430, 400, 120, 50 } },
    { (Rectangle){ 610, 400, 190, 50 } },

    // plataformas intermedias
    { (Rectangle){ 120, 320, 120, 20 } },
    { (Rectangle){ 320, 290, 120, 20 } },
    { (Rectangle){ 520, 260, 120, 20 } },

    // techo fragmentado
    { (Rectangle){ 0,   60, 200, 20 } },
    { (Rectangle){ 300, 60, 150, 20 } },
    { (Rectangle){ 520, 60, 280, 20 } }
};

Spike level3Spikes[] = {
    // Suelo entre fragmentos
    { (Rectangle){ 180, 380, 80, 20 } },
    { (Rectangle){ 380, 380, 80, 20 } },
    // En medio del nivel
    { (Rectangle){ 240, 340, 60, 20 } },
    { (Rectangle){ 440, 310, 60, 20 } },
    { (Rectangle){ 640, 280, 80, 20 } },
    // Bajo el techo (para castigar mal uso de la gravedad invertida)
    { (Rectangle){ 60,  80, 60, 20 } },
    { (Rectangle){ 340, 80, 60, 20 } },
    { (Rectangle){ 600, 80, 80, 20 } }
};

// Definición de niveles
Level levels[3];

// ------------ Auxiliares ---------------------

Rectangle GetPlayerRect(Player p) {
    return (Rectangle){ p.position.x, p.position.y, (float)p.width, (float)p.height };
}

void LoadLevels() {
    // Nivel 1
    levels[0].platforms     = level1Platforms;
    levels[0].platformCount = sizeof(level1Platforms)/sizeof(level1Platforms[0]);
    levels[0].spikes        = level1Spikes;
    levels[0].spikeCount    = sizeof(level1Spikes)/sizeof(level1Spikes[0]);
    levels[0].goal          = (Rectangle){ 730, 360, 40, 40 };
    levels[0].playerStart   = (Vector2){ 40, 360 };

    // Nivel 2
    levels[1].platforms     = level2Platforms;
    levels[1].platformCount = sizeof(level2Platforms)/sizeof(level2Platforms[0]);
    levels[1].spikes        = level2Spikes;
    levels[1].spikeCount    = sizeof(level2Spikes)/sizeof(level2Spikes[0]);
    levels[1].goal          = (Rectangle){ 740, 360, 40, 40 };
    levels[1].playerStart   = (Vector2){ 40, 360 };

    // Nivel 3
    levels[2].platforms     = level3Platforms;
    levels[2].platformCount = sizeof(level3Platforms)/sizeof(level3Platforms[0]);
    levels[2].spikes        = level3Spikes;
    levels[2].spikeCount    = sizeof(level3Spikes)/sizeof(level3Spikes[0]);
    // Meta accesible sobre el último bloque de piso
    levels[2].goal          = (Rectangle){ 700, 360, 40, 40 };
    levels[2].playerStart   = (Vector2){ 40, 360 };
}

void ResetPlayer(Player *p, Level level) {
    p->position   = level.playerStart;
    p->velocity   = (Vector2){ 0, 0 };
    p->width      = 24;
    p->height     = 32;
    p->onGround   = false;
    p->gravityDir = 1;
}

// Colisiones sencillas eje por eje
void ResolveCollisions(Player *p, Level level, float dt, float gravity) {
    Rectangle playerRect;
    p->onGround = false;

    // Horizontal
    p->position.x += p->velocity.x * dt;
    playerRect = GetPlayerRect(*p);

    for (int i = 0; i < level.platformCount; i++) {
        if (CheckCollisionRecs(playerRect, level.platforms[i].rect)) {
            Rectangle plat = level.platforms[i].rect;
            if (p->velocity.x > 0) {
                p->position.x = plat.x - p->width;
            } else if (p->velocity.x < 0) {
                p->position.x = plat.x + plat.width;
            }
            playerRect = GetPlayerRect(*p);
        }
    }

    // Vertical (gravedad)
    p->velocity.y += gravity * p->gravityDir * dt;
    p->position.y += p->velocity.y * dt;
    playerRect = GetPlayerRect(*p);

    for (int i = 0; i < level.platformCount; i++) {
        Rectangle plat = level.platforms[i].rect;
        if (CheckCollisionRecs(playerRect, plat)) {
            if (p->gravityDir == 1) {
                p->position.y = plat.y - p->height;
            } else {
                p->position.y = plat.y + plat.height;
            }
            p->velocity.y = 0;
            p->onGround   = true;
            playerRect    = GetPlayerRect(*p);
        }
    }
}

int main(void) {
    const int screenWidth  = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "Gravity Shift - 3 niveles con pinchos");
    SetTargetFPS(60);

    LoadLevels();

    Player player;
    int currentLevel = 0;
    ResetPlayer(&player, levels[currentLevel]);

    float moveSpeed = 200.0f;
    float gravity   = 700.0f;

    GameState state = GAME_MENU;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (state == GAME_MENU) {
            if (IsKeyPressed(KEY_ENTER)) {
                state = GAME_PLAYING;
                currentLevel = 0;
                ResetPlayer(&player, levels[currentLevel]);
            }
        }
        else if (state == GAME_PLAYING) {
            if (IsKeyPressed(KEY_R)) {
                ResetPlayer(&player, levels[currentLevel]);
            }

            // Movimiento
            float move = 0.0f;
            if (IsKeyDown(KEY_A)) move -= 1.0f;
            if (IsKeyDown(KEY_D)) move += 1.0f;
            player.velocity.x = move * moveSpeed;

            // Invertir gravedad solo si está en superficie
            if (IsKeyPressed(KEY_F) && player.onGround) {
                player.gravityDir *= -1;
                player.velocity.y = 0;
                player.onGround   = false;
            }

            // Física y plataformas
            ResolveCollisions(&player, levels[currentLevel], dt, gravity);

            // Caer fuera de pantalla = muerte
            if (player.position.y > screenHeight + 80 ||
                player.position.y < -80) {
                ResetPlayer(&player, levels[currentLevel]);
            }

            Rectangle playerRect = GetPlayerRect(player);

            // Colisión con pinchos = muerte
            for (int i = 0; i < levels[currentLevel].spikeCount; i++) {
                if (CheckCollisionRecs(playerRect, levels[currentLevel].spikes[i].rect)) {
                    ResetPlayer(&player, levels[currentLevel]);
                    break;
                }
            }

            // Meta
            playerRect = GetPlayerRect(player);
            if (CheckCollisionRecs(playerRect, levels[currentLevel].goal)) {
                if (currentLevel < 2) {
                    currentLevel++;
                    ResetPlayer(&player, levels[currentLevel]);
                } else {
                    state = GAME_VICTORY;
                }
            }
        }
        else if (state == GAME_VICTORY) {
            if (IsKeyPressed(KEY_ENTER)) {
                state = GAME_PLAYING;
                currentLevel = 0;
                ResetPlayer(&player, levels[currentLevel]);
            }
        }

        // ---------- DIBUJO ----------
        BeginDrawing();
        ClearBackground((Color){25, 25, 40, 255});

        if (state == GAME_MENU) {
            DrawText("GRAVITY SHIFT", 230, 110, 40, YELLOW);
            DrawText("3 niveles con pinchos y gravedad invertible", 150, 160, 20, RAYWHITE);
            DrawText("Controles:", 150, 210, 22, RAYWHITE);
            DrawText("A / D  - mover", 170, 240, 20, RAYWHITE);
            DrawText("F      - invertir gravedad (tocando suelo/techo)", 170, 270, 20, RAYWHITE);
            DrawText("R      - reiniciar nivel", 170, 300, 20, RAYWHITE);
            DrawText("ENTER  - empezar", 170, 330, 20, GREEN);
        }
        else if (state == GAME_PLAYING) {
            Level *lvl = &levels[currentLevel];

            // Plataformas
            for (int i = 0; i < lvl->platformCount; i++) {
                DrawRectangleRec(lvl->platforms[i].rect,
                                 (Color){90, 90, 140, 255});
            }

            // Pinchos (rectángulos rojos)
            for (int i = 0; i < lvl->spikeCount; i++) {
                DrawRectangleRec(lvl->spikes[i].rect, (Color){200, 40, 40, 255});
            }

            // Meta
            DrawRectangleRec(lvl->goal, (Color){0, 200, 0, 255});

            // Jugador
            Rectangle playerRect = GetPlayerRect(player);
            DrawRectangleRec(playerRect, (Color){230, 230, 50, 255});

            DrawText(TextFormat("Nivel %d/3", currentLevel + 1), 10, 10, 20, RAYWHITE);
            DrawText("A/D: mover   F: invertir gravedad (tocando superficie)   R: reiniciar   ESC: salir",
                     10, screenHeight - 30, 16, RAYWHITE);
        }
        else if (state == GAME_VICTORY) {
            DrawText("¡HAS COMPLETADO LOS 3 NIVELES!", 140, 180, 26, YELLOW);
            DrawText("Cuidado con los pinchos ;)", 220, 220, 20, RAYWHITE);
            DrawText("Presiona ENTER para jugar otra vez o ESC para salir.",
                     140, 260, 20, RAYWHITE);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}