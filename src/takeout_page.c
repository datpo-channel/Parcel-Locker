#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "takeout_page.h"
#include "ret_codes.h"
#include "config.h"
#include "keyboard_input.h"
#include "pickup_monitor.h"
#include "lcd_ui_display.h"
#include "touchpad.h"

/**************************************************************************
 *
 *   @brief : 重绘取件码输入界面，恢复已输入的数字显示
 *   @arg   : ui     指向 ui_context_t 结构体的指针
 *   @arg   : digits 已输入的数字数组
 *   @arg   : count  已输入的数字个数
 *
 *   @retval: 无
 *
 ***************************************************************************/
static void redraw_takeout_code(ui_context_t *ui, const int *digits, int count)
{
    const int start_x = 457;
    const int start_y = 346;
    const int step_y = 70;

    lcd_show_menu(&ui->lcd, "takeout_code.jpg");

    for (int i = 0; i < count; i++)
    {
        lcd_show_digit(&ui->lcd, digits[i], start_x, start_y - i * step_y);
    }
}

/**************************************************************************
 *
 *   @brief : 取件码输入界面
 *   @arg   : ui   指向 ui_context_t 结构体的指针
 *   @arg   : code 输出参数，用户输入的 4 位取件码（需至少 5 字节）
 *
 *   @retval: RET_TAKEOUT_OK     取件码输入完成
 *            RET_TAKEOUT_BACK   用户返回
 *            RET_TAKEOUT_QUERY  用户点击查询
 *            RET_TIMEOUT        页面超时
 *            RET_SCAN_PICKUP    后台检测到扫码取件
 *
 ***************************************************************************/
int show_takeout_code(ui_context_t *ui, char *code)
{
    int ts_x, ts_y;
    int digits[4];
    int count = 0;
    int key;
    time_t start_time;
    const int start_x = 457;
    const int start_y = 346;
    const int step_y = 70;

    if (ui == NULL || code == NULL)
    {
        return -1;
    }

    memset(digits, 0, sizeof(digits));
    lcd_show_menu(&ui->lcd, "takeout_code.jpg");
    start_time = time(NULL);

    while (1)
    {
        if (ui_check_pickup_notify())
        {
            return RET_SCAN_PICKUP;
        }

        if ((time(NULL) - start_time) >= PAGE_TIMEOUT_SEC)
        {
            printf("[超时] 取件界面操作超时(%d秒)，返回主页\n", PAGE_TIMEOUT_SEC);
            return RET_TIMEOUT;
        }

        if (touchpad_get_coord(&ui->touch, &ts_x, &ts_y) == 0)
        {
            key = get_key_from_touch(ts_x, ts_y);

            if (key == UI_KEY_BACK)
            {
                return RET_TAKEOUT_BACK;
            }
            if (key == UI_KEY_QUERY)
            {
                return RET_TAKEOUT_QUERY;
            }

            if (key >= 0 && key <= 9 && count < 4)
            {
                digits[count] = key;
                lcd_show_digit(&ui->lcd, key, start_x, start_y - count * step_y);
                count++;
            }
            else if (key == UI_KEY_DELETE)
            {
                if (count > 0)
                {
                    count--;
                    digits[count] = 0;
                    redraw_takeout_code(ui, digits, count);
                }
            }
            else if (key == UI_KEY_CONFIRM)
            {
                if (count == 4)
                {
                    for (int i = 0; i < 4; i++)
                    {
                        code[i] = '0' + digits[i];
                    }
                    code[4] = '\0';
                    return RET_TAKEOUT_OK;
                }
            }
        }
        usleep(50000);
    }
}

/**************************************************************************
 *
 *   @brief : 显示取件码（取件成功后展示）
 *   @arg   : ui   指向 ui_context_t 结构体的指针
 *   @arg   : code 4 位取件码字符串
 *
 *   @retval: 无
 *   @note  : 显示取件码后等待用户点击返回按钮退出
 *
 ***************************************************************************/
void show_pickup_code(ui_context_t *ui, const char *code)
{
    int ts_x, ts_y;
    const int start_x = 457;
    const int start_y = 346;
    const int step_y = 70;

    lcd_show_menu(&ui->lcd, "takeout_code.jpg");

    for (int i = 0; i < 4; i++)
    {
        lcd_show_digit(&ui->lcd, code[i] - '0', start_x, start_y - i * step_y);
    }

    for (int i = 0; i < 5; i++)
    {
        touchpad_get_coord(&ui->touch, &ts_x, &ts_y);
        usleep(20000);
    }

    while (1)
    {
        if (touchpad_get_coord(&ui->touch, &ts_x, &ts_y) == 0)
        {
            if (ts_x >= 17 && ts_x <= 62 && ts_y >= 427 && ts_y <= 472)
            {
                break;
            }
        }
        usleep(50000);
    }
}

/**************************************************************************
 *
 *   @brief : 取件成功页面
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *
 *   @retval: 1  继续取件
 *            0  返回首页（或超时）
 *   @note  : 显示取件成功画面，用户可选择"继续取件"或"返回首页"
 *
 ***************************************************************************/
int show_takeout_success(ui_context_t *ui)
{
    int ts_x, ts_y;
    time_t start_time;

    if (ui == NULL)
    {
        return 0;
    }

    lcd_show_menu(&ui->lcd, "takeout_success.jpg");
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
            printf("[超时] 取件成功页面显示超时(%d秒)，自动返回主页\n", PAGE_TIMEOUT_SEC);
            return 0;
        }

        if (touchpad_get_coord(&ui->touch, &ts_x, &ts_y) == 0)
        {
            if (ts_x >= 663 && ts_x <= 711 && ts_y >= 247 && ts_y <= 441)
            {
                printf("[取件成功] 继续取件\n");
                return 1;
            }
            if (ts_x >= 664 && ts_x <= 711 && ts_y >= 38 && ts_y <= 230)
            {
                printf("[取件成功] 返回首页\n");
                return 0;
            }
        }
        usleep(50000);
    }
}