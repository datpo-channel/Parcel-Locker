#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "lvgl_store_page.h"
#include "config.h"
#include "data_store.h"
#include "ui_logic.h"
#include "pickup_monitor.h"
#include "ui_layout.h"
#include "input.h"

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

typedef struct { int x, y; } btn_pos_t;

/* 按键坐标表 —— 与 lvgl_takeout_page.c 保持一致；
 * 水平位置相对键盘区起点(UI_KEYPAD_X)推导，随屏幕宽度自适应 */
static const btn_pos_t KEY_POSITIONS[] = {
    {UI_KEYPAD_X + 15, 312}, {UI_KEYPAD_X + 15, 196}, {UI_KEYPAD_X + 15, 81},
    {UI_KEYPAD_X + 57, 312}, {UI_KEYPAD_X + 57, 196}, {UI_KEYPAD_X + 57, 81},
    {UI_KEYPAD_X + 99, 312}, {UI_KEYPAD_X + 99, 196}, {UI_KEYPAD_X + 99, 81},
    {UI_KEYPAD_X + 140, 312}, {UI_KEYPAD_X + 140, 196}, {UI_KEYPAD_X + 140, 81},
};

static const int KEY_CODES[] = {
    1, 2, 3,
    4, 5, 6,
    7, 8, 9,
    UI_KEY_CONFIRM, 0, UI_KEY_DELETE,
};

#define BOX_SMALL_X1  378
#define BOX_SMALL_Y1  251
#define BOX_SMALL_X2  407
#define BOX_SMALL_Y2  325

#define BOX_MEDIUM_X1 378
#define BOX_MEDIUM_Y1 48
#define BOX_MEDIUM_X2 407
#define BOX_MEDIUM_Y2 124

#define BOX_LARGE_X1  457
#define BOX_LARGE_Y1  250
#define BOX_LARGE_X2  489
#define BOX_LARGE_Y2  325

#define DUR_1H_X1  581
#define DUR_1H_Y1  334
#define DUR_1H_X2  621
#define DUR_1H_Y2  429

#define DUR_2H_X1  581
#define DUR_2H_Y1  222
#define DUR_2H_X2  621
#define DUR_2H_Y2  318

#define DUR_4H_X1  641
#define DUR_4H_Y1  334
#define DUR_4H_X2  681
#define DUR_4H_Y2  429

#define DUR_8H_X1  641
#define DUR_8H_Y1  222
#define DUR_8H_X2  681
#define DUR_8H_Y2  318

#define CONFIRM_X1 716
#define CONFIRM_Y1 25
#define CONFIRM_X2 776
#define CONFIRM_Y2 458

#define BACK_X1    34
#define BACK_Y1    425
#define BACK_X2    84
#define BACK_Y2    471

#define PHONE_FIELD_X1  163
#define PHONE_FIELD_Y1  41
#define PHONE_FIELD_X2  212
#define PHONE_FIELD_Y2  433

#define CODE_FIELD_X1   257
#define CODE_FIELD_Y1   74
#define CODE_FIELD_X2   305
#define CODE_FIELD_Y2   414

#define CODE_DIGIT_CANVAS_SIZE  24
#define CODE_DIGIT_STEP_Y  100
#define CODE_DIGIT_X  269
#define CODE_DIGIT_Y0  84
#define CODE_DIGIT_Y1 184
#define CODE_DIGIT_Y2 284
#define CODE_DIGIT_Y3 384

#define LOCKER_CANVAS_SIZE  24
#define LOCKER_LETTER_X  494
#define LOCKER_LETTER_Y  186
#define LOCKER_DIGIT1_X  494
#define LOCKER_DIGIT1_Y  164
#define LOCKER_DIGIT2_X  494
#define LOCKER_DIGIT2_Y  142


#define NOBOX_CANVAS_W  68
#define NOBOX_CANVAS_H  LOCKER_CANVAS_SIZE
#define NOBOX_CANVAS_X  472
#define NOBOX_CANVAS_Y  164

#define MAX_PHONE_DIGITS  11
#define MAX_CODE_DIGITS   4

