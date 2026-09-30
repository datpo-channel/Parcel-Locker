#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <jpeglib.h>
#include <setjmp.h>
#include "lvgl_takeout_page.h"
#include "config.h"
#include "ret_codes.h"
#include "pickup_monitor.h"
#include "ui_layout.h"
#include "input.h"

/* 取件码显示容器尺寸（旋转 90° 后为竖条，宽度=旋转后高度） */
#define CANVAS_W 310
#define CANVAS_H 20

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

/* 按键坐标表 —— 与 keyboard_input.c 的判定范围严格对齐；
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

/* 查询按钮/返回按钮坐标 (与 keyboard_input.c 对齐，右/下边缘相对屏幕推导) */
#define QUERY_X1  732
#define QUERY_Y1  25
#define QUERY_X2  (UI_SCREEN_W - 21)
#define QUERY_Y2  (UI_SCREEN_H - 22)

#define BACK_X1   17
#define BACK_Y1   427
#define BACK_X2   62
#define BACK_Y2   (UI_SCREEN_H - 8)

/* 显示状态 */
static lv_obj_t *s_code_canvas = NULL;
static lv_color_t *s_canvas_buf = NULL;
static lv_obj_t *s_qr_img = NULL;
static int s_code_digits[4] = {-1, -1, -1, -1};
static int s_code_count = 0;
static int s_pending_action = -1;

/* QR 码像素缓冲区 (用于 libjpeg 解码后传给 LVGL) */
static lv_color_t *s_qr_pixels = NULL;
static lv_img_dsc_t s_qr_dsc = {0};

/* libjpeg 错误处理：默认 error_exit 会直接 exit() 整个程序，
 * 必须接管并 longjmp 回调用点，避免损坏的 JPG 导致程序崩溃 */
struct jpeg_error_buf
{
    struct jpeg_error_mgr pub;
    jmp_buf jb;
};

static void _jpeg_error_exit(j_common_ptr cinfo)
{
    struct jpeg_error_buf *err = (struct jpeg_error_buf *)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(err->jb, 1);
}

/* ============================================================
 * libjpeg 解码 JPG 文件到原始像素 (32bit ARGB8888)
 * ============================================================ */
