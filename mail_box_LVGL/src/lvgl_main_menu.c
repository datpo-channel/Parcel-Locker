#include "lvgl_main_menu.h"
#include "lvgl.h"
#include "input.h"
#include <stdio.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string.h>
#include <wchar.h>

static int g_menu_action = 0;
static volatile int g_anim_done = 0;
static lv_timer_t *g_anim_timer = NULL;

static lv_style_prop_t g_trans_props[] = {
    LV_STYLE_TRANSFORM_WIDTH, LV_STYLE_TRANSFORM_HEIGHT, LV_STYLE_TEXT_LETTER_SPACE, 0
};

static lv_style_transition_dsc_t g_transition_dsc_def;
static lv_style_transition_dsc_t g_transition_dsc_pr;
static int g_transition_inited = 0;

static lv_obj_t *g_time_canvas = NULL;
static lv_color_t *g_time_canvas_buf = NULL;
static lv_obj_t *g_ad_imgs[3] = {NULL, NULL, NULL};
static lv_obj_t *g_ad_viewport = NULL;  /* 广告滚动视口容器：裁剪超出广告区域的部分 */
static lv_timer_t *g_ad_timer = NULL;

static const char *g_ad_files[] = {
    "/Workspace/mail_box_pic/resource/sjpeg/AD_pic/AD1.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/AD_pic/AD2.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/AD_pic/AD3.sjpg",
};
static int g_ad_count = 3;
static int g_ad_cur = 0;

/* 广告滚动切换：间隔与动画时长 */
#define AD_SWITCH_MS      5000   /* 每 5 秒滚动一个广告 */
#define AD_SWITCH_ANIM_MS 1000   /* 滚动动画时长（毫秒），缓慢平滑滑入 */
/* 广告区尺寸：与背景 main_menu.jpg 米白广告位(约 x95~235,y18~462)及
 * AD*.jpg 源图尺寸(140x444) 1:1 对应，创建时即固定，不等图片解码完成 */
#define AD_AREA_W  140
#define AD_AREA_H  444

static volatile int g_ad_animating = 0;  /* 滚动动画进行中标志，防重复触发 */
static int g_ad_next = 0;                /* 下一次滚动目标索引 */

#define CANVAS_W          90
#define CANVAS_H          230
#define FONT_PATH         "/font/msyh.ttc"
#define FONT_SIZE         17
/* 竖排字符间距（按字符类型区分）：
 * - ASCII 数字/符号保持原紧凑间距（时间 "00:00:00" 视觉不变）
 * - 中文/全角（>=0x80）须 ≥ 字号否则字形重叠，+3 保留自然空隙
 *   （雅黑等方正字体重叠尤为明显） */
#define FONT_CHAR_SPACING_ASCII (FONT_SIZE * 6 / 10)  /* 10px */
#define FONT_CHAR_SPACING_CJK   (FONT_SIZE + 3)       /* 20px */
#define TIME_X_OFFSET     20          // 时间段落 X 偏移
#define WEATHER_X_OFFSET  TIME_X_OFFSET + 30 // 天气段落 X 偏移
#define WEATHER_COL_GAP   20              // 天气段落列间距
#define TEXT_Y_OFFSET     5           // 文字底部距 canvas 底边距
#define CANVAS_BG_COLOR   lv_color_hex(0x0A5F39)   // 背景色
#define COLOR_TIME        lv_color_hex(0xFFFFFF)   // 时间文字色
#define COLOR_CITY        lv_color_hex(0xFFFFFF)   // 城市文字色
#define COLOR_WEATHER     lv_color_hex(0x00BFFF)   // 天气文字色

static FT_Library  g_ft_lib = NULL;
static FT_Face     g_ft_face = NULL;
static int         g_ft_ready = 0;

