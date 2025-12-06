#include "raylib.h"

#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h> 

#define SCREEN_W      960
#define SCREEN_H      540
#define FPS           60

#define MAX_PLATFORMS 30
#define MAX_COINS     40
#define MAX_SLIMES    10
#define MAX_LEVELS    3

// TAMAÑOS ESCALADOS 
#define PLAYER_SCALE  2.0f
#define PLAYER_SIZE_W (int)(16 * PLAYER_SCALE)
#define PLAYER_SIZE_H (int)(28 * PLAYER_SCALE)
#define COIN_SIZE     32.0f 
#define SLIME_SIZE_W  (int)(32 * PLAYER_SCALE)
#define SLIME_SIZE_H  (int)(28 * PLAYER_SCALE)

// TAMAÑO BASE DEL SPRITE DE PLATAFORMA 
#define PLATFORM_TILE_W 32.0f
#define PLATFORM_TILE_H 16.0f

// CONSTANTES DE PUERTA
#define DOOR_W        32
#define DOOR_H        48 

// ===> ANIMACIÓN AÑADIDA <===
#define PLAYER_FRAME_WIDTH 16.0f
#define ANIMATION_SPEED    0.1f // Tiempo que dura cada frame de animación (en segundos)
#define WALK_FRAMES        4    // Asumiendo que los primeros 4 frames son para caminar

// ---------------------------------------------------------------------------
//   GENERADOR DE ONDA DE SONIDO 
// ---------------------------------------------------------------------------
Wave GenerateSineWave(float frequency, float seconds, int sampleRate)
{
    int sampleCount = (int)(seconds * sampleRate);
    short *data = calloc(sampleCount, sizeof(short)); 
    if (data == NULL) return (Wave){ 0 };

    for (int i = 0; i < sampleCount; i++)
    {
        float t = (float)i / sampleRate;
        float v = sinf(2.0f * PI * frequency * t);
        data[i] = (short)(v * 32000);
    }

    Wave w = { 0 };
    w.frameCount = sampleCount;
    w.sampleRate = sampleRate;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = data;
    return w;
}

// ---------------------------------------------------------------------------
//                            ESTRUCTURAS
// ---------------------------------------------------------------------------
typedef struct {
    Rectangle rect;
    bool isMoving;
    Vector2 dir;
    float range;
    float speed;
    Vector2 startPos;
} Platform;

typedef struct {
    Vector2 center;
    bool collected;
} Coin;

typedef struct {
    Rectangle rect;
    bool isLocked;
} Door;

typedef enum { SLIME_GREEN, SLIME_PURPLE } SlimeType;

typedef struct {
    Rectangle rect;
    SlimeType type;
    float speed;
    Vector2 startPos;
    float range;
    int dir;
    bool isActive;
} Slime;

typedef enum { MENU, PLAYING, GAMEOVER, VICTORY } GameState;

// ---------------------------------------------------------------------------
int currentLevel = 1;
int score = 0;
bool isGravityInverted = false; 

// ===> VARIABLES DE ANIMACIÓN <===
float animTimer = 0.0f;
int currentFrame = 0;
float playerFacing = 1.0f; // 1.0f = Derecha, -1.0f = Izquierda

// Texturas y Fuentes (RUTAS ABSOLUTAS)
Texture2D texKnight;
Texture2D texCoin;
Texture2D texPlatforms;
Texture2D texSlimePurple;
Texture2D texDoor; 
Font gameFont;

// Rectángulos de origen para el sprite sheet
Rectangle texRecPlayer = { 0, 0, 16, 28 }; 
Rectangle texRecCoin = { 0, 0, 16, 16 };   
Rectangle texRecSlimePurple = { 0, 0, 32, 28 }; 
Rectangle texRecPlatform = { 0, 0, 32, 16 }; 

// Sonidos
Sound fxCoin;
Sound fxExplosion;
Sound fxHurt;
Sound fxJump;

// Puerta del nivel actual
Door levelDoor; 