static ui_context_t *s_ui = NULL;

static lv_obj_t *s_phone_canvas = NULL;
static lv_color_t *s_phone_canvas_buf = NULL;

static lv_obj_t *s_code_canvases[MAX_CODE_DIGITS] = {NULL};
static lv_color_t *s_code_canvas_bufs[MAX_CODE_DIGITS] = {NULL};

static lv_obj_t *s_locker_canvases[3] = {NULL};
static lv_color_t *s_locker_canvas_bufs[3] = {NULL};

static lv_obj_t *s_nobox_canvas = NULL;
static lv_color_t *s_nobox_canvas_buf = NULL;

static int s_phone_digits[MAX_PHONE_DIGITS] = {-1};
static int s_phone_count = 0;
static int s_code_digits[MAX_CODE_DIGITS] = {-1};
static int s_code_auto_generated = 0;

static int s_active_field = 0;
static int s_pending_action = -1;
static lv_timer_t *s_cursor_timer = NULL;
static int s_cursor_visible = 0;
static lv_obj_t *s_key_btns[12] = {NULL};
static lv_obj_t *s_keyboard_overlay = NULL;
static lv_obj_t *s_keyboard_bg = NULL;
static int s_keyboard_visible = 0;

static int s_sel_box = 0;
static int s_sel_duration = 0;
static lv_obj_t *s_dur_btns[4] = {NULL};

static const char *DUR_IMG_PATHS[] = {
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/1hours.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/2hours.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/4hours.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/8hours.sjpg",
};
static const char *DUR_GREEN_PATHS[] = {
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/1hours_green.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/2hours_green.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/4hours_green.sjpg",
    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/8hours_green.sjpg",
};

static void _set_btn_img(lv_obj_t *btn, const char *src)
{
    if (btn == NULL) return;
    lv_obj_t *img = lv_obj_get_child(btn, 0);
    if (img) lv_img_set_src(img, src);
}

#define PHONE_CANVAS_W  361
#define PHONE_CANVAS_H  24

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
        lv_obj_add_event_cb(btn, cb, LV_EVENT_PRESSED, user_data);

    return btn;
}

static lv_obj_t *_create_transparent_btn(lv_coord_t x1, lv_coord_t y1,
                                          lv_coord_t x2, lv_coord_t y2,
                                          lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_pos(btn, x1, y1);
    lv_obj_set_size(btn, x2 - x1, y2 - y1);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    if (cb != NULL)
        lv_obj_add_event_cb(btn, cb, LV_EVENT_PRESSED, user_data);

    return btn;
}

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

static void _redraw_code(void)
{
    for (int i = 0; i < MAX_CODE_DIGITS; i++)
    {
        if (s_code_canvases[i] == NULL || s_code_canvas_bufs[i] == NULL)
            continue;

        char ch[2] = " ";
        if (s_code_auto_generated && s_code_digits[i] >= 0)
            ch[0] = (char)('0' + s_code_digits[i]);

        lv_canvas_fill_bg(s_code_canvases[i], lv_color_hex(0xFFFFFF), LV_OPA_COVER);
        lv_draw_label_dsc_t label_dsc;
        lv_draw_label_dsc_init(&label_dsc);
        label_dsc.font = &lv_font_montserrat_20;
        label_dsc.color = lv_color_black();
        label_dsc.align = LV_TEXT_ALIGN_CENTER;
        lv_canvas_draw_text(s_code_canvases[i], 0, 2, CODE_DIGIT_CANVAS_SIZE, &label_dsc, ch);
        lv_obj_invalidate(s_code_canvases[i]);
    }
}

static void _cursor_timer_cb(lv_timer_t *t)
{
    (void)t;
    s_cursor_visible = !s_cursor_visible;
    if (s_active_field == 1)
        _redraw_phone();
}

/* 创建中途失败时清理已分配的缓冲与已创建的 canvas 对象，
 * 避免 OOM 路径内存泄漏与悬垂指针 */
