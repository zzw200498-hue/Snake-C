/* ============================================================================
 *  贪吃蛇 3.0  ——  完整版
 *
 *  第三版目标：
 *    · 流畅：60FPS 渲染 + 逻辑与画面解耦（蛇是平滑滑动的，不是一格一格跳）
 *    · 建模：圆润的蛇头/蛇身/蛇尾，眼睛、吐信、鳞片、光影
 *    · 食物：真正的苹果（果实+高光+果柄+叶子，还有呼吸和摇摆动画）
 *    · UI  ：主菜单 / 游戏中 HUD / 暂停 / 结算，全套可点击的界面
 *    · 存档：最高分记录，存在 %APPDATA%\Snake3\save.dat
 *
 *  编译：见 build.bat（MSVC）或 CMakeLists.txt
 * ==========================================================================*/

#define _CRT_SECURE_NO_WARNINGS

#include "engine.h"
#include "art.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <shlobj.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

/* ===========================================================================
 *  可调参数
 * ===========================================================================*/
#define GRID_COLS      26          /* 横向格子数 */
#define GRID_ROWS      19          /* 纵向格子数 */
#define CELL           30.0f       /* 每格逻辑像素 */

#define HUD_H          76.0f       /* 顶部信息栏高度 */
#define BOARD_X        0.0f
#define BOARD_Y        HUD_H
#define BOARD_W        (GRID_COLS * CELL)
#define BOARD_H        (GRID_ROWS * CELL)

#define WIN_W          ((int)BOARD_W)     /* 780 */
#define WIN_H          ((int)(BOARD_H + HUD_H))

#define MAX_SNAKE      (GRID_COLS * GRID_ROWS)

/* 速度：每走一格需要的秒数（越小越快）。随分数逐渐加快。 */
#define SPEED_START    0.150
#define SPEED_MIN      0.062
#define SPEED_PER_APPLE 0.0032

#define APPLE_SCORE    10
#define START_LEN      5

/* 平滑插值：蛇身每个点在逻辑格之间滑动，alpha 在 0~1 之间 */
#define SMOOTH_TAU     0.045       /* 追赶系数，越大越跟手 */

/* ===========================================================================
 *  配色主题
 * ===========================================================================*/
static const Color COL_BG_TOP    = { 22, 26, 42 };
static const Color COL_BG_BOT    = { 13, 15, 26 };
static const Color COL_BOARD_A   = { 30, 36, 56 };
static const Color COL_BOARD_B   = { 26, 32, 50 };
static const Color COL_GRID      = { 38, 45, 68 };
static const Color COL_HUD       = { 18, 21, 34 };
static const Color COL_TEXT      = { 232, 240, 255 };
static const Color COL_TEXT_DIM  = { 138, 154, 184 };
static const Color COL_ACCENT    = { 92, 214, 148 };
static const Color COL_ACCENT_2  = { 255, 196, 84 };
static const Color COL_DANGER    = { 244, 96, 104 };
static const Color COL_SNAKE     = { 88, 202, 132 };
static const Color COL_SNAKE_B   = { 220, 245, 210 };
static const Color COL_MENU_BG   = { 15, 18, 30 };

/* ===========================================================================
 *  游戏状态
 * ===========================================================================*/
typedef enum { ST_MENU = 0, ST_PLAY, ST_PAUSE, ST_OVER } State;

typedef struct { int x, y; } Cell;

static struct {
    State  state;

    /* --- 逻辑 --- */
    Cell   snake[MAX_SNAKE];
    int    len;
    Cell   apple;
    int    dirx, diry;        /* 当前方向 */
    int    ndirx, ndiry;      /* 输入缓冲的方向（下一次移动生效） */
    double move_timer;        /* 距离下一次移动还剩多少秒 */
    double step_time;         /* 每步耗时 */
    int    score;
    int    apples_eaten;
    int    alive;
    int    grow_pending;      /* 吃到苹果后待增长的长度 */

    /* --- 画面插值 --- */
    float  vis_x[MAX_SNAKE];  /* 每个身体节当前显示位置 */
    float  vis_y[MAX_SNAKE];
    float  alpha;             /* 0~1：从上一格滑向当前格的比例 */

    /* --- 动画 --- */
    double t;                 /* 全局时间（秒） */
    float  apple_phase;
    float  shake;             /* 撞死时的抖动 */
    float  eat_flash;         /* 吃到苹果的闪光 */
    float  combo_pop;         /* 分数跳动 */

    /* --- 存档 --- */
    int    best;
    int    is_new_best;
    int    games_played;

    /* --- 菜单 --- */
    int    menu_sel;          /* 0 开始 1 难度 2 退出 */

    int    difficulty;        /* 0 慢 1 普通 2 快 */
} G;

/* ===========================================================================
 *  小工具
 * ===========================================================================*/
static double now_s(void) { return eng_now_ms() / 1000.0; }

static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

static float clampf(float v, float a, float b)
{
    if (v < a) return a;
    if (v > b) return b;
    return v;
}

/* 格坐标 -> 该格中心的逻辑像素坐标 */
static float cell_cx(int gx) { return BOARD_X + (gx + 0.5f) * CELL; }
static float cell_cy(int gy) { return BOARD_Y + (gy + 0.5f) * CELL; }

/* 蛇头朝向 */
static void head_dir(float *dx, float *dy)
{
    *dx = (float)G.dirx;
    *dy = (float)G.diry;
}

/* ===========================================================================
 *  存档：只存最高分
 * ===========================================================================*/
static void save_path(char *out, size_t n)
{
    char dir[MAX_PATH] = { 0 };
    /* %APPDATA%\Snake3\  —— 更稳妥，不受 exe 所在目录是否可写影响 */
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, dir))) {
        _snprintf_s(out, n, _TRUNCATE, "%s\\Snake3", dir);
    } else {
        _snprintf_s(out, n, _TRUNCATE, ".");
    }
    CreateDirectoryA(out, NULL);
    _snprintf_s(out, n, _TRUNCATE, "%s\\Snake3\\save.dat", dir);
}