// ---------------------------------------------------------------------------
// PROTOTIPOS DE FUNCIONES 
// ---------------------------------------------------------------------------
void LoadAssets(void);
void UnloadAssets(void);
Color GetPlatformColor(int level);
bool CheckCollisionRectEx(Rectangle a, Rectangle b);
bool IsTouchingSurface(Rectangle player, Platform platforms[], int platformCount, bool isAbove);
void ResolveVerticalCollisions(Rectangle *player, float *velY, Platform platforms[], int platformCount, bool gravityInverted);
void ResolveHorizontalCollisions(Rectangle *player, Platform platforms[], int platformCount);
void UpdatePlatforms(Platform platforms[], int platformCount, float dt);
void UpdateSlimes(Slime slimes[], int slimeCount, float dt);
void CheckCoinCollection(Rectangle player, Coin coins[], int coinCount, int *score);
bool CheckSlimeCollision(Rectangle player, Slime slimes[], int slimeCount);
void LoadLevel(int levelId, Rectangle *player, float *velY,
               Platform platforms[], int *platformCount,
               Coin coins[], int *coinCount,
               Slime slimes[], int *slimeCount,
               Door *door); 

// ---------------------------------------------------------------------------
//                     DEFINICIONES DE FUNCIONES
// ---------------------------------------------------------------------------
// Las funciones auxiliares (GetPlatformColor, CheckCollisionRectEx, etc.) se mantienen igual.
Color GetPlatformColor(int level) {
    switch (level) {
        case 1: return (Color){139, 69, 19, 255};
        case 2: return (Color){0, 105, 148, 255};
        case 3: return (Color){85, 26, 139, 255};
        default: return DARKGRAY;
    }
}

bool CheckCollisionRectEx(Rectangle a, Rectangle b) {
    return !(a.x + a.width <= b.x || a.x >= b.x + b.width ||
             a.y + a.height <= b.y || a.y >= b.y + b.height);
}

bool IsTouchingSurface(Rectangle player, Platform platforms[], int platformCount, bool isAbove) {
    Rectangle onePixel = player;
    onePixel.y += isAbove ? -1 : 1;

    for (int i = 0; i < platformCount; i++) {
        if (CheckCollisionRectEx(onePixel, platforms[i].rect)) return true;
    }
    return false;
}

void ResolveVerticalCollisions(Rectangle *player, float *velY, Platform platforms[], int platformCount, bool gravityInverted) 
{
    for (int i = 0; i < platformCount; i++) {
        Rectangle p = platforms[i].rect;

        if (CheckCollisionRectEx(*player, p)) {
            float centerPlayer = player->y + player->height * 0.5f;
            float centerPlat   = p.y + p.height * 0.5f;

            if (!gravityInverted) {
                if (*velY > 0 && centerPlayer < centerPlat) {
                    player->y = p.y - player->height;
                    *velY = 0;
                } else if (*velY < 0 && centerPlayer > centerPlat) {
                    player->y = p.y + p.height;
                    *velY = 0;
                }
            } else {
                if (*velY < 0 && centerPlayer > centerPlat) {
                    player->y = p.y + p.height;
                    *velY = 0;
                } else if (*velY > 0 && centerPlayer < centerPlat) {
                    player->y = p.y - player->height;
                    *velY = 0;
                }
            }
        }
    }
}

void ResolveHorizontalCollisions(Rectangle *player, Platform platforms[], int platformCount) 
{
    for (int i = 0; i < platformCount; i++) {
        Rectangle p = platforms[i].rect;

        if (CheckCollisionRectEx(*player, p)) {
            float overlapLeft  = (player->x + player->width) - p.x;
            float overlapRight = (p.x + p.width) - player->x;

            if (overlapLeft < overlapRight && overlapLeft > 0)
                player->x -= overlapLeft;
            else if (overlapRight > 0)
                player->x += overlapRight;
        }
    }
}

