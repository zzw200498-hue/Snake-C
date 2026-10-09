/* ============================================================================
 *  贪吃蛇 3.0 —— 美术层实现
 *
 *  说明：全部用矢量图形拼出来，不依赖任何图片文件。
 *        这样整个项目拷到任何地方都能直接编译运行，也方便你改配色。
 * ==========================================================================*/

#include "art.h"
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

/* ===========================================================================
 *  苹果
 * ===========================================================================*/
void art_apple(float cx, float cy, float size, float phase)
{
    /* 轻微呼吸 + 摇摆，让静止的食物也有生气 */
    float breathe = 1.0f + 0.045f * (float)sin(phase * 2.2);
    float tilt    = 0.09f * (float)sin(phase * 1.5);
    float rx = size * breathe;
    float ry = size * breathe * 0.94f;

    /* 配色 */
    Color red_dark  = rgb(150, 20, 34);
    Color red_main  = rgb(216, 44, 52);
    Color red_light = rgb(246, 96, 86);
    Color shine     = rgb(255, 208, 202);
    Color stem      = rgb(104, 68, 38);
    Color leaf_dark = rgb(30, 100, 40);
    Color leaf_main = rgb(72, 172, 74);
    Color leaf_lite = rgb(150, 220, 128);

    float ox, oy;   /* 摇摆后的中心 */

    /* ---- 落地阴影 ---- */
    eng_ellipse(cx + rx * 0.10f, cy + ry * 0.98f, rx * 0.84f, ry * 0.24f,
                rgb(24, 26, 40));

    /* ---- 果柄：短而粗，才看得见 ---- */
    {
        float sx = cx + rx * 0.04f, sy = cy - ry * 0.78f;
        float ex = cx + rx * 0.22f + tilt * 5.0f, ey = cy - ry * 1.70f;
        eng_line(sx, sy, ex, ey, size * 0.30f, stem);
    }

    /* ---- 叶子：一片饱满的椭圆叶 + 中脉 ---- */
    {
        float lx = cx + rx * 0.26f + tilt * 4.0f, ly = cy - ry * 1.52f;
        float lw = size * 0.72f, lh = size * 0.40f;
        float a  = -0.38f + tilt * 1.2f;
        float ca = (float)cos(a), sa = (float)sin(a);
        float ex = lx + ca * lw, ey = ly + sa * lw;   /* 叶尖 */

        /* 深色描边层，比叶子大一圈 */
        eng_pie(lx, ly, lw * 1.10f, lh * 1.22f, a - PI * 0.5f, a + PI * 0.5f, leaf_dark);
        eng_pie(ex, ey, lw * 1.10f, lh * 1.22f, a + PI * 0.5f, a + PI * 1.5f, leaf_dark);
        /* 叶子本体 */
        eng_pie(lx, ly, lw, lh * 1.10f, a - PI * 0.48f, a + PI * 0.48f, leaf_main);
        eng_pie(ex, ey, lw, lh * 1.10f, a + PI * 0.52f, a + PI * 1.52f, leaf_main);
        /* 中脉：让"这是叶子"毫不含糊 */
        eng_line(lx, ly, ex, ey, size * 0.10f, leaf_lite);
    }

    /* ---- 果实 ---- */
    ox = cx + tilt * 2.2f;
    oy = cy;

    /* 深色底，形成描边 */
    eng_ellipse(ox, oy, rx * 1.06f, ry * 1.08f, red_dark);
    /* 主体 */
    eng_ellipse(ox, oy, rx, ry, red_main);
    /* 左下方暗部 */
    eng_pie(ox, oy, rx * 0.98f, ry * 0.98f, PI * 0.08f, PI * 1.02f,
            mix(red_dark, red_main, 0.50f));
    /* 右上方受光面 */
    eng_pie(ox - rx * 0.12f, oy - ry * 0.18f, rx * 0.70f, ry * 0.64f,
            PI * 1.08f, PI * 1.92f, red_light);
    /* 高光点 */
    eng_ellipse(ox - rx * 0.36f, oy - ry * 0.42f, rx * 0.20f, ry * 0.23f, shine);

    /* 顶部凹口（苹果特有的形状） */
    eng_pie(ox, cy - ry * 0.90f, rx * 0.32f, ry * 0.22f,
            0.0f, PI, mix(red_dark, red_main, 0.20f));
}