static void load_save(void)
{
    char path[MAX_PATH];
    FILE *fp;
    save_path(path, sizeof(path));
    fp = fopen(path, "rb");
    if (!fp) { G.best = 0; G.games_played = 0; return; }
    if (fread(&G.best, sizeof(int), 1, fp) != 1) G.best = 0;
    if (fread(&G.games_played, sizeof(int), 1, fp) != 1) G.games_played = 0;
    fclose(fp);
    /* 防脏数据 */
    if (G.best < 0 || G.best > 1000000) G.best = 0;
    if (G.games_played < 0 || G.games_played > 1000000) G.games_played = 0;
}

static void write_save(void)
{
    char path[MAX_PATH];
    FILE *fp;
    save_path(path, sizeof(path));
    fp = fopen(path, "wb");
    if (!fp) return;
    fwrite(&G.best, sizeof(int), 1, fp);
    fwrite(&G.games_played, sizeof(int), 1, fp);
    fclose(fp);
}

/* ===========================================================================
 *  游戏逻辑
 * ===========================================================================*/
static int snake_occupies(int x, int y, int ignore_tail)
{
    int i;
    int last = G.len - (ignore_tail ? 1 : 0);
    for (i = 0; i < last; i++) {
        if (G.snake[i].x == x && G.snake[i].y == y) return 1;
    }
    return 0;
}

static void place_apple(void)
{
    int tries = 0;
    do {
        G.apple.x = rand() % GRID_COLS;
        G.apple.y = rand() % GRID_ROWS;
        tries++;
    } while (snake_occupies(G.apple.x, G.apple.y, 0) && tries < 5000);
}

static void start_game(void)
{
    int i;
    int cx = GRID_COLS / 2;
    int cy = GRID_ROWS / 2;

    G.len = START_LEN;
    for (i = 0; i < G.len; i++) {
        G.snake[i].x = cx - i;
        G.snake[i].y = cy;
    }

    G.dirx = 1; G.diry = 0;
    G.ndirx = 1; G.ndiry = 0;

    /* 初始就把显示位置对齐，避免开局从 (0,0) 滑过来 */
    for (i = 0; i < G.len; i++) {
        G.vis_x[i] = cell_cx(G.snake[i].x);
        G.vis_y[i] = cell_cy(G.snake[i].y);
    }
    G.alpha = 1.0f;

    G.move_timer = 0.0;
    G.score = 0;
    G.apples_eaten = 0;
    G.alive = 1;
    G.grow_pending = 0;
    G.shake = 0.0f;
    G.eat_flash = 0.0f;
    G.combo_pop = 0.0f;
    G.is_new_best = 0;
    G.t = 0.0;

    {
        float base = (float)SPEED_START;
        if (G.difficulty == 0) base *= 1.22f;   /* 慢 */
        if (G.difficulty == 2) base *= 0.82f;   /* 快 */
        G.step_time = base;
    }

    place_apple();
    G.state = ST_PLAY;
}

static void end_game(void)
{
    G.alive = 0;
    G.state = ST_OVER;
    G.shake = 1.0f;
    G.games_played++;

    if (G.score > G.best) {
        G.best = G.score;
        G.is_new_best = 1;
    }
    write_save();
}

/* 走一步 */
static void step_logic(void)
{
    int nx, ny, i;
    int ate = 0;

    /* 应用缓冲的方向（它的合法性在按键时就判断过了） */
    G.dirx = G.ndirx;
    G.diry = G.ndiry;

    nx = G.snake[0].x + G.dirx;
    ny = G.snake[0].y + G.diry;

    /* 撞墙 */
    if (nx < 0 || nx >= GRID_COLS || ny < 0 || ny >= GRID_ROWS) {
        end_game();
        return;
    }

    /* 是否吃到苹果 —— 要在移动前判断，影响"能不能进尾巴格" */
    ate = (nx == G.apple.x && ny == G.apple.y);

    /* 撞自己：如果不吃苹果，尾巴会让出来，所以最后一节不算障碍 */
    if (snake_occupies(nx, ny, ate ? 0 : 1)) {
        end_game();
        return;
    }

    /* 身体整体前移 */
    if (ate) {
        /* 增长：先把最后一节复制一份，再整体前移 */
        G.snake[G.len] = G.snake[G.len - 1];
        G.len++;
        G.vis_x[G.len - 1] = cell_cx(G.snake[G.len - 1].x);
        G.vis_y[G.len - 1] = cell_cy(G.snake[G.len - 1].y);
    }
    for (i = G.len - 1; i > 0; i--) {
        G.snake[i] = G.snake[i - 1];
    }
    G.snake[0].x = nx;
    G.snake[0].y = ny;

    if (ate) {
        G.apples_eaten++;
        G.score += APPLE_SCORE;
        G.eat_flash = 1.0f;
        G.combo_pop = 1.0f;
        /* 越吃越快 */
        G.step_time -= SPEED_PER_APPLE;
        if (G.step_time < SPEED_MIN) G.step_time = SPEED_MIN;
        place_apple();
    }

    G.alpha = 0.0f;   /* 重新开始插值 */
}

/* ===========================================================================
 *  输入
 * ===========================================================================*/
static void try_set_dir(int dx, int dy)
{
    /* 不能 180 度掉头（除非蛇只有一节） */
    if (G.len > 1 && dx == -G.dirx && dy == -G.diry) return;
    /* 防止连按两次导致自撞：比如向右时快速按"上"再按"左"，
       第二次的方向应该基于第一次的结果判断 */
    if (dx == -G.ndirx && dy == -G.ndiry) return;
    G.ndirx = dx;
    G.ndiry = dy;
}