void UpdatePlatforms(Platform platforms[], int platformCount, float dt) 
{
    for (int i = 0; i < platformCount; i++) {
        if (!platforms[i].isMoving) continue;

        platforms[i].rect.x += platforms[i].dir.x * platforms[i].speed * dt;
        platforms[i].rect.y += platforms[i].dir.y * platforms[i].speed * dt;

        float dx = platforms[i].rect.x - platforms[i].startPos.x;
        float dy = platforms[i].rect.y - platforms[i].startPos.y;
        float dist = sqrtf(dx*dx + dy*dy);

        if (dist >= platforms[i].range) {
            platforms[i].dir.x *= -1;
            platforms[i].dir.y *= -1;
            platforms[i].startPos = (Vector2){ platforms[i].rect.x, platforms[i].rect.y };
        }
    }
}

void UpdateSlimes(Slime slimes[], int slimeCount, float dt) 
{
    for (int i = 0; i < slimeCount; i++) {
        if (!slimes[i].isActive) continue;

        slimes[i].rect.x += slimes[i].dir * slimes[i].speed * dt;

        float dx = slimes[i].rect.x - slimes[i].startPos.x;
        float dist = fabsf(dx);

        if (dist >= slimes[i].range) {
            slimes[i].dir *= -1;
            slimes[i].startPos.x = slimes[i].rect.x;
        }
    }
}

void CheckCoinCollection(Rectangle player, Coin coins[], int coinCount, int *score) 
{
    for (int i = 0; i < coinCount; i++) {
        if (!coins[i].collected) {
            if (CheckCollisionCircleRec(coins[i].center, COIN_SIZE / 2, player)) {
                coins[i].collected = true;
                (*score)++;
                PlaySound(fxCoin);
            }
        }
    }
}

bool CheckSlimeCollision(Rectangle player, Slime slimes[], int slimeCount) {
    for (int i = 0; i < slimeCount; i++)
        if (slimes[i].isActive && CheckCollisionRectEx(player, slimes[i].rect))
            return true;
    return false;
}

// ---------------------------------------------------------------------------
//                   CARGA Y DESCARGA DE ASSETS (RUTAS ABSOLUTAS)
// ---------------------------------------------------------------------------
void LoadAssets(void)
{
    // Carga de Texturas (Asegúrate de que las rutas sean correctas)
    texKnight = LoadTexture("sprites/knight.png");
    texCoin = LoadTexture("sprites/coin.png");
    texPlatforms = LoadTexture("sprites/platforms.png");
    texSlimePurple = LoadTexture("sprites/slime_purple.png");
    texDoor = LoadTexture("sprites/door.png"); 

    // Carga de Fuente
    gameFont = LoadFont("fonts/PixelOperator8-Bold.ttf");

    InitAudioDevice();
    if (IsAudioDeviceReady()) 
    {
        Wave w1 = GenerateSineWave(440, 0.1f, 44100);
        Wave w2 = GenerateSineWave(200, 0.3f, 44100);
        Wave w3 = GenerateSineWave(800, 0.1f, 44100);

        fxCoin = LoadSoundFromWave(w1);
        fxHurt = LoadSoundFromWave(w2);
        fxJump = LoadSoundFromWave(w3);

        UnloadWave(w1);
        UnloadWave(w2);
        UnloadWave(w3);
    }
}

void UnloadAssets(void)
{
    UnloadTexture(texKnight);
    UnloadTexture(texCoin);
    UnloadTexture(texPlatforms);
    UnloadTexture(texSlimePurple);
    UnloadTexture(texDoor);
    UnloadFont(gameFont);

    if (IsAudioDeviceReady()) {
        UnloadSound(fxCoin);
        UnloadSound(fxHurt);
        UnloadSound(fxJump);
        CloseAudioDevice();
    }
}

// ---------------------------------------------------------------------------
//                           CARGA DE NIVELES (3 NIVELES)
// ---------------------------------------------------------------------------

