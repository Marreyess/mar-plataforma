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

// ===> TAMAÑOS DE TEXTURA (SOURCE) <===
// Asumimos que tus dibujos son de 32x32 en el archivo
#define SPRITE_SIZE_SRC 32 

// ===> TAMAÑOS EN PANTALLA (DESTINATION) <===
#define PLAYER_SCALE  2.0f
// El jugador se verá de 64x64 aprox (ajustado al bounding box)
#define PLAYER_SIZE_W (int)(20 * PLAYER_SCALE) // Ancho de colisión un poco menor al sprite
#define PLAYER_SIZE_H (int)(30 * PLAYER_SCALE)

// Moneda más grande (48x48 en pantalla)
#define COIN_SCALE    1.5f 
#define COIN_SIZE     (SPRITE_SIZE_SRC * COIN_SCALE)

// Slimes
#define SLIME_SCALE   2.0f
#define SLIME_W_DRAW  (SPRITE_SIZE_SRC * SLIME_SCALE)
#define SLIME_H_DRAW  (SPRITE_SIZE_SRC * SLIME_SCALE)
// Caja de colisión del slime (un poco más pequeña que el dibujo)
#define SLIME_HITBOX_W (int)(24 * SLIME_SCALE)
#define SLIME_HITBOX_H (int)(20 * SLIME_SCALE)

#define PLATFORM_TILE_W 32.0f
#define PLATFORM_TILE_H 32.0f // Ajustado a 32 si usas tiles cuadrados

#define DOOR_W        32
#define DOOR_H        48 

// ===> CONFIGURACIÓN DE ANIMACIÓN <===
#define ANIM_SPEED_PLAYER 0.1f
#define ANIM_SPEED_SLIME  0.15f
#define ANIM_SPEED_COIN   0.1f

#define PLAYER_FRAMES 4
#define SLIME_FRAMES  4  // Asumiendo que el slime tiene 4 cuadros de movimiento
#define COIN_FRAMES   4  // Asumiendo que la moneda gira en 4 cuadros

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
    // Animación Moneda
    int currentFrame;
    float animTimer;
} Coin;

typedef struct {
    Rectangle rect;
    bool isLocked;
} Door;

typedef enum { SLIME_GREEN, SLIME_PURPLE } SlimeType;

typedef struct {
    Rectangle rect; // Esta es la HITBOX (Colisión)
    SlimeType type;
    float speed;
    Vector2 startPos;
    float range;
    int dir;
    bool isActive;
    // Animación Slime
    int currentFrame;
    float animTimer;
} Slime;

typedef enum { MENU, PLAYING, GAMEOVER, VICTORY } GameState;

// ---------------------------------------------------------------------------
int currentLevel = 1;
int score = 0;
bool isGravityInverted = false; 

// ===> VARIABLES DE ANIMACIÓN JUGADOR <===
float animTimerPlayer = 0.0f;
int currentFramePlayer = 0;
float playerFacing = 1.0f; 

// Texturas
Texture2D texKnight;
Texture2D texCoin;
Texture2D texPlatforms;
Texture2D texSlimePurple;
Texture2D texDoor; 
Font gameFont;

// Rectángulos BASE (Source)
Rectangle texRecPlayer = { 0, 0, SPRITE_SIZE_SRC, SPRITE_SIZE_SRC }; 
Rectangle texRecCoin   = { 0, 0, SPRITE_SIZE_SRC, SPRITE_SIZE_SRC };   
Rectangle texRecSlime  = { 0, 0, SPRITE_SIZE_SRC, SPRITE_SIZE_SRC }; 
Rectangle texRecPlatform = { 0, 0, 32, 16 }; 

// Sonidos
Sound fxCoin;
Sound fxExplosion;
Sound fxHurt;
Sound fxJump;

Door levelDoor; 