static int _decode_jpg_to_pixels(const char *path, lv_color_t **out_buf,
                                  int *out_w, int *out_h)
{
    struct jpeg_error_buf jerr;
    struct jpeg_decompress_struct cinfo;
    int w, h, y, comps;
    FILE *fp = NULL;
    /* buf/row_ptrs 在 setjmp 之后才可能被赋值，故声明为 volatile 并置于
     * setjmp 之前初始化为 NULL，longjmp 错误分支才能可靠判断并释放 */
    lv_color_t * volatile buf = NULL;
    JSAMPARRAY volatile row_ptrs = NULL;

    if (path == NULL || out_buf == NULL || out_w == NULL || out_h == NULL)
    {
        return -1;
    }

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = _jpeg_error_exit;

    if (setjmp(jerr.jb))
    {
        /* libjpeg 内部出错（损坏/不支持的 JPG）跳转至此 */
        jpeg_destroy_decompress(&cinfo);
        if (fp != NULL) fclose(fp);
        /* 释放已分配的像素缓冲与行缓冲，避免 longjmp 路径泄漏 */
        if (buf != NULL) free(buf);
        if (row_ptrs != NULL)
        {
            if (row_ptrs[0] != NULL) free(row_ptrs[0]);
            free(row_ptrs);
        }
        fprintf(stderr, "[QR] jpeg decode failed: %s\n", path);
        return -1;
    }

    jpeg_create_decompress(&cinfo);

    fp = fopen(path, "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "[QR] fopen %s failed\n", path);
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }

    jpeg_stdio_src(&cinfo, fp);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK)
    {
        fprintf(stderr, "[QR] jpeg_read_header failed: %s\n", path);
        fclose(fp);
        fp = NULL;
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }

    jpeg_start_decompress(&cinfo);
    w = (int)cinfo.output_width;
    h = (int)cinfo.output_height;
    comps = (int)cinfo.output_components;

    buf = (lv_color_t *)malloc((size_t)w * (size_t)h * sizeof(lv_color_t));
    if (buf == NULL)
    {
        fprintf(stderr, "[QR] malloc %dx%d failed\n", w, h);
        jpeg_destroy_decompress(&cinfo);
        fclose(fp);
        fp = NULL;
        return -1;
    }

    row_ptrs = (JSAMPARRAY)malloc(sizeof(JSAMPROW));
    if (row_ptrs == NULL)
    {
        free(buf);
        jpeg_destroy_decompress(&cinfo);
        fclose(fp);
        fp = NULL;
        return -1;
    }
    row_ptrs[0] = (JSAMPROW)malloc((size_t)w * (size_t)comps);
    if (row_ptrs[0] == NULL)
    {
        free(buf);
        free(row_ptrs);
        jpeg_destroy_decompress(&cinfo);
        fclose(fp);
        fp = NULL;
        return -1;
    }

    y = 0;
    while (y < h)
    {
        jpeg_read_scanlines(&cinfo, row_ptrs, 1);
        for (int x = 0; x < w; x++)
        {
            uint8_t r, g, b;
            if (comps >= 4)
            {
                /* CMYK 图：反相得到 RGB（像素值 = 255 - 分量） */
                r = (uint8_t)(255 - row_ptrs[0][x * 4]);
                g = (uint8_t)(255 - row_ptrs[0][x * 4 + 1]);
                b = (uint8_t)(255 - row_ptrs[0][x * 4 + 2]);
            }
            else if (comps == 3)
            {
                r = row_ptrs[0][x * 3];
                g = row_ptrs[0][x * 3 + 1];
                b = row_ptrs[0][x * 3 + 2];
            }
            else
            {
                /* 灰度图：单通道复制到 RGB */
                r = g = b = row_ptrs[0][x];
            }
            buf[y * w + x] = lv_color_make(r, g, b);
        }
        y++;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(fp);
    free(row_ptrs[0]);
    free(row_ptrs);

    *out_buf = buf;
    *out_w = w;
    *out_h = h;
    return 0;
}

/* ============================================================
 * 按键事件回调
 * ============================================================ */
static void _key_btn_cb(lv_event_t *e)
{
    int key = (int)(intptr_t)lv_event_get_user_data(e);

    if (key >= 0 && key <= 9)
    {
        if (s_code_count < 4)
        {
            s_code_digits[s_code_count++] = key;
        }
    }
    else if (key == UI_KEY_DELETE)
    {
        if (s_code_count > 0)
        {
            s_code_digits[--s_code_count] = -1;
        }
    }
    else if (key == UI_KEY_CONFIRM)
    {
        if (s_code_count == 4)
        {
            s_pending_action = UI_KEY_CONFIRM;
        }
    }

    if (s_code_canvas != NULL && s_canvas_buf != NULL)
    {
        char buf[128] = {0};
        int pos = 0;
        for (int s = 0; s < 8; s++) buf[pos++] = ' ';
        for (int i = 0; i < 4; i++)
        {
            if (s_code_digits[i] >= 0)
                buf[pos++] = (char)('0' + s_code_digits[i]);
            else
                buf[pos++] = ' ';
            if (i < 3) { for (int s = 0; s < 17; s++) buf[pos++] = ' '; }
        }
        lv_canvas_fill_bg(s_code_canvas, lv_color_hex(0xFFFFFF), LV_OPA_TRANSP);
        lv_draw_label_dsc_t label_dsc;
        lv_draw_label_dsc_init(&label_dsc);
        label_dsc.color = lv_color_black();
        label_dsc.align = LV_TEXT_ALIGN_LEFT;
        lv_canvas_draw_text(s_code_canvas, 0, 0, CANVAS_W, &label_dsc, buf);
        lv_obj_invalidate(s_code_canvas);
    }
}

