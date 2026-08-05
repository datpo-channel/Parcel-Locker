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
 *   @brief : 快件查询界面
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *
 *   @retval: RET_QUERY_OK     查询完成
 *            RET_QUERY_BACK   用户返回
 *            RET_TIMEOUT      页面超时
 *            RET_SCAN_PICKUP  后台检测到扫码取件
 *            RET_LOGIN_FAILED 验证码错误次数超限
 *   @note  : 分两步：输入手机号 → 短信验证 → 显示快件状态
 *            查询结果页需点击返回按钮退出
 *
 ***************************************************************************/
int show_received_query(ui_context_t *ui)
{
    int ts_x, ts_y;
    int phone_digits[11];
    int phone_len = 0;
    int code_digits[4];
    int code_len = 0;
    int step = 1;
    int key;
    int fail_count = 0;
    time_t start_time;
    char phone[12];
    char input_code[7];

    if (ui == NULL)
    {
        return -1;
    }

    memset(phone_digits, 0, sizeof(phone_digits));
    memset(code_digits, 0, sizeof(code_digits));
    lcd_show_menu(&ui->lcd, "received_query.jpg");
    start_time = time(NULL);

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

        if (touchpad_get_coord(&ui->touch, &ts_x, &ts_y) == 0)
        {
            if (ts_x >= 17 && ts_x <= 62 && ts_y >= 427 && ts_y <= 472)
            {
                return RET_QUERY_BACK;
            }

            key = get_key_from_touch(ts_x, ts_y);

            if (step == 1)
            {
                if (key >= 0 && key <= 9 && phone_len < 11)
                {
                    phone_digits[phone_len] = key;
                    lcd_show_digit(&ui->lcd, key, 457, 346 - phone_len * 70);
                    phone_len++;
                }
                else if (key == UI_KEY_DELETE)
                {
                    if (phone_len > 0)
                    {
                        phone_len--;
                        phone_digits[phone_len] = 0;
                        lcd_show_menu(&ui->lcd, "received_query.jpg");
                        for (int i = 0; i < phone_len; i++)
                        {
                            lcd_show_digit(&ui->lcd, phone_digits[i], 457, 346 - i * 70);
                        }
                    }
                }
                else if (key == UI_KEY_CONFIRM)
                {
                    if (phone_len == 11)
                    {
                        for (int i = 0; i < 11; i++)
                        {
                            phone[i] = '0' + phone_digits[i];
                        }
                        phone[11] = '\0';

                        lcd_show_menu(&ui->lcd, "received_query_button.jpg");
                        step = 2;
                        printf("[查询] 手机号: %s, 进入验证码输入\n", phone);

                        if (strcmp(phone, DEMO_PHONE) != 0)
                        {
                            char sms_code[7];
                            generate_random_code(sms_code, 4);
                            int sms_ret = send_sms_code(phone, sms_code);
                            if (sms_ret == 0)
                            {
                                safe_strcpy(g_last_sms_phone, sizeof(g_last_sms_phone), phone);
                                safe_strcpy(g_last_sms_code, sizeof(g_last_sms_code), sms_code);
                                g_last_sms_time = time(NULL);
                                printf("[查询] 短信验证码 %s 已发送到 %s\n", sms_code, phone);
                            }
                            else
                            {
                                printf("[查询] 短信发送失败(ret=%d)，请稍后重试\n", sms_ret);
                                return RET_LOGIN_FAILED;
                            }
                        }
                        else
                        {
                            printf("[查询] Demo 手机号，跳过短信发送\n");
                            safe_strcpy(g_last_sms_phone, sizeof(g_last_sms_phone), phone);
                            safe_strcpy(g_last_sms_code, sizeof(g_last_sms_code), DEMO_CODE);
                            g_last_sms_time = time(NULL);
                        }
                    }
                }
            }
            else if (step == 2)
            {
                if (key >= 0 && key <= 9 && code_len < 4)
                {
                    code_digits[code_len] = key;
                    lcd_show_digit(&ui->lcd, key, 349, 346 - code_len * 70);
                    code_len++;
                }
                else if (key == UI_KEY_DELETE)
                {
                    if (code_len > 0)
                    {
                        code_len--;
                        code_digits[code_len] = 0;
                        lcd_show_menu(&ui->lcd, "received_query_button.jpg");
                        for (int i = 0; i < code_len; i++)
                        {
                            lcd_show_digit(&ui->lcd, code_digits[i], 349, 346 - i * 70);
                        }
                    }
                }
                else if (key == UI_KEY_CONFIRM)
                {
                    if (code_len == 4)
                    {
                        for (int i = 0; i < 4; i++)
                        {
                            input_code[i] = '0' + code_digits[i];
                        }
                        input_code[4] = '\0';

                        if (verify_phone_code(phone, input_code))
                        {
                            printf("[查询] 验证成功，查询 %s 的快件\n", phone);
                            lcd_show_menu(&ui->lcd, "received_query.jpg");

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
                                lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/takeout_code.jpg", 0, 0);
                                lcd_show_digit(&ui->lcd, first->locker_getID[0] - '0', 457, 346);
                                lcd_show_digit(&ui->lcd, first->locker_getID[1] - '0', 457, 276);
                                lcd_show_digit(&ui->lcd, first->locker_getID[2] - '0', 457, 206);
                                lcd_show_digit(&ui->lcd, first->locker_getID[3] - '0', 457, 136);
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
                                    if (ts_x >= 17 && ts_x <= 62 && ts_y >= 427 && ts_y <= 472)
                                    {
                                        break;
                                    }
                                }
                                usleep(50000);
                            }

                            return RET_QUERY_OK;
                        }
                        else
                        {
                            fail_count++;
                            printf("[查询] 验证失败(%d/%d)\n", fail_count, MAX_VERIFY_FAIL);
                            if (fail_count >= MAX_VERIFY_FAIL)
                            {
                                printf("[查询] 验证码错误次数过多，返回主页\n");
                                return RET_LOGIN_FAILED;
                            }
                            code_len = 0;
                            memset(code_digits, 0, sizeof(code_digits));
                            lcd_show_menu(&ui->lcd, "received_query_button.jpg");
                        }
                    }
                }
            }
        }
        usleep(50000);
    }
}