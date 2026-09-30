#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "lvgl_login_page.h"
#include "config.h"
#include "ret_codes.h"
#include "pickup_monitor.h"
#include "ui_layout.h"
#include "input.h"

/* 按键图标路径表 (按键 1..9, 删除, 0, 确认) */
static const char *KEY_ICON_PATHS[] = {
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/num_1.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/num_2.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/num_3.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/num_4.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/num_5.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/num_6.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/num_7.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/num_8.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/num_9.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/num_yes.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/num_0.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/num_delete.sjpg",
};

/* 按键坐标表 —— 与 lvgl_takeout_page.c 保持一致；
 * 水平位置相对键盘区起点(UI_KEYPAD_X)推导，随屏幕宽度自适应 */
typedef struct { int x, y; } btn_pos_t;

static const btn_pos_t KEY_POSITIONS[] = {
    /* 1, 2, 3 */    {UI_KEYPAD_X + 15, 312}, {UI_KEYPAD_X + 15, 196},{UI_KEYPAD_X + 15, 81} ,
    /* 4, 5, 6 */    {UI_KEYPAD_X + 57, 312}, {UI_KEYPAD_X + 57, 196},{UI_KEYPAD_X + 57, 81} ,
    /* 7, 8, 9 */    {UI_KEYPAD_X + 99, 312}, {UI_KEYPAD_X + 99, 196},{UI_KEYPAD_X + 99, 81} ,
    /* 确认, 0, 删除 */ {UI_KEYPAD_X + 140, 312},{UI_KEYPAD_X + 140, 196},{UI_KEYPAD_X + 140, 81}
};

/* 按键对应的码值 (与 KEY_POSITIONS 顺序一一对应) */
static const int KEY_CODES[] = {
    1, 2, 3,
    4, 5, 6,
    7, 8, 9,
    UI_KEY_CONFIRM, 0, UI_KEY_DELETE,
};

/* 功能按钮坐标 */
#define GET_CODE_X1  278
#define GET_CODE_Y1  56
#define GET_CODE_X2  328
#define GET_CODE_Y2  228

#define LOGIN_X1    352
#define LOGIN_Y1    55
#define LOGIN_X2    403
#define LOGIN_Y2    425

#define BACK_X1     45
#define BACK_Y1     425
#define BACK_X2     100
#define BACK_Y2     475

/* 输入框坐标 */
#define PHONE_FIELD_X1  198
#define PHONE_FIELD_Y1  54
#define PHONE_FIELD_X2  245
#define PHONE_FIELD_Y2  425

#define CODE_FIELD_X1   280
#define CODE_FIELD_Y1   244
#define CODE_FIELD_X2   328
#define CODE_FIELD_Y2   425

/* 状态变量 */
#define MAX_PHONE_DIGITS  11
#define MAX_CODE_DIGITS   4

static lv_obj_t *s_phone_canvas = NULL;
static lv_color_t *s_phone_canvas_buf = NULL;
static lv_obj_t *s_code_canvas = NULL;
static lv_color_t *s_code_canvas_buf = NULL;

static int s_phone_digits[MAX_PHONE_DIGITS] = {-1};
static int s_phone_count = 0;
static int s_code_digits[MAX_CODE_DIGITS] = {-1};
static int s_code_count = 0;

static int s_active_field = 0;  /* 0=none, 1=phone, 2=code */
static int s_pending_action = -1;
static lv_timer_t *s_cursor_timer = NULL;
static int s_cursor_visible = 0;
static lv_obj_t *s_key_btns[12] = {NULL};
static lv_obj_t *s_keyboard_overlay = NULL;
static lv_obj_t *s_keyboard_bg = NULL;
static int s_keyboard_visible = 0;

#define PHONE_CANVAS_W  361
#define PHONE_CANVAS_H  24
#define CODE_CANVAS_W   171
#define CODE_CANVAS_H   24

/* ============================================================
 * 创建一个与 lv_example_btn_1 风格一致的按钮
 *   参照 lvgl_takeout_page.c 中的 _create_img_btn
 * ============================================================ */