void LoadLevel(int levelId, Rectangle *player, float *velY,
               Platform platforms[], int *platformCount,
               Coin coins[], int *coinCount,
               Slime slimes[], int *slimeCount,
               Door *door)
{
    *platformCount = 0;
    *coinCount     = 0;
    *slimeCount    = 0;
    *velY          = 0.0f;
    *player = (Rectangle){ 0, 0, PLAYER_SIZE_W, PLAYER_SIZE_H };
    
    door->isLocked = true;
    isGravityInverted = false;

    // Altura del suelo (Y)
    float groundY = 500.0f;
    float ceilingY = 40.0f;
    float coinOffset = COIN_SIZE / 2.0f;

    // Reiniciar animación al cargar nivel
    animTimer = 0.0f;
    currentFrame = 0;
    playerFacing = 1.0f;

    switch (levelId)
    {
        case 1: 
            player->x = 50.0f;
            player->y = groundY - PLAYER_SIZE_H - 40.0f; 
            
            door->rect = (Rectangle){ SCREEN_W - 80, groundY - 40 - DOOR_H, DOOR_W, DOOR_H };

            // Suelo y Techo
            platforms[(*platformCount)++] = (Platform){ { 0, groundY, SCREEN_W, 40 }, false, {0}, 0, 0, {0} };
            platforms[(*platformCount)++] = (Platform){ { 0, 0, SCREEN_W, 40 }, false, {0}, 0, 0, {0} };
            
            // Plataformas intermedias
            platforms[(*platformCount)++] = (Platform){ { 200, 350, 150, 20 }, false, {0}, 0, 0, {0} };
            platforms[(*platformCount)++] = (Platform){ { 450, 200, 100, 20 }, false, {0}, 0, 0, {0} };

            // Monedas 
            coins[(*coinCount)++] = (Coin){ { 275.0f, 350.0f - coinOffset }, false }; 
            coins[(*coinCount)++] = (Coin){ { 500.0f, 200.0f - coinOffset }, false }; 
            coins[(*coinCount)++] = (Coin){ { 850.0f, groundY - 40.0f - coinOffset }, false }; 
            
            // Slimes 
            slimes[(*slimeCount)++] = (Slime){ { 650, groundY - SLIME_SIZE_H, SLIME_SIZE_W, SLIME_SIZE_H }, SLIME_PURPLE, 70.0f, {650.0f, groundY - SLIME_SIZE_H}, 100.0f, 1, true };
            
            break;

        case 2: 
            player->x = 50.0f;
            player->y = groundY - PLAYER_SIZE_H - 40.0f;
            
            // Puerta: Accessible por el agujero en el techo
            door->rect = (Rectangle){ 250, ceilingY, DOOR_W, DOOR_H }; 
            
            // Suelo, Techo (con agujero) y Plataformas
            platforms[(*platformCount)++] = (Platform){ { 0, groundY, SCREEN_W, 40 }, false, {0}, 0, 0, {0} };
            platforms[(*platformCount)++] = (Platform){ { 0, 0, 200, 40 }, false, {0}, 0, 0, {0} };       
            platforms[(*platformCount)++] = (Platform){ { 350, 0, SCREEN_W-350, 40 }, false, {0}, 0, 0, {0} }; 
            
            // Plataformas para acceder al techo
            platforms[(*platformCount)++] = (Platform){ { 100, 400, 100, 20 }, false, {0}, 0, 0, {0} }; // Peldaño 1
            platforms[(*platformCount)++] = (Platform){ { 300, 300, 100, 20 }, false, {0}, 0, 0, {0} }; // Peldaño 2
            
            // Plataforma Móvil para desafío
            platforms[(*platformCount)++] = (Platform){ { 500, 350, 100, 20 }, true, {-1.0f, 0.0f}, 250.0f, 120.0f, {500.0f, 350.0f} }; 

            // Monedas (4 monedas para más desafío)
            coins[(*coinCount)++] = (Coin){ { 150.0f, 400.0f - coinOffset }, false }; // Sobre Peldaño 1
            coins[(*coinCount)++] = (Coin){ { 550.0f, 350.0f - coinOffset }, false }; // Sobre plataforma móvil
            coins[(*coinCount)++] = (Coin){ { 350.0f, 300.0f - coinOffset }, false }; // Sobre Peldaño 2
            coins[(*coinCount)++] = (Coin){ { 275.0f, ceilingY + DOOR_H + coinOffset }, false }; // Cerca de la puerta (arriba)

            // Slimes (AÑADIDO UN SLIME EXTRA, total 2)
            slimes[(*slimeCount)++] = (Slime){ { 100, groundY - SLIME_SIZE_H, SLIME_SIZE_W, SLIME_SIZE_H }, SLIME_PURPLE, 75.0f, {100.0f, groundY - SLIME_SIZE_H}, 150.0f, 1, true };
            slimes[(*slimeCount)++] = (Slime){ { 700, groundY - SLIME_SIZE_H, SLIME_SIZE_W, SLIME_SIZE_H }, SLIME_PURPLE, 95.0f, {700.0f, groundY - SLIME_SIZE_H}, 120.0f, -1, true };
            
            break;
            
        case 3: 
            player->x = 50.0f;
            player->y = groundY - PLAYER_SIZE_H - 40.0f;
            
            door->rect = (Rectangle){ 850, groundY - 40 - DOOR_H, DOOR_W, DOOR_H };
            
            // Suelo 
            platforms[(*platformCount)++] = (Platform){ { 0, groundY, 300, 40 }, false, {0}, 0, 0, {0} };
            platforms[(*platformCount)++] = (Platform){ { 400, groundY, SCREEN_W - 400, 40 }, false, {0}, 0, 0, {0} };

            // Techo 
            platforms[(*platformCount)++] = (Platform){ { 0, 0, 450, 40 }, false, {0}, 0, 0, {0} }; 
            platforms[(*platformCount)++] = (Platform){ { 550, 0, SCREEN_W - 550, 40 }, false, {0}, 0, 0, {0} }; 
            
            // Plataformas INTERMEDIAS
            platforms[(*platformCount)++] = (Platform){ { 200, 400, 100, 20 }, false, {0}, 0, 0, {0} }; 
            platforms[(*platformCount)++] = (Platform){ { 400, 300, 100, 20 }, false, {0}, 0, 0, {0} }; 
            platforms[(*platformCount)++] = (Platform){ { 600, 200, 100, 20 }, false, {0}, 0, 0, {0} }; 
            
            // Plataforma Móvil
            platforms[(*platformCount)++] = (Platform){ { 100, 100, 100, 20 }, true, {1.0f, 0.0f}, 250.0f, 80.0f, {100.0f, 100.0f} }; 
            
            // Monedas 
            coins[(*coinCount)++] = (Coin){ { 250.0f, 400.0f - coinOffset }, false }; 
            coins[(*coinCount)++] = (Coin){ { 450.0f, 300.0f - coinOffset }, false }; 
            coins[(*coinCount)++] = (Coin){ { 650.0f, 200.0f - coinOffset }, false }; 
            coins[(*coinCount)++] = (Coin){ { 750.0f, 100.0f - coinOffset }, false }; 

            // Slimes (SIN TRAMPAS)
            slimes[(*slimeCount)++] = (Slime){ { 100, groundY - SLIME_SIZE_H, SLIME_SIZE_W, SLIME_SIZE_H }, SLIME_PURPLE, 80.0f, {100.0f, groundY - SLIME_SIZE_H}, 180.0f, 1, true };
            slimes[(*slimeCount)++] = (Slime){ { 700, groundY - SLIME_SIZE_H, SLIME_SIZE_W, SLIME_SIZE_H }, SLIME_PURPLE, 90.0f, {700.0f, groundY - SLIME_SIZE_H}, 150.0f, -1, true };
            slimes[(*slimeCount)++] = (Slime){ { 500, ceilingY, SLIME_SIZE_W, SLIME_SIZE_H }, SLIME_PURPLE, 100.0f, {500.0f, ceilingY}, 300.0f, 1, true };
            
            break;

        default:
            break;
    }
}