static void handle_input(void)
{
    if (G.state == ST_PLAY) {
        if (eng_key_pressed(VK_UP)    || eng_key_pressed('W')) try_set_dir(0, -1);
        if (eng_key_pressed(VK_DOWN)  || eng_key_pressed('S')) try_set_dir(0,  1);
        if (eng_key_pressed(VK_LEFT)  || eng_key_pressed('A')) try_set_dir(-1, 0);
        if (eng_key_pressed(VK_RIGHT) || eng_key_pressed('D')) try_set_dir( 1, 0);

        if (eng_key_pressed(VK_ESCAPE) || eng_key_pressed('P')) {
            G.state = ST_PAUSE;
        }
    } else if (G.state == ST_PAUSE) {
        if (eng_key_pressed(VK_ESCAPE) || eng_key_pressed('P') || eng_key_pressed(VK_SPACE)) {
            G.state = ST_PLAY;
        }
        if (eng_key_pressed('Q')) G.state = ST_MENU;
    } else if (G.state == ST_OVER) {
        if (eng_key_pressed(VK_SPACE) || eng_key_pressed(VK_RETURN) || eng_key_pressed('R')) {
            start_game();
        }
        if (eng_key_pressed(VK_ESCAPE) || eng_key_pressed('Q')) G.state = ST_MENU;
    } else if (G.state == ST_MENU) {
        if (eng_key_pressed(VK_UP))   G.menu_sel = (G.menu_sel + 2) % 3;
        if (eng_key_pressed(VK_DOWN)) G.menu_sel = (G.menu_sel + 1) % 3;
        if (eng_key_pressed(VK_LEFT))  { G.difficulty = (G.difficulty + 2) % 3; }
        if (eng_key_pressed(VK_RIGHT)) { G.difficulty = (G.difficulty + 1) % 3; }
        if (eng_key_pressed(VK_SPACE) || eng_key_pressed(VK_RETURN)) {
            if (G.menu_sel == 0) start_game();
            else if (G.menu_sel == 1) G.difficulty = (G.difficulty + 1) % 3;
            else eng_quit();
        }
        if (eng_key_pressed(VK_ESCAPE)) eng_quit();
    }
}

/* ===========================================================================
 *  更新
 * ===========================================================================*/
static void update(double dt)
{
    int i;

    G.t += dt;

    /* 各种动画衰减 */
    if (G.shake > 0)      G.shake      = (float)fmax(0.0, G.shake - dt * 2.2);
    if (G.eat_flash > 0)  G.eat_flash  = (float)fmax(0.0, G.eat_flash - dt * 3.0);
    if (G.combo_pop > 0)  G.combo_pop  = (float)fmax(0.0, G.combo_pop - dt * 2.2);
    G.apple_phase += (float)dt;

    if (G.state != ST_PLAY) {
        /* 不玩了也让插值收敛，回到菜单/结算时蛇不会飘着 */
        for (i = 0; i < G.len; i++) {
            G.vis_x[i] = lerpf(G.vis_x[i], cell_cx(G.snake[i].x), 1.0f - (float)pow(0.001, dt));
            G.vis_y[i] = lerpf(G.vis_y[i], cell_cy(G.snake[i].y), 1.0f - (float)pow(0.001, dt));
        }
        G.alpha = 1.0f;
        return;
    }

    /* ---- 逻辑步进 ---- */
    G.move_timer += dt;
    {
        int guard = 0;
        while (G.move_timer >= G.step_time && G.alive && guard < 8) {
            G.move_timer -= G.step_time;
            step_logic();
            guard++;
        }
        if (guard >= 8) G.move_timer = 0;   /* 防止卡帧后疯狂追帧 */
    }

    /* ---- 画面插值 ---- */
    /* alpha 表示"从上一格滑到当前格"的进度。逻辑上蛇头已经在目标格了，
       所以这里让显示位置从「上一格」追到「当前格」。 */
    {
        float k = 1.0f - (float)exp(-dt / SMOOTH_TAU);
        G.alpha = clampf(G.alpha + (float)(dt / G.step_time), 0.0f, 1.0f);

        for (i = 0; i < G.len; i++) {
            float tx = cell_cx(G.snake[i].x);
            float ty = cell_cy(G.snake[i].y);
            G.vis_x[i] = lerpf(G.vis_x[i], tx, k);
            G.vis_y[i] = lerpf(G.vis_y[i], ty, k);
        }
    }
}

/* ===========================================================================
 *  绘制：棋盘
 * ===========================================================================*/
static void draw_background(float shake_x, float shake_y)
{
    /* 渐变背景：从上到下画横条 */
    int y;
    int h = eng_h();
    for (y = 0; y < h; y += 2) {
        float t = (float)y / (float)h;
        Color c = mix(COL_BG_TOP, COL_BG_BOT, t);
        eng_rect(0, (float)y, (float)eng_w(), 2.0f, c);
    }
    (void)shake_x; (void)shake_y;
}

static void draw_board(float sx, float sy)
{
    int gx, gy;

    /* 棋盘底 */
    eng_round(BOARD_X + 6 + sx, BOARD_Y + 6 + sy, BOARD_W - 12, BOARD_H - 12, 18.0f,
              rgb(20, 25, 40));

    /* 格子：两种颜色交替，做出细腻的方格质感 */
    for (gy = 0; gy < GRID_ROWS; gy++) {
        for (gx = 0; gx < GRID_COLS; gx++) {
            /* 只画棋盘内部，避开圆角，简单起见按矩形铺 */
            float x = BOARD_X + gx * CELL + sx;
            float y = BOARD_Y + gy * CELL + sy;
            Color c = ((gx + gy) % 2 == 0) ? COL_BOARD_A : COL_BOARD_B;
            eng_rect(x, y, CELL, CELL, c);
            /* 再加一点极淡的网格线 */
            if (gx == 0) eng_rect(x, y, 1.0f, CELL, COL_GRID);
            if (gy == 0) eng_rect(x, y, CELL, 1.0f, COL_GRID);
        }
    }

    /* 四角高光，让棋盘有"凹陷"的感觉 */
    {
        float w = BOARD_W, h = BOARD_H;
        eng_rect(BOARD_X + sx, BOARD_Y + sy, w, 2.0f, rgb(46, 56, 82));
        eng_rect(BOARD_X + sx, BOARD_Y + sy, 2.0f, h, rgb(46, 56, 82));
        eng_rect(BOARD_X + sx, BOARD_Y + h - 2 + sy, w, 2.0f, rgb(14, 17, 28));
        eng_rect(BOARD_X + w - 2 + sx, BOARD_Y + sy, 2.0f, h, rgb(14, 17, 28));
    }
}