static void _free_partial(void)
{
    /* 创建中途失败时同时清除背景图样式，防残留 */
    lv_obj_set_style_bg_img_src(lv_scr_act(), NULL, 0);

    if (s_phone_canvas_buf != NULL) { free(s_phone_canvas_buf); s_phone_canvas_buf = NULL; }
    if (s_phone_canvas != NULL) { lv_obj_del(s_phone_canvas); s_phone_canvas = NULL; }

    for (int i = 0; i < MAX_CODE_DIGITS; i++)
    {
        if (s_code_canvas_bufs[i] != NULL) { free(s_code_canvas_bufs[i]); s_code_canvas_bufs[i] = NULL; }
        if (s_code_canvases[i] != NULL) { lv_obj_del(s_code_canvases[i]); s_code_canvases[i] = NULL; }
    }
    for (int i = 0; i < 3; i++)
    {
        if (s_locker_canvas_bufs[i] != NULL) { free(s_locker_canvas_bufs[i]); s_locker_canvas_bufs[i] = NULL; }
        if (s_locker_canvases[i] != NULL) { lv_obj_del(s_locker_canvases[i]); s_locker_canvases[i] = NULL; }
    }

    if (s_nobox_canvas_buf != NULL) { free(s_nobox_canvas_buf); s_nobox_canvas_buf = NULL; }
    if (s_nobox_canvas != NULL) { lv_obj_del(s_nobox_canvas); s_nobox_canvas = NULL; }
}

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
    if (s_active_field != 0) _redraw_phone();
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

static void _generate_code(void)
{
    for (int i = 0; i < MAX_CODE_DIGITS; i++)
        s_code_digits[i] = rand() % 10;
    s_code_auto_generated = 1;
    _redraw_code();
    printf("[存件] 手机号已完整，自动生成随机取件码：");
    for (int i = 0; i < MAX_CODE_DIGITS; i++)
        printf("%d", s_code_digits[i]);
    printf("\n");
}

static void _key_btn_cb(lv_event_t *e)
{
    int key = (int)(intptr_t)lv_event_get_user_data(e);

    if (s_active_field == 0) return;

    if (key >= 0 && key <= 9)
    {
        if (s_active_field == 1 && s_phone_count < MAX_PHONE_DIGITS)
        {
            s_phone_digits[s_phone_count++] = key;
            _redraw_phone();

            if (s_phone_count == MAX_PHONE_DIGITS && !s_code_auto_generated)
            {
                _generate_code();
                printf("[存件] 手机号已完整，自动生成随机取件码\n");
            }
        }
    }
    else if (key == UI_KEY_DELETE)
    {
        if (s_active_field == 1 && s_phone_count > 0)
        {
            s_phone_digits[--s_phone_count] = -1;
            _redraw_phone();

            if (s_phone_count < MAX_PHONE_DIGITS && s_code_auto_generated)
            {
                s_code_auto_generated = 0;
                memset(s_code_digits, -1, sizeof(s_code_digits));
                _redraw_code();
                printf("[存件] 手机号未完整，已清除随机取件码\n");
            }
        }
    }
    else if (key == UI_KEY_CONFIRM)
    {
        _hide_keyboard();
        s_pending_action = UI_KEY_CONFIRM;
    }
}

static void _back_btn_cb(lv_event_t *e)
{
    (void)e;
    s_pending_action = UI_KEY_BACK;
}

static void _confirm_btn_cb(lv_event_t *e)
{
    (void)e;
    _hide_keyboard();
    s_pending_action = UI_KEY_CONFIRM;
}

static void _phone_field_cb(lv_event_t *e)
{
    (void)e;
    s_active_field = 1;
    _show_keyboard();
    if (s_cursor_timer == NULL)
        s_cursor_timer = lv_timer_create(_cursor_timer_cb, 500, NULL);
    s_cursor_visible = 1;
    _redraw_phone();
}