// ---------------------------------------------------------------------------
//                                   MAIN
// ---------------------------------------------------------------------------
int main(void)
{
    InitWindow(SCREEN_W, SCREEN_H, "Gravity Shift Platformer");
    LoadAssets(); 

    SetTargetFPS(FPS);

    GameState gamestate = MENU;

    Rectangle player = { 100, 100, PLAYER_SIZE_W, PLAYER_SIZE_H };
    float velY = 0.0f;
    float gravity = 600.0f;

    Platform platforms[MAX_PLATFORMS];
    Coin coins[MAX_COINS];
    Slime slimes[MAX_SLIMES];

    int platformCount = 0;
    int coinCount = 0;
    int slimeCount = 0;
    
    isGravityInverted = false;

    LoadLevel(currentLevel, &player, &velY,
              platforms, &platformCount,
              coins, &coinCount,
              slimes, &slimeCount,
              &levelDoor);

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        bool isMoving = false; // Variable para saber si el jugador está caminando

        switch (gamestate)
        {
        case MENU:
            if (IsKeyPressed(KEY_ENTER)) {
                gamestate = PLAYING;
            }
            break;

        case PLAYING:
        { 
            float moveSpeed = 200.0f; 
            
            // --- Lógica de Movimiento y Orientación ---
            if (IsKeyDown(KEY_LEFT)) {
                player.x -= moveSpeed * dt;
                playerFacing = -1.0f; // Mirando a la izquierda
                isMoving = true;
            }
            if (IsKeyDown(KEY_RIGHT)) {
                player.x += moveSpeed * dt;
                playerFacing = 1.0f; // Mirando a la derecha
                isMoving = true;
            }
            
            bool onGround = !isGravityInverted ?
                IsTouchingSurface(player, platforms, platformCount, false) :
                IsTouchingSurface(player, platforms, platformCount, true);

            // ----------- Salto -----------
            if (onGround && IsKeyPressed(KEY_SPACE))
            {
                velY = isGravityInverted ?  -350.0f : 350.0f; 
                velY *= -1.0f; 
                PlaySound(fxJump);
            }

            // ----------- Cambio de gravedad (SOLO EN EL PISO) -----------
            if (onGround && IsKeyPressed(KEY_UP))
                isGravityInverted = !isGravityInverted;

            // ===> Lógica de Animación (Update) <===
            if (isMoving && onGround)
            {
                animTimer += dt;
                if (animTimer >= ANIMATION_SPEED)
                {
                    // Cicla los frames de caminar (0, 1, 2, 3)
                    currentFrame = (currentFrame + 1) % WALK_FRAMES; 
                    animTimer = 0.0f;
                }
            }
            else
            {
                // Reposo (Idle) o Salto/Caída
                currentFrame = 0; 
                animTimer = 0.0f;
            }
            
            // Actualizar el rectángulo de origen de la textura del jugador (Frame X)
            texRecPlayer.x = currentFrame * PLAYER_FRAME_WIDTH;
            
            // ----------- Física vertical -----------
            velY += (isGravityInverted ? -gravity : gravity) * dt;
            player.y += velY * dt;

            ResolveVerticalCollisions(&player, &velY, platforms, platformCount, isGravityInverted);
            ResolveHorizontalCollisions(&player, platforms, platformCount);
            
            if (player.x < 0) player.x = 0;
            if (player.x + player.width > SCREEN_W) player.x = SCREEN_W - player.width;

            UpdatePlatforms(platforms, platformCount, dt);
            UpdateSlimes(slimes, slimeCount, dt);

            CheckCoinCollection(player, coins, coinCount, &score);

            // ----------- Colisiones mortales -----------
            if (CheckSlimeCollision(player, slimes, slimeCount))
            {
                PlaySound(fxHurt);
                gamestate = GAMEOVER;
            }

            // ===> Game Over por volar fuera de límites (CORREGIDO) <===
            if ((isGravityInverted && (player.y + player.height < 0)) || // Si gravedad invertida Y sales por el TECHO
                (!isGravityInverted && (player.y > SCREEN_H)))          // O si gravedad normal Y sales por el SUELO
            {
                 PlaySound(fxHurt);
                 gamestate = GAMEOVER;
            }
            
            // ----------- Lógica de la Puerta y Fin del Nivel -----------
            bool allCollected = true;
            for (int i = 0; i < coinCount; i++)
                if (!coins[i].collected) allCollected = false;
            
            if (allCollected) levelDoor.isLocked = false;

            if (!levelDoor.isLocked && CheckCollisionRectEx(player, levelDoor.rect))
            {
                currentLevel++;
                if (currentLevel > MAX_LEVELS) gamestate = VICTORY;
                else {
                    LoadLevel(currentLevel, &player, &velY,
                              platforms, &platformCount,
                              coins, &coinCount,
                              slimes, &slimeCount,
                              &levelDoor);
                }
            }
        } 
        break;

        case GAMEOVER:
            if (IsKeyPressed(KEY_ENTER))
            {
                currentLevel = 1;
                score = 0;
                isGravityInverted = false; 
                LoadLevel(currentLevel, &player, &velY, platforms, &platformCount, coins, &coinCount, slimes, &slimeCount, &levelDoor);
                gamestate = PLAYING;
            }
            break;

        case VICTORY:
            if (IsKeyPressed(KEY_ENTER))
            {
                currentLevel = 1;
                score = 0;
                isGravityInverted = false; 
                LoadLevel(currentLevel, &player, &velY, platforms, &platformCount, coins, &coinCount, slimes, &slimeCount, &levelDoor);
                gamestate = MENU;
            }
            break;
        }

        // -------------------------------------------------------------------
        //                               DRAW 
        // -------------------------------------------------------------------
        BeginDrawing();
        ClearBackground((Color){135, 206, 235, 255}); // AZUL CIELO
        
        int fontSize = 20; 
        int victoryFontSize = 60; 
        
        // --- Cálculo de si la puerta está desbloqueada para mostrar el mensaje ---
        bool allCoinsCollected = true;
        if (gamestate == PLAYING) {
            for (int i = 0; i < coinCount; i++)
                if (!coins[i].collected) allCoinsCollected = false;
        }

        switch (gamestate)
        {
        case MENU:
            DrawTextEx(gameFont, "GRAVITY SHIFT PLATFORMER", (Vector2){SCREEN_W/2 - MeasureTextEx(gameFont, "GRAVITY SHIFT PLATFORMER", fontSize, 0).x/2, SCREEN_H/2 - 40}, fontSize, 0, RAYWHITE);
            DrawTextEx(gameFont, "Presiona ENTER para comenzar", (Vector2){SCREEN_W/2 - MeasureTextEx(gameFont, "Presiona ENTER para comenzar", fontSize, 0).x/2, SCREEN_H/2 + 20}, fontSize, 0, GRAY);
            break;

        case PLAYING:
        { 
            // Dibujar plataformas 
            Rectangle sourcePlat = { 0, 0, PLATFORM_TILE_W, PLATFORM_TILE_H }; 
            for (int i = 0; i < platformCount; i++)
            {
                int numTiles = (int)ceilf(platforms[i].rect.width / PLATFORM_TILE_W);
                for (int j = 0; j < numTiles; j++)
                {
                    Rectangle dest = { platforms[i].rect.x + j * PLATFORM_TILE_W, 
                                       platforms[i].rect.y, 
                                       PLATFORM_TILE_W, 
                                       platforms[i].rect.height };

                    if (dest.x + dest.width > platforms[i].rect.x + platforms[i].rect.width)
                    {
                        sourcePlat.width = platforms[i].rect.x + platforms[i].rect.width - dest.x;
                        dest.width = sourcePlat.width;
                    } else {
                        sourcePlat.width = PLATFORM_TILE_W;
                    }
                    
                    DrawTexturePro(texPlatforms, sourcePlat, dest, (Vector2){0}, 0.0f, WHITE);
                }
                sourcePlat.width = PLATFORM_TILE_W; 
            }

            // Dibujar monedas
            for (int i = 0; i < coinCount; i++) {
                if (!coins[i].collected) {
                    Vector2 coinDrawPos = { coins[i].center.x - COIN_SIZE / 2, coins[i].center.y - COIN_SIZE / 2 };
                    DrawTextureRec(texCoin, texRecCoin, coinDrawPos, WHITE); 
                }
            }
            
            // Dibujar slimes Púrpura (ESCALADOS)
            for (int i = 0; i < slimeCount; i++)
                if (slimes[i].isActive)
                    DrawTexturePro(texSlimePurple, texRecSlimePurple, slimes[i].rect, (Vector2){0}, 0.0f, WHITE);


            // ===> Dibujar jugador con animación y flip (ESCALADO) <===
            Rectangle destRecPlayer = player;
            
            // Aplicar 'flip': Multiplicamos el ancho del sprite de origen por la dirección
            Rectangle finalSourceRec = texRecPlayer;
            finalSourceRec.width *= playerFacing; 
            
            DrawTexturePro(texKnight, finalSourceRec, destRecPlayer, (Vector2){0}, 0.0f, WHITE);
            
            // Dibujar Puerta (cerrada o abierta)
            Rectangle doorSource = levelDoor.isLocked ? (Rectangle){0, 0, 32, 48} : (Rectangle){32, 0, 32, 48}; 
            DrawTexturePro(texDoor, doorSource, levelDoor.rect, (Vector2){0}, 0.0f, WHITE);

            // --- HUD y MENSAJE DE PUERTA ---
            DrawTextEx(gameFont, TextFormat("Nivel: %d/%d", currentLevel, MAX_LEVELS), (Vector2){10, 10}, fontSize, 0, RAYWHITE);
            DrawTextEx(gameFont, TextFormat("Puntaje: %d/%d", score, coinCount), (Vector2){10, 35}, fontSize, 0, RAYWHITE);
            DrawTextEx(gameFont, isGravityInverted ? "Gravedad: Arriba (UP)" : "Gravedad: Abajo (UP)", (Vector2){10, 60}, fontSize, 0, RAYWHITE);
            
            // Mensaje de Desbloqueo de Puerta
            if (allCoinsCollected) {
                const char *msg = "¡PUERTA DESBLOQUEADA! Adelante (->)";
                float msgW = MeasureTextEx(gameFont, msg, fontSize, 0).x;
                DrawTextEx(gameFont, msg, (Vector2){SCREEN_W/2 - msgW/2, 90}, fontSize, 0, GREEN);
            }
            
        } 
        break;

        case GAMEOVER:
            DrawTextEx(gameFont, "GAME OVER", (Vector2){SCREEN_W/2 - MeasureTextEx(gameFont, "GAME OVER", 40, 0).x/2, SCREEN_H/2 - 40}, 40, 0, RED);
            DrawTextEx(gameFont, TextFormat("Puntaje Final: %d", score), (Vector2){SCREEN_W/2 - MeasureTextEx(gameFont, TextFormat("Puntaje Final: %d", score), fontSize, 0).x/2, SCREEN_H/2 + 70}, fontSize, 0, RAYWHITE);
            DrawTextEx(gameFont, "Presiona ENTER para reiniciar", (Vector2){SCREEN_W/2 - MeasureTextEx(gameFont, "Presiona ENTER para reiniciar", fontSize, 0).x/2, SCREEN_H/2 + 20}, fontSize, 0, GRAY);
            break;

        case VICTORY:
            // "GANASTE" más grande
            DrawTextEx(gameFont, "GANASTE", (Vector2){SCREEN_W/2 - MeasureTextEx(gameFont, "GANASTE", victoryFontSize, 0).x/2, SCREEN_H/2 - 40}, victoryFontSize, 0, GREEN);
            DrawTextEx(gameFont, TextFormat("Puntaje Final: %d", score), (Vector2){SCREEN_W/2 - MeasureTextEx(gameFont, TextFormat("Puntaje Final: %d", score), fontSize, 0).x/2, SCREEN_H/2 + 70}, fontSize, 0, RAYWHITE);
            DrawTextEx(gameFont, "Presiona ENTER para volver al menu", (Vector2){SCREEN_W/2 - MeasureTextEx(gameFont, "Presiona ENTER para volver al menu", fontSize, 0).x/2, SCREEN_H/2 + 20}, fontSize, 0, GRAY);
            break;
        }

        EndDrawing();
    }

    UnloadAssets();
    CloseWindow();
    return 0;
}