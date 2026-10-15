#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspaudio.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

PSP_MODULE_INFO("Pocket Jumper", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define PLAYER_W 16.0f
#define PLAYER_H 22.0f
#define GRAVITY 0.42f
#define JUMP_VELOCITY -7.8f
#define LEVEL_COUNT 3
#define MAX_COINS 6
#define AUDIO_RATE 44100
#define AUDIO_SAMPLES 512
#define TWO_PI 6.28318530718f

static unsigned int __attribute__((aligned(16))) display_list[262144];
static short __attribute__((aligned(64))) audio_buffer[AUDIO_SAMPLES * 2];
static int audio_channel = -1;

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y, w, h;
} Rect;

typedef struct {
    const char *name;
    float world_w;
    const Rect *platforms;
    unsigned int platform_count;
    const Rect *coins;
    unsigned int coin_count;
    unsigned int background;
    unsigned int platform_color;
    unsigned int platform_top_color;
    unsigned int coin_color;
    unsigned int player_color;
} Level;

static const Rect level1_platforms[] = {
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

static const Rect level1_coins[] = {
    {335, 188, 10, 10},
    {485, 158, 10, 10},
    {820, 170, 10, 10},
    {975, 142, 10, 10},
    {1360, 160, 10, 10},
    {1540, 188, 10, 10}
};

static const Rect level2_platforms[] = {
    {0, 248, 220, 24},
    {255, 218, 95, 18},
    {390, 184, 100, 18},
    {535, 218, 100, 18},
    {680, 166, 110, 18},
    {835, 202, 100, 18},
    {980, 230, 130, 42},
    {1155, 190, 100, 18},
    {1305, 158, 105, 18},
    {1460, 215, 340, 57}
};

static const Rect level2_coins[] = {
    {285, 184, 10, 10},
    {420, 150, 10, 10},
    {715, 132, 10, 10},
    {865, 168, 10, 10},
    {1190, 156, 10, 10},
    {1345, 124, 10, 10}
};

static const Rect level3_platforms[] = {
    {0, 248, 205, 24},
    {240, 220, 95, 18},
    {370, 188, 90, 18},
    {500, 150, 95, 18},
    {635, 194, 105, 18},
    {785, 226, 100, 18},
    {930, 184, 95, 18},
    {1070, 150, 110, 18},
    {1230, 202, 95, 18},
    {1370, 222, 430, 50}
};

static const Rect level3_coins[] = {
    {270, 186, 10, 10},
    {400, 154, 10, 10},
    {530, 116, 10, 10},
    {820, 192, 10, 10},
    {1110, 116, 10, 10},
    {1270, 168, 10, 10}
};

#define ARRAY_COUNT(a) ((unsigned int)(sizeof(a) / sizeof((a)[0])))

static const Level levels[LEVEL_COUNT] = {
    {
        "GREEN HILLS", 1800.0f,
        level1_platforms, ARRAY_COUNT(level1_platforms),
        level1_coins, ARRAY_COUNT(level1_coins),
        0xFF302018, 0xFF397A43, 0xFF59B65F, 0xFF22CFFF, 0xFF3978F5
    },
    {
        "SKY ISLANDS", 1800.0f,
        level2_platforms, ARRAY_COUNT(level2_platforms),
        level2_coins, ARRAY_COUNT(level2_coins),
        0xFF402719, 0xFF8A6BC2, 0xFFBBA1F4, 0xFFFFD45A, 0xFF45B7E8
    },
    {
        "LAVA RUN", 1800.0f,
        level3_platforms, ARRAY_COUNT(level3_platforms),
        level3_coins, ARRAY_COUNT(level3_coins),
        0xFF24102B, 0xFF9A342E, 0xFFFF6B42, 0xFFFFD45A, 0xFF65D9D0
    }
};

static int coin_taken[MAX_COINS];
static int current_level = 0;
static int selected_level = 0;
static float player_x = 24.0f;
static float player_y = 200.0f;
static float velocity_x = 0.0f;
static float velocity_y = 0.0f;
static int grounded = 0;
static int score = 0;

enum GameState {
    STATE_MENU,
    STATE_PLAYING,
    STATE_WON,
    STATE_LOST
};

static enum GameState game_state = STATE_MENU;

/* Each glyph uses five columns and seven rows. */
static const unsigned char font[][7] = {
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
        int index = -1;

        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');

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

static void setup_audio(void)
{
    audio_channel = sceAudioChReserve(
        -1, AUDIO_SAMPLES, PSP_AUDIO_FORMAT_STEREO
    );
}

/* Procedurally generated retro tones, so no external audio files are needed. */
static void play_tone(float start_frequency, float end_frequency,
                      int duration_ms, int volume)
{
    if (audio_channel < 0 || duration_ms <= 0)
        return;

    int total_frames = AUDIO_RATE * duration_ms / 1000;
    int rendered = 0;
    float phase = 0.0f;

    while (rendered < total_frames) {
        int frames = total_frames - rendered;
        if (frames > AUDIO_SAMPLES)
            frames = AUDIO_SAMPLES;

        for (int i = 0; i < AUDIO_SAMPLES; ++i) {
            float sample = 0.0f;

            if (i < frames) {
                float progress = (float)(rendered + i) /
                (float)total_frames;
                float frequency = start_frequency +
                (end_frequency - start_frequency) * progress;
                float envelope = 1.0f - progress;
                sample = sinf(phase) * envelope * (float)volume;
                phase += TWO_PI * frequency / (float)AUDIO_RATE;
                if (phase >= TWO_PI)
                    phase -= TWO_PI;
            }

            short value = (short)sample;
            audio_buffer[i * 2] = value;
            audio_buffer[i * 2 + 1] = value;
        }

        sceAudioOutputPannedBlocking(
            audio_channel, PSP_AUDIO_VOLUME_MAX,
            PSP_AUDIO_VOLUME_MAX, audio_buffer
        );
        rendered += frames;
    }
}

static void sound_jump(void)
{
    play_tone(520.0f, 760.0f, 55, 10500);
}

static void sound_coin(void)
{
    play_tone(920.0f, 1320.0f, 65, 11500);
}

static void sound_win(void)
{
    play_tone(660.0f, 660.0f, 65, 10000);
    play_tone(880.0f, 880.0f, 65, 10000);
    play_tone(1100.0f, 1320.0f, 100, 11000);
}

static void sound_lose(void)
{
    play_tone(420.0f, 190.0f, 170, 10000);
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

static void reset_level(void)
{
    player_x = 24.0f;
    player_y = 200.0f;
    velocity_x = 0.0f;
    velocity_y = 0.0f;
    grounded = 0;
    score = 0;
    memset(coin_taken, 0, sizeof(coin_taken));
}

static void update_game(unsigned int buttons, unsigned int pressed)
{
    const Level *level = &levels[current_level];

    if (game_state == STATE_MENU) {
        if (pressed & PSP_CTRL_LEFT) {
            selected_level--;
            if (selected_level < 0)
                selected_level = LEVEL_COUNT - 1;
        }

        if (pressed & PSP_CTRL_RIGHT) {
            selected_level++;
            if (selected_level >= LEVEL_COUNT)
                selected_level = 0;
        }

        if (pressed & PSP_CTRL_CROSS) {
            current_level = selected_level;
            reset_level();
            game_state = STATE_PLAYING;
        }
        return;
    }

    if (game_state == STATE_WON) {
        if (pressed & PSP_CTRL_CROSS) {
            if (current_level + 1 < LEVEL_COUNT) {
                current_level++;
                selected_level = current_level;
                reset_level();
                game_state = STATE_PLAYING;
            } else {
                selected_level = 0;
                game_state = STATE_MENU;
            }
        }
        return;
    }

    if (game_state == STATE_LOST) {
        if (pressed & PSP_CTRL_CROSS) {
            reset_level();
            game_state = STATE_PLAYING;
        }
        return;
    }

    velocity_x = 0.0f;

    if (buttons & PSP_CTRL_LEFT)
        velocity_x = -2.3f;
    if (buttons & PSP_CTRL_RIGHT)
        velocity_x = 2.3f;

    if ((pressed & PSP_CTRL_CROSS) && grounded) {
        velocity_y = JUMP_VELOCITY;
        grounded = 0;
        sound_jump();
    }

    float old_y = player_y;
    player_x += velocity_x;

    if (player_x < 0.0f)
        player_x = 0.0f;
    if (player_x > level->world_w - PLAYER_W)
        player_x = level->world_w - PLAYER_W;

    velocity_y += GRAVITY;
    if (velocity_y > 8.0f)
        velocity_y = 8.0f;

    player_y += velocity_y;
    grounded = 0;

    Rect player = {player_x, player_y, PLAYER_W, PLAYER_H};

    for (unsigned int i = 0; i < level->platform_count; ++i) {
        Rect platform = level->platforms[i];

        if (player.x < platform.x + platform.w &&
            player.x + player.w > platform.x &&
            old_y + PLAYER_H <= platform.y &&
            player_y + PLAYER_H >= platform.y &&
            velocity_y >= 0.0f) {
            player_y = platform.y - PLAYER_H;
        velocity_y = 0.0f;
        grounded = 1;
        player.y = player_y;
            }
    }

    player = (Rect){player_x, player_y, PLAYER_W, PLAYER_H};

    for (unsigned int i = 0; i < level->coin_count; ++i) {
        if (!coin_taken[i] && intersects(player, level->coins[i])) {
            coin_taken[i] = 1;
            ++score;
            sound_coin();
        }
    }

    if (player_y > SCREEN_H + 80.0f) {
        game_state = STATE_LOST;
        sound_lose();
        return;
    }

    if (player_x > level->world_w - 80.0f) {
        game_state = STATE_WON;
        sound_win();
    }
}

static void begin_frame(unsigned int background)
{
    sceGuStart(GU_DIRECT, display_list);
    sceGuClearColor(background);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
}

static void end_frame(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static void render_menu(void)
{
    begin_frame(0xFF18202E);

    draw_rect(0.0f, 218.0f, SCREEN_W, 54.0f, 0xFF25394B);
    draw_rect(0.0f, 218.0f, SCREEN_W, 5.0f, 0xFF4FC5B5);
    draw_rect(58.0f, 186.0f, 80.0f, 18.0f, 0xFF4C8B61);
    draw_rect(176.0f, 160.0f, 75.0f, 18.0f, 0xFF4C8B61);
    draw_rect(300.0f, 190.0f, 90.0f, 18.0f, 0xFF4C8B61);
    draw_rect(224.0f, 130.0f, PLAYER_W, PLAYER_H, 0xFF3978F5);

    draw_text("POCKET JUMPER", 118.0f, 28.0f, 3.0f, 0xFFFFFFFF);
    draw_text("SELECT LEVEL", 177.0f, 70.0f, 1.5f, 0xFFB7D8E8);

    for (int i = 0; i < LEVEL_COUNT; ++i) {
        float y = 104.0f + (float)i * 29.0f;
        unsigned int color = (i == selected_level)
        ? 0xFF70F0B0 : 0xFFDAE3EB;

        if (i == selected_level)
            draw_rect(105.0f, y - 4.0f, 270.0f, 22.0f, 0xFF304C55);

        draw_text(levels[i].name, 145.0f, y, 1.5f, color);
        draw_text(i == 0 ? "1" : (i == 1 ? "2" : "3"),
                  119.0f, y, 1.5f, color);
    }

    draw_text("LEFT RIGHT TO SELECT", 139.0f, 205.0f,
              1.0f, 0xFFDAE3EB);
    draw_text("PRESS X TO PLAY", 165.0f, 237.0f,
              1.5f, 0xFFFFFFFF);

    end_frame();
}

static void render_game(void)
{
    const Level *level = &levels[current_level];
    float camera_x = player_x - 120.0f;

    if (camera_x < 0.0f)
        camera_x = 0.0f;
    if (camera_x > level->world_w - SCREEN_W)
        camera_x = level->world_w - SCREEN_W;

    begin_frame(level->background);

    /* Small background shapes provide depth without external assets. */
    draw_rect(25.0f, 28.0f, 42.0f, 5.0f, 0xFF55443A);
    draw_rect(350.0f, 54.0f, 70.0f, 5.0f, 0xFF55443A);
    draw_rect(215.0f, 86.0f, 34.0f, 4.0f, 0xFF55443A);

    for (unsigned int i = 0; i < level->platform_count; ++i) {
        float x = level->platforms[i].x - camera_x;

        if (x + level->platforms[i].w < 0.0f || x > SCREEN_W)
            continue;

        draw_rect(x, level->platforms[i].y, level->platforms[i].w,
                  level->platforms[i].h, level->platform_color);
        draw_rect(x, level->platforms[i].y, level->platforms[i].w,
                  4.0f, level->platform_top_color);
    }

    for (unsigned int i = 0; i < level->coin_count; ++i) {
        if (coin_taken[i])
            continue;

        float x = level->coins[i].x - camera_x;
        if (x < -12.0f || x > SCREEN_W + 12.0f)
            continue;

        draw_rect(x, level->coins[i].y, 10.0f, 10.0f, level->coin_color);
        draw_rect(x + 3.0f, level->coins[i].y + 2.0f,
                  4.0f, 6.0f, 0xFFFFFFFF);
    }

    draw_rect(player_x - camera_x, player_y,
              PLAYER_W, PLAYER_H, level->player_color);
    draw_rect(player_x - camera_x + 3.0f, player_y + 4.0f,
              3.0f, 3.0f, 0xFFFFFFFF);
    draw_rect(player_x - camera_x + 10.0f, player_y + 4.0f,
              3.0f, 3.0f, 0xFFFFFFFF);

    draw_rect(5.0f, 5.0f, 190.0f, 40.0f, 0xFF101820);
    draw_text("POCKET JUMPER", 10.0f, 9.0f, 1.5f, 0xFFFFFFFF);

    char hud_text[40];
    snprintf(hud_text, sizeof(hud_text), "LEVEL %d OF 3", current_level + 1);
    draw_text(hud_text, 10.0f, 23.0f, 1.0f, 0xFFB7D8E8);
    snprintf(hud_text, sizeof(hud_text), "COINS %d OF %d",
             score, (int)level->coin_count);
    draw_text(hud_text, 10.0f, 34.0f, 1.0f, level->coin_color);

    draw_rect(5.0f, 51.0f, 110.0f, 4.0f, 0xFF101010);
    draw_rect(5.0f, 51.0f,
              110.0f * (player_x / level->world_w),
              4.0f, level->platform_top_color);

    if (game_state == STATE_WON) {
        draw_rect(55.0f, 84.0f, 370.0f, 104.0f, 0xFF173D25);
        draw_text(current_level == LEVEL_COUNT - 1 ? "YOU WIN" : "LEVEL CLEAR",
                  current_level == LEVEL_COUNT - 1 ? 168.0f : 130.0f,
                  102.0f, 2.5f, 0xFF70F090);
        draw_text(current_level == LEVEL_COUNT - 1 ? "ALL LEVELS DONE" : "PRESS X NEXT",
                  current_level == LEVEL_COUNT - 1 ? 143.0f : 165.0f,
                  145.0f, 1.5f, 0xFFFFFFFF);
    } else if (game_state == STATE_LOST) {
        draw_rect(70.0f, 90.0f, 340.0f, 90.0f, 0xFF25202D);
        draw_text("GAME OVER", 145.0f, 105.0f, 2.5f, 0xFFFF7777);
        draw_text("PRESS X RETRY", 153.0f, 145.0f, 1.5f, 0xFFFFFFFF);
    }

    end_frame();
}

int main(void)
{
    SceCtrlData pad;
    unsigned int previous_buttons = 0;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    setup_audio();
    setup_graphics();

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_HOME)
            break;

        unsigned int pressed = pad.Buttons & ~previous_buttons;
        previous_buttons = pad.Buttons;

        update_game(pad.Buttons, pressed);

        if (game_state == STATE_MENU)
            render_menu();
        else
            render_game();
    }

    sceGuTerm();

    if (audio_channel >= 0)
        sceAudioChRelease(audio_channel);

    sceKernelExitGame();
    return 0;
}
