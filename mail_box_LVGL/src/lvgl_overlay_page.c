#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "lvgl_overlay_page.h"
#include "input.h"

static int s_result = 0;
static lv_obj_t *s_bg_img = NULL;
static lv_obj_t *s_btn1 = NULL;
static lv_obj_t *s_btn2 = NULL;

static lv_obj_t *_create_img_btn(lv_coord_t x, lv_coord_t y,
                                  lv_coord_t w, lv_coord_t h,
                                  const char *img_src,
                                  lv_event_cb_t cb, void *user_data,
                                  int radius, lv_color_t bg_color)
{
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_radius(btn, radius, 0);
    lv_obj_set_style_clip_corner(btn, true, 0);
    lv_obj_set_style_bg_color(btn, bg_color, 0);
    lv_obj_set_style_bg_color(btn, bg_color, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

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
        /* 与键盘/功能按钮统一使用 PRESSED 事件（按下即触发，响应更直接） */
        lv_obj_add_event_cb(btn, cb, LV_EVENT_PRESSED, user_data);
    }
    return btn;
}

static void _pay_confirm_cb(lv_event_t *e)
{
    (void)e;
    s_result = 1;
    printf("[支付] 用户选择: 我已支付\n");
}

static void _pay_cancel_cb(lv_event_t *e)
{
    (void)e;
    s_result = 0;
    printf("[支付] 用户选择: 取消支付\n");
}

static void _continue_cb(lv_event_t *e)
{
    (void)e;
    s_result = 1;
    printf("[成功] 继续操作\n");
}

static void _back_home_cb(lv_event_t *e)
{
    (void)e;
    s_result = 0;
    printf("[成功] 返回首页\n");
}

int lvgl_overlay_get_result(void)
{
    return s_result;
}

void lvgl_overlay_reset(void)
{
    s_result = -1;
}

void lvgl_pay_create(void)
{
    /* 页面切换时重置输入设备状态，清除上一页残留的按压/坐标 */
    evdev_reset_for_page_switch();

    s_result = -1;

    s_bg_img = lv_img_create(lv_scr_act());
    lv_img_set_src(s_bg_img, "/Workspace/mail_box_pic/resource/sjpeg/menu_pic/pay_info.sjpg");
    lv_obj_set_pos(s_bg_img, 0, 0);

    s_btn1 = _create_img_btn(581, 40, 55, 382,
                    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/pay_confirm.sjpg",
                    _pay_confirm_cb, NULL, 20,
                    lv_color_hex(0x03722E));

    s_btn2 = _create_img_btn(668, 40, 53, 382,
                    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/pay_cancel.sjpg",
                    _pay_cancel_cb, NULL, 20,
                    lv_color_hex(0xFFFFFF));
    lv_obj_set_style_border_width(s_btn2, 1, 0);
    lv_obj_set_style_border_color(s_btn2, lv_color_black(), 0);
}

static void _overlay_destroy_objs(void)
{
    if (s_btn1) { lv_obj_del(s_btn1); s_btn1 = NULL; }
    if (s_btn2) { lv_obj_del(s_btn2); s_btn2 = NULL; }
    if (s_bg_img) { lv_obj_del(s_bg_img); s_bg_img = NULL; }
}

void lvgl_pay_destroy(void)
{
    _overlay_destroy_objs();
}

void lvgl_success_create(int type)
{
    /* 页面切换时重置输入设备状态，清除上一页残留的按压/坐标 */
    evdev_reset_for_page_switch();

    s_result = -1;

    const char *bg_path;
    lv_coord_t x1, y1, w1, h1;
    lv_coord_t x2, y2, w2, h2;
    const char *img1;
    lv_color_t color1;

    if (type == 0)
    {
        bg_path = "/Workspace/mail_box_pic/resource/sjpeg/menu_pic/send_success.sjpg";
        x1 = 610; y1 = 37; w1 = 51; h1 = 405;
        img1 = "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/save_box_continue.sjpg";
        color1 = lv_color_hex(0x0F8A49);
        x2 = 686; y2 = 38; w2 = 48; h2 = 402;
    }
    else
    {
        bg_path = "/Workspace/mail_box_pic/resource/sjpeg/menu_pic/takeout_success.sjpg";
        x1 = 589; y1 = 36; w1 = 46; h1 = 405;
        img1 = "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/takeout_continue.sjpg";
        color1 = lv_color_hex(0x07AB55);
        x2 = 664; y2 = 37; w2 = 46; h2 = 403;
    }

    s_bg_img = lv_img_create(lv_scr_act());
    lv_img_set_src(s_bg_img, bg_path);
    lv_obj_set_pos(s_bg_img, 0, 0);

    s_btn1 = _create_img_btn(x1, y1, w1, h1, img1,
                    _continue_cb, NULL, 20, color1);

    s_btn2 = _create_img_btn(x2, y2, w2, h2,
                    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/back_main_menu.sjpg",
                    _back_home_cb, NULL, 20,
                    lv_color_hex(0xFFFFFF));
    lv_obj_set_style_border_width(s_btn2, 2, 0);
    lv_obj_set_style_border_color(s_btn2, lv_color_black(), 0);
}

void lvgl_success_destroy(void)
{
    _overlay_destroy_objs();
}