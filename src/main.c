#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("Pocket Jumper", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define WORLD_W 1800.0f
#define PLAYER_W 16.0f
#define PLAYER_H 22.0f
#define GRAVITY 0.42f
#define JUMP_VELOCITY -7.8f

static unsigned int __attribute__((aligned(16))) display_list[262144];

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y, w, h;
} Rect;

static const Rect platforms[] = {
    {0, 246, 260, 26},
    {300, 220, 110, 18},
    {450, 190, 100, 18},
    {590, 224, 150, 48},
    {790, 202, 100, 18},
    {930, 174, 120, 18},
    {1100, 220, 160, 52},
    {1320, 194, 100, 18},
    {1470, 222, 330, 50}
};

#define PLATFORM_COUNT (sizeof(platforms) / sizeof(platforms[0]))

static Rect coins[] = {
    {335, 188, 10, 10},
    {485, 158, 10, 10},
    {820, 170, 10, 10},
    {975, 142, 10, 10},
    {1360, 160, 10, 10},
    {1540, 188, 10, 10}
};

#define COIN_COUNT (sizeof(coins) / sizeof(coins[0]))

static int coin_taken[COIN_COUNT];
static float player_x = 24.0f;
static float player_y = 200.0f;
static float velocity_x = 0.0f;
static float velocity_y = 0.0f;
static int grounded = 0;
static int score = 0;
static int won = 0;
static int lost = 0;

/* Each glyph uses five columns and seven rows. */
static const unsigned char font[][7] = {
    /* 0-9 */
    {14,17,19,21,25,17,14},
    {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2},
    {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},
    {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},

    /* A-Z */
    {14,17,17,31,17,17,17},
    {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31},
    {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15},
    {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14},
    {7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17},
    {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17},
    {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13},
    {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30},
    {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10},
    {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},
    {31,1,2,4,8,16,31}
};

static int intersects(Rect a, Rect b)
{
    return a.x < b.x + b.w && a.x + a.w > b.x &&
    a.y < b.y + b.h && a.y + a.h > b.y;
}

static void draw_rect(float x, float y, float w, float h,
                      unsigned int color)
{
    Vertex vertices[4] = {
        {color, x,     y,     0.0f},
        {color, x + w, y,     0.0f},
        {color, x,     y + h, 0.0f},
        {color, x + w, y + h, 0.0f}
    };

    sceGuDrawArray(
        GU_TRIANGLE_STRIP,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        4, NULL, vertices
    );
}

/* Draw uppercase letters, digits, spaces, and basic punctuation. */
static void draw_text(const char *text, float x, float y,
                      float scale, unsigned int color)
{
    while (*text) {
        char c = *text++;

        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');

        int index = -1;

        if (c >= '0' && c <= '9')
            index = c - '0';
        else if (c >= 'A' && c <= 'Z')
            index = 10 + c - 'A';

        if (index >= 0) {
            for (int row = 0; row < 7; ++row) {
                for (int col = 0; col < 5; ++col) {
                    if (font[index][row] & (1 << (4 - col))) {
                        draw_rect(
                            x + col * scale,
                            y + row * scale,
                            scale, scale, color
                        );
                    }
                }
            }
        } else if (c == ':') {
            draw_rect(x + 2 * scale, y + 2 * scale,
                      scale, scale, color);
            draw_rect(x + 2 * scale, y + 5 * scale,
                      scale, scale, color);
        } else if (c == '!') {
            draw_rect(x + 2 * scale, y,
                      scale, 5 * scale, color);
            draw_rect(x + 2 * scale, y + 6 * scale,
                      scale, scale, color);
        } else if (c == '.') {
            draw_rect(x + 2 * scale, y + 6 * scale,
                      scale, scale, color);
        }

        x += 6.0f * scale;
    }
}

static void setup_graphics(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, display_list);

    sceGuDrawBuffer(GU_PSM_8888, (void *)0, 512);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, (void *)0x88000, 512);
    sceGuDepthBuffer((void *)0x110000, 512);

    sceGuOffset(2048 - SCREEN_W / 2, 2048 - SCREEN_H / 2);
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuDepthRange(65535, 0);

    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void reset_game(void)
{
    player_x = 24.0f;
    player_y = 200.0f;
    velocity_x = 0.0f;
    velocity_y = 0.0f;
    grounded = 0;
    score = 0;
    won = 0;
    lost = 0;

    memset(coin_taken, 0, sizeof(coin_taken));
}

