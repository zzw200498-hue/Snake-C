/* ============================================================================
 *  贪吃蛇 3.0 —— 美术层
 *
 *  所有"贴图"都是用矢量代码画出来的（没有外部图片文件，拷到哪都能跑）：
 *    · 苹果：果实 + 高光 + 阴影 + 果柄 + 叶子，带轻微旋转和呼吸动画
 *    · 蛇头：圆润的脑袋 + 两只眼睛（瞳孔会盯着前方）+ 分叉的舌头
 *    · 蛇身：从粗到细的锥形连接 + 圆润的身体节 + 背部鳞片花纹
 *    · 蛇尾：自然收窄成尖端
 * ==========================================================================*/

#ifndef ART_H
#define ART_H

#include "engine.h"

/* ---------------------------------------------------------------------------
 *  苹果
 *  cx, cy : 中心（逻辑坐标）
 *  size   : 果实半径
 *  phase  : 动画相位（用来做轻微的呼吸 / 摇摆）
 * -------------------------------------------------------------------------*/
void art_apple(float cx, float cy, float size, float phase);

/* 缩小版苹果，用在 UI 里当图标 */
void art_apple_icon(float cx, float cy, float size, float phase);

/* ---------------------------------------------------------------------------
 *  蛇
 *
 *  画法说明（这是这一版"蛇身建模"的关键）：
 *    1. 先用"胶囊"把相邻的两个身体点连起来，铺出一整条连续的轮廓
 *       —— 这样蛇身是一个整体，不会变成一颗一颗断开的珠子
 *    2. 身体从头到尾分成 8 段颜色渐变的色带（见 game.c 的 draw_snake）
 *    3. 在轮廓上叠一条连续的背部高光和一条腹部亮色，做出圆柱体的体积感
 *    4. 最后每隔一节点一片鳞纹，两种颜色交替，增加细节
 *    5. 蛇头单独绘制（更大、有眼睛、会吐信）
 * -------------------------------------------------------------------------*/

/* 蛇头 */
void art_snake_head(float x, float y, float radius, float dirx, float diry, float t,
                    Color body_color, Color belly_color);

/* 身体上的一片鳞纹：顺身体方向的菱形。
   dirx/diry 是这一节身体的方向（单位向量）。
   注意：它只是把三角形加进当前批处理，不会立刻绘制。
   调用方把深浅两种颜色的鳞片分成两批，最后各自 eng_shapes_fill() 提交。 */
void art_snake_scale(float x, float y, float radius,
                     float dirx, float diry, float t);

/* 鳞纹用的两种颜色（深 / 浅交替） */
Color art_snake_scale_color_a(Color body_color, Color belly_color);
Color art_snake_scale_color_b(Color body_color, Color belly_color);

#endif /* ART_H */