static int _ft_init(void)
{
    if (g_ft_ready) return 0;

    if (FT_Init_FreeType(&g_ft_lib)) {
        fprintf(stderr, "[主菜单] FreeType 初始化失败\n");
        return -1;
    }
    if (FT_New_Face(g_ft_lib, FONT_PATH, 0, &g_ft_face)) {
        fprintf(stderr, "[主菜单] FreeType 字体加载失败: %s\n", FONT_PATH);
        FT_Done_FreeType(g_ft_lib);
        g_ft_lib = NULL;
        return -1;
    }
    FT_Select_Charmap(g_ft_face, FT_ENCODING_UNICODE);
    g_ft_ready = 1;
    return 0;
}

static void _ft_set_size(void)
{
    if (!g_ft_ready) return;
    FT_Set_Pixel_Sizes(g_ft_face, 0, FONT_SIZE);
}

static void _ft_cleanup(void)
{
    if (g_ft_face) {
        FT_Done_Face(g_ft_face);
        g_ft_face = NULL;
    }
    if (g_ft_lib) {
        FT_Done_FreeType(g_ft_lib);
        g_ft_lib = NULL;
    }
    g_ft_ready = 0;
}

/* ============================================================
 * UTF-8 多字节解码为 wchar_t (支持 1~3 字节序列, 非法字节跳过)
 * 板端默认 C locale 下 mbstowcs 直接返回 -1 导致中文不渲染,
 * 故手写解码替代, 不依赖任何 locale 设置
 * ============================================================ */