static void _back_btn_cb(lv_event_t *e)
{
    (void)e;
    s_pending_action = UI_KEY_BACK;
}

static void _query_btn_cb(lv_event_t *e)
{
    (void)e;
    s_pending_action = UI_KEY_QUERY;
}

/* ============================================================
 * 创建一个与 lv_example_btn_1 风格一致的按钮
 *   btn_color = 0xFFFFFF, press_color = 0x000000 与原项目一致
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
 * 创建取件码界面
 * ============================================================ */
int lvgl_takeout_create(ui_context_t *ui)
{
    if (ui == NULL) return -1;

    /* 页面切换时重置输入设备状态，清除上一页残留的按压/坐标 */
    evdev_reset_for_page_switch();

    lv_obj_t *screen = lv_scr_act();

    /* 清空状态 */
    memset(s_code_digits, -1, sizeof(s_code_digits));
    s_code_count = 0;
    s_pending_action = -1;

    /* 释放旧 QR 像素缓冲 */
    if (s_qr_pixels != NULL)
    {
        free(s_qr_pixels);
        s_qr_pixels = NULL;
        memset(&s_qr_dsc, 0, sizeof(s_qr_dsc));
    }

    /* ---- 背景 ---- */
    lv_obj_set_style_bg_img_src(screen, "/Workspace/mail_box_pic/resource/sjpeg/menu_pic/qujianback.sjpg", 0);
    lv_obj_set_style_bg_img_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_TRANSP, 0);

    /* ---- 二维码 (初始隐藏) ---- */
    s_qr_img = lv_img_create(screen);
    lv_obj_set_pos(s_qr_img, 188, 188);
    lv_obj_add_flag(s_qr_img, LV_OBJ_FLAG_HIDDEN);

    /* ---- 取件码 4 位显示, canvas 旋转 90° ---- */
    s_canvas_buf = (lv_color_t *)malloc(CANVAS_W * CANVAS_H * sizeof(lv_color_t));
    if (s_canvas_buf == NULL)
    {
        /* 清理已创建的二维码对象，避免悬垂指针 */
        if (s_qr_img != NULL)
        {
            lv_obj_del(s_qr_img);
            s_qr_img = NULL;
        }
        return -1;
    }
    s_code_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(s_code_canvas, s_canvas_buf, CANVAS_W, CANVAS_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_code_canvas, 313, 227);
    lv_canvas_fill_bg(s_code_canvas, lv_color_hex(0xFFFFFF), LV_OPA_TRANSP);
    lv_img_set_angle(s_code_canvas, -900);
    lv_obj_set_style_bg_opa(s_code_canvas, LV_OPA_TRANSP, 0);

    /* ---- 12 个按键 (3x4 键盘) —— 参照原项目 lv_example_btn_1 实现 ---- */
    for (int i = 0; i < 12; i++)
    {
        _create_img_btn(KEY_POSITIONS[i].x, KEY_POSITIONS[i].y,
                        KEY_ICON_PATHS[i], _key_btn_cb,
                        (void *)(intptr_t)KEY_CODES[i], 0, 0,
                        lv_color_hex(0xFFFFFF), lv_color_hex(0x000000));
    }

    /* ---- 查询按钮 (732-779, 25-458) ---- */
    {
        _create_img_btn(QUERY_X1, QUERY_Y1,
                    "/Workspace/mail_box_pic/resource/sjpeg/btn_pic/takeout_search.sjpg",
                    _query_btn_cb, NULL, 1, 20,
                    lv_color_hex(0xFF8023), lv_color_hex(0xFF8023));
    }

    /* ---- 返回按钮 (17-62, 427-472) ---- */
    {
        lv_obj_t *btn = _create_img_btn(BACK_X1, BACK_Y1,
                    NULL,
                    _back_btn_cb, NULL, 1, 20,
                    lv_color_hex(0xFFFFFF), lv_color_hex(0xFFFFFF));
        lv_obj_set_size(btn, BACK_X2 - BACK_X1, BACK_Y2 - BACK_Y1);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    }

    lv_obj_invalidate(screen);
    lv_timer_handler();
    return 0;
}

