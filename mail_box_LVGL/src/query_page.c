#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "query_page.h"
#include "ret_codes.h"
#include "config.h"
#include "utils.h"
#include "login_page.h"
#include "lvgl_query_page.h"
#include "pickup_monitor.h"
#include "data_store.h"
#include "ui_logic.h"
#include "sms.h"

#define MAX_VERIFY_FAIL   3

/* 删除查询结果页对象并置 NULL，防止悬垂指针在下次查询/退出路径重复删除 */
static void _query_del_result(lv_obj_t **canvas, lv_obj_t **overlay)
{
    if (*canvas != NULL)
    {
        lv_obj_del(*canvas);
        *canvas = NULL;
    }
    if (*overlay != NULL)
    {
        lv_obj_del(*overlay);
        *overlay = NULL;
    }
}

int show_received_query(ui_context_t *ui)
{
    int verify_fail_count = 0;
    int ret;
    lv_obj_t *result_overlay = NULL;
    lv_obj_t *result_canvas = NULL;

    if (ui == NULL)
    {
        return -1;
    }

    ret = lvgl_query_create(ui, "/Workspace/mail_box_pic/resource/sjpeg/menu_pic/received.sjpg");
    if (ret != 0)
    {
        printf("[查询] 创建 LVGL 查询界面失败\n");
        return -1;
    }

    while (1)
    {
        if (PICKUP_NOTIFY_GET())
        {
            printf("[查询] 检测到扫码取件\n");
            _query_del_result(&result_canvas, &result_overlay);
            lvgl_query_destroy();
            return RET_SCAN_PICKUP;
        }

        int action = lvgl_query_get_action(ui);

        if (action == -1)
        {
            printf("[查询] 查询界面操作超时(%d秒)，返回主页\n", PAGE_TIMEOUT_SEC);
            _query_del_result(&result_canvas, &result_overlay);
            lvgl_query_destroy();
            return RET_TIMEOUT;
        }

        if (action == UI_KEY_BACK)
        {
            printf("[查询] 用户点击返回键\n");
            _query_del_result(&result_canvas, &result_overlay);
            lvgl_query_destroy();
            return RET_QUERY_BACK;
        }

        if (action == UI_KEY_QUERY)
        {
            char phone_str[12];
            if (lvgl_query_get_phone(phone_str, sizeof(phone_str)) == 0)
            {
                request_sms_code(phone_str);
            }
            else
            {
                printf("[短信] 请先输入完整的手机号\n");
            }
            continue;
        }

        if (action == UI_KEY_CONFIRM)
        {
            char phone_str[12];
            char code_str[5];

            int phone_ret = lvgl_query_get_phone(phone_str, sizeof(phone_str));
            int code_ret = lvgl_query_get_code(code_str, sizeof(code_str));

            if (phone_ret != 0 || code_ret != 0)
            {
                printf("[查询] 请先完整填写手机号和验证码\n");
                continue;
            }

            if (!verify_phone_code(phone_str, code_str))
            {
                verify_fail_count++;
                printf("[查询] 验证失败（第%d次）\n", verify_fail_count);

                if (verify_fail_count >= MAX_VERIFY_FAIL)
                {
                    printf("[查询] 验证码错误次数过多，返回主页\n");
                    _query_del_result(&result_canvas, &result_overlay);
                    lvgl_query_destroy();
                    return RET_LOGIN_FAILED;
                }

                lvgl_query_clear_code();
                continue;
            }

            printf("[查询] 验证成功，查询 %s 的快件\n", phone_str);

            locker_node_t *head = ui_get_locker_head();
            locker_node_t *node = head;
            locker_node_t *first = NULL;
            char first_code[LOCKER_CODE_LEN] = {0};
            int pkg_count = 0;

            /* 监控线程持锁写节点字段，遍历读取须持锁；锁内仅复制所需的取件码 */
            locker_lock();
            while (node != NULL)
            {
                if (node->loc_data == LOCKER_OCCUPIED &&
                    strcmp(node->small_phone, phone_str) == 0)
                {
                    if (first == NULL)
                    {
                        first = node;
                        strncpy(first_code, node->locker_getID, LOCKER_CODE_LEN - 1);
                    }
                    pkg_count++;
                    printf("[查询] 包裹 #%d: 柜号 %s, 取件码 %s\n",
                           pkg_count, node->locker_ID, node->locker_getID);
                }
                node = node->next;
            }
            locker_unlock();

            /* 二次查询时先释放旧结果对象，再重建 */
            _query_del_result(&result_canvas, &result_overlay);

            result_overlay = lv_img_create(lv_scr_act());
            lv_obj_set_pos(result_overlay, 488, 43);

            if (pkg_count > 0)
            {
                printf("[查询] 手机号 %s 共有 %d 个快件\n", phone_str, pkg_count);
                lv_img_set_src(result_overlay, "/Workspace/mail_box_pic/resource/sjpeg/menu_pic/had_received.sjpg");

                char pickup_code[LOCKER_CODE_LEN];
                strncpy(pickup_code, first_code, LOCKER_CODE_LEN - 1);
                pickup_code[LOCKER_CODE_LEN - 1] = '\0';

                int code_len = strlen(pickup_code);
                /* 防御异常数据：取件码最多 LOCKER_CODE_LEN-1 个字符 */
                if (code_len > LOCKER_CODE_LEN - 1) code_len = LOCKER_CODE_LEN - 1;

                /* 相邻字符间插入两个空格，提升取件码可读性（缓冲按最大长度计算，防溢出） */
                char formatted[3 * (LOCKER_CODE_LEN - 1) + 1];
                int j = 0;
                for (int i = 0; i < code_len; i++)
                {
                    formatted[j++] = pickup_code[i];
                    if (i < code_len - 1)
                    {
                        formatted[j++] = ' ';
                        formatted[j++] = ' ';
                    }
                }
                formatted[j] = '\0';

                /* 取件码显示区：绿底黑字、旋转 -900 竖排，视觉与手机号输入字段一致；
                 * 位置尺寸对齐原项目四位数字组合区域 (x=611~633, y=143~251) */
                static lv_color_t code_cbuf[108 * 24];
                result_canvas = lv_canvas_create(lv_scr_act());
                lv_canvas_set_buffer(result_canvas, code_cbuf, 108, 24, LV_IMG_CF_TRUE_COLOR);
                lv_canvas_fill_bg(result_canvas, lv_color_hex(0xD8FFDE), LV_OPA_COVER);
                lv_obj_set_pos(result_canvas, 569, 185);
                lv_img_set_angle(result_canvas, -900);
                lv_obj_set_style_bg_opa(result_canvas, LV_OPA_TRANSP, 0);

                lv_draw_label_dsc_t dsc;
                lv_draw_label_dsc_init(&dsc);
                dsc.color = lv_color_black();
                dsc.font = &lv_font_montserrat_20;
                dsc.align = LV_TEXT_ALIGN_CENTER;
                lv_canvas_draw_text(result_canvas, 0, (24 - 20) / 2, 108, &dsc, formatted);

                printf("[查询] 取件码已显示: %s\n", pickup_code);
            }
            else
            {
                printf("[查询] 手机号 %s 没有快件\n", phone_str);
                lv_img_set_src(result_overlay, "/Workspace/mail_box_pic/resource/sjpeg/menu_pic/not_received.sjpg");
            }

            if (result_overlay) lv_obj_move_foreground(result_overlay);
            if (result_canvas) lv_obj_move_foreground(result_canvas);

            /* 结果页等待：期间 LVGL evdev 独占输入；用户可再次点击输入框调出键盘、
             * 点击查询重新查询、点击返回退出 */
            time_t result_start = time(NULL);
            while (1)
            {
                if (PICKUP_NOTIFY_GET())
                {
                    printf("[查询] 结果页检测到扫码取件\n");
                    _query_del_result(&result_canvas, &result_overlay);
                    lvgl_query_destroy();
                    return RET_SCAN_PICKUP;
                }

                if (time(NULL) - result_start > 30)
                {
                    printf("[查询] 结果页超时，返回主页\n");
                    _query_del_result(&result_canvas, &result_overlay);
                    lvgl_query_destroy();
                    return RET_TIMEOUT;
                }

                lv_timer_handler();

                /* 用户再次操作（查询/返回/获取验证码等），回到主循环处理 */
                if (lvgl_query_has_action() != -1)
                {
                    break;
                }
                usleep(20000);
            }
        }
    }
}