/* ============================================================================
 *  贪吃蛇 3.0 —— 图形引擎实现（Windows GDI）
 * ==========================================================================*/

#define _CRT_SECURE_NO_WARNINGS

#include "engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ===========================================================================
 *  内部状态
 * ===========================================================================*/
#define ENG_MAX_POLY 512

typedef struct {
    HWND   hwnd;
    HDC    mem_dc;        /* 后台缓冲 DC */
    HBITMAP mem_bmp;      /* 后台缓冲位图 */
    HBITMAP old_bmp;
    int    buf_w, buf_h;  /* 缓冲区像素尺寸 */

    int    client_w, client_h;   /* 客户区像素尺寸 */
    float  scale;                /* DPI 缩放 */
    int    logical_w, logical_h; /* 逻辑尺寸 */

    /* 画笔/画刷缓存：每次改颜色就重建太慢，这里做简单复用 */
    HPEN   pen;
    HBRUSH brush;
    COLORREF pen_col, brush_col;

    HFONT  fonts[4];

    /* 输入 */
    unsigned char keys[256];
    unsigned char keys_prev[256];
    int    mouse_x, mouse_y, mouse_down, mouse_prev_down;

    int    running;
    int    frame_count;
    double start_ms;
} Eng;

static Eng g_eng;

/* ===========================================================================
 *  UTF-8 -> UTF-16
 * ===========================================================================*/
static WCHAR *utf8_to_wide(const char *s)
{
    int n;
    WCHAR *out;
    if (!s) s = "";
    n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) n = 1;
    out = (WCHAR *)malloc(sizeof(WCHAR) * (n + 1));
    if (!out) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, out, n) <= 0) out[0] = 0;
    out[n] = 0;
    return out;
}

/* ===========================================================================
 *  颜色 / GDI 对象缓存
 * ===========================================================================*/
static COLORREF to_ref(Color c) { return RGB(c.r, c.g, c.b); }

/* 建一支画笔。GDI 画笔的宽度查不出来，所以我们自己记住上次的宽度。 */
static int g_pen_w = -1;

static void set_pen(Color c, float thick)
{
    int w = (int)(thick * g_eng.scale + 0.5f);
    COLORREF ref = to_ref(c);
    if (w < 1) w = 1;
    if (g_eng.pen && g_eng.pen_col == ref && g_pen_w == w) return;  /* 没变，直接用 */
    if (g_eng.pen) DeleteObject(g_eng.pen);
    g_eng.pen = CreatePen(PS_SOLID, w, ref);
    g_eng.pen_col = ref;
    g_pen_w = w;
    SelectObject(g_eng.mem_dc, g_eng.pen);
}

static void set_brush(Color c)
{
    COLORREF ref = to_ref(c);
    if (g_eng.brush && g_eng.brush_col == ref) {
        SelectObject(g_eng.mem_dc, g_eng.brush);
        return;
    }
    if (g_eng.brush) DeleteObject(g_eng.brush);
    g_eng.brush = CreateSolidBrush(ref);
    g_eng.brush_col = ref;
    SelectObject(g_eng.mem_dc, g_eng.brush);
}

/* 逻辑坐标 -> 像素 */
static int px(float v) { return (int)(v * g_eng.scale + 0.5f); }

/* ===========================================================================
 *  窗口过程
 * ===========================================================================*/
static LRESULT CALLBACK eng_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CLOSE:
        g_eng.running = 0;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_eng.running = 0;
        PostQuitMessage(0);
        return 0;

    case WM_ERASEBKGND:
        return 1;   /* 自己重绘，禁止系统擦背景 —— 防闪烁的关键 */

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        /* 直接把后台缓冲贴上去，不重算逻辑 */
        if (g_eng.mem_dc) {
            BitBlt(dc, 0, 0, g_eng.client_w, g_eng.client_h,
                   g_eng.mem_dc, 0, 0, SRCCOPY);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wp < 256) g_eng.keys[wp] = 1;
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wp < 256) g_eng.keys[wp] = 0;
        return 0;

    case WM_MOUSEMOVE:
        g_eng.mouse_x = (int)((short)LOWORD(lp) / g_eng.scale);
        g_eng.mouse_y = (int)((short)HIWORD(lp) / g_eng.scale);
        return 0;

    case WM_LBUTTONDOWN:
        g_eng.mouse_down = 1;
        SetCapture(hwnd);
        return 0;

    case WM_LBUTTONUP:
        g_eng.mouse_down = 0;
        ReleaseCapture();
        return 0;

    case WM_KILLFOCUS:
        /* 失焦时清空按键，避免"卡住一直走" */
        memset(g_eng.keys, 0, sizeof(g_eng.keys));
        g_eng.mouse_down = 0;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ===========================================================================
 *  创建后台缓冲
 * ===========================================================================*/