static void _show_locker_digit(lv_obj_t *canvas, lv_color_t *buf, char ch)
{
    if (canvas == NULL || buf == NULL) return;
    lv_canvas_fill_bg(canvas, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.font = &lv_font_montserrat_20;
    dsc.color = lv_color_black();
    dsc.align = LV_TEXT_ALIGN_CENTER;
    char str[2] = {ch, '\0'};
    lv_canvas_draw_text(canvas, 0, 2, LOCKER_CANVAS_SIZE, &dsc, str);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(canvas);
}

static void _show_locker(const char *locker_id)
{
    if (locker_id == NULL || strlen(locker_id) < 3) return;

    if (s_nobox_canvas)
        lv_obj_add_flag(s_nobox_canvas, LV_OBJ_FLAG_HIDDEN);

    _show_locker_digit(s_locker_canvases[0], s_locker_canvas_bufs[0], locker_id[0]);
    _show_locker_digit(s_locker_canvases[1], s_locker_canvas_bufs[1], locker_id[1]);
    _show_locker_digit(s_locker_canvases[2], s_locker_canvas_bufs[2], locker_id[2]);
}

static void _show_no_box(void)
{
    if (s_ui == NULL) return;

    for (int i = 0; i < 3; i++)
    {
        if (s_locker_canvases[i])
            lv_obj_add_flag(s_locker_canvases[i], LV_OBJ_FLAG_HIDDEN);
    }

    if (s_nobox_canvas == NULL || s_nobox_canvas_buf == NULL)
    {
        printf("[存件] 错误：nobox canvas 未初始化\n");
        return;
    }

    lv_canvas_fill_bg(s_nobox_canvas, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.font = &lv_font_montserrat_14;
    dsc.color = lv_color_black();
    dsc.align = LV_TEXT_ALIGN_CENTER;
    lv_canvas_draw_text(s_nobox_canvas, 0, (NOBOX_CANVAS_H - 14) / 2, NOBOX_CANVAS_W, &dsc, "No Box");
    lv_obj_clear_flag(s_nobox_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_nobox_canvas);
}

static const struct { const char *prefix; const char *sel_log; const char *none_log; } BOX_OPTIONS[] = {
    {"A", "小箱(A)", "小柜(A)"},
    {"B", "中箱(B)", "中柜(B)"},
    {"C", "大箱(C)", "大柜(C)"},
};
static const int DUR_VALUES[] = {1, 2, 4, 8};

static void _box_cb(lv_event_t *e)
{
    int sel = (int)(intptr_t)lv_event_get_user_data(e);
    if (sel < 1 || sel > 3) return;
    s_sel_box = sel;
    printf("[存件] 选择%s\n", BOX_OPTIONS[sel - 1].sel_log);
    if (s_ui == NULL) return;
    char locker_id[LOCKER_ID_LEN] = {0};
    /* 监控线程持锁写节点字段，查找须持锁；锁内仅复制所需的柜号 */
    locker_lock();
    locker_node_t *lk = locker_find_first_empty_by_prefix(
                            ui_get_locker_head(), BOX_OPTIONS[sel - 1].prefix);
    if (lk)
        strncpy(locker_id, lk->locker_ID, LOCKER_ID_LEN - 1);
    locker_unlock();
    if (locker_id[0] != '\0')
        _show_locker(locker_id);
    else
    {
        _show_no_box();
        printf("[存件] 警告：没有可用的%s\n", BOX_OPTIONS[sel - 1].none_log);
    }
}

static void _dur_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx > 3) return;
    s_sel_duration = DUR_VALUES[idx];
    for (int i = 0; i < 4; i++)
    {
        if (s_dur_btns[i]) lv_obj_clear_state(s_dur_btns[i], LV_STATE_CHECKED);
        _set_btn_img(s_dur_btns[i], DUR_IMG_PATHS[i]);
    }
    if (s_dur_btns[idx]) lv_obj_add_state(s_dur_btns[idx], LV_STATE_CHECKED);
    _set_btn_img(s_dur_btns[idx], DUR_GREEN_PATHS[idx]);
    printf("[存件] 选择存放时长：%d小时\n", DUR_VALUES[idx]);
}