static lv_obj_t *_create_img_btn(lv_coord_t x, lv_coord_t y,
                                  const char *img_src,
                                  lv_event_cb_t cb, void *user_data,
                                  int flat, int radius,
                                  lv_color_t bg_color,
                                  lv_color_t pressed_color)
{
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_radius(btn, radius, 0);
    lv_obj_set_style_clip_corner(btn, true, 0);
    lv_obj_set_style_bg_color(btn, bg_color, 0);
    lv_obj_set_style_bg_color(btn, pressed_color, LV_STATE_PRESSED);
    lv_obj_set_style_color_filter_opa(btn, 0, LV_STATE_PRESSED);

    if (flat)
    {
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
    }
    else
    {
        lv_obj_set_style_border_color(btn, lv_color_black(), 0);
        lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_RIGHT, 0);
        lv_obj_set_style_border_width(btn, 2, 0);
        lv_obj_set_style_shadow_width(btn, 4, 0);
        lv_obj_set_style_shadow_ofs_x(btn, 2, 0);
        lv_obj_set_style_shadow_ofs_y(btn, 2, 0);
        lv_obj_set_style_shadow_color(btn, lv_color_hex(0x888888), 0);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_50, 0);
    }

    if (img_src != NULL)
    {
        lv_obj_t *img = lv_img_create(btn);
        lv_img_set_src(img, img_src);
        lv_obj_center(img);
        lv_obj_set_style_radius(img, radius, 0);
        lv_obj_set_style_clip_corner(img, true, 0);
    }

    if (cb != NULL)
    {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_PRESSED, user_data);
    }

    return btn;
}

/* ============================================================
 * 创建透明功能按钮 (用于背景图上的交互区域)
 * ============================================================ */
static lv_obj_t *_create_transparent_btn(lv_coord_t x1, lv_coord_t y1,
                                          lv_coord_t x2, lv_coord_t y2,
                                          lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, x2 - x1, y2 - y1);
    lv_obj_set_pos(btn, x1, y1);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    if (cb != NULL)
    {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_PRESSED, user_data);
    }
    return btn;
}

/* ============================================================
 * 光标闪烁定时器回调
 * ============================================================ */
static void _redraw_all(void);

static void _cursor_timer_cb(lv_timer_t *t)
{
    (void)t;
    s_cursor_visible = !s_cursor_visible;
    _redraw_all();
}

/* ============================================================
 * 重绘手机号显示
 * ============================================================ */
