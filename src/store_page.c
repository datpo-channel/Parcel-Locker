#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "store_page.h"
#include "ret_codes.h"
#include "config.h"
#include "utils.h"
#include "keyboard_input.h"
#include "lcd_ui_display.h"
#include "locker_info.h"
#include "ui_logic.h"
#include "touchpad.h"

/**************************************************************************
 *
 *   @brief : 重绘存件信息输入界面，恢复已输入的数字显示
 *   @arg   : ui        指向 ui_context_t 结构体的指针
 *   @arg   : phone     已输入的手机号字符串
 *   @arg   : phone_len 已输入的手机号长度
 *   @arg   : code      已输入的取件码数字数组
 *   @arg   : code_len  已输入的取件码长度
 *   @arg   : step      当前步骤（1=手机号, 2=取件码）
 *
 *   @retval: 无
 *
 ***************************************************************************/
static void redraw_store_info(ui_context_t *ui, const char *phone, int phone_len,
                              const int *code, int code_len, int step)
{
    const int phone_x = 457;
    const int phone_y = 346;
    const int phone_step = 70;

    lcd_show_menu(&ui->lcd, "save_info_set.jpg");

    for (int i = 0; i < phone_len && i < 11; i++)
    {
        lcd_show_digit(&ui->lcd, phone[i] - '0', phone_x, phone_y - i * phone_step);
    }

    if (step >= 2)
    {
        lcd_show_digit(&ui->lcd, code[0], 349, 346);
        lcd_show_digit(&ui->lcd, code[1], 349, 276);
        lcd_show_digit(&ui->lcd, code[2], 349, 206);
        lcd_show_digit(&ui->lcd, code[3], 349, 136);
    }
}

/**************************************************************************
 *
 *   @brief : 存件信息输入界面（手机号 + 取件码）
 *   @arg   : ui       指向 ui_context_t 结构体的指针
 *   @arg   : phone    输出参数，收件人手机号（需至少 12 字节）
 *   @arg   : code     输出参数，用户输入的取件码（需至少 5 字节）
 *   @arg   : box_size 输出参数，柜子大小（当前固定为 1）
 *   @arg   : duration 输出参数，存储时长（当前固定为 1）
 *
 *   @retval: RET_TAKEOUT_OK   输入完成
 *            RET_TAKEOUT_BACK 用户返回
 *            RET_TIMEOUT      页面超时
 *   @note  : 分两步：先输入手机号，再输入 4 位取件码
 *
 ***************************************************************************/
int show_store_info(ui_context_t *ui, char *phone, char *code,
                    int *box_size, int *duration)
{
    int ts_x, ts_y;
    int phone_digits[11];
    int phone_len = 0;
    int code_digits[4];
    int code_len = 0;
    int step = 1;
    int key;
    time_t start_time;

    if (ui == NULL || phone == NULL || code == NULL || box_size == NULL || duration == NULL)
    {
        return -1;
    }

    *box_size = 1;
    *duration = 1;

    memset(phone_digits, 0, sizeof(phone_digits));
    memset(code_digits, 0, sizeof(code_digits));
    lcd_show_menu(&ui->lcd, "save_info_set.jpg");
    start_time = time(NULL);

    while (1)
    {
        if ((time(NULL) - start_time) >= PAGE_TIMEOUT_SEC)
        {
            printf("[超时] 存件信息界面操作超时(%d秒)，返回主页\n", PAGE_TIMEOUT_SEC);
            return RET_TIMEOUT;
        }

        if (touchpad_get_coord(&ui->touch, &ts_x, &ts_y) == 0)
        {
            key = get_key_from_touch(ts_x, ts_y);

            if (key == UI_KEY_BACK)
            {
                return RET_TAKEOUT_BACK;
            }

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
                        redraw_store_info(ui, "", 0, code_digits, 0, step);
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
                        step = 2;
                        printf("[存件] 手机号输入完成，进入验证码输入\n");
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
                        redraw_store_info(ui, "", 0, code_digits, 0, 2);
                        for (int i = 0; i < phone_len; i++)
                        {
                            lcd_show_digit(&ui->lcd, phone_digits[i], 457, 346 - i * 70);
                        }
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
                        for (int i = 0; i < phone_len; i++)
                        {
                            phone[i] = '0' + phone_digits[i];
                        }
                        phone[phone_len] = '\0';
                        for (int i = 0; i < 4; i++)
                        {
                            code[i] = '0' + code_digits[i];
                        }
                        code[4] = '\0';
                        return RET_TAKEOUT_OK;
                    }
                }
            }
        }
        usleep(50000);
    }
}

/**************************************************************************
 *
 *   @brief : 支付验证码输入界面
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *
 *   @retval: 1  支付成功
 *            0  取消支付或超时
 *   @note  : 用户输入 4 位支付验证码，确认后返回支付成功
 *
 ***************************************************************************/
int show_pay_info(ui_context_t *ui)
{
    int ts_x, ts_y;
    int code_digits[4];
    int code_len = 0;
    int key;
    time_t start_time;
    char verify_code[5];

    if (ui == NULL)
    {
        return 0;
    }

    memset(code_digits, 0, sizeof(code_digits));
    lcd_show_menu(&ui->lcd, "save_pay.jpg");
    start_time = time(NULL);

    while (1)
    {
        if ((time(NULL) - start_time) >= PAGE_TIMEOUT_SEC)
        {
            printf("[超时] 支付界面操作超时(%d秒)，返回主页\n", PAGE_TIMEOUT_SEC);
            return 0;
        }

        if (touchpad_get_coord(&ui->touch, &ts_x, &ts_y) == 0)
        {
            if (ts_x >= 17 && ts_x <= 62 && ts_y >= 427 && ts_y <= 472)
            {
                printf("[支付] 用户点击返回，取消支付\n");
                return 0;
            }

            key = get_key_from_touch(ts_x, ts_y);

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
                    lcd_show_menu(&ui->lcd, "save_pay.jpg");
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
                        verify_code[i] = '0' + code_digits[i];
                    }
                    verify_code[4] = '\0';

                    printf("[支付] 验证码已输入: %s\n", verify_code);
                    printf("[支付] 支付成功\n");
                    return 1;
                }
            }
        }
        usleep(50000);
    }
}

/**************************************************************************
 *
 *   @brief : 存件成功页面
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *
 *   @retval: 无
 *   @note  : 显示存件成功画面，用户可选择"继续存件"或"返回首页"
 *
 ***************************************************************************/
void show_send_success(ui_context_t *ui)
{
    int ts_x, ts_y;
    time_t start_time;

    if (ui == NULL)
    {
        return;
    }

    lcd_show_menu(&ui->lcd, "send_success.jpg");
    start_time = time(NULL);

    for (int i = 0; i < 5; i++)
    {
        touchpad_get_coord(&ui->touch, &ts_x, &ts_y);
        usleep(20000);
    }

    while (1)
    {
        if ((time(NULL) - start_time) >= PAGE_TIMEOUT_SEC)
        {
            printf("[超时] 存件成功页面显示超时(%d秒)，自动返回主页\n", PAGE_TIMEOUT_SEC);
            return;
        }

        if (touchpad_get_coord(&ui->touch, &ts_x, &ts_y) == 0)
        {
            if (ts_x >= 663 && ts_x <= 711 && ts_y >= 247 && ts_y <= 441)
            {
                printf("[存件成功] 继续存件\n");
                return;
            }
            if (ts_x >= 664 && ts_x <= 711 && ts_y >= 38 && ts_y <= 230)
            {
                printf("[存件成功] 返回首页\n");
                return;
            }
        }
        usleep(50000);
    }
}