static void update_game(void)
{
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);

    if ((pad.Buttons & PSP_CTRL_START) && (won || lost)) {
        reset_game();
        return;
    }

    if (won || lost)
        return;

    velocity_x = 0.0f;

    if (pad.Buttons & PSP_CTRL_LEFT)
        velocity_x = -2.3f;

    if (pad.Buttons & PSP_CTRL_RIGHT)
        velocity_x = 2.3f;

    if ((pad.Buttons & PSP_CTRL_CROSS) && grounded) {
        velocity_y = JUMP_VELOCITY;
        grounded = 0;
    }

    float old_y = player_y;

    player_x += velocity_x;

    if (player_x < 0.0f)
        player_x = 0.0f;

    if (player_x > WORLD_W - PLAYER_W)
        player_x = WORLD_W - PLAYER_W;

    velocity_y += GRAVITY;

    if (velocity_y > 8.0f)
        velocity_y = 8.0f;

    player_y += velocity_y;
    grounded = 0;

    Rect player = {player_x, player_y, PLAYER_W, PLAYER_H};

    for (unsigned int i = 0; i < PLATFORM_COUNT; ++i) {
        Rect p = platforms[i];

        if (player.x < p.x + p.w &&
            player.x + player.w > p.x &&
            old_y + PLAYER_H <= p.y &&
            player_y + PLAYER_H >= p.y &&
            velocity_y >= 0.0f) {

            player_y = p.y - PLAYER_H;
        velocity_y = 0.0f;
        grounded = 1;
        player.y = player_y;
            }
    }

    player = (Rect){player_x, player_y, PLAYER_W, PLAYER_H};

    for (unsigned int i = 0; i < COIN_COUNT; ++i) {
        if (!coin_taken[i] && intersects(player, coins[i])) {
            coin_taken[i] = 1;
            ++score;
        }
    }

    if (player_y > SCREEN_H + 80.0f)
        lost = 1;

    if (player_x > 1700.0f)
        won = 1;
}

static void render_game(void)
{
    float camera_x = player_x - 120.0f;

    if (camera_x < 0.0f)
        camera_x = 0.0f;

    if (camera_x > WORLD_W - SCREEN_W)
        camera_x = WORLD_W - SCREEN_W;

    sceGuStart(GU_DIRECT, display_list);

    sceGuClearColor(0xFF302018);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    /* Background decorations */
    draw_rect(25.0f, 28.0f, 42.0f, 5.0f, 0xFF55443A);
    draw_rect(350.0f, 54.0f, 70.0f, 5.0f, 0xFF55443A);

    /* Platforms */
    for (unsigned int i = 0; i < PLATFORM_COUNT; ++i) {
        float x = platforms[i].x - camera_x;

        if (x + platforms[i].w < 0 || x > SCREEN_W)
            continue;

        draw_rect(x, platforms[i].y, platforms[i].w,
                  platforms[i].h, 0xFF397A43);
        draw_rect(x, platforms[i].y, platforms[i].w,
                  4.0f, 0xFF59B65F);
    }

    /* Coins */
    for (unsigned int i = 0; i < COIN_COUNT; ++i) {
        if (coin_taken[i])
            continue;

        float x = coins[i].x - camera_x;

        if (x < -12 || x > SCREEN_W + 12)
            continue;

        draw_rect(x, coins[i].y, 10.0f, 10.0f, 0xFF22CFFF);
        draw_rect(x + 3.0f, coins[i].y + 2.0f,
                  4.0f, 6.0f, 0xFF80EFFF);
    }

    /* Player */
    draw_rect(player_x - camera_x, player_y,
              PLAYER_W, PLAYER_H, 0xFF3978F5);
    draw_rect(player_x - camera_x + 3.0f, player_y + 4.0f,
              3.0f, 3.0f, 0xFFFFFFFF);
    draw_rect(player_x - camera_x + 10.0f, player_y + 4.0f,
              3.0f, 3.0f, 0xFFFFFFFF);

    /* HUD background */
    draw_rect(5.0f, 5.0f, 170.0f, 38.0f, 0xFF101820);

    draw_text("POCKET JUMPER", 10.0f, 9.0f,
              1.5f, 0xFFFFFFFF);

    char score_text[32];
    snprintf(score_text, sizeof(score_text), "COINS %d OF 6", score);

    draw_text(score_text, 10.0f, 24.0f,
              1.0f, 0xFF22CFFF);

    /* Progress bar */
    draw_rect(5.0f, 48.0f, 110.0f, 4.0f, 0xFF101010);
    draw_rect(5.0f, 48.0f,
              110.0f * (player_x / WORLD_W),
              4.0f, 0xFF59B65F);

    if (won) {
        draw_rect(70.0f, 90.0f, 340.0f, 90.0f, 0xFF173D25);
        draw_text("YOU WIN", 170.0f, 105.0f,
                  3.0f, 0xFF59E879);
        draw_text("PRESS START", 153.0f, 145.0f,
                  2.0f, 0xFFFFFFFF);
    } else if (lost) {
        draw_rect(70.0f, 90.0f, 340.0f, 90.0f, 0xFF25202D);
        draw_text("GAME OVER", 145.0f, 105.0f,
                  2.5f, 0xFF5577FF);
        draw_text("PRESS START", 153.0f, 145.0f,
                  2.0f, 0xFFFFFFFF);
    }

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

int main(void)
{
    SceCtrlData pad;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    setup_graphics();

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_HOME)
            break;

        update_game();
        render_game();
    }

    sceGuTerm();
    sceKernelExitGame();

    return 0;
}
