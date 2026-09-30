#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include "store_page.h"
#include "ret_codes.h"
#include "config.h"
#include "utils.h"
#include "data_store.h"
#include "pickup_monitor.h"
#include "ui_logic.h"
#include "lvgl_store_page.h"
#include "lvgl_overlay_page.h"

int show_store_info(ui_context_t *ui, char *phone, char *code,
                    int *box_size, int *duration)
{
    if (ui == NULL || phone == NULL || code == NULL ||
        box_size == NULL || duration == NULL)
    {
        return -1;
    }

    int ret = lvgl_store_create(ui, "/Workspace/mail_box_pic/resource/sjpeg/menu_pic/save_info_set.sjpg");
    if (ret != 0)
    {
        printf("[存件] 创建 LVGL 存件界面失败\n");
        return -1;
    }

    while (1)
    {
        if (PICKUP_NOTIFY_GET())
        {
            printf("[存件] 检测到扫码取件\n");
            lvgl_store_destroy();
            return RET_SCAN_PICKUP;
        }

        int action = lvgl_store_get_action(ui);

        if (action == -1)
        {
            printf("[存件] 存件界面操作超时(%d秒)，返回主页\n", PAGE_TIMEOUT_SEC);
            lvgl_store_destroy();
            return RET_TIMEOUT;
        }

        if (action == UI_KEY_BACK)
        {
            printf("[存件] 用户点击返回键\n");
            lvgl_store_destroy();
            return RET_TAKEOUT_BACK;
        }

        if (action == UI_KEY_CONFIRM)
        {
            if (lvgl_store_get_phone(phone, 12) != 0)
            {
                printf("[存件] 请先输入完整的手机号\n");
                continue;
            }
            if (lvgl_store_get_code(code, 5) != 0)
            {
                printf("[存件] 取件码尚未生成，请先输入手机号\n");
                continue;
            }
            int bs = lvgl_store_get_box_size();
            if (bs <= 0)
            {
                printf("[存件] 请先选择箱体大小\n");
                continue;
            }
            int dur = lvgl_store_get_duration();
            if (dur <= 0)
            {
                printf("[存件] 请先选择存放时长\n");
                continue;
            }

            locker_lock();
            locker_node_t *lk = locker_find_first_empty_by_prefix(
                                    ui_get_locker_head(), LOCKER_PREFIXES[bs]);
            locker_unlock();
            if (lk == NULL)
            {
                printf("[存件] 错误：当前选择的柜子类型没有可用柜子\n");
                continue;
            }

            *box_size = bs;
            *duration = dur;
            lvgl_store_destroy();
            return RET_TAKEOUT_OK;
        }
    }
}

static void _send_success_create(void)
{
    lvgl_success_create(0);
}

/**************************************************************************
 *
 *   @brief : 支付界面，用户选择"我已支付"或"取消支付"
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *
 *   @retval: 1  用户点击"我已支付"
 *            0  用户点击"取消支付"或超时
 *   @note  : 显示 pay_info.jpg 背景
 *            我已支付按钮坐标: (581,40)-(636,422)
 *            取消支付按钮坐标: (668,40)-(721,422)
 *
 ***************************************************************************/
int show_pay_info(ui_context_t *ui)
{
    return ui_wait_overlay(ui, lvgl_pay_create, lvgl_pay_destroy, "支付界面");
}

/**************************************************************************
 *
 *   @brief : 存件成功页面
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *
 *   @retval: 1  继续存件
 *            0  返回首页（或超时）
 *   @note  : 显示 send_success.jpg 背景
 *            继续存件按钮坐标: (610,37)-(661,442)
 *            返回首页按钮坐标: (686,38)-(734,440)
 *
 ***************************************************************************/
int show_send_success(ui_context_t *ui)
{
    return ui_wait_overlay(ui, _send_success_create, lvgl_success_destroy, "存件成功页面");
}