int lvgl_store_create(ui_context_t *ui, const char *bg_sjpg_path)
{
    if (ui == NULL || bg_sjpg_path == NULL) return -1;

    /* 页面切换时重置输入设备状态，清除上一页残留的按压/坐标 */
    evdev_reset_for_page_switch();

    lv_obj_t *screen = lv_scr_act();

    memset(s_phone_digits, -1, sizeof(s_phone_digits));
    memset(s_code_digits, -1, sizeof(s_code_digits));
    s_phone_count = 0;
    s_code_auto_generated = 0;
    s_active_field = 0;
    s_pending_action = -1;
    s_sel_box = 0;
    s_sel_duration = 0;

    s_ui = ui;

    lv_obj_set_style_bg_img_src(screen, bg_sjpg_path, 0);
    lv_obj_set_style_bg_img_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_TRANSP, 0);

    s_phone_canvas_buf = (lv_color_t *)malloc(PHONE_CANVAS_W * PHONE_CANVAS_H * sizeof(lv_color_t));
    if (s_phone_canvas_buf == NULL) { _free_partial(); return -1; }
    s_phone_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(s_phone_canvas, s_phone_canvas_buf, PHONE_CANVAS_W, PHONE_CANVAS_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_phone_canvas,
                   PHONE_FIELD_X1 + (PHONE_FIELD_X2 - PHONE_FIELD_X1) / 2 - PHONE_CANVAS_W / 2,
                   PHONE_FIELD_Y1 + (PHONE_FIELD_Y2 - PHONE_FIELD_Y1) / 2 - PHONE_CANVAS_H / 2);
    lv_canvas_fill_bg(s_phone_canvas, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    lv_img_set_angle(s_phone_canvas, -900);
    lv_obj_set_style_bg_opa(s_phone_canvas, LV_OPA_TRANSP, 0);

    _redraw_phone();

    {
        static const int y_pos[] = {CODE_DIGIT_Y0, CODE_DIGIT_Y1, CODE_DIGIT_Y2, CODE_DIGIT_Y3};
        for (int i = 0; i < MAX_CODE_DIGITS; i++)
        {
            s_code_canvas_bufs[i] = (lv_color_t *)malloc(CODE_DIGIT_CANVAS_SIZE * CODE_DIGIT_CANVAS_SIZE * sizeof(lv_color_t));
            if (s_code_canvas_bufs[i] == NULL) { _free_partial(); return -1; }
            s_code_canvases[i] = lv_canvas_create(screen);
            lv_canvas_set_buffer(s_code_canvases[i], s_code_canvas_bufs[i],
                                 CODE_DIGIT_CANVAS_SIZE, CODE_DIGIT_CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);
            lv_obj_set_pos(s_code_canvases[i], CODE_DIGIT_X, y_pos[i]);
            lv_canvas_fill_bg(s_code_canvases[i], lv_color_hex(0xFFFFFF), LV_OPA_COVER);
            lv_img_set_angle(s_code_canvases[i], -900);
            lv_obj_set_style_bg_opa(s_code_canvases[i], LV_OPA_TRANSP, 0);
        }
    }
    _redraw_code();

    {
        static const struct { int x, y; } locker_pos[] = {
            {LOCKER_LETTER_X, LOCKER_LETTER_Y},
            {LOCKER_DIGIT1_X, LOCKER_DIGIT1_Y},
            {LOCKER_DIGIT2_X, LOCKER_DIGIT2_Y},
        };
        for (int i = 0; i < 3; i++)
        {
            s_locker_canvas_bufs[i] = (lv_color_t *)malloc(LOCKER_CANVAS_SIZE * LOCKER_CANVAS_SIZE * sizeof(lv_color_t));
            if (s_locker_canvas_bufs[i] == NULL) { _free_partial(); return -1; }
            s_locker_canvases[i] = lv_canvas_create(screen);
            lv_canvas_set_buffer(s_locker_canvases[i], s_locker_canvas_bufs[i],
                                 LOCKER_CANVAS_SIZE, LOCKER_CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);
            lv_obj_set_pos(s_locker_canvases[i], locker_pos[i].x, locker_pos[i].y);
            lv_canvas_fill_bg(s_locker_canvases[i], lv_color_hex(0xFFFFFF), LV_OPA_COVER);
            lv_img_set_angle(s_locker_canvases[i], -900);
            lv_obj_set_style_bg_opa(s_locker_canvases[i], LV_OPA_TRANSP, 0);
            lv_obj_add_flag(s_locker_canvases[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    s_nobox_canvas_buf = (lv_color_t *)malloc(NOBOX_CANVAS_W * NOBOX_CANVAS_H * sizeof(lv_color_t));
    if (s_nobox_canvas_buf == NULL) { _free_partial(); return -1; }
    s_nobox_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(s_nobox_canvas, s_nobox_canvas_buf, NOBOX_CANVAS_W, NOBOX_CANVAS_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_nobox_canvas, NOBOX_CANVAS_X, NOBOX_CANVAS_Y);
    lv_canvas_fill_bg(s_nobox_canvas, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    lv_img_set_angle(s_nobox_canvas, -900);
    lv_obj_set_style_bg_opa(s_nobox_canvas, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(s_nobox_canvas, LV_OBJ_FLAG_HIDDEN);

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

    _create_transparent_btn(PHONE_FIELD_X1 - 5, PHONE_FIELD_Y1,
                            PHONE_FIELD_X2 + 5, PHONE_FIELD_Y2,
                            _phone_field_cb, NULL);

    for (int i = 0; i < 12; i++)
    {
        s_key_btns[i] = _create_img_btn(KEY_POSITIONS[i].x, KEY_POSITIONS[i].y,
                        KEY_ICON_PATHS[i], _key_btn_cb,
                        (void *)(intptr_t)KEY_CODES[i], 0, 0,
                        lv_color_hex(0xFFFFFF), lv_color_hex(0x000000));
        lv_obj_add_flag(s_key_btns[i], LV_OBJ_FLAG_HIDDEN);
    }

    {
        lv_obj_t *btn = _create_img_btn(BACK_X1, BACK_Y1,
                    NULL,
                    _back_btn_cb, NULL, 1, 20,
                    lv_color_hex(0xFFFFFF), lv_color_hex(0xFFFFFF));
        lv_obj_set_size(btn, BACK_X2 - BACK_X1, BACK_Y2 - BACK_Y1);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    }

    _create_img_btn(CONFIRM_X1, CONFIRM_Y1,
                    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/save_confirm.sjpg",
                    _confirm_btn_cb, NULL, 1, 20,
                    lv_color_hex(0xF97B26), lv_color_hex(0xF97B26));

    _create_img_btn(BOX_SMALL_X1, BOX_SMALL_Y1,
                    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/box_choose.sjpg",
                    _box_cb, (void *)(intptr_t)1, 1, 10,
                    lv_color_hex(0xFFFFFF), lv_color_hex(0x036445));
    _create_img_btn(BOX_MEDIUM_X1, BOX_MEDIUM_Y1,
                    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/box_choose.sjpg",
                    _box_cb, (void *)(intptr_t)2, 1, 10,
                    lv_color_hex(0xFFFFFF), lv_color_hex(0x036445));
    _create_img_btn(BOX_LARGE_X1, BOX_LARGE_Y1,
                    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/box_choose.sjpg",
                    _box_cb, (void *)(intptr_t)3, 1, 10,
                    lv_color_hex(0xFFFFFF), lv_color_hex(0x036445));

    {
        static const lv_coord_t dur_x1[] = {DUR_1H_X1, DUR_2H_X1, DUR_4H_X1, DUR_8H_X1};
        static const lv_coord_t dur_y1[] = {DUR_1H_Y1, DUR_2H_Y1, DUR_4H_Y1, DUR_8H_Y1};
        static const lv_coord_t dur_x2[] = {DUR_1H_X2, DUR_2H_X2, DUR_4H_X2, DUR_8H_X2};
        static const lv_coord_t dur_y2[] = {DUR_1H_Y2, DUR_2H_Y2, DUR_4H_Y2, DUR_8H_Y2};
        for (int i = 0; i < 4; i++)
        {
            s_dur_btns[i] = _create_img_btn(dur_x1[i], dur_y1[i],
                            DUR_IMG_PATHS[i],
                            _dur_cb, (void *)(intptr_t)i, 1, 20,
                            lv_color_hex(0xFFFFFF), lv_color_hex(0x036445));
            lv_obj_set_size(s_dur_btns[i], dur_x2[i] - dur_x1[i], dur_y2[i] - dur_y1[i]);
            lv_obj_set_style_border_width(s_dur_btns[i], 1, 0);
            lv_obj_set_style_border_color(s_dur_btns[i], lv_color_black(), 0);
            /* CHECKED 背景色只对时长按钮有意义（仅时长按钮会进入 CHECKED 状态） */
            lv_obj_set_style_bg_color(s_dur_btns[i], lv_color_hex(0x036445), LV_STATE_CHECKED);
        }
    }

    lv_obj_move_foreground(s_keyboard_bg);
    lv_obj_move_foreground(s_keyboard_overlay);
    for (int i = 0; i < 12; i++)
        if (s_key_btns[i]) lv_obj_move_foreground(s_key_btns[i]);
    /* 柜号/无柜提示置于键盘之上，键盘弹出时仍可见 */
    for (int i = 0; i < 3; i++)
        if (s_locker_canvases[i]) lv_obj_move_foreground(s_locker_canvases[i]);
    if (s_nobox_canvas) lv_obj_move_foreground(s_nobox_canvas);

    lv_obj_invalidate(screen);
    lv_timer_handler();
    return 0;
}

void lvgl_store_destroy(void)
{
    s_ui = NULL;

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

    if (s_phone_canvas_buf != NULL)
    {
        free(s_phone_canvas_buf);
        s_phone_canvas_buf = NULL;
    }

    for (int i = 0; i < MAX_CODE_DIGITS; i++)
    {
        s_code_canvases[i] = NULL;
        if (s_code_canvas_bufs[i] != NULL)
        {
            free(s_code_canvas_bufs[i]);
            s_code_canvas_bufs[i] = NULL;
        }
    }

    for (int i = 0; i < 3; i++)
    {
        s_locker_canvases[i] = NULL;
        if (s_locker_canvas_bufs[i] != NULL)
        {
            free(s_locker_canvas_bufs[i]);
            s_locker_canvas_bufs[i] = NULL;
        }
    }

    s_nobox_canvas = NULL;
    if (s_nobox_canvas_buf != NULL)
    {
        free(s_nobox_canvas_buf);
        s_nobox_canvas_buf = NULL;
    }

    s_phone_count = 0;
    s_code_auto_generated = 0;
    s_active_field = 0;
    s_pending_action = -1;
    s_sel_box = 0;
    s_sel_duration = 0;
    memset(s_dur_btns, 0, sizeof(s_dur_btns));
    memset(s_phone_digits, -1, sizeof(s_phone_digits));
    memset(s_code_digits, -1, sizeof(s_code_digits));
}

int lvgl_store_get_action(ui_context_t *ui)
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
            printf("[存件] 检测到扫码取件通知，中断当前操作\n");
            return RET_SCAN_PICKUP;
        }

        lv_timer_handler();
        usleep(20000);
    }

    int action = s_pending_action;
    s_pending_action = -1;
    return action;
}

int lvgl_store_get_phone(char *phone, size_t phone_size)
{
    if (phone == NULL || phone_size < 12) return -1;
    if (s_phone_count != MAX_PHONE_DIGITS) return -1;
    for (int i = 0; i < MAX_PHONE_DIGITS; i++)
        phone[i] = (char)('0' + s_phone_digits[i]);
    phone[MAX_PHONE_DIGITS] = '\0';
    return 0;
}

int lvgl_store_get_code(char *code, size_t code_size)
{
    if (code == NULL || code_size < 5) return -1;
    if (!s_code_auto_generated) return -1;
    for (int i = 0; i < MAX_CODE_DIGITS; i++)
        code[i] = (char)('0' + s_code_digits[i]);
    code[MAX_CODE_DIGITS] = '\0';
    return 0;
}

int lvgl_store_get_box_size(void)
{
    return s_sel_box;
}

int lvgl_store_get_duration(void)
{
    return s_sel_duration;
}