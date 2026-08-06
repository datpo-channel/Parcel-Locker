#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "query_page.h"
#include "ret_codes.h"
#include "config.h"
#include "utils.h"
#include "login_page.h"
#include "keyboard_input.h"
#include "pickup_monitor.h"
#include "lcd_ui_display.h"
#include "locker_info.h"
#include "ui_logic.h"
#include "sms.h"
#include "touchpad.h"

#define MAX_VERIFY_FAIL   3

/**************************************************************************
 *
 *   @brief : 重绘查询界面，恢复已输入的数字显示
 *
 ***************************************************************************/
static void redraw_query(ui_context_t *ui,
                         const int *phone_digits, int phone_count,
                         const int *code_digits, int code_count,
                         int phone_x, int phone_y, int phone_step,
                         int code_x, int code_y, int code_step)
{
    lcd_show_menu(&ui->lcd, "received.jpg");

    for (int i = 0; i < phone_count; i++)
    {
        lcd_show_digit(&ui->lcd, phone_digits[i], phone_x, phone_y - i * phone_step);
    }
    for (int i = 0; i < code_count; i++)
    {
        lcd_show_digit(&ui->lcd, code_digits[i], code_x, code_y - i * code_step);
    }
}

/**************************************************************************
 *
 *   @brief : 快件查询界面
 *          点击输入框弹出键盘，点击确认/键盘外收起键盘
 *
 *   坐标布局：
 *     返回键          (19-67, 430-477)
 *     获取验证码按钮   (313-362, 43-236)
 *     查询按钮        (386-437, 43-438)
 *     手机号输入框    (250-296, 43-438)
 *     验证码输入框    (315-362, 243-438)
 *     键盘显示位置    (493, 0)
 *     手机号显示      (264, 408 - i*28)
 *     验证码显示      (329, 408 - i*45)
 *
 ***************************************************************************/