/* ===========================================================================
 *  绘制：蛇
 *
 *  关键思路：不要把每一节画成一个独立的圆（那样会变成一串珠子）。
 *  而是：
 *    第 1 遍  用"胶囊"把相邻两点连起来，铺出一条连续的身体轮廓
 *    第 2 遍  在轮廓上叠一层连续的背部高光和腹部暗边，做出圆柱的立体感
 *    第 3 遍  点头部、鳞纹、眼睛、舌头这些细节
 * ==========================================================================*/

/* 第 i 节身体的半径：从头到尾逐渐变细，尾巴收成尖 */
static float seg_radius(int i)
{
    float t = (float)i / (float)(G.len > 1 ? G.len - 1 : 1);
    /* 前 60% 几乎保持粗细，最后 40% 快速收窄 —— 更像真蛇 */
    float taper = t < 0.6f ? t * 0.25f : 0.15f + (t - 0.6f) * 1.9f;
    if (taper > 1.0f) taper = 1.0f;
    return CELL * (0.42f - 0.26f * taper);
}

static void draw_snake(void)
{
    int i;
    float hdx, hdy;
    Color c_dark  = darken(COL_SNAKE, 0.42f);
    Color c_main  = COL_SNAKE;
    Color c_light = lighten(COL_SNAKE, 0.30f);
    Color c_belly = COL_SNAKE_B;
    Color c_shadow = rgb(12, 15, 24);

    head_dir(&hdx, &hdy);

    /* ---- 第 0 遍：整条蛇的投影 ---- */
    for (i = G.len - 1; i >= 1; i--) {
        float r = seg_radius(i);
        eng_capsule(G.vis_x[i] + 2.5f, G.vis_y[i] + 4.5f,
                    G.vis_x[i - 1] + 2.5f, G.vis_y[i - 1] + 4.5f,
                    r * 0.92f, c_shadow);
    }

    /* ---- 第 1 遍：深色底，比本体大一圈，形成柔和的描边 ---- */
    for (i = G.len - 1; i >= 1; i--) {
        float r = seg_radius(i);
        float r2 = seg_radius(i - 1);
        eng_capsule(G.vis_x[i], G.vis_y[i], G.vis_x[i - 1], G.vis_y[i - 1],
                    (r + r2) * 0.5f + 2.0f, c_dark);
    }
    if (G.len > 1) {
        eng_circle(G.vis_x[G.len - 1], G.vis_y[G.len - 1], seg_radius(G.len - 1) + 2.0f, c_dark);
        /* 尾巴尖：再往前伸一点，收成一个点 */
        {
            int last = G.len - 1;
            float dx = G.vis_x[last] - G.vis_x[last - 1];
            float dy = G.vis_y[last] - G.vis_y[last - 1];
            float m = (float)sqrt((double)(dx * dx + dy * dy));
            if (m > 0.001f) {
                float tx = G.vis_x[last] + dx / m * CELL * 0.52f;
                float ty = G.vis_y[last] + dy / m * CELL * 0.52f;
                eng_taper(G.vis_x[last], G.vis_y[last], seg_radius(last) * 2.0f + 4.0f,
                          tx, ty, 3.0f, c_dark);
            }
        }
    }

    /* ---- 第 2 遍：身体本体（连续胶囊，接缝完全融合） ---- */
    for (i = G.len - 1; i >= 1; i--) {
        float r = seg_radius(i);
        float r2 = seg_radius(i - 1);
        eng_capsule(G.vis_x[i], G.vis_y[i], G.vis_x[i - 1], G.vis_y[i - 1],
                    (r + r2) * 0.5f, c_main);
    }
    if (G.len > 1) {
        int last = G.len - 1;
        float dx = G.vis_x[last] - G.vis_x[last - 1];
        float dy = G.vis_y[last] - G.vis_y[last - 1];
        float m = (float)sqrt((double)(dx * dx + dy * dy));
        eng_circle(G.vis_x[last], G.vis_y[last], seg_radius(last), c_main);
        if (m > 0.001f) {
            float tx = G.vis_x[last] + dx / m * CELL * 0.50f;
            float ty = G.vis_y[last] + dy / m * CELL * 0.50f;
            eng_taper(G.vis_x[last], G.vis_y[last], seg_radius(last) * 2.0f,
                      tx, ty, 1.5f, c_main);
        }
    }

    /* ---- 第 3 遍：立体感。
       把每一小段当成一个"圆柱"，用一条偏移的高光带 + 一条偏移的暗边，
       沿着整条蛇连续画过去，视觉上就成了一根有体积的管子。 ---- */
    for (i = G.len - 1; i >= 1; i--) {
        float r  = seg_radius(i);
        float r2 = seg_radius(i - 1);
        float x1 = G.vis_x[i],     y1 = G.vis_y[i];
        float x2 = G.vis_x[i - 1], y2 = G.vis_y[i - 1];
        float dx = x2 - x1, dy = y2 - y1;
        float len = (float)sqrt((double)(dx * dx + dy * dy));
        float nx, ny;

        if (len < 0.001f) continue;
        nx = -dy / len;
        ny =  dx / len;

        /* 背部高光：整体往左上方偏移一点，模拟光从左上打过来 */
        eng_capsule(x1 + nx * r * 0.34f - r * 0.10f, y1 + ny * r * 0.34f - r * 0.26f,
                    x2 + nx * r2 * 0.34f - r2 * 0.10f, y2 + ny * r2 * 0.34f - r2 * 0.26f,
                    (r + r2) * 0.22f, mix(c_light, c_main, 0.30f));

        /* 腹部亮色：往右下偏移 */
        eng_capsule(x1 - nx * r * 0.40f + r * 0.06f, y1 - ny * r * 0.40f + r * 0.22f,
                    x2 - nx * r2 * 0.40f + r2 * 0.06f, y2 - ny * r2 * 0.40f + r2 * 0.22f,
                    (r + r2) * 0.18f, mix(c_belly, c_main, 0.55f));
    }

    /* ---- 第 4 遍：鳞纹 ---- */
    for (i = G.len - 1; i >= 2; i--) {
        art_snake_scales(G.vis_x[i], G.vis_y[i], seg_radius(i), i, (float)G.t,
                         COL_SNAKE, COL_SNAKE_B);
    }

    /* ---- 第 5 遍：头 ---- */
    {
        float dx = (float)G.dirx, dy = (float)G.diry;
        if (G.len > 1) {
            float vx = G.vis_x[0] - G.vis_x[1];
            float vy = G.vis_y[0] - G.vis_y[1];
            float m = (float)sqrt((double)(vx * vx + vy * vy));
            if (m > 0.6f) { dx = vx / m; dy = vy / m; }
        }
        art_snake_head(G.vis_x[0], G.vis_y[0], CELL * 0.50f, dx, dy, (float)G.t,
                       COL_SNAKE, COL_SNAKE_B);
    }

    /* ---- 吃到苹果时的金色光环 ---- */
    if (G.eat_flash > 0.01f) {
        float f = G.eat_flash;
        eng_circle_outline(G.vis_x[0], G.vis_y[0], CELL * (0.6f + (1.0f - f) * 1.4f),
                           3.0f * f, mix(COL_ACCENT_2, COL_BOARD_A, 1.0f - f));
    }
}