// ---------------------------------------------------------------------------
// PROTOTIPOS
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
void UpdateCoinsAnim(Coin coins[], int coinCount, float dt); // NUEVA
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

        // Movimiento
        slimes[i].rect.x += slimes[i].dir * slimes[i].speed * dt;
        float dx = slimes[i].rect.x - slimes[i].startPos.x;
        if (fabsf(dx) >= slimes[i].range) {
            slimes[i].dir *= -1;
            slimes[i].startPos.x = slimes[i].rect.x;
        }

        // Animación
        slimes[i].animTimer += dt;
        if (slimes[i].animTimer >= ANIM_SPEED_SLIME) {
            slimes[i].animTimer = 0.0f;
            slimes[i].currentFrame++;
            if (slimes[i].currentFrame >= SLIME_FRAMES) {
                slimes[i].currentFrame = 0;
            }
        }
    }
}

void UpdateCoinsAnim(Coin coins[], int coinCount, float dt)
{
    for (int i = 0; i < coinCount; i++) {
        if (coins[i].collected) continue;
        
        coins[i].animTimer += dt;
        if (coins[i].animTimer >= ANIM_SPEED_COIN) {
            coins[i].animTimer = 0.0f;
            coins[i].currentFrame++;
            if (coins[i].currentFrame >= COIN_FRAMES) {
                coins[i].currentFrame = 0;
            }
        }
    }
}