/* ============================================================
 * 销毁取件码界面
 * ============================================================ */
void lvgl_takeout_destroy(void)
{
    lv_obj_set_style_bg_img_src(lv_scr_act(), NULL, 0);
    lv_obj_clean(lv_scr_act());
    s_qr_img = NULL;
    s_code_canvas = NULL;
    if (s_canvas_buf != NULL)
    {
        free(s_canvas_buf);
        s_canvas_buf = NULL;
    }
    s_code_count = 0;
    memset(s_code_digits, -1, sizeof(s_code_digits));
    s_pending_action = -1;

    if (s_qr_pixels != NULL)
    {
        free(s_qr_pixels);
        s_qr_pixels = NULL;
        memset(&s_qr_dsc, 0, sizeof(s_qr_dsc));
    }
}

/* ============================================================
 * 获取用户操作
 * ============================================================ */
int lvgl_takeout_get_action(ui_context_t *ui)
{
    if (ui == NULL) return -1;

    time_t start_time = time(NULL);

    while (s_pending_action == -1)
    {
        if (PICKUP_NOTIFY_GET())
        {
            /* 与其他页面保持一致：扫码通知返回 RET_SCAN_PICKUP */
            return RET_SCAN_PICKUP;
        }
        int elapsed = (int)(time(NULL) - start_time);
        if (elapsed >= PAGE_TIMEOUT_SEC)
        {
            return -1;
        }

        lv_timer_handler();
        usleep(20000);
    }

    int action = s_pending_action;
    s_pending_action = -1;
    return action;
}

/* ============================================================
 * 更新二维码 (JPG → libjpeg → lv_img_dsc_t)
 * ============================================================ */
int lvgl_takeout_set_qr(const char *qr_path)
{
    int w, h;

    if (qr_path == NULL) return -1;
    if (s_qr_img == NULL) return -1;

    if (s_qr_pixels != NULL)
    {
        free(s_qr_pixels);
        s_qr_pixels = NULL;
    }

    if (_decode_jpg_to_pixels(qr_path, &s_qr_pixels, &w, &h) != 0)
    {
        fprintf(stderr, "[QR] decode failed: %s\n", qr_path);
        /* 失败时清除 dsc 对已释放缓冲的引用，避免 lv_img 重绘 use-after-free */
        memset(&s_qr_dsc, 0, sizeof(s_qr_dsc));
        if (s_qr_img) lv_img_set_src(s_qr_img, NULL);
        return -1;
    }

    memset(&s_qr_dsc, 0, sizeof(s_qr_dsc));
    s_qr_dsc.header.always_zero = 0;
    s_qr_dsc.header.w = (lv_coord_t)w;
    s_qr_dsc.header.h = (lv_coord_t)h;
    s_qr_dsc.data_size = (uint32_t)w * (uint32_t)h * (uint32_t)sizeof(lv_color_t);
    s_qr_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    s_qr_dsc.data = (const uint8_t *)s_qr_pixels;

    lv_img_set_src(s_qr_img, &s_qr_dsc);
    lv_obj_clear_flag(s_qr_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_qr_img);
    lv_timer_handler();
    return 0;
}

/* ============================================================
 * 获取已输入的 4 位取件码 (供 main.c 解析)
 * ============================================================ */
int lvgl_takeout_get_code(char *code, size_t code_size)
{
    if (code == NULL || code_size < 5) return -1;
    if (s_code_count != 4) return -1;
    for (int i = 0; i < 4; i++)
    {
        code[i] = (char)('0' + s_code_digits[i]);
    }
    code[4] = '\0';
    return 0;
}