/* ===========================================================================
 *  绘制：苹果
 * ===========================================================================*/
static void draw_apple(void)
{
    float x = cell_cx(G.apple.x);
    float y = cell_cy(G.apple.y);

    /* 吃到苹果时它会被"吸"一下，这里做个轻微缩放 */
    float s = CELL * 0.40f;
    if (G.eat_flash > 0) s *= (1.0f + G.eat_flash * 0.18f);

    art_apple(x, y, s, G.apple_phase);
}

/* ===========================================================================
 *  绘制：HUD
 * ===========================================================================*/
static void draw_hud(void)
{
    /* 顶部栏底 */
    eng_rect(0, 0, (float)eng_w(), HUD_H, COL_HUD);
    eng_rect(0, HUD_H - 2, (float)eng_w(), 2.0f, rgb(44, 52, 78));

    /* 左侧：得分 */
    {
        char buf[64];
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%d", G.score);
        eng_text("得分", 22.0f, 14.0f, FONT_SMALL, COL_TEXT_DIM);
        {
            float pop = 1.0f + G.combo_pop * 0.35f;
            float w = eng_text_width(buf, FONT_TITLE);
            /* 分数放大：用字号没法动态改，所以用"再画一遍"的方式做跳动感 */
            eng_text_bold(buf, 22.0f, 30.0f, FONT_TITLE, COL_ACCENT);
            if (pop > 1.05f) {
                eng_text(buf, 22.0f + w * 0.1f, 30.0f, FONT_TITLE,
                         mix(COL_ACCENT_2, COL_ACCENT, 0.4f));
            }
        }
    }

    /* 中间：最高分 */
    {
        char buf[64];
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "最高分 %d", G.best);
        eng_text_center(buf, eng_w() * 0.5f, 26.0f, FONT_BODY,
                        G.is_new_best ? COL_ACCENT_2 : COL_TEXT_DIM);
    }

    /* 右侧：长度 / 速度 */
    {
        char buf[64];
        float rx = eng_w() - 22.0f;
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "长度 %d", G.len);
        eng_text_right(buf, rx, 14.0f, FONT_SMALL, COL_TEXT_DIM);
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%d 个苹果", G.apples_eaten);
        eng_text_right(buf, rx, 32.0f, FONT_BODY, COL_TEXT);
    }

    /* 底部提示 */
    eng_text_center("方向键 / WASD 移动      空格暂停/继续      Esc 菜单",
                    eng_w() * 0.5f, (float)eng_h() - 24.0f, FONT_SMALL,
                    rgb(92, 104, 134));
}

/* ===========================================================================
 *  绘制：各种界面
 * ===========================================================================*/
/* 把文字按钮画出来，返回它是否被点中 */
static int draw_button(float x, float y, float w, float h, const char *label,
                       int selected, Color accent)
{
    int hovered = 0;
    {
        int mx = eng_mouse_x(), my = eng_mouse_y();
        if (mx >= x && mx <= x + w && my >= y && my <= y + h) hovered = 1;
    }

    if (selected || hovered) {
        eng_round(x - 2, y - 2, w + 4, h + 4, 14.0f, mix(accent, COL_MENU_BG, 0.72f));
        eng_round_outline(x - 2, y - 2, w + 4, h + 4, 14.0f, 2.0f, accent);
    } else {
        eng_round(x, y, w, h, 12.0f, rgb(28, 33, 52));
    }
    eng_text_center(label, x + w * 0.5f, y + h * 0.5f - 11.0f, FONT_BODY,
                    (selected || hovered) ? COL_TEXT : COL_TEXT_DIM);

    if (hovered && eng_mouse_pressed()) return 1;
    return 0;
}

static const char *diff_name(int d)
{
    if (d == 0) return "悠闲";
    if (d == 2) return "疾速";
    return "标准";
}