int show_received_query(ui_context_t *ui)
{
    int ts_x, ts_y;
    int phone_digits[11];
    int phone_count = 0;
    int code_digits[4];
    int code_count = 0;
    int key;
    int active_field = 0;
    int verify_fail_count = 0;
    int pending_click = 0;
    time_t start_time;
    char phone[12] = {0};

    const int phone_x = 264;
    const int phone_y = 408;
    const int phone_step = 28;
    const int code_x = 329;
    const int code_y = 408;
    const int code_step = 45;

    if (ui == NULL)
    {
        return -1;
    }

    memset(phone_digits, 0, sizeof(phone_digits));
    memset(code_digits, 0, sizeof(code_digits));

    lcd_show_menu(&ui->lcd, "received.jpg");
    start_time = time(NULL);

    for (int i = 0; i < 5; i++)
    {
        touchpad_get_coord(&ui->touch, &ts_x, &ts_y);
        usleep(20000);
    }

    while (1)
    {
        if (ui_check_pickup_notify())
        {
            return RET_SCAN_PICKUP;
        }

        if ((time(NULL) - start_time) >= PAGE_TIMEOUT_SEC)
        {
            printf("[超时] 查询界面操作超时(%d秒)，返回主页\n", PAGE_TIMEOUT_SEC);
            return RET_TIMEOUT;
        }

        if (!pending_click)
        {
            if (touchpad_get_coord(&ui->touch, &ts_x, &ts_y) != 0)
            {
                usleep(50000);
                continue;
            }
        }
        pending_click = 0;

        if (ts_x >= 19 && ts_x <= 67 && ts_y >= 430 && ts_y <= 477)
        {
            printf("[查询] 用户点击返回键\n");
            return RET_QUERY_BACK;
        }

        if (ts_x >= 313 && ts_x <= 362 && ts_y >= 43 && ts_y <= 236)
        {
            if (phone_count != 11)
            {
                printf("[查询] 请先输入完整的手机号\n");
                usleep(50000);
                continue;
            }

            for (int i = 0; i < 11; i++)
                phone[i] = '0' + phone_digits[i];
            phone[11] = '\0';

            time_t now = time(NULL);
            if (strcmp(g_last_sms_phone, phone) == 0 &&
                (now - g_last_sms_time) < SMS_COOLDOWN_SEC)
            {
                printf("[短信] 请 %d 秒后再试\n",
                       SMS_COOLDOWN_SEC - (int)(now - g_last_sms_time));
            }
            else
            {
                char sms_code[7];
                generate_random_code(sms_code, 4);

                int ret = send_sms_code(phone, sms_code);
                if (ret == 0)
                {
                    safe_strcpy(g_last_sms_phone, sizeof(g_last_sms_phone), phone);
                    safe_strcpy(g_last_sms_code, sizeof(g_last_sms_code), sms_code);
                    g_last_sms_time = now;
                    printf("[短信] 验证码已发送到 %s: %s\n", phone, sms_code);
                }
                else
                {
                    printf("[短信] 验证码发送失败，错误码: %d\n", ret);
                }
            }
            usleep(100000);
            continue;
        }

        if (ts_x >= 386 && ts_x <= 437 && ts_y >= 43 && ts_y <= 438)
        {
            if (phone_count != 11 || code_count != 4)
            {
                printf("[查询] 请先完整填写手机号和验证码\n");
                usleep(50000);
                continue;
            }

            char code[5];
            for (int i = 0; i < 4; i++)
                code[i] = '0' + code_digits[i];
            code[4] = '\0';

            for (int i = 0; i < 11; i++)
                phone[i] = '0' + phone_digits[i];
            phone[11] = '\0';

            if (!verify_phone_code(phone, code))
            {
                verify_fail_count++;
                printf("[查询] 验证失败（第%d次）\n", verify_fail_count);

                if (verify_fail_count >= MAX_VERIFY_FAIL)
                {
                    printf("[查询] 验证码错误次数过多，返回主页\n");
                    return RET_LOGIN_FAILED;
                }

                memset(code_digits, 0, sizeof(code_digits));
                code_count = 0;
                redraw_query(ui, phone_digits, phone_count,
                             code_digits, code_count,
                             phone_x, phone_y, phone_step,
                             code_x, code_y, code_step);
                usleep(100000);
                continue;
            }

            printf("[查询] 验证成功，查询 %s 的快件\n", phone);

            locker_node_t *head = ui_get_locker_head();
            locker_node_t *node = head;
            locker_node_t *first = NULL;
            int pkg_count = 0;

            while (node != NULL)
            {
                if (node->loc_data == LOCKER_OCCUPIED &&
                    strcmp(node->small_phone, phone) == 0)
                {
                    if (first == NULL)
                        first = node;
                    pkg_count++;
                    printf("[查询] 包裹 #%d: 柜号 %s, 取件码 %s\n",
                           pkg_count, node->locker_ID, node->locker_getID);
                }
                node = node->next;
            }

            if (pkg_count > 0)
            {
                printf("[查询] 手机号 %s 共有 %d 个快件\n", phone, pkg_count);
                lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/had_received.jpg", 488, 43);

                char pickup_code[16];
                strncpy(pickup_code, first->locker_getID, sizeof(pickup_code) - 1);
                pickup_code[sizeof(pickup_code) - 1] = '\0';

                int code_len = strlen(pickup_code);
                for (int i = 0; i < code_len; i++)
                {
                    int digit = pickup_code[i] - '0';
                    if (digit >= 0 && digit <= 9)
                    {
                        char num_path[128];
                        snprintf(num_path, sizeof(num_path),
                                 "resource/num_pic/b_num_%d_g.jpg", digit);
                        lcd_show_jpg(&ui->lcd, num_path, 611, 233 - i * 30);
                        usleep(10000);
                    }
                }
                printf("[查询] 取件码已显示: %s\n", pickup_code);
            }
            else
            {
                lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/not_received.jpg", 488, 43);
                printf("[查询] 手机号 %s 没有快件\n", phone);
            }

            time_t result_start = time(NULL);
            while (1)
            {
                if (time(NULL) - result_start > 30)
                {
                    printf("[查询] 结果页超时，返回主页\n");
                    return RET_TIMEOUT;
                }
                if (touchpad_get_coord(&ui->touch, &ts_x, &ts_y) == 0)
                {
                    if (ts_x >= 19 && ts_x <= 67 && ts_y >= 430 && ts_y <= 477)
                    {
                        break;
                    }
                }
                usleep(50000);
            }

            return RET_QUERY_OK;
        }

        if (ts_x >= 250 && ts_x <= 296 && ts_y >= 43 && ts_y <= 438)
        {
            active_field = 1;
            lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/keyboard.jpg", 493, 0);
        }
        else if (ts_x >= 315 && ts_x <= 362 && ts_y >= 243 && ts_y <= 438)
        {
            if (phone_count != 11)
            {
                printf("[查询] 请先输入手机号\n");
                usleep(50000);
                continue;
            }
            active_field = 2;
            lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/keyboard.jpg", 493, 0);
        }
        else
        {
            usleep(30000);
            continue;
        }

        while (active_field != 0)
        {
            if (ui_check_pickup_notify())
            {
                return RET_SCAN_PICKUP;
            }

            if ((time(NULL) - start_time) >= PAGE_TIMEOUT_SEC)
            {
                printf("[超时] 查询界面操作超时(%d秒)，返回主页\n", PAGE_TIMEOUT_SEC);
                return RET_TIMEOUT;
            }

            if (touchpad_get_coord(&ui->touch, &ts_x, &ts_y) != 0)
            {
                usleep(50000);
                continue;
            }

            if (ts_x < 493)
            {
                int is_func_button = 0;

                if (ts_x >= 19 && ts_x <= 67 && ts_y >= 430 && ts_y <= 477)
                {
                    is_func_button = 1;
                }
                else if (ts_x >= 313 && ts_x <= 362 && ts_y >= 43 && ts_y <= 236)
                {
                    is_func_button = 1;
                }
                else if (ts_x >= 386 && ts_x <= 437 && ts_y >= 43 && ts_y <= 438)
                {
                    is_func_button = 1;
                }

                if (is_func_button)
                {
                    pending_click = 1;
                }
                else
                {
                    printf("[查询] 用户点击键盘外区域，收起键盘\n");
                }

                active_field = 0;
                redraw_query(ui, phone_digits, phone_count,
                             code_digits, code_count,
                             phone_x, phone_y, phone_step,
                             code_x, code_y, code_step);
                break;
            }

            key = get_key_from_touch(ts_x, ts_y);

            if (key == UI_KEY_BACK)
            {
                printf("[查询] 用户点击键盘返回键\n");
                active_field = 0;
                redraw_query(ui, phone_digits, phone_count,
                             code_digits, code_count,
                             phone_x, phone_y, phone_step,
                             code_x, code_y, code_step);
                break;
            }

            if (key == UI_KEY_CONFIRM || key == -1)
            {
                active_field = 0;
                redraw_query(ui, phone_digits, phone_count,
                             code_digits, code_count,
                             phone_x, phone_y, phone_step,
                             code_x, code_y, code_step);
                break;
            }

            if (active_field == 1)
            {
                if (key >= 0 && key <= 9 && phone_count < 11)
                {
                    phone_digits[phone_count] = key;
                    lcd_show_digit(&ui->lcd, key, phone_x,
                                   phone_y - phone_count * phone_step);
                    phone_count++;
                }
                else if (key == UI_KEY_DELETE)
                {
                    if (phone_count > 0)
                    {
                        phone_count--;
                        phone_digits[phone_count] = 0;
                        redraw_query(ui, phone_digits, phone_count,
                                     code_digits, code_count,
                                     phone_x, phone_y, phone_step,
                                     code_x, code_y, code_step);
                        lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/keyboard.jpg", 493, 0);
                    }
                }
            }
            else if (active_field == 2)
            {
                if (key >= 0 && key <= 9 && code_count < 4)
                {
                    code_digits[code_count] = key;
                    lcd_show_digit(&ui->lcd, key, code_x,
                                   code_y - code_count * code_step);
                    code_count++;
                }
                else if (key == UI_KEY_DELETE)
                {
                    if (code_count > 0)
                    {
                        code_count--;
                        code_digits[code_count] = 0;
                        redraw_query(ui, phone_digits, phone_count,
                                     code_digits, code_count,
                                     phone_x, phone_y, phone_step,
                                     code_x, code_y, code_step);
                        lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/keyboard.jpg", 493, 0);
                    }
                }
            }

            usleep(50000);
        }

        usleep(50000);
    }
}