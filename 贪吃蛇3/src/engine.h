/* ============================================================================
 *  贪吃蛇 3.0 —— 图形引擎层
 *
 *  一个极小的 2D 绘图库，全部基于 Windows GDI（系统自带，零第三方依赖）。
 *
 *  提供：
 *    · 双缓冲窗口（彻底无闪烁）
 *    · 固定 60FPS 主循环
 *    · 抗锯齿的圆角矩形 / 椭圆 / 圆 / 多边形 / 线段
 *    · 中文文字绘制（自动选一个好用的字体）
 *    · 按键状态查询
 * ==========================================================================*/

#ifndef ENGINE_H
#define ENGINE_H

#include <windows.h>

/* ---------------------------------------------------------------------------
 *  颜色与坐标
 * -------------------------------------------------------------------------*/
typedef struct { unsigned char r, g, b; } Color;

static Color rgb(int r, int g, int b) { Color c; c.r = (unsigned char)r; c.g = (unsigned char)g; c.b = (unsigned char)b; return c; }

/* 把两个颜色按 t(0~1) 混合 */
static Color mix(Color a, Color b, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return rgb((int)(a.r + (b.r - a.r) * t),
               (int)(a.g + (b.g - a.g) * t),
               (int)(a.b + (b.b - a.b) * t));
}

/* 变亮 / 变暗 */
static Color lighten(Color c, float t) { return mix(c, rgb(255, 255, 255), t); }
static Color darken (Color c, float t) { return mix(c, rgb(0, 0, 0), t); }

/* ---------------------------------------------------------------------------
 *  引擎接口
 * -------------------------------------------------------------------------*/

/* 启动。width/height 是客户区像素尺寸。内部会自动按 DPI 放大。
   title 用 UTF-8 传中文（内部转 UTF-16）。返回 0 成功。 */
int   eng_init(const char *title, int width, int height);

/* 按 DPI 缩放后的逻辑尺寸（游戏里就用这个坐标系，比如 900x640） */
int   eng_w(void);
int   eng_h(void);
float eng_scale(void);

/* 主循环：调一次 eng_begin 开始画，画完调 eng_end。
   返回 0 表示窗口已被关闭，游戏应当退出。 */
int   eng_begin(void);
void  eng_end(void);

/* 屏幕清屏 */
void  eng_clear(Color c);

/* --- 基本图形（全部抗锯齿） --- */
void  eng_rect(float x, float y, float w, float h, Color c);
void  eng_round(float x, float y, float w, float h, float r, Color c);
void  eng_round_outline(float x, float y, float w, float h, float r, float thick, Color c);
void  eng_circle(float cx, float cy, float radius, Color c);
void  eng_circle_outline(float cx, float cy, float radius, float thick, Color c);
void  eng_ellipse(float cx, float cy, float rx, float ry, Color c);
void  eng_ellipse_outline(float cx, float cy, float rx, float ry, float thick, Color c);
void  eng_line(float x1, float y1, float x2, float y2, float thick, Color c);
void  eng_triangle(float x1, float y1, float x2, float y2, float x3, float y3, Color c);

/* 扇形（画蛇头/苹果高光/圆弧用）。angle 是弧度，从 +x 轴开始顺时针。 */
void  eng_pie(float cx, float cy, float rx, float ry, float a0, float a1, Color c);

/* 一条"锥形"线段：起点粗、终点细。用来画会聚拢的蛇尾。 */
void  eng_taper(float x1, float y1, float w1, float x2, float y2, float w2, Color c);

/* 胶囊：一条有圆头圆尾的粗线。半径固定的粗线用它最自然，接缝处不会断开。
   —— 画连续蛇身的核心图元。 */
void  eng_capsule(float x1, float y1, float x2, float y2, float radius, Color c);

/* 裁剪区：只在指定矩形内绘制。用来防止棋盘边缘的苹果/蛇画到信息栏上去。
   用完记得 eng_clip_reset()。 */
void  eng_clip(float x, float y, float w, float h);
void  eng_clip_reset(void);

/* --- 文字 --- */
typedef enum {
    FONT_SMALL = 0,   /* 小字，说明用 */
    FONT_BODY  = 1,   /* 正文 */
    FONT_TITLE = 2,   /* 大标题 */
    FONT_HUGE  = 3    /* 超大标题 */
} FontId;

void  eng_text(const char *utf8, float x, float y, FontId font, Color c);
void  eng_text_bold(const char *utf8, float x, float y, FontId font, Color c);
/* 居中 / 右对齐 */
void  eng_text_center(const char *utf8, float cx, float y, FontId font, Color c);
void  eng_text_right(const char *utf8, float rx, float y, FontId font, Color c);

/* 量一段文字的宽度 */
float eng_text_width(const char *utf8, FontId font);

/* --- 输入 --- */
int   eng_key_down(int vk);           /* 当前是否按住 */
int   eng_key_pressed(int vk);        /* 这一帧是否刚按下（边沿触发） */
int   eng_mouse_x(void);
int   eng_mouse_y(void);
int   eng_mouse_down(void);
int   eng_mouse_pressed(void);

/* --- 其他 --- */
void  eng_sleep_ms(int ms);
double eng_now_ms(void);
void  eng_show_console(int show);     /* 发布版把控制台藏起来 */
void  eng_set_cursor_visible(int visible);
void  eng_quit(void);

/* 开发用：把客户区截图存成 BMP，方便无显示器时自测 */
int   eng_screenshot(const char *path);

#endif /* ENGINE_H */