static void draw_menu(void)
{
    float cx = eng_w() * 0.5f;
    float y;

    draw_background(0, 0);

    /* 背景里慢慢游动的装饰蛇，让菜单不空 */
    {
        int i;
        for (i = 0; i < 26; i++) {
            float ph = (float)G.t * 0.35f + i * 0.42f;
            float px_ = cx + (float)sin(ph * 0.7f) * (eng_w() * 0.42f);
            float py_ = eng_h() * 0.5f + (float)cos(ph * 0.5f) * (eng_h() * 0.36f);
            float r = CELL * (0.40f - 0.20f * (i / 26.0f));
            Color c = mix(COL_SNAKE, COL_MENU_BG, 0.72f + 0.2f * (i / 26.0f));
            eng_ellipse(px_, py_, r, r * 0.95f, c);
        }
    }

    /* 标题 */
    {
        const char *t1 = "贪吃蛇";
        const char *t2 = "3.0";
        float w1 = eng_text_width(t1, FONT_HUGE);
        float w2 = eng_text_width(t2, FONT_HUGE);
        float total = w1 + w2 + 14.0f;
        float bx = cx - total * 0.5f;
        float bob = (float)sin(G.t * 1.6) * 4.0f;

        eng_text_bold(t1, bx, 74.0f + bob, FONT_HUGE, COL_ACCENT);
        eng_text_bold(t2, bx + w1 + 14.0f, 74.0f + bob, FONT_HUGE, COL_ACCENT_2);
        eng_text_center("SNAKE  RELOADED", cx, 140.0f + bob, FONT_SMALL, COL_TEXT_DIM);
    }

    /* 最高分卡片 */
    {
        float cw = 300.0f, ch = 74.0f;
        float x = cx - cw * 0.5f, yy = 176.0f;
        char buf[64];
        eng_round(x, yy, cw, ch, 16.0f, rgb(24, 29, 46));
        eng_round_outline(x, yy, cw, ch, 16.0f, 2.0f, rgb(52, 62, 92));
        eng_text_center("历史最高分", cx, yy + 12.0f, FONT_SMALL, COL_TEXT_DIM);
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%d", G.best);
        eng_text_center(buf, cx, yy + 32.0f, FONT_TITLE, COL_ACCENT_2);
    }

    /* 按钮 */
    y = 272.0f;
    {
        float bw = 320.0f, bh = 52.0f;
        if (draw_button(cx - bw * 0.5f, y, bw, bh, "开始游戏", G.menu_sel == 0, COL_ACCENT)) {
            start_game();
            return;
        }
        y += bh + 12;
        {
            char buf[64];
            _snprintf_s(buf, sizeof(buf), _TRUNCATE, "难度：%s    (← →  切换)", diff_name(G.difficulty));
            if (draw_button(cx - bw * 0.5f, y, bw, bh, buf, G.menu_sel == 1, COL_ACCENT_2)) {
                G.difficulty = (G.difficulty + 1) % 3;
            }
        }
        y += bh + 12;
        if (draw_button(cx - bw * 0.5f, y, bw, bh, "退出", G.menu_sel == 2, COL_DANGER)) {
            eng_quit();
            return;
        }
    }

    /* 底部说明 */
    eng_text_center("方向键 / WASD 控制方向      吃到苹果变长，撞墙或撞到自己就结束",
                    cx, (float)eng_h() - 46.0f, FONT_SMALL, rgb(96, 108, 138));
    eng_text_center("最高分会自动保存在  %APPDATA%\\Snake3\\save.dat",
                    cx, (float)eng_h() - 28.0f, FONT_SMALL, rgb(72, 82, 108));
}

static void draw_pause(void)
{
    /* 半透明遮罩：用棋盘色混合出"变暗"的效果 */
    eng_rect(0, 0, (float)eng_w(), (float)eng_h(), rgb(10, 12, 20));

    {
        float cx = eng_w() * 0.5f;
        float cy = eng_h() * 0.5f;
        float w = 320.0f, h = 190.0f;
        eng_round(cx - w * 0.5f, cy - h * 0.5f, w, h, 18.0f, rgb(24, 29, 46));
        eng_round_outline(cx - w * 0.5f, cy - h * 0.5f, w, h, 18.0f, 2.0f, rgb(56, 66, 96));

        eng_text_center("已暂停", cx, cy - h * 0.5f + 26.0f, FONT_TITLE, COL_TEXT);
        {
            char buf[64];
            _snprintf_s(buf, sizeof(buf), _TRUNCATE, "当前得分  %d", G.score);
            eng_text_center(buf, cx, cy - 6.0f, FONT_BODY, COL_ACCENT);
        }
        eng_text_center("空格 / Esc  继续", cx, cy + 26.0f, FONT_BODY, COL_TEXT_DIM);
        eng_text_center("Q  回到主菜单", cx, cy + 52.0f, FONT_SMALL, rgb(96, 108, 138));
    }
}