void art_apple_icon(float cx, float cy, float size, float phase)
{
    art_apple(cx, cy, size, phase);
}

/* ===========================================================================
 *  蛇头
 * ===========================================================================*/
void art_snake_head(float x, float y, float radius, float dirx, float diry, float t,
                    Color body_color, Color belly_color)
{
    float ang = (float)atan2(diry, dirx);
    float ca  = (float)cos(ang);
    float sa  = (float)sin(ang);

    /* 垂直方向单位向量（用来把眼睛往两侧推） */
    float nx = -sa, ny = ca;

    Color dark   = darken(body_color, 0.42f);
    Color light  = lighten(body_color, 0.30f);
    Color belly  = belly_color;
    Color eye_w  = rgb(255, 255, 255);
    Color eye_d  = rgb(16, 18, 30);
    Color eye_hi = rgb(215, 235, 255);
    Color tongue = rgb(232, 64, 92);
    Color mouth  = darken(body_color, 0.60f);

    /* ---- 头影 ---- */
    eng_ellipse(x + 2.0f, y + radius * 0.42f, radius * 1.02f, radius * 0.52f,
                rgb(26, 30, 44));

    /* ---- 头：比身体略大的圆润椭圆，朝向前方 ---- */
    {
        float hx = x + ca * radius * 0.18f;
        float hy = y + sa * radius * 0.18f;
        /* 深色底 */
        eng_ellipse(hx, hy, radius * 1.16f, radius * 1.10f, dark);
        /* 主体 */
        eng_ellipse(hx, hy, radius * 1.06f, radius * 1.00f, body_color);
        /* 上半受光 */
        eng_pie(hx - nx * radius * 0.12f, hy - ny * radius * 0.12f,
                radius * 0.92f, radius * 0.88f,
                ang + PI * 0.55f, ang + PI * 1.45f, light);
        /* 下半偏亮（蛇腹） */
        eng_pie(hx, hy, radius * 0.80f, radius * 0.74f,
                ang - PI * 0.42f, ang + PI * 0.42f, mix(belly, body_color, 0.35f));
    }

    /* ---- 嘴：鼻尖后面一条短短的横线（俯视角度下蛇嘴的样子） ---- */
    {
        float mx = x + ca * radius * 0.96f;
        float my = y + sa * radius * 0.96f;
        eng_line(mx + nx * radius * 0.20f, my + ny * radius * 0.20f,
                 mx - nx * radius * 0.20f, my - ny * radius * 0.20f,
                 radius * 0.09f, mouth);
    }

    /* ---- 舌头：有节奏地一伸一收。
         flick 在 0~1 之间摆动，只有超过 0.3 才伸出来，
         这样舌头是"吐一下、缩回去、再吐一下"，而不是一直抖。 ---- */
    {
        float flick = (float)sin(t * 5.5f);
        float out   = 0.5f + 0.5f * flick;          /* 0~1 */
        if (out > 0.3f) {
            float strength = (out - 0.3f) / 0.7f;   /* 伸出的力度 0~1 */
            float base_len = radius * 1.05f;
            float len  = base_len + radius * 0.75f * strength;
            float bx = x + ca * radius * 1.05f, by = y + sa * radius * 1.05f;
            float tx = x + ca * len,            ty = y + sa * len;
            float w  = radius * 0.13f;

            eng_line(bx, by, tx, ty, w, tongue);
            /* 分叉 */
            {
                float fx = x + ca * (len + radius * 0.30f);
                float fy = y + sa * (len + radius * 0.30f);
                eng_line(tx, ty, fx + nx * radius * 0.26f, fy + ny * radius * 0.26f, w * 0.85f, tongue);
                eng_line(tx, ty, fx - nx * radius * 0.26f, fy - ny * radius * 0.26f, w * 0.85f, tongue);
            }
        }
    }

    /* ---- 眼睛：略微往前 + 往两侧，比旧版大一圈 ---- */
    {
        float ex_f = radius * 0.32f;    /* 往前 */
        float ex_s = radius * 0.54f;    /* 往两侧 */
        float ey_r = radius * 0.34f;    /* 眼白半径（旧版 0.30） */
        int i;
        for (i = -1; i <= 1; i += 2) {
            float ex = x + ca * ex_f + nx * ex_s * i;
            float ey = y + sa * ex_f + ny * ex_s * i;

            /* 眼白 */
            eng_ellipse(ex, ey, ey_r, ey_r * 1.06f, eye_w);
            /* 瞳孔：稍微偏向行进方向 */
            eng_ellipse(ex + ca * ey_r * 0.26f, ey + sa * ey_r * 0.26f,
                        ey_r * 0.52f, ey_r * 0.60f, eye_d);
            /* 两个反光点：一大一小，眼睛立刻就有神了 */
            eng_circle(ex - ey_r * 0.28f, ey - ey_r * 0.36f, ey_r * 0.22f, eye_hi);
            eng_circle(ex + ey_r * 0.18f, ey - ey_r * 0.10f, ey_r * 0.10f, eye_hi);
        }
    }

    /* ---- 鼻孔：两个小点 ---- */
    {
        float ex_f = radius * 1.00f;
        float ex_s = radius * 0.20f;
        int i;
        for (i = -1; i <= 1; i += 2) {
            float ex = x + ca * ex_f + nx * ex_s * i;
            float ey = y + sa * ex_f + ny * ex_s * i;
            eng_circle(ex, ey, radius * 0.07f, darken(body_color, 0.6f));
        }
    }
}