static int _utf8_to_wchar(wchar_t *wbuf, size_t wbuf_size, const char *text)
{
    const unsigned char *p = (const unsigned char *)text;
    size_t n = 0;

    while (*p != '\0' && n + 1 < wbuf_size)
    {
        unsigned char c = *p;
        uint32_t cp;
        int len;

        if (c < 0x80)
        {
            cp = c;
            len = 1;
        }
        else if ((c & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80)
        {
            cp = ((uint32_t)(c & 0x1F) << 6) | (p[1] & 0x3F);
            len = 2;
        }
        else if ((c & 0xF0) == 0xE0 &&
                 (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80)
        {
            cp = ((uint32_t)(c & 0x0F) << 12) |
                 ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            len = 3;
        }
        else
        {
            /* 非法字节：跳过 */
            p++;
            continue;
        }

        wbuf[n++] = (wchar_t)cp;
        p += len;
    }

    wbuf[n] = L'\0';
    return (int)n;
}

/**************************************************************************
 *
 *   @brief : 在 canvas 缓冲区上绘制文字（垂直方向，从下往上）
 *   @arg   : buf    canvas 缓冲区
 *   @arg   : w      缓冲区宽度（像素）
 *   @arg   : h      缓冲区高度（像素）
 *   @arg   : x      文字起始 x 坐标（在缓冲区内）
 *   @arg   : y_bottom 文字底部 y 坐标（文字从下往上绘制）
 *   @arg   : text   要绘制的 UTF-8 文字
 *   @arg   : color  文字颜色
 *
 *   @note  : 文字底部朝左，字符从下往上排列
 *
 ***************************************************************************/
static void _canvas_draw_text(lv_color_t *buf, int w, int h,
                               int x, int y_bottom,
                               const char *text, lv_color_t color)
{
    if (!g_ft_ready || buf == NULL || text == NULL) return;

    wchar_t wbuf[256];
    int len = _utf8_to_wchar(wbuf, 256, text);
    if (len <= 0) return;

    _ft_set_size();
    FT_GlyphSlot slot = g_ft_face->glyph;

    int sy = y_bottom;

    for (int i = 0; i < len; i++) {
        if (FT_Load_Char(g_ft_face, wbuf[i], FT_LOAD_RENDER))
            continue;

        for (int j = 0; j < (int)slot->bitmap.rows; j++) {
            for (int k = 0; k < (int)slot->bitmap.width; k++) {
                unsigned char val = slot->bitmap.buffer[j * slot->bitmap.width + k];
                int rx = x - slot->bitmap_top + j;
                int ry = sy - slot->bitmap_left - k;

                if (rx < 0 || rx >= w || ry < 0 || ry >= h)
                    continue;

                if (val > 128) {
                    buf[ry * w + rx] = color;
                } else if (val > 64) {
                    /* 抗锯齿：按字形灰度混合前景/背景，lv_color_mix 自动适配当前色深 */
                    buf[ry * w + rx] = lv_color_mix(color, buf[ry * w + rx], val);
                }
            }
        }
        /* 中文/全角字符用宽松间距，ASCII 数字/符号保持原紧凑间距 */
        if (wbuf[i] >= 0x80)
        {
            sy -= FONT_CHAR_SPACING_CJK;
        }
        else
        {
            sy -= FONT_CHAR_SPACING_ASCII;
        }
    }
}

static char g_cached_time[32] = {0};
static char g_cached_city[64] = {0};
static char g_cached_weather[64] = {0};

static char g_last_time[32] = {0};
static char g_last_city[64] = {0};
static char g_last_weather[64] = {0};
static int g_canvas_first = 1;

static void _clear_region(lv_color_t *buf, int w, int x1, int x2)
{
    if (x1 < 0) x1 = 0;
    if (x2 > w) x2 = w;
    for (int y = 0; y < CANVAS_H; y++) {
        for (int x = x1; x < x2; x++) {
            buf[y * w + x] = CANVAS_BG_COLOR;
        }
    }
}

/**************************************************************************
 *
 *   @brief : 刷新时间天气 canvas（增量刷新，只重绘变化区域）
 *
 ***************************************************************************/
static void _refresh_canvas(void)
{
    if (g_time_canvas_buf == NULL || g_time_canvas == NULL) return;

    lv_color_t *buf = g_time_canvas_buf;
    int w = CANVAS_W;
    int h = CANVAS_H;
    int y_base = h - TEXT_Y_OFFSET;

    if (g_canvas_first) {
        for (int i = 0; i < w * h; i++) {
            buf[i] = CANVAS_BG_COLOR;
        }
        _canvas_draw_text(buf, w, h, TIME_X_OFFSET, y_base,
                          g_cached_time, COLOR_TIME);

        if (g_cached_city[0] != '\0') {
            _canvas_draw_text(buf, w, h, WEATHER_X_OFFSET, y_base,
                              g_cached_city, COLOR_CITY);
        }
        if (g_cached_weather[0] != '\0') {
            _canvas_draw_text(buf, w, h, WEATHER_X_OFFSET + WEATHER_COL_GAP,
                              y_base, g_cached_weather, COLOR_WEATHER);
        }

        strncpy(g_last_time, g_cached_time, sizeof(g_last_time) - 1);
        strncpy(g_last_city, g_cached_city, sizeof(g_last_city) - 1);
        strncpy(g_last_weather, g_cached_weather, sizeof(g_last_weather) - 1);
        g_canvas_first = 0;
        lv_obj_invalidate(g_time_canvas);
        return;
    }

    int time_changed  = (strcmp(g_cached_time, g_last_time) != 0);
    int city_changed  = (strcmp(g_cached_city, g_last_city) != 0);
    int wthr_changed  = (strcmp(g_cached_weather, g_last_weather) != 0);

    if (!time_changed && !city_changed && !wthr_changed) {
        return;
    }

    if (time_changed) {
        _clear_region(buf, w, TIME_X_OFFSET - FONT_SIZE,
                      TIME_X_OFFSET + FONT_SIZE);
        _canvas_draw_text(buf, w, h, TIME_X_OFFSET, y_base,
                          g_cached_time, COLOR_TIME);
        strncpy(g_last_time, g_cached_time, sizeof(g_last_time) - 1);
    }

    if (city_changed || wthr_changed) {
        _clear_region(buf, w, WEATHER_X_OFFSET - FONT_SIZE,
                      WEATHER_X_OFFSET + WEATHER_COL_GAP + FONT_SIZE);
        if (g_cached_city[0] != '\0') {
            _canvas_draw_text(buf, w, h, WEATHER_X_OFFSET, y_base,
                              g_cached_city, COLOR_CITY);
        }
        if (g_cached_weather[0] != '\0') {
            _canvas_draw_text(buf, w, h, WEATHER_X_OFFSET + WEATHER_COL_GAP,
                              y_base, g_cached_weather, COLOR_WEATHER);
        }
        strncpy(g_last_city, g_cached_city, sizeof(g_last_city) - 1);
        strncpy(g_last_weather, g_cached_weather, sizeof(g_last_weather) - 1);
    }

    if (g_time_canvas != NULL) {
        lv_obj_invalidate(g_time_canvas);
    }
}

static void _anim_done_cb(lv_timer_t *timer)
{
    /* 一次性动画定时器：回调内删除自身，避免每次进出主菜单泄漏常驻定时器 */
    lv_timer_del(timer);
    g_anim_done = 1;
    g_anim_timer = NULL;
}

static void _menu_btn_cb(lv_event_t *e)
{
    if (!g_anim_done) return;
    g_menu_action = (int)(intptr_t)lv_event_get_user_data(e);
}

static void _create_btn(lv_coord_t pos_x, lv_coord_t pos_y, lv_coord_t pad,
                         uint32_t bg_color, uint32_t press_bg_color,
                         const char *img_src, lv_event_cb_t event_cb,
                         void *user_data)
{
    if (!g_transition_inited) {
        lv_style_transition_dsc_init(&g_transition_dsc_def, g_trans_props,
                                      lv_anim_path_overshoot, 150, 50, NULL);
        lv_style_transition_dsc_init(&g_transition_dsc_pr, g_trans_props,
                                      lv_anim_path_ease_in_out, 150, 0, NULL);
        g_transition_inited = 1;
    }

    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_pos(btn, pos_x, pos_y);
    lv_obj_set_style_pad_hor(btn, pad + 7, 0);
    lv_obj_set_style_pad_ver(btn, pad, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg_color), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(press_bg_color), LV_STATE_PRESSED);
    lv_obj_set_style_color_filter_opa(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_width(btn, 5, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(btn, -5, LV_STATE_PRESSED);
    lv_obj_set_style_text_letter_space(btn, 10, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn, &g_transition_dsc_def, 0);
    lv_obj_set_style_transition(btn, &g_transition_dsc_pr, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, user_data);

    if (img_src != NULL) {
        lv_obj_t *img = lv_img_create(btn);
        lv_img_set_src(img, img_src);
        lv_obj_center(img);
    }
}

/* 滚动动画执行回调：垂直平移图片 */
static void _ad_anim_exec(void *var, int32_t v)
{
    lv_obj_set_style_translate_y((lv_obj_t *)var, v, 0);
}

/* 滚动动画完成回调：隐藏移出的旧图并复位，更新当前索引 */
static void _ad_roll_ready_cb(lv_anim_t *a)
{
    lv_obj_t *old = lv_anim_get_user_data(a);
    if (old != NULL)
    {
        lv_obj_add_flag(old, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_translate_y(old, 0, 0);
    }
    g_ad_cur = g_ad_next;
    g_ad_animating = 0;
}

static void _ad_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (g_ad_animating) return;

    /* 首次滚动前校准视口：广告图此时已完成解码，按其实际大小设置
     * 容器并启用裁剪，确保滚动只发生在广告显示区域内 */
    {
        lv_coord_t aw = lv_obj_get_width(g_ad_imgs[g_ad_cur]);
        lv_coord_t ah = lv_obj_get_height(g_ad_imgs[g_ad_cur]);
        /* 诊断：确认板端 sjpg 解码尺寸是否与广告位一致（转换异常时会偏离） */
        if (aw != AD_AREA_W || ah != AD_AREA_H)
        {
            printf("[广告] 尺寸异常: 解码 %dx%d, 预期 %dx%d\n",
                   aw, ah, AD_AREA_W, AD_AREA_H);
        }
        if (aw > 0 && ah > 0)
        {
            lv_obj_set_size(g_ad_viewport, aw, ah);
            lv_obj_clear_flag(g_ad_viewport, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        }
    }

    int next = (g_ad_cur + 1) % g_ad_count;
    lv_obj_t *cur = g_ad_imgs[g_ad_cur];
    lv_obj_t *nxt = g_ad_imgs[next];

    if (cur == NULL || nxt == NULL) return;

    lv_coord_t h = lv_obj_get_height(cur);
    if (h <= 0) return;  /* 图片未解码完成，跳过本次 */

    g_ad_animating = 1;
    g_ad_next = next;

    /* 新图从下方进入，旧图向上移出（垂直滚动切换） */
    lv_obj_clear_flag(nxt, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_translate_y(nxt, h, 0);

    lv_anim_t a_in;
    lv_anim_init(&a_in);
    lv_anim_set_var(&a_in, nxt);
    lv_anim_set_exec_cb(&a_in, _ad_anim_exec);
    lv_anim_set_values(&a_in, h, 0);
    lv_anim_set_time(&a_in, AD_SWITCH_ANIM_MS);
    lv_anim_set_path_cb(&a_in, lv_anim_path_ease_in_out);
    lv_anim_start(&a_in);

    lv_anim_t a_out;
    lv_anim_init(&a_out);
    lv_anim_set_var(&a_out, cur);
    lv_anim_set_exec_cb(&a_out, _ad_anim_exec);
    lv_anim_set_values(&a_out, 0, -h);
    lv_anim_set_time(&a_out, AD_SWITCH_ANIM_MS);
    lv_anim_set_path_cb(&a_out, lv_anim_path_ease_in_out);
    lv_anim_set_user_data(&a_out, cur);
    lv_anim_set_ready_cb(&a_out, _ad_roll_ready_cb);
    lv_anim_start(&a_out);
}

void lv_main_menu_create(void)
{
    /* 页面切换时重置输入设备状态，清除上一页残留的按压/坐标，
     * 防止误触发本页按钮（如取件后手指未抬起导致成功页被跳过） */
    evdev_reset_for_page_switch();

    g_menu_action = 0;

    lv_obj_set_style_bg_img_src(lv_scr_act(), "/Workspace/mail_box_pic/resource/sjpeg/menu_pic/main_menu.sjpg", 0);

    _create_btn(284, 251, 10, 0xFD9029, 0xFD9029,
                "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/takeout_box.sjpg",
                _menu_btn_cb, (void *)(intptr_t)1);

    _create_btn(284, 43, 10, 0x2AB562, 0x2AB562,
                "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/save_box.sjpg",
                _menu_btn_cb, (void *)(intptr_t)2);

    _create_btn(503, 251, 10, 0x2AB562, 0x2AB562,
                "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/sendman_login.sjpg",
                _menu_btn_cb, (void *)(intptr_t)3);

    _create_btn(503, 43, 10, 0xFD9029, 0xFD9029,
                "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/box_search.sjpg",
                _menu_btn_cb, (void *)(intptr_t)4);

    /* 广告滚动视口：容器定位在广告显示区。
     * 创建时按 AD_AREA_W/H 固定尺寸并裁剪（见下）；
     * 首次滚动时再按图片实际解码尺寸校准，容忍解码延迟 */
    g_ad_viewport = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(g_ad_viewport);
    lv_obj_clear_flag(g_ad_viewport, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(g_ad_viewport, 96, 19);
    /* 创建即固定广告区尺寸并裁剪，消除首次滚动前(约5秒)的无裁剪窗口：
     * 此前图片未解码完成时 lv_img 为 0x0，广告位会裸露背景装饰图案 */
    lv_obj_set_size(g_ad_viewport, AD_AREA_W, AD_AREA_H);
    lv_obj_clear_flag(g_ad_viewport, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    for (int i = 0; i < g_ad_count; i++) {
        g_ad_imgs[i] = lv_img_create(g_ad_viewport);
        lv_obj_set_pos(g_ad_imgs[i], 0, 0);
        lv_img_set_src(g_ad_imgs[i], g_ad_files[i]);
        if (i != 0) {
            lv_obj_add_flag(g_ad_imgs[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* 预解码全部广告图：滚动时新图无需现场解码，
     * 避免首帧空白/解码中残片提前显示 */
    for (int i = 0; i < g_ad_count; i++) {
        _lv_img_cache_open(g_ad_files[i], lv_color_white(), 0);
    }
    g_ad_cur = 0;
    g_ad_animating = 0;

    g_ad_timer = lv_timer_create(_ad_timer_cb, AD_SWITCH_MS, NULL);

    g_anim_done = 0;
    g_anim_timer = lv_timer_create(_anim_done_cb, 150, NULL);

    _ft_init();

    g_time_canvas_buf = (lv_color_t *)malloc(CANVAS_W * CANVAS_H * sizeof(lv_color_t));
    g_canvas_first = 1;
    if (g_time_canvas_buf != NULL) {
        g_time_canvas = lv_canvas_create(lv_scr_act());
        lv_canvas_set_buffer(g_time_canvas, g_time_canvas_buf,
                             CANVAS_W, CANVAS_H, LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_pos(g_time_canvas, 0, 250);
        strncpy(g_cached_time, "00:00:00", sizeof(g_cached_time) - 1);
        strncpy(g_cached_city, "加载中", sizeof(g_cached_city) - 1);
        g_cached_weather[0] = '\0';
        _refresh_canvas();
    }
}

void lv_main_menu_destroy(void)
{
    if (g_ad_timer != NULL) {
        lv_timer_del(g_ad_timer);
        g_ad_timer = NULL;
    }
    if (g_anim_timer != NULL) {
        lv_timer_del(g_anim_timer);
        g_anim_timer = NULL;
    }
    g_ad_animating = 0;

    /* 先清除屏幕背景图样式再清空对象，避免残留上一页背景 */
    lv_obj_set_style_bg_img_src(lv_scr_act(), NULL, 0);
    lv_obj_clean(lv_scr_act());

    _ft_cleanup();

    if (g_time_canvas_buf != NULL) {
        free(g_time_canvas_buf);
        g_time_canvas_buf = NULL;
    }

    g_time_canvas = NULL;
    g_ad_viewport = NULL;

    for (int i = 0; i < g_ad_count; i++) {
        /* 移除滚动动画，避免页面销毁后动画回调访问已删除对象 */
        if (g_ad_imgs[i] != NULL) {
            lv_anim_del(g_ad_imgs[i], NULL);
            g_ad_imgs[i] = NULL;
        }
    }

    g_menu_action = 0;
}

int lv_main_menu_get_action(void)
{
    int action = g_menu_action;
    g_menu_action = 0;
    return action;
}

void lv_main_menu_update_time(const char *time_str)
{
    if (time_str != NULL) {
        strncpy(g_cached_time, time_str, sizeof(g_cached_time) - 1);
        g_cached_time[sizeof(g_cached_time) - 1] = '\0';
    }
    _refresh_canvas();
}

void lv_main_menu_update_weather_info(const char *city, const char *weather_desc)
{
    if (city != NULL) {
        strncpy(g_cached_city, city, sizeof(g_cached_city) - 1);
        g_cached_city[sizeof(g_cached_city) - 1] = '\0';
    }
    if (weather_desc != NULL) {
        strncpy(g_cached_weather, weather_desc, sizeof(g_cached_weather) - 1);
        g_cached_weather[sizeof(g_cached_weather) - 1] = '\0';
    }
    _refresh_canvas();
}