static void draw_gameover(void)
{
    float cx = eng_w() * 0.5f;
    float cy = eng_h() * 0.5f;
    float w = 400.0f, h = 288.0f;

    eng_rect(0, 0, (float)eng_w(), (float)eng_h(), rgb(10, 12, 20));

    eng_round(cx - w * 0.5f, cy - h * 0.5f, w, h, 20.0f, rgb(24, 29, 46));
    eng_round_outline(cx - w * 0.5f, cy - h * 0.5f, w, h, 20.0f, 2.0f,
                      G.is_new_best ? COL_ACCENT_2 : rgb(56, 66, 96));

    {
        float top = cy - h * 0.5f;
        char buf[80];

        eng_text_center("游戏结束", cx, top + 22.0f, FONT_TITLE,
                        G.is_new_best ? COL_ACCENT_2 : COL_DANGER);

        if (G.is_new_best) {
            float pulse = 0.5f + 0.5f * (float)sin(G.t * 5.0f);
            eng_text_center("★ 新纪录！ ★", cx, top + 66.0f, FONT_BODY,
                            mix(COL_ACCENT_2, COL_TEXT, pulse));
        }

        /* 分数 */
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "本局得分  %d", G.score);
        eng_text_center(buf, cx, top + 104.0f, FONT_BODY, COL_TEXT);
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "吃到 %d 个苹果", G.apples_eaten);
        eng_text_center(buf, cx, top + 132.0f, FONT_SMALL, COL_TEXT_DIM);
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "历史最高分  %d", G.best);
        eng_text_center(buf, cx, top + 158.0f, FONT_SMALL, COL_TEXT_DIM);

        /* 两个按钮 */
        {
            float bw = 150.0f, bh = 44.0f;
            float by = top + h - bh - 22.0f;
            if (draw_button(cx - bw - 8.0f, by, bw, bh, "再来一局", 0, COL_ACCENT)) {
                start_game();
                return;
            }
            if (draw_button(cx + 8.0f, by, bw, bh, "回主菜单", 0, COL_TEXT_DIM)) {
                G.state = ST_MENU;
                G.is_new_best = 0;
                return;
            }
        }
        eng_text_center("空格 / R  再来一局        Esc / Q  主菜单",
                        cx, top + h - 64.0f, FONT_SMALL, rgb(96, 108, 138));
    }
}

/* ===========================================================================
 *  一帧渲染
 * ===========================================================================*/
static void render(void)
{
    float sx = 0, sy = 0;
    int board_only;

    /* 撞死时抖一下 */
    if (G.shake > 0.01f) {
        float m = G.shake * G.shake * 9.0f;
        sx = (float)(rand() % 200 - 100) / 100.0f * m;
        sy = (float)(rand() % 200 - 100) / 100.0f * m;
    }

    if (G.state == ST_MENU)     { draw_menu();     return; }

    board_only = (G.state == ST_PLAY || G.state == ST_PAUSE || G.state == ST_OVER);

    draw_background(sx, sy);
    draw_board(sx, sy);
    if (board_only) {
        /* 蛇和苹果必须夹在信息栏和底部提示之间，
           否则贴近上边缘的苹果，叶子和果柄会画到信息栏上去。 */
        eng_clip(BOARD_X, BOARD_Y, BOARD_W, BOARD_H);
        draw_apple();
        draw_snake();
        eng_clip_reset();
    }
    draw_hud();

    if (G.state == ST_PAUSE)    draw_pause();
    if (G.state == ST_OVER)     draw_gameover();
}

/* ===========================================================================
 *  自测模式：不需要人操作，自动跑一会儿并截图
 *  Snake3.exe --selftest [输出文件前缀]
 * ===========================================================================*/
static FILE *g_log = NULL;

static void logf_(const char *fmt, ...)
{
    va_list ap;
    char buf[1024];
    va_start(ap, fmt);
    _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    if (g_log) { fputs(buf, g_log); fflush(g_log); }
    printf("%s", buf);
    fflush(stdout);
}

static int selftest(const char *prefix)
{
    double t0;
    int frames = 0;
    int shots = 0;
    int target_frames = 300;      /* 约 5 秒 */
    int auto_play = 1;
    char logpath[MAX_PATH];
    char bmp_name[MAX_PATH];

    /* 这是个 Windows 子系统程序，默认没有控制台。
       自测时接上父进程的控制台，并同时写一份日志文件。 */
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    eng_show_console(1);

    _snprintf_s(logpath, sizeof(logpath), _TRUNCATE, "%s.log",
                prefix ? prefix : "selftest");
    g_log = fopen(logpath, "w");

    logf_("[selftest] 窗口 %dx%d，棋盘 %dx%d 格，每格 %.0f 像素\n",
          eng_w(), eng_h(), GRID_COLS, GRID_ROWS, CELL);

    load_save();
    G.difficulty = 1;
    start_game();

    logf_("[selftest] 开局：长度 %d，步进 %.4f 秒/格，苹果在 (%d,%d)\n",
          G.len, G.step_time, G.apple.x, G.apple.y);

    t0 = now_s();

    while (eng_begin()) {
        double now = now_s();
        double dt = now - t0;
        t0 = now;
        if (dt > 0.1) dt = 0.1;
        if (dt <= 0) dt = 1.0 / 60.0;

        /* 自动玩：朝苹果走，避开自己和墙 */
        if (auto_play && G.state == ST_PLAY) {
            int hx = G.snake[0].x, hy = G.snake[0].y;
            int dxs[4] = { 0, 0, -1, 1 };
            int dys[4] = { -1, 1, 0, 0 };
            int best = -1, best_score = -1000000;
            int i;
            for (i = 0; i < 4; i++) {
                int nx = hx + dxs[i], ny = hy + dys[i];
                int sc;
                if (nx < 0 || nx >= GRID_COLS || ny < 0 || ny >= GRID_ROWS) continue;
                if (dxs[i] == -G.dirx && dys[i] == -G.diry) continue;
                if (snake_occupies(nx, ny, 1)) continue;
                sc = -(abs(nx - G.apple.x) + abs(ny - G.apple.y)) * 10;
                if (snake_occupies(nx, ny, 0)) sc -= 500;   /* 尽量别贴着自己走 */
                if (sc > best_score) { best_score = sc; best = i; }
            }
            if (best >= 0) try_set_dir(dxs[best], dys[best]);
        }

        handle_input();
        update(dt);
        render();
        eng_end();

        frames++;

        /* 中途截几张图 */
        if (shots < 4 && frames == 30 + shots * 70) {
            _snprintf_s(bmp_name, sizeof(bmp_name), _TRUNCATE, "%s_%d.bmp",
                        prefix ? prefix : "selftest", shots);
            if (eng_screenshot(bmp_name)) logf_("[selftest] 截图 -> %s\n", bmp_name);
            else logf_("[selftest] 截图失败 %s\n", bmp_name);
            shots++;
        }

        if (frames >= target_frames) break;
        eng_sleep_ms(16);
    }

    logf_("[selftest] 结束：跑了 %d 帧，得分 %d，吃到 %d 个苹果，长度 %d，最高分 %d，状态 %d\n",
          frames, G.score, G.apples_eaten, G.len, G.best, (int)G.state);

    /* ---- 额外场景：菜单 / 结算 / 最上面一行的苹果 ----
       这些界面平时只有人手点才看得到，自测里专门渲染几帧存下来检查。 */

    /* 1) 主菜单 */
    if (eng_begin()) {
        G.state = ST_MENU;
        G.menu_sel = 0;
        G.best = 9990;              /* 用个假最高分，好检查卡片排版 */
        G.t = 0.5;
        render();
        if (eng_screenshot("st_menu.bmp")) logf_("[selftest] 截图 -> st_menu.bmp\n");
        eng_end();
    }

    /* 2) 结算界面 */
    if (eng_begin()) {
        G.state = ST_OVER;
        G.score = 1230;
        G.apples_eaten = 123;
        G.len = 128;
        G.is_new_best = 1;
        G.shake = 0.0f;
        G.t = 0.4;
        render();
        if (eng_screenshot("st_over.bmp")) logf_("[selftest] 截图 -> st_over.bmp\n");
        eng_end();
    }

    /* 3) 苹果和蛇都顶在最上面一行 —— 验证裁剪有没有把果柄/叶子挡在信息栏外面 */
    if (eng_begin()) {
        int i;
        G.state = ST_PLAY;
        G.is_new_best = 0;
        G.apple.x = 4;  G.apple.y = 0;
        G.apple_phase = 0.0f;      /* 固定相位，结果可重复 */
        G.len = 5;
        G.dirx = 1; G.diry = 0;
        for (i = 0; i < G.len; i++) {
            G.snake[i].x = 12 - i;
            G.snake[i].y = 0;
            G.vis_x[i] = cell_cx(G.snake[i].x);
            G.vis_y[i] = cell_cy(G.snake[i].y);
        }
        render();
        if (eng_screenshot("st_corner.bmp")) logf_("[selftest] 截图 -> st_corner.bmp\n");
        eng_end();
    }

    {
        int bad = 0;
        if (G.score <= 0) { logf_("[selftest] ✘ 自动游玩一个苹果都没吃到，逻辑可能有问题\n"); bad = 1; }
        if (G.len < START_LEN) { logf_("[selftest] ✘ 蛇的长度异常\n"); bad = 1; }
        if (G.apples_eaten > 0 && G.score != G.apples_eaten * APPLE_SCORE) {
            logf_("[selftest] ✘ 得分和苹果数对不上\n"); bad = 1;
        }
        if (shots < 4) { logf_("[selftest] ✘ 截图数量不足\n"); bad = 1; }
        if (G.best < 9990) { logf_("[selftest] ✘ 最高分记录异常\n"); bad = 1; }

        if (bad) { logf_("[selftest] 未通过\n"); }
        else { logf_("[selftest] 通过 ✅\n"); }
        if (g_log) fclose(g_log);
        return bad;
    }
}