/* ===========================================================================
 *  身体鳞纹
 *
 *  蛇身的"整体轮廓 + 立体感"由 game.c 里的胶囊连接负责。
 *  这里只把一片鳞纹加进当前的批处理 —— 它不直接绘制，而是攒起来
 *  由调用方一次性提交（PolyPolygon）。这样蛇再长也不会拖慢帧率。
 *
 *  旧版的鳞片是不管蛇往哪走的竖直小三角，横着走的时候看起来很别扭。
 *  现在改成顺身体方向的菱形：一头尖顺着前进方向，像真的鳞片排列。
 * ==========================================================================*/
void art_snake_scale(float x, float y, float radius,
                     float dirx, float diry, float t)
{
    /* 菱形的四个"半径"：沿身体方向长一点，横方向窄一点 */
    float s_front = radius * 0.42f;   /* 朝头那一侧的尖 */
    float s_back  = radius * 0.30f;   /* 朝尾巴那一侧的尖 */
    float s_side  = radius * 0.34f;   /* 两侧的尖 */
    float perp_x, perp_y;
    float wobble;

    /* 垂直于身体方向的单位向量 */
    perp_x = -diry;
    perp_y =  dirx;

    /* 轻微的呼吸起伏，让鳞片不是死死贴着不动的 */
    wobble = (float)sin(t * 3.0f + x * 0.15f + y * 0.15f) * radius * 0.04f;

    /* 菱形拆成两个三角形提交（批处理只收三角形） */
    {
        float front_x = x + dirx * (s_front + wobble);
        float front_y = y + diry * (s_front + wobble);
        float back_x  = x - dirx * s_back;
        float back_y  = y - diry * s_back;
        float left_x  = x + perp_x * s_side;
        float left_y  = y + perp_y * s_side;
        float right_x = x - perp_x * s_side;
        float right_y = y - perp_y * s_side;

        eng_shape_triangle(front_x, front_y, left_x, left_y, back_x, back_y);
        eng_shape_triangle(front_x, front_y, back_x, back_y, right_x, right_y);
    }
}

/* 鳞纹的两种颜色：深浅交替，铺出来就是规律的鳞片花纹 */
Color art_snake_scale_color_a(Color body_color, Color belly_color)
{
    return mix(darken(body_color, 0.34f), belly_color, 0.20f);
}

Color art_snake_scale_color_b(Color body_color, Color belly_color)
{
    return mix(lighten(body_color, 0.16f), belly_color, 0.42f);
}