static void create_backbuffer(HWND hwnd, int w, int h)
{
    HDC screen = GetDC(hwnd);

    if (g_eng.mem_dc) {
        SelectObject(g_eng.mem_dc, g_eng.old_bmp);
        DeleteDC(g_eng.mem_dc);
        g_eng.mem_dc = NULL;
    }
    if (g_eng.mem_bmp) { DeleteObject(g_eng.mem_bmp); g_eng.mem_bmp = NULL; }

    /* 重建缓冲区后，之前的画笔/画刷/字体都绑在旧 DC 上，全部作废 */
    if (g_eng.pen)   { DeleteObject(g_eng.pen);   g_eng.pen = NULL;   g_pen_w = -1; }
    if (g_eng.brush) { DeleteObject(g_eng.brush); g_eng.brush = NULL; }

    g_eng.mem_dc = CreateCompatibleDC(screen);
    g_eng.mem_bmp = CreateCompatibleBitmap(screen, w, h);
    g_eng.old_bmp = (HBITMAP)SelectObject(g_eng.mem_dc, g_eng.mem_bmp);

    ReleaseDC(hwnd, screen);

    g_eng.buf_w = w;
    g_eng.buf_h = h;

    /* 画图质量：透明背景 + 平滑缩放 */
    SetBkMode(g_eng.mem_dc, TRANSPARENT);
    SetStretchBltMode(g_eng.mem_dc, HALFTONE);
}

/* ===========================================================================
 *  字体
 * ===========================================================================*/
static void create_fonts(void)
{
    /* 优先用微软雅黑，没有就退回系统默认 */
    const char *faces[] = { "Microsoft YaHei UI", "Microsoft YaHei", "SimHei" };
    int i;

    int sizes[4];
    sizes[FONT_SMALL] = (int)(13 * g_eng.scale + 0.5f);
    sizes[FONT_BODY]  = (int)(17 * g_eng.scale + 0.5f);
    sizes[FONT_TITLE] = (int)(34 * g_eng.scale + 0.5f);
    sizes[FONT_HUGE]  = (int)(54 * g_eng.scale + 0.5f);

    for (i = 0; i < 4; i++) {
        int f;
        if (g_eng.fonts[i]) DeleteObject(g_eng.fonts[i]);
        g_eng.fonts[i] = NULL;
        for (f = 0; f < 3 && !g_eng.fonts[i]; f++) {
            WCHAR *wf = utf8_to_wide(faces[f]);
            g_eng.fonts[i] = CreateFontW(-sizes[i], 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, wf);
            free(wf);
        }
        if (!g_eng.fonts[i]) g_eng.fonts[i] = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }
}

/* ===========================================================================
 *  初始化
 * ===========================================================================*/
int eng_init(const char *title, int width, int height)
{
    WNDCLASSEXW wc;
    WCHAR *wt;
    HINSTANCE inst = GetModuleHandleW(NULL);
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rc;
    HDC screen;
    int dpi;

    memset(&g_eng, 0, sizeof(g_eng));

    wt = utf8_to_wide("Snake3WndClass");
    memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = eng_wndproc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = wt;
    if (!RegisterClassExW(&wc)) { free(wt); return -1; }

    wt = utf8_to_wide(title ? title : "Snake");

    /* 按 DPI 缩放窗口，保证高分屏下也清晰 */
    screen = GetDC(NULL);
    dpi = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(NULL, screen);
    if (dpi <= 0) dpi = 96;
    g_eng.scale = (float)dpi / 96.0f;

    g_eng.logical_w = width;
    g_eng.logical_h = height;

    rc.left = 0; rc.top = 0;
    rc.right  = (LONG)(width  * g_eng.scale);
    rc.bottom = (LONG)(height * g_eng.scale);
    AdjustWindowRect(&rc, style, FALSE);

    g_eng.hwnd = CreateWindowExW(0, L"Snake3WndClass", wt,
                                 style,
                                 CW_USEDEFAULT, CW_USEDEFAULT,
                                 rc.right - rc.left, rc.bottom - rc.top,
                                 NULL, NULL, inst, NULL);
    free(wt);
    if (!g_eng.hwnd) return -2;

    g_eng.client_w = (int)(width  * g_eng.scale);
    g_eng.client_h = (int)(height * g_eng.scale);

    create_backbuffer(g_eng.hwnd, g_eng.client_w, g_eng.client_h);
    create_fonts();

    g_eng.running = 1;
    g_eng.start_ms = eng_now_ms();

    ShowWindow(g_eng.hwnd, SW_SHOW);
    UpdateWindow(g_eng.hwnd);

    return 0;
}