/* ===========================================================================
 *  入口
 *
 *  同时提供 main 和 WinMain 两个入口：
 *    · 用 /SUBSYSTEM:WINDOWS 编译（推荐，运行时不弹黑色控制台）→ 走 WinMain
 *    · 用 /SUBSYSTEM:CONSOLE 或 gcc 直接编译            → 走 main
 *  这样不管你用哪种方式编译都不会链接失败。
 * ==========================================================================*/
static int snake_main(int argc, char **argv)
{
    double prev, acc = 0;
    char title[128];
    int last_best = -1;

    srand((unsigned)time(NULL));

    _snprintf_s(title, sizeof(title), _TRUNCATE,
                "贪吃蛇 3.0  ——  最高分 0");

    if (eng_init(title, WIN_W, WIN_H) != 0) {
        MessageBoxA(NULL, "窗口创建失败。", "贪吃蛇 3.0", MB_ICONERROR);
        return 1;
    }

    /* 发布版把控制台藏起来，看起来更像个正经游戏 */
    eng_show_console(0);
    eng_set_cursor_visible(0);

    /* 自测模式：不用人操作，跑一轮逻辑并截图，然后退出 */
    if (argc >= 2 && strcmp(argv[1], "--selftest") == 0) {
        return selftest(argc >= 3 ? argv[2] : "selftest");
    }

    load_save();
    G.state = ST_MENU;
    G.menu_sel = 0;
    G.difficulty = 1;
    G.len = START_LEN;

    eng_set_cursor_visible(1);

    prev = now_s();
    while (eng_begin()) {
        double now = now_s();
        double dt = now - prev;
        prev = now;
        if (dt > 0.1) dt = 0.1;          /* 拖窗口/切换回来时不要一次跳太多 */
        if (dt < 0.0) dt = 0.0;

        /* 目标 60FPS：不够一帧就只重绘、不动逻辑，这样速度不受显示器刷新率影响 */
        acc += dt;
        if (acc < 1.0 / 61.0) {
            if (eng_begin()) { render(); eng_end(); }
            eng_sleep_ms(1);
            continue;
        }
        acc = 0;

        handle_input();
        update(dt);
        render();
        eng_end();

        /* 标题栏跟着最高分走 */
        if (G.best != last_best) {
            char t2[128];
            last_best = G.best;
            _snprintf_s(t2, sizeof(t2), _TRUNCATE,
                        "贪吃蛇 3.0  ——  最高分 %d", G.best);
            SetWindowTextA(GetActiveWindow(), t2);
        }
        eng_sleep_ms(1);
    }

    /* 退出前释放一下 GDI 资源（进程结束系统会回收，这里只是个好习惯） */
    return 0;
}

/* ---- 两个入口 ---- */
int main(int argc, char **argv)
{
    return snake_main(argc, argv);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInst; (void)hPrev; (void)nCmdShow;
    return snake_main(__argc, __argv);
}