void CheckCoinCollection(Rectangle player, Coin coins[], int coinCount, int *score) 
{
    for (int i = 0; i < coinCount; i++) {
        if (!coins[i].collected) {
            // Ajustamos la colisión considerando el nuevo tamaño visual
            if (CheckCollisionCircleRec(coins[i].center, COIN_SIZE / 3, player)) {
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
//                   CARGA Y DESCARGA DE ASSETS
// ---------------------------------------------------------------------------
void LoadAssets(void)
{
    // Carga de Texturas 
    texKnight = LoadTexture("sprites/knight.png");
    texCoin = LoadTexture("sprites/coin.png");
    texPlatforms = LoadTexture("sprites/platforms.png");
    texSlimePurple = LoadTexture("sprites/slime_purple.png");
    texDoor = LoadTexture("sprites/door.png"); 
    
    // ===> ESTO SOLUCIONA LO BORROSO Y EL "SANGRADO" DE PIXELES <===
    SetTextureFilter(texKnight, TEXTURE_FILTER_POINT);
    SetTextureFilter(texCoin, TEXTURE_FILTER_POINT);
    SetTextureFilter(texSlimePurple, TEXTURE_FILTER_POINT);
    SetTextureFilter(texPlatforms, TEXTURE_FILTER_POINT);
    SetTextureFilter(texDoor, TEXTURE_FILTER_POINT);

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
//                           CARGA DE NIVELES
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

    float groundY = 500.0f;
    float ceilingY = 40.0f;
    float coinOffset = COIN_SIZE / 2.0f;

    animTimerPlayer = 0.0f;
    currentFramePlayer = 0;
    playerFacing = 1.0f;

    switch (levelId)
    {
        case 1: 
            player->x = 50.0f;
            player->y = groundY - PLAYER_SIZE_H - 40.0f; 
            door->rect = (Rectangle){ SCREEN_W - 80, groundY - 40 - DOOR_H, DOOR_W, DOOR_H };

            platforms[(*platformCount)++] = (Platform){ { 0, groundY, SCREEN_W, 40 }, false, {0}, 0, 0, {0} };
            platforms[(*platformCount)++] = (Platform){ { 0, 0, SCREEN_W, 40 }, false, {0}, 0, 0, {0} };
            platforms[(*platformCount)++] = (Platform){ { 200, 350, 150, 20 }, false, {0}, 0, 0, {0} };
            platforms[(*platformCount)++] = (Platform){ { 450, 200, 100, 20 }, false, {0}, 0, 0, {0} };

            coins[(*coinCount)++] = (Coin){ { 275.0f, 350.0f - coinOffset }, false, 0, 0.0f }; 
            coins[(*coinCount)++] = (Coin){ { 500.0f, 200.0f - coinOffset }, false, 0, 0.0f }; 
            coins[(*coinCount)++] = (Coin){ { 850.0f, groundY - 40.0f - coinOffset }, false, 0, 0.0f }; 
            
            // Slime inicializado con variables de animación
            slimes[(*slimeCount)++] = (Slime){ { 650, groundY - SLIME_HITBOX_H, SLIME_HITBOX_W, SLIME_HITBOX_H }, SLIME_PURPLE, 70.0f, {650.0f, groundY - SLIME_HITBOX_H}, 100.0f, 1, true, 0, 0.0f };
            break;

        case 2: 
            player->x = 50.0f;
            player->y = groundY - PLAYER_SIZE_H - 40.0f;
            door->rect = (Rectangle){ 250, ceilingY, DOOR_W, DOOR_H }; 
            
            platforms[(*platformCount)++] = (Platform){ { 0, groundY, SCREEN_W, 40 }, false, {0}, 0, 0, {0} };
            platforms[(*platformCount)++] = (Platform){ { 0, 0, 200, 40 }, false, {0}, 0, 0, {0} };       
            platforms[(*platformCount)++] = (Platform){ { 350, 0, SCREEN_W-350, 40 }, false, {0}, 0, 0, {0} }; 
            platforms[(*platformCount)++] = (Platform){ { 100, 400, 100, 20 }, false, {0}, 0, 0, {0} }; 
            platforms[(*platformCount)++] = (Platform){ { 300, 300, 100, 20 }, false, {0}, 0, 0, {0} }; 
            platforms[(*platformCount)++] = (Platform){ { 500, 350, 100, 20 }, true, {-1.0f, 0.0f}, 250.0f, 120.0f, {500.0f, 350.0f} }; 

            coins[(*coinCount)++] = (Coin){ { 150.0f, 400.0f - coinOffset }, false, 0, 0.0f }; 
            coins[(*coinCount)++] = (Coin){ { 550.0f, 350.0f - coinOffset }, false, 0, 0.0f }; 
            coins[(*coinCount)++] = (Coin){ { 350.0f, 300.0f - coinOffset }, false, 0, 0.0f }; 
            coins[(*coinCount)++] = (Coin){ { 275.0f, ceilingY + DOOR_H + coinOffset }, false, 0, 0.0f }; 

            slimes[(*slimeCount)++] = (Slime){ { 100, groundY - SLIME_HITBOX_H, SLIME_HITBOX_W, SLIME_HITBOX_H }, SLIME_PURPLE, 75.0f, {100.0f, groundY - SLIME_HITBOX_H}, 150.0f, 1, true, 0, 0.0f };
            slimes[(*slimeCount)++] = (Slime){ { 700, groundY - SLIME_HITBOX_H, SLIME_HITBOX_W, SLIME_HITBOX_H }, SLIME_PURPLE, 95.0f, {700.0f, groundY - SLIME_HITBOX_H}, 120.0f, -1, true, 0, 0.0f };
            break;
            
        case 3: 
            player->x = 50.0f;
            player->y = groundY - PLAYER_SIZE_H - 40.0f;
            door->rect = (Rectangle){ 850, groundY - 40 - DOOR_H, DOOR_W, DOOR_H };
            
            platforms[(*platformCount)++] = (Platform){ { 0, groundY, 300, 40 }, false, {0}, 0, 0, {0} };
            platforms[(*platformCount)++] = (Platform){ { 400, groundY, SCREEN_W - 400, 40 }, false, {0}, 0, 0, {0} };
            platforms[(*platformCount)++] = (Platform){ { 0, 0, 450, 40 }, false, {0}, 0, 0, {0} }; 
            platforms[(*platformCount)++] = (Platform){ { 550, 0, SCREEN_W - 550, 40 }, false, {0}, 0, 0, {0} }; 
            platforms[(*platformCount)++] = (Platform){ { 200, 400, 100, 20 }, false, {0}, 0, 0, {0} }; 
            platforms[(*platformCount)++] = (Platform){ { 400, 300, 100, 20 }, false, {0}, 0, 0, {0} }; 
            platforms[(*platformCount)++] = (Platform){ { 600, 200, 100, 20 }, false, {0}, 0, 0, {0} }; 
            platforms[(*platformCount)++] = (Platform){ { 100, 100, 100, 20 }, true, {1.0f, 0.0f}, 250.0f, 80.0f, {100.0f, 100.0f} }; 
            
            coins[(*coinCount)++] = (Coin){ { 250.0f, 400.0f - coinOffset }, false, 0, 0.0f }; 
            coins[(*coinCount)++] = (Coin){ { 450.0f, 300.0f - coinOffset }, false, 0, 0.0f }; 
            coins[(*coinCount)++] = (Coin){ { 650.0f, 200.0f - coinOffset }, false, 0, 0.0f }; 
            coins[(*coinCount)++] = (Coin){ { 750.0f, 100.0f - coinOffset }, false, 0, 0.0f }; 

            slimes[(*slimeCount)++] = (Slime){ { 100, groundY - SLIME_HITBOX_H, SLIME_HITBOX_W, SLIME_HITBOX_H }, SLIME_PURPLE, 80.0f, {100.0f, groundY - SLIME_HITBOX_H}, 180.0f, 1, true, 0, 0.0f };
            slimes[(*slimeCount)++] = (Slime){ { 700, groundY - SLIME_HITBOX_H, SLIME_HITBOX_W, SLIME_HITBOX_H }, SLIME_PURPLE, 90.0f, {700.0f, groundY - SLIME_HITBOX_H}, 150.0f, -1, true, 0, 0.0f };
            slimes[(*slimeCount)++] = (Slime){ { 500, ceilingY, SLIME_HITBOX_W, SLIME_HITBOX_H }, SLIME_PURPLE, 100.0f, {500.0f, ceilingY}, 300.0f, 1, true, 0, 0.0f };
            break;

        default: break;
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
        bool isMoving = false; 

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
            
            // --- Movimiento y Orientación ---
            if (IsKeyDown(KEY_LEFT)) {
                player.x -= moveSpeed * dt;
                playerFacing = -1.0f; 
                isMoving = true;
            }
            if (IsKeyDown(KEY_RIGHT)) {
                player.x += moveSpeed * dt;
                playerFacing = 1.0f; 
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

            // ----------- Cambio de gravedad -----------
            if (onGround && IsKeyPressed(KEY_UP))
                isGravityInverted = !isGravityInverted;

            // ===> Lógica de Animación JUGADOR <===
            if (isMoving && onGround)
            {
                animTimerPlayer += dt;
                if (animTimerPlayer >= ANIM_SPEED_PLAYER)
                {
                    currentFramePlayer = (currentFramePlayer + 1) % PLAYER_FRAMES; 
                    animTimerPlayer = 0.0f;
                }
            }
            else
            {
                currentFramePlayer = 0; 
                animTimerPlayer = 0.0f;
            }
            
            // Actualizar Frame X del jugador
            texRecPlayer.x = currentFramePlayer * SPRITE_SIZE_SRC;
            
            // ----------- Física vertical -----------
            velY += (isGravityInverted ? -gravity : gravity) * dt;
            player.y += velY * dt;

            ResolveVerticalCollisions(&player, &velY, platforms, platformCount, isGravityInverted);
            ResolveHorizontalCollisions(&player, platforms, platformCount);
            
            if (player.x < 0) player.x = 0;
            if (player.x + player.width > SCREEN_W) player.x = SCREEN_W - player.width;

            UpdatePlatforms(platforms, platformCount, dt);
            
            // ===> ACTUALIZACIÓN DE ANIMACIONES <===
            UpdateSlimes(slimes, slimeCount, dt);
            UpdateCoinsAnim(coins, coinCount, dt);

            CheckCoinCollection(player, coins, coinCount, &score);

            if (CheckSlimeCollision(player, slimes, slimeCount))
            {
                PlaySound(fxHurt);
                gamestate = GAMEOVER;
            }

            if ((isGravityInverted && (player.y + player.height < 0)) || 
                (!isGravityInverted && (player.y > SCREEN_H)))          
            {
                 PlaySound(fxHurt);
                 gamestate = GAMEOVER;
            }
            
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
        ClearBackground((Color){135, 206, 235, 255}); 
        
        int fontSize = 20; 
        int victoryFontSize = 60; 
        
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

            // ===> Dibujar Monedas con Animación y Escala Grande <===
            for (int i = 0; i < coinCount; i++) {
                if (!coins[i].collected) {
                    // Seleccionamos el cuadro actual de la moneda
                    texRecCoin.x = coins[i].currentFrame * SPRITE_SIZE_SRC;

                    // Centramos la moneda más grande (ajuste visual)
                    Vector2 drawPos = { coins[i].center.x - COIN_SIZE/2, coins[i].center.y - COIN_SIZE/2 };
                    Rectangle destCoin = { drawPos.x, drawPos.y, COIN_SIZE, COIN_SIZE };
                    
                    DrawTexturePro(texCoin, texRecCoin, destCoin, (Vector2){0}, 0.0f, WHITE);
                }
            }
            
            // ===> Dibujar Slimes con Animación <===
            for (int i = 0; i < slimeCount; i++) {
                if (slimes[i].isActive) {
                    // Actualizamos source rect basado en frame
                    texRecSlime.x = slimes[i].currentFrame * SPRITE_SIZE_SRC;

                    // Dibujamos un poco más grande que la hitbox para que se vea bien
                    // Centramos el dibujo sobre la hitbox
                    float drawX = slimes[i].rect.x + (slimes[i].rect.width - SLIME_W_DRAW) / 2;
                    float drawY = slimes[i].rect.y + (slimes[i].rect.height - SLIME_H_DRAW); 

                    Rectangle destSlime = { drawX, drawY, SLIME_W_DRAW, SLIME_H_DRAW };

                    // Flip horizontal si cambia de dirección (opcional, aquí no implementado pero preparado)
                    Rectangle src = texRecSlime; 
                    if (slimes[i].dir > 0) src.width = -SPRITE_SIZE_SRC; // Espejo si va a la derecha (depende de tu arte)
                    else src.width = SPRITE_SIZE_SRC;

                    DrawTexturePro(texSlimePurple, src, destSlime, (Vector2){0}, 0.0f, WHITE);
                }
            }

            // ===> Dibujar Jugador <===
            Rectangle destRecPlayer = player;
            // Ajuste visual para que el sprite cubra la hitbox
            destRecPlayer.x -= 10; // Ajuste fino visual
            destRecPlayer.width = PLAYER_SIZE_W * 1.5; // Dibujamos más ancho que la hitbox
            
            Rectangle finalSourceRec = texRecPlayer;
            finalSourceRec.width = SPRITE_SIZE_SRC * playerFacing; 
            
            DrawTexturePro(texKnight, finalSourceRec, destRecPlayer, (Vector2){0}, 0.0f, WHITE);
            
            // Dibujar Puerta
            Rectangle doorSource = levelDoor.isLocked ? (Rectangle){0, 0, 32, 48} : (Rectangle){32, 0, 32, 48}; 
            DrawTexturePro(texDoor, doorSource, levelDoor.rect, (Vector2){0}, 0.0f, WHITE);

            // --- HUD ---
            DrawTextEx(gameFont, TextFormat("Nivel: %d/%d", currentLevel, MAX_LEVELS), (Vector2){10, 10}, fontSize, 0, RAYWHITE);
            DrawTextEx(gameFont, TextFormat("Puntaje: %d/%d", score, coinCount), (Vector2){10, 35}, fontSize, 0, RAYWHITE);
            DrawTextEx(gameFont, isGravityInverted ? "Gravedad: Arriba" : "Gravedad: Abajo", (Vector2){10, 60}, fontSize, 0, RAYWHITE);
            
            if (allCoinsCollected) {
                const char *msg = "¡PUERTA ABIERTA!";
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