int   eng_w(void)     { return g_eng.logical_w; }
int   eng_h(void)     { return g_eng.logical_h; }
float eng_scale(void) { return g_eng.scale; }

/* ===========================================================================
 *  主循环
 * ===========================================================================*/
int eng_begin(void)
{
    MSG msg;

    /* 处理全部消息 */
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { g_eng.running = 0; return 0; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (!g_eng.running || !IsWindow(g_eng.hwnd)) return 0;

    g_eng.frame_count++;
    return 1;
}

void eng_end(void)
{
    HDC dc;

    if (!g_eng.hwnd || !g_eng.mem_dc) return;

    /* 把后台缓冲一次贴到窗口上 */
    dc = GetDC(g_eng.hwnd);
    BitBlt(dc, 0, 0, g_eng.client_w, g_eng.client_h, g_eng.mem_dc, 0, 0, SRCCOPY);
    ReleaseDC(g_eng.hwnd, dc);

    /* 这一帧的按键状态存档，供"刚按下"判断 */
    memcpy(g_eng.keys_prev, g_eng.keys, sizeof(g_eng.keys));
    g_eng.mouse_prev_down = g_eng.mouse_down;
}

/* ===========================================================================
 *  图形
 * ===========================================================================*/
void eng_clear(Color c)
{
    RECT r;
    r.left = 0; r.top = 0; r.right = g_eng.client_w; r.bottom = g_eng.client_h;
    set_brush(c);
    FillRect(g_eng.mem_dc, &r, g_eng.brush);
}

void eng_rect(float x, float y, float w, float h, Color c)
{
    RECT r;
    r.left = px(x); r.top = px(y);
    r.right = px(x + w); r.bottom = px(y + h);
    set_brush(c);
    /* 用 ExtSelectClipRgn 之外最简单可靠的方式：FillRect 不走抗锯齿，
       但纯矩形本来就该是硬边，这里没问题 */
    FillRect(g_eng.mem_dc, &r, g_eng.brush);
}

/* 圆角矩形：GDI 自带 RoundRect，按像素放大后调用 */
void eng_round(float x, float y, float w, float h, float r, Color c)
{
    int l = px(x), t = px(y), rr = px(x + w), bb = px(y + h);
    int d = px(r * 2);
    set_pen(c, 1.0f);
    set_brush(c);
    RoundRect(g_eng.mem_dc, l, t, rr, bb, d, d);
}

void eng_round_outline(float x, float y, float w, float h, float r, float thick, Color c)
{
    int l = px(x), t = px(y), rr = px(x + w), bb = px(y + h);
    int d = px(r * 2);
    HGDIOBJ old = SelectObject(g_eng.mem_dc, GetStockObject(NULL_BRUSH));
    set_pen(c, thick);
    RoundRect(g_eng.mem_dc, l, t, rr, bb, d, d);
    SelectObject(g_eng.mem_dc, old);
}

void eng_circle(float cx, float cy, float radius, Color c)
{
    eng_ellipse(cx, cy, radius, radius, c);
}

void eng_ellipse(float cx, float cy, float rx, float ry, Color c)
{
    int l = px(cx - rx), t = px(cy - ry);
    int r = px(cx + rx), b = px(cy + ry);
    set_pen(c, 1.0f);
    set_brush(c);
    Ellipse(g_eng.mem_dc, l, t, r, b);
}

void eng_circle_outline(float cx, float cy, float radius, float thick, Color c)
{
    eng_ellipse_outline(cx, cy, radius, radius, thick, c);
}

void eng_ellipse_outline(float cx, float cy, float rx, float ry, float thick, Color c)
{
    int l = px(cx - rx), t = px(cy - ry);
    int r = px(cx + rx), b = px(cy + ry);
    HGDIOBJ old = SelectObject(g_eng.mem_dc, GetStockObject(NULL_BRUSH));
    set_pen(c, thick);
    Ellipse(g_eng.mem_dc, l, t, r, b);
    SelectObject(g_eng.mem_dc, old);
}

void eng_line(float x1, float y1, float x2, float y2, float thick, Color c)
{
    set_pen(c, thick);
    MoveToEx(g_eng.mem_dc, px(x1), px(y1), NULL);
    LineTo(g_eng.mem_dc, px(x2), px(y2));
}

void eng_triangle(float x1, float y1, float x2, float y2, float x3, float y3, Color c)
{
    POINT p[3];
    p[0].x = px(x1); p[0].y = px(y1);
    p[1].x = px(x2); p[1].y = px(y2);
    p[2].x = px(x3); p[2].y = px(y3);
    set_pen(c, 1.0f);
    set_brush(c);
    Polygon(g_eng.mem_dc, p, 3);
}

void eng_pie(float cx, float cy, float rx, float ry, float a0, float a1, Color c)
{
    int l = px(cx - rx), t = px(cy - ry);
    int r = px(cx + rx), b = px(cy + ry);
    /* GDI 的 Pie 用端点坐标定义扇形，端点落在椭圆上即可。
       角度是屏幕坐标系（y 向下），sin 自带方向，所以直接用。 */
    int x0 = px(cx + rx * (float)cos(a0));
    int y0 = px(cy + ry * (float)sin(a0));
    int x1 = px(cx + rx * (float)cos(a1));
    int y1 = px(cy + ry * (float)sin(a1));
    set_pen(c, 1.0f);
    set_brush(c);
    Pie(g_eng.mem_dc, l, t, r, b, x0, y0, x1, y1);
}

/* 锥形线段：用上下两条边 + 两个半圆端帽近似，效果足够好 */
void eng_taper(float x1, float y1, float w1, float x2, float y2, float w2, Color c)
{
    float dx = x2 - x1, dy = y2 - y1;
    float len = (float)sqrt((double)(dx * dx + dy * dy));
    float nx, ny;
    POINT p[4];

    if (len < 0.0001f) { eng_circle(x1, y1, w1 * 0.5f, c); return; }
    nx = -dy / len;
    ny =  dx / len;

    p[0].x = px(x1 + nx * w1 * 0.5f); p[0].y = px(y1 + ny * w1 * 0.5f);
    p[1].x = px(x2 + nx * w2 * 0.5f); p[1].y = px(y2 + ny * w2 * 0.5f);
    p[2].x = px(x2 - nx * w2 * 0.5f); p[2].y = px(y2 - ny * w2 * 0.5f);
    p[3].x = px(x1 - nx * w1 * 0.5f); p[3].y = px(y1 - ny * w1 * 0.5f);

    set_pen(c, 1.0f);
    set_brush(c);
    Polygon(g_eng.mem_dc, p, 4);
    /* 两端补圆，衔接更顺 */
    eng_circle(x1, y1, w1 * 0.5f, c);
    eng_circle(x2, y2, w2 * 0.5f, c);
}

/* 胶囊：一条固定粗细、两端是半圆的粗线。
   把相邻两点用胶囊连起来，接缝处就会完全融合，蛇身看起来是一整条。 */
void eng_capsule(float x1, float y1, float x2, float y2, float radius, Color c)
{
    float dx = x2 - x1, dy = y2 - y1;
    float len = (float)sqrt((double)(dx * dx + dy * dy));
    float nx, ny;
    POINT p[4];

    if (radius < 0.5f) radius = 0.5f;

    if (len < 0.0001f) { eng_circle(x1, y1, radius, c); return; }

    nx = -dy / len * radius;
    ny =  dx / len * radius;

    p[0].x = px(x1 + nx); p[0].y = px(y1 + ny);
    p[1].x = px(x2 + nx); p[1].y = px(y2 + ny);
    p[2].x = px(x2 - nx); p[2].y = px(y2 - ny);
    p[3].x = px(x1 - nx); p[3].y = px(y1 - ny);

    set_pen(c, 1.0f);
    set_brush(c);
    Polygon(g_eng.mem_dc, p, 4);
    eng_circle(x1, y1, radius, c);
    eng_circle(x2, y2, radius, c);
}

/* ===========================================================================
 *  裁剪区
 * ===========================================================================*/
void eng_clip(float x, float y, float w, float h)
{
    HRGN rgn = CreateRectRgn(px(x), px(y), px(x + w), px(y + h));
    SelectClipRgn(g_eng.mem_dc, rgn);
    DeleteObject(rgn);
}

void eng_clip_reset(void)
{
    SelectClipRgn(g_eng.mem_dc, NULL);
}

/* ===========================================================================
 *  文字
 * ===========================================================================*/
static void draw_text_raw(const char *utf8, float x, float y, FontId font, Color c)
{
    WCHAR *w = utf8_to_wide(utf8);
    if (!w) return;
    SelectObject(g_eng.mem_dc, g_eng.fonts[(int)font]);
    SetTextColor(g_eng.mem_dc, to_ref(c));
    TextOutW(g_eng.mem_dc, px(x), px(y), w, (int)wcslen(w));
    free(w);
}

void eng_text(const char *utf8, float x, float y, FontId font, Color c)
{
    draw_text_raw(utf8, x, y, font, c);
}

void eng_text_bold(const char *utf8, float x, float y, FontId font, Color c)
{
    /* 画两遍，偏移 1 像素，视觉上加粗（GDI 没有轻量的描边） */
    draw_text_raw(utf8, x, y, font, c);
    draw_text_raw(utf8, x + 0.6f, y, font, c);
}

static int text_pixel_width(const char *utf8, FontId font)
{
    WCHAR *w = utf8_to_wide(utf8);
    SIZE sz;
    int result = 0;
    if (!w) return 0;
    SelectObject(g_eng.mem_dc, g_eng.fonts[(int)font]);
    if (GetTextExtentPoint32W(g_eng.mem_dc, w, (int)wcslen(w), &sz)) {
        result = (int)(sz.cx / g_eng.scale + 0.5f);
    }
    free(w);
    return result;
}

float eng_text_width(const char *utf8, FontId font)
{
    return (float)text_pixel_width(utf8, font);
}

void eng_text_center(const char *utf8, float cx, float y, FontId font, Color c)
{
    float w = eng_text_width(utf8, font);
    draw_text_raw(utf8, cx - w * 0.5f, y, font, c);
}

void eng_text_right(const char *utf8, float rx, float y, FontId font, Color c)
{
    float w = eng_text_width(utf8, font);
    draw_text_raw(utf8, rx - w, y, font, c);
}

/* ===========================================================================
 *  输入
 * ===========================================================================*/
int eng_key_down(int vk)
{
    if (vk < 0 || vk > 255) return 0;
    return g_eng.keys[vk] != 0;
}

int eng_key_pressed(int vk)
{
    if (vk < 0 || vk > 255) return 0;
    return (g_eng.keys[vk] != 0) && (g_eng.keys_prev[vk] == 0);
}

int eng_mouse_x(void) { return g_eng.mouse_x; }
int eng_mouse_y(void) { return g_eng.mouse_y; }
int eng_mouse_down(void) { return g_eng.mouse_down; }
int eng_mouse_pressed(void) { return g_eng.mouse_down && !g_eng.mouse_prev_down; }

/* ===========================================================================
 *  杂项
 * ===========================================================================*/
void eng_sleep_ms(int ms)
{
    if (ms > 0) Sleep((DWORD)ms);
}

double eng_now_ms(void)
{
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    if (freq.QuadPart == 0) return 0;
    return (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
}

void eng_show_console(int show)
{
    HWND con = GetConsoleWindow();
    if (con) ShowWindow(con, show ? SW_SHOW : SW_HIDE);
}

void eng_set_cursor_visible(int visible)
{
    if (visible) {
        while (ShowCursor(TRUE) < 0) { /* 一直到显示出来 */ }
    } else {
        while (ShowCursor(FALSE) >= 0) { /* 一直到藏起来 */ }
    }
}

void eng_quit(void)
{
    g_eng.running = 0;
    if (g_eng.hwnd) PostMessageW(g_eng.hwnd, WM_CLOSE, 0, 0);
}

/* 把后台缓冲存成 24 位 BMP（自测用） */
int eng_screenshot(const char *path)
{
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    FILE *fp;
    int x, y, rowbytes;
    unsigned char *row;
    int ok = 1;

    if (!g_eng.mem_dc || !g_eng.buf_w) return 0;

    rowbytes = ((g_eng.buf_w * 3 + 3) / 4) * 4;
    row = (unsigned char *)malloc((size_t)rowbytes);
    if (!row) return 0;

    memset(&fh, 0, sizeof(fh));
    memset(&ih, 0, sizeof(ih));
    ih.biSize = sizeof(ih);
    ih.biWidth = g_eng.buf_w;
    ih.biHeight = g_eng.buf_h;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = BI_RGB;

    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + rowbytes * g_eng.buf_h;

    fp = fopen(path, "wb");
    if (!fp) { free(row); return 0; }

    fwrite(&fh, sizeof(fh), 1, fp);
    fwrite(&ih, sizeof(ih), 1, fp);

    for (y = g_eng.buf_h - 1; y >= 0; y--) {   /* BMP 是自底向上 */
        for (x = 0; x < g_eng.buf_w; x++) {
            COLORREF c = GetPixel(g_eng.mem_dc, x, y);
            row[x * 3 + 0] = GetBValue(c);
            row[x * 3 + 1] = GetGValue(c);
            row[x * 3 + 2] = GetRValue(c);
        }
        if (fwrite(row, (size_t)rowbytes, 1, fp) != 1) { ok = 0; break; }
    }
    fclose(fp);
    free(row);
    return ok;
}