static void _redraw_phone(void)
{
    if (s_phone_canvas == NULL || s_phone_canvas_buf == NULL)
        return;

    char buf[128] = {0};
    int pos = 0;

    for (int s = 0; s < 4; s++) buf[pos++] = ' ';
    for (int i = 0; i < MAX_PHONE_DIGITS; i++)
    {
        if (s_phone_digits[i] >= 0)
            buf[pos++] = (char)('0' + s_phone_digits[i]);
        else if (i == s_phone_count && s_active_field == 1 && s_cursor_visible)
            buf[pos++] = '|';
        else
            buf[pos++] = ' ';
        if (i < MAX_PHONE_DIGITS - 1)
            buf[pos++] = ' ';
    }

    lv_canvas_fill_bg(s_phone_canvas, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = &lv_font_montserrat_20;
    label_dsc.color = lv_color_black();
    label_dsc.align = LV_TEXT_ALIGN_LEFT;
    lv_canvas_draw_text(s_phone_canvas, 0, 0, PHONE_CANVAS_W, &label_dsc, buf);
    lv_obj_invalidate(s_phone_canvas);
}

/* ============================================================
 * 重绘验证码显示
 * ============================================================ */
static void _redraw_code(void)
{
    if (s_code_canvas == NULL || s_code_canvas_buf == NULL)
        return;

    char buf[128] = {0};
    int pos = 0;

    for (int s = 0; s < 4; s++) buf[pos++] = ' ';
    for (int i = 0; i < MAX_CODE_DIGITS; i++)
    {
        if (s_code_digits[i] >= 0)
            buf[pos++] = (char)('0' + s_code_digits[i]);
        else if (i == s_code_count && s_active_field == 2 && s_cursor_visible)
            buf[pos++] = '|';
        else
            buf[pos++] = ' ';
        if (i < MAX_CODE_DIGITS - 1)
            buf[pos++] = ' ';
    }

    lv_canvas_fill_bg(s_code_canvas, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = &lv_font_montserrat_20;
    label_dsc.color = lv_color_black();
    label_dsc.align = LV_TEXT_ALIGN_LEFT;
    lv_canvas_draw_text(s_code_canvas, 0, 0, CODE_CANVAS_W, &label_dsc, buf);
    lv_obj_invalidate(s_code_canvas);
}

/* ============================================================
 * 重绘所有显示
 * ============================================================ */
static void _redraw_all(void)
{
    _redraw_phone();
    _redraw_code();
}

/* ============================================================
 * 键盘显示/隐藏
 * ============================================================ */
static void _hide_keyboard(void)
{
    if (!s_keyboard_visible) return;
    for (int i = 0; i < 12; i++)
    {
        if (s_key_btns[i])
            lv_obj_add_flag(s_key_btns[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (s_keyboard_overlay)
        lv_obj_add_flag(s_keyboard_overlay, LV_OBJ_FLAG_HIDDEN);
    if (s_keyboard_bg)
        lv_obj_add_flag(s_keyboard_bg, LV_OBJ_FLAG_HIDDEN);
    s_keyboard_visible = 0;
    /* 退出输入状态前清除光标：先置 visible=0 并重绘（此时 active_field 仍有效），
     * 避免光标字符残留在 canvas 上常亮 */
    s_cursor_visible = 0;
    if (s_active_field != 0) _redraw_all();
    s_active_field = 0;
    /* 键盘收起后暂停光标定时器，避免空转重绘 */
    if (s_cursor_timer != NULL)
    {
        lv_timer_pause(s_cursor_timer);
    }
}

static void _show_keyboard(void)
{
    if (s_keyboard_visible) return;
    if (s_keyboard_bg)
        lv_obj_clear_flag(s_keyboard_bg, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 12; i++)
    {
        if (s_key_btns[i])
            lv_obj_clear_flag(s_key_btns[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (s_keyboard_overlay)
        lv_obj_clear_flag(s_keyboard_overlay, LV_OBJ_FLAG_HIDDEN);
    s_keyboard_visible = 1;
    if (s_cursor_timer != NULL)
    {
        lv_timer_resume(s_cursor_timer);
    }
}

static void _overlay_cb(lv_event_t *e)
{
    (void)e;
    _hide_keyboard();
}

/* ============================================================
 * 按键事件回调
 * ============================================================ */
static void _key_btn_cb(lv_event_t *e)
{
    int key = (int)(intptr_t)lv_event_get_user_data(e);

    if (s_active_field == 0) return;

    if (key >= 0 && key <= 9)
    {
        if (s_active_field == 1)
        {
            if (s_phone_count < MAX_PHONE_DIGITS)
            {
                s_phone_digits[s_phone_count++] = key;
                _redraw_phone();
            }
        }
        else if (s_active_field == 2)
        {
            if (s_code_count < MAX_CODE_DIGITS)
            {
                s_code_digits[s_code_count++] = key;
                _redraw_code();
            }
        }
    }
    else if (key == UI_KEY_DELETE)
    {
        if (s_active_field == 1 && s_phone_count > 0)
        {
            s_phone_digits[--s_phone_count] = -1;
            _redraw_phone();
        }
        else if (s_active_field == 2 && s_code_count > 0)
        {
            s_code_digits[--s_code_count] = -1;
            _redraw_code();
        }
    }
    else if (key == UI_KEY_CONFIRM)
    {
        _hide_keyboard();
        s_pending_action = UI_KEY_CONFIRM;
    }
}

/* ============================================================
 * 返回按钮回调
 * ============================================================ */
static void _back_btn_cb(lv_event_t *e)
{
    (void)e;
    s_pending_action = UI_KEY_BACK;
}

/* ============================================================
 * 获取验证码按钮回调
 * ============================================================ */
static void _get_code_btn_cb(lv_event_t *e)
{
    (void)e;
    _hide_keyboard();
    s_pending_action = UI_KEY_QUERY;
}

/* ============================================================
 * 登录按钮回调
 * ============================================================ */
static void _login_btn_cb(lv_event_t *e)
{
    (void)e;
    _hide_keyboard();
    s_pending_action = UI_KEY_CONFIRM;
}

/* ============================================================
 * 手机号输入框点击回调
 * ============================================================ */
static void _phone_field_cb(lv_event_t *e)
{
    (void)e;
    s_active_field = 1;
    _show_keyboard();
    if (s_cursor_timer == NULL)
        s_cursor_timer = lv_timer_create(_cursor_timer_cb, 500, NULL);
    s_cursor_visible = 1;
    _redraw_all();
}

/* ============================================================
 * 验证码输入框点击回调
 * ============================================================ */
static void _code_field_cb(lv_event_t *e)
{
    (void)e;
    s_active_field = 2;
    _show_keyboard();
    if (s_cursor_timer == NULL)
        s_cursor_timer = lv_timer_create(_cursor_timer_cb, 500, NULL);
    s_cursor_visible = 1;
    _redraw_all();
}

/* ============================================================
 * 创建登录界面
 * ============================================================ */
int lvgl_login_create(ui_context_t *ui, const char *bg_sjpg_path)
{
    if (ui == NULL || bg_sjpg_path == NULL) return -1;

    /* 页面切换时重置输入设备状态，清除上一页残留的按压/坐标 */
    evdev_reset_for_page_switch();

    lv_obj_t *screen = lv_scr_act();

    /* 清空状态 */
    memset(s_phone_digits, -1, sizeof(s_phone_digits));
    memset(s_code_digits, -1, sizeof(s_code_digits));
    s_phone_count = 0;
    s_code_count = 0;
    s_active_field = 0;
    s_pending_action = -1;
    /* 复位键盘/光标可见状态，避免依赖上一次 destroy */
    s_keyboard_visible = 0;
    s_cursor_visible = 0;

    /* ---- 背景 ---- */
    lv_obj_set_style_bg_img_src(screen, bg_sjpg_path, 0);
    lv_obj_set_style_bg_img_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_TRANSP, 0);

    /* ---- 手机号显示 canvas (竖排) ---- */
    s_phone_canvas_buf = (lv_color_t *)malloc(PHONE_CANVAS_W * PHONE_CANVAS_H * sizeof(lv_color_t));
    if (s_phone_canvas_buf == NULL)
    {
        /* 清除背景样式，防残留 */
        lv_obj_set_style_bg_img_src(screen, NULL, 0);
        return -1;
    }
    s_phone_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(s_phone_canvas, s_phone_canvas_buf, PHONE_CANVAS_W, PHONE_CANVAS_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_phone_canvas,
                   PHONE_FIELD_X1 + (PHONE_FIELD_X2 - PHONE_FIELD_X1) / 2 - PHONE_CANVAS_W / 2,
                   PHONE_FIELD_Y1 + (PHONE_FIELD_Y2 - PHONE_FIELD_Y1) / 2 - PHONE_CANVAS_H / 2);
    lv_canvas_fill_bg(s_phone_canvas, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    lv_img_set_angle(s_phone_canvas, -900);
    lv_obj_set_style_bg_opa(s_phone_canvas, LV_OPA_TRANSP, 0);

    /* ---- 验证码显示 canvas (竖排) ---- */
    s_code_canvas_buf = (lv_color_t *)malloc(CODE_CANVAS_W * CODE_CANVAS_H * sizeof(lv_color_t));
    if (s_code_canvas_buf == NULL)
    {
        /* 清理已创建的手机号 canvas，避免悬垂指针 */
        if (s_phone_canvas != NULL)
        {
            lv_obj_del(s_phone_canvas);
            s_phone_canvas = NULL;
        }
        free(s_phone_canvas_buf);
        s_phone_canvas_buf = NULL;
        /* 清除背景样式，防残留 */
        lv_obj_set_style_bg_img_src(screen, NULL, 0);
        return -1;
    }
    s_code_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(s_code_canvas, s_code_canvas_buf, CODE_CANVAS_W, CODE_CANVAS_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_code_canvas,
                   CODE_FIELD_X1 + (CODE_FIELD_X2 - CODE_FIELD_X1) / 2 - CODE_CANVAS_W / 2,
                   CODE_FIELD_Y1 + (CODE_FIELD_Y2 - CODE_FIELD_Y1) / 2 - CODE_CANVAS_H / 2);
    lv_canvas_fill_bg(s_code_canvas, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    lv_img_set_angle(s_code_canvas, -900);
    lv_obj_set_style_bg_opa(s_code_canvas, LV_OPA_TRANSP, 0);

    /* 初始重绘 */
    _redraw_all();

    /* ---- 键盘遮罩层 (点击空白区域收起键盘) ---- */
    s_keyboard_overlay = lv_btn_create(screen);
    lv_obj_set_size(s_keyboard_overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_keyboard_overlay, 0, 0);
    lv_obj_set_style_bg_opa(s_keyboard_overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_keyboard_overlay, 0, 0);
    lv_obj_set_style_shadow_width(s_keyboard_overlay, 0, 0);
    lv_obj_add_event_cb(s_keyboard_overlay, _overlay_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_flag(s_keyboard_overlay, LV_OBJ_FLAG_HIDDEN);

    s_keyboard_bg = lv_obj_create(screen);
    lv_obj_set_pos(s_keyboard_bg, UI_KEYPAD_X, UI_KEYPAD_Y);
    lv_obj_set_size(s_keyboard_bg, UI_KEYPAD_W, UI_KEYPAD_H);
    lv_obj_set_style_bg_color(s_keyboard_bg, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_keyboard_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_keyboard_bg, 0, 0);
    lv_obj_set_style_shadow_width(s_keyboard_bg, 0, 0);
    lv_obj_add_flag(s_keyboard_bg, LV_OBJ_FLAG_HIDDEN);

    /* ---- 手机号输入框点击区域 ---- */
    _create_transparent_btn(190, PHONE_FIELD_Y1,
                            260, PHONE_FIELD_Y2,
                            _phone_field_cb, NULL);

    /* ---- 验证码输入框点击区域 ---- */
    _create_transparent_btn(265, 230,
                            340, CODE_FIELD_Y2,
                            _code_field_cb, NULL);

    /* ---- 12 个按键 (3x4 键盘) —— 参照 lvgl_takeout_page.c 实现 ---- */
    for (int i = 0; i < 12; i++)
    {
        s_key_btns[i] = _create_img_btn(KEY_POSITIONS[i].x, KEY_POSITIONS[i].y,
                        KEY_ICON_PATHS[i], _key_btn_cb,
                        (void *)(intptr_t)KEY_CODES[i], 0, 0,
                        lv_color_hex(0xFFFFFF), lv_color_hex(0x000000));
        lv_obj_add_flag(s_key_btns[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* ---- 返回按钮 ---- */
    {
        lv_obj_t *btn = _create_img_btn(BACK_X1, BACK_Y1,
                    NULL,
                    _back_btn_cb, NULL, 1, 20,
                    lv_color_hex(0xFFFFFF), lv_color_hex(0xFFFFFF));
        lv_obj_set_size(btn, BACK_X2 - BACK_X1, BACK_Y2 - BACK_Y1);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    }

    /* ---- 获取验证码按钮 ---- */
    {
        lv_obj_t *btn = _create_img_btn(GET_CODE_X1, GET_CODE_Y1,
                    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/receive_phonecode.sjpg",
                    _get_code_btn_cb, NULL, 1, 10,
                    lv_color_hex(0xFD8625), lv_color_hex(0xFD8625));
        lv_obj_set_size(btn, GET_CODE_X2 - GET_CODE_X1, GET_CODE_Y2 - GET_CODE_Y1);
    }

    /* ---- 登录按钮 ---- */
    {
        lv_obj_t *btn = _create_img_btn(LOGIN_X1, LOGIN_Y1,
                    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/login.sjpg",
                    _login_btn_cb, NULL, 1, 10,
                    lv_color_hex(0x01682B), lv_color_hex(0x01682B));
        lv_obj_set_size(btn, LOGIN_X2 - LOGIN_X1, LOGIN_Y2 - LOGIN_Y1);
    }

    lv_obj_invalidate(screen);
    lv_timer_handler();
    return 0;
}

/* ============================================================
 * 销毁登录界面
 * ============================================================ */
void lvgl_login_destroy(void)
{
    if (s_cursor_timer != NULL)
    {
        lv_timer_del(s_cursor_timer);
        s_cursor_timer = NULL;
    }
    s_cursor_visible = 0;
    s_keyboard_visible = 0;
    s_keyboard_overlay = NULL;
    s_keyboard_bg = NULL;
    memset(s_key_btns, 0, sizeof(s_key_btns));

    lv_obj_set_style_bg_img_src(lv_scr_act(), NULL, 0);
    lv_obj_clean(lv_scr_act());

    s_phone_canvas = NULL;
    s_code_canvas = NULL;

    if (s_phone_canvas_buf != NULL)
    {
        free(s_phone_canvas_buf);
        s_phone_canvas_buf = NULL;
    }
    if (s_code_canvas_buf != NULL)
    {
        free(s_code_canvas_buf);
        s_code_canvas_buf = NULL;
    }

    s_phone_count = 0;
    s_code_count = 0;
    s_active_field = 0;
    s_pending_action = -1;
    memset(s_phone_digits, -1, sizeof(s_phone_digits));
    memset(s_code_digits, -1, sizeof(s_code_digits));
}

/* ============================================================
 * 获取用户操作
 * ============================================================ */
int lvgl_login_get_action(ui_context_t *ui)
{
    if (ui == NULL) return -1;

    time_t start_time = time(NULL);

    while (s_pending_action == -1)
    {
        int elapsed = (int)(time(NULL) - start_time);
        if (elapsed >= PAGE_TIMEOUT_SEC)
        {
            return -1;
        }

        if (PICKUP_NOTIFY_GET())
        {
            printf("[登录] 检测到扫码取件通知，中断当前操作\n");
            return RET_SCAN_PICKUP;
        }

        lv_timer_handler();
        usleep(20000);
    }

    int action = s_pending_action;
    s_pending_action = -1;
    return action;
}

/* ============================================================
 * 获取已输入的手机号
 * ============================================================ */
int lvgl_login_get_phone(char *phone, size_t phone_size)
{
    if (phone == NULL || phone_size < 12) return -1;
    if (s_phone_count != MAX_PHONE_DIGITS) return -1;
    for (int i = 0; i < MAX_PHONE_DIGITS; i++)
    {
        phone[i] = (char)('0' + s_phone_digits[i]);
    }
    phone[MAX_PHONE_DIGITS] = '\0';
    return 0;
}

/* ============================================================
 * 获取已输入的验证码
 * ============================================================ */
int lvgl_login_get_code(char *code, size_t code_size)
{
    if (code == NULL || code_size < 5) return -1;
    if (s_code_count != MAX_CODE_DIGITS) return -1;
    for (int i = 0; i < MAX_CODE_DIGITS; i++)
    {
        code[i] = (char)('0' + s_code_digits[i]);
    }
    code[MAX_CODE_DIGITS] = '\0';
    return 0;
}

/* ============================================================
 * 清空验证码
 * ============================================================ */
void lvgl_login_clear_code(void)
{
    memset(s_code_digits, -1, sizeof(s_code_digits));
    s_code_count = 0;
    _redraw_code();
}