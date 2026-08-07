#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
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
 *   @brief : 生成随机数字数组
 *   @arg   : digits  输出缓冲区
 *   @arg   : length  数字个数 (1-6)
 *
 *   @retval: 无
 *
 ***************************************************************************/
static void generate_random_code_int(int *digits, int length)
{
    if (digits == NULL || length <= 0 || length > 6)
    {
        return;
    }

    srand((unsigned int)time(NULL) + clock());
    for (int i = 0; i < length; i++)
    {
        digits[i] = rand() % 10;
    }
}

/**************************************************************************
 *
 *   @brief : 重绘存件信息界面，恢复所有已输入/已选择的状态
 *          包括手机号、取件码、箱体选择、存放时长
 *   @arg   : ui                指向 ui_context_t 结构体的指针
 *   @arg   : phone_digits      手机号数字数组
 *   @arg   : phone_count       手机号已输入位数
 *   @arg   : code_digits       取件码数字数组
 *   @arg   : code_auto_generated 取件码是否已自动生成
 *   @arg   : sel_box           选中的箱号（>0表示已选）
 *   @arg   : sel_letter        选中的箱字母
 *   @arg   : sel_d1/sel_d2     选中的箱数字
 *   @arg   : no_available_box  无可用箱标志
 *   @arg   : sel_duration      选中的存放时长（1/2/4/8小时）
 *
 *   @retval: 无
 *
 ***************************************************************************/
static void redraw_store_info(ui_context_t *ui,
                              const int *phone_digits, int phone_count,
                              const int *code_digits, int code_auto_generated,
                              int sel_box, char sel_letter,
                              char sel_d1, char sel_d2,
                              int no_available_box,
                              int sel_duration)
{
    const int code_cx[4] = {273, 273, 273, 273};
    const int code_cy[4] = {390, 293, 187, 86};

    lcd_show_menu(&ui->lcd, "save_info_set.jpg");

    for (int i = 0; i < phone_count; i++)
    {
        int dy = 408 - i * 20;
        lcd_show_digit(&ui->lcd, phone_digits[i], 181, dy);
    }

    if (code_auto_generated)
    {
        for (int i = 0; i < 4; i++)
        {
            lcd_show_digit(&ui->lcd, code_digits[i], code_cx[i], code_cy[i]);
        }
    }

    if (sel_box > 0)
    {
        if (no_available_box)
        {
            lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/havent_box_.jpg", 494, 142);
        }
        else
        {
            lcd_show_letter(&ui->lcd, sel_letter, 494, 186);
            lcd_show_bnum(&ui->lcd, sel_d1 - '0', 494, 164);
            lcd_show_bnum(&ui->lcd, sel_d2 - '0', 494, 142);
        }
    }

    if (sel_duration > 0)
    {
        char dur_path[128];
        int dur_x = 0, dur_y = 0;
        switch (sel_duration)
        {
            case 1: dur_x = 581; dur_y = 333; break;
            case 2: dur_x = 579; dur_y = 222; break;
            case 4: dur_x = 641; dur_y = 333; break;
            case 8: dur_x = 641; dur_y = 223; break;
        }
        snprintf(dur_path, sizeof(dur_path),
                 "resource/menu_pic/jpg/%dhours_chos.jpg", sel_duration);
        lcd_show_jpg(&ui->lcd, dur_path, dur_x, dur_y);
    }
}

/**************************************************************************
 *
 *   @brief : 存件信息输入界面（手机号 + 自动取件码 + 箱体 + 时长）
 *   @arg   : ui       指向 ui_context_t 结构体的指针
 *   @arg   : phone    输出参数，收件人手机号（需至少 12 字节）
 *   @arg   : code     输出参数，自动生成的取件码（需至少 5 字节）
 *   @arg   : box_size 输出参数，柜子大小 (1=小, 2=中, 3=大)
 *   @arg   : duration 输出参数，存储时长 (1/2/4/8 小时)
 *
 *   @retval: RET_TAKEOUT_OK    输入完成
 *            RET_TAKEOUT_BACK  用户返回
 *            RET_TIMEOUT       页面超时
 *            -1                参数错误
 *
 *   坐标布局：
 *     返回键          (34-84, 425-471)
 *     确认键          (716-776, 25-458)
 *     手机号输入框    (163-212, 41-433)
 *     取件码区域      (257-305, 74-414)  自动生成，点击仅提示
 *     小箱            (378-407, 251-325)
 *     中箱            (378-407, 48-124)
 *     大箱            (457-489, 250-325)
 *     时长 1h         (581-621, 334-429)
 *     时长 2h         (581-621, 222-318)
 *     时长 4h         (641-681, 334-429)
 *     时长 8h         (641-681, 222-318)
 *     键盘显示位置    (493, 0)
 *     手机号显示      (181, 408 - i*20)
 *     取件码显示      (273, code_cy[i])
 *
 ***************************************************************************/
int show_store_info(ui_context_t *ui, char *phone, char *code,
                    int *box_size, int *duration)
{
    int ts_x, ts_y;
    int phone_digits[11];
    int phone_count = 0;
    int code_digits[4];
    int sel_box = 0;
    int sel_duration = 0;
    int active_field = 0;
    int key;
    char sel_letter = 0;
    char sel_d1 = 0, sel_d2 = 0;
    int no_available_box = 0;
    int code_auto_generated = 0;
    int pending_click = 0;
    time_t start_time;

    if (ui == NULL || phone == NULL || code == NULL ||
        box_size == NULL || duration == NULL)
    {
        return -1;
    }

    memset(phone_digits, 0, sizeof(phone_digits));
    memset(code_digits, 0, sizeof(code_digits));

    lcd_show_menu(&ui->lcd, "save_info_set.jpg");
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
            printf("[超时] 存物信息界面操作超时(%d秒)，返回主页\n", PAGE_TIMEOUT_SEC);
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

        /* 返回键 */
        if (ts_x >= 34 && ts_x <= 84 && ts_y >= 425 && ts_y <= 471)
        {
            return RET_TAKEOUT_BACK;
        }

        /* 确认键 */
        if (ts_x >= 716 && ts_x <= 776 && ts_y >= 25 && ts_y <= 458)
        {
            if (no_available_box)
            {
                printf("[存件] 错误：当前选择的柜子类型没有可用柜子\n");
                usleep(50000);
                continue;
            }

            if (phone_count == 11 && code_auto_generated &&
                sel_box > 0 && sel_duration > 0)
            {
                for (int i = 0; i < 11; i++)
                    phone[i] = '0' + phone_digits[i];
                phone[11] = '\0';

                for (int i = 0; i < 4; i++)
                    code[i] = '0' + code_digits[i];
                code[4] = '\0';

                *box_size = sel_box;
                *duration = sel_duration;
                return RET_TAKEOUT_OK;
            }
            usleep(50000);
            continue;
        }

        /* 手机号输入区域 */
        if (ts_x >= 163 && ts_x <= 212 && ts_y >= 41 && ts_y <= 433)
        {
            active_field = 1;
            lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/keyboard.jpg", 493, 0);
        }
        /* 取件码区域 - 自动生成，点击仅提示 */
        else if (ts_x >= 257 && ts_x <= 305 && ts_y >= 74 && ts_y <= 414)
        {
            if (!code_auto_generated)
            {
                printf("[存件] 提示：请先输入手机号，系统将自动生成随机取件码\n");
            }
            else
            {
                printf("[存件] 提示：取件码已自动生成：");
                for (int i = 0; i < 4; i++)
                    printf("%d", code_digits[i]);
                printf("\n");
            }
            usleep(50000);
            continue;
        }
        /* 小箱 */
        else if (ts_x >= 378 && ts_x <= 407 && ts_y >= 251 && ts_y <= 325)
        {
            sel_box = 1;
            sel_letter = 'A';
            no_available_box = 0;
            locker_node_t *lk = locker_find_first_empty_by_prefix(
                                    ui_get_locker_head(), "A");
            if (lk)
            {
                sel_d1 = lk->locker_ID[1];
                sel_d2 = lk->locker_ID[2];
                lcd_show_letter(&ui->lcd, 'A', 494, 186);
                lcd_show_bnum(&ui->lcd, lk->locker_ID[1] - '0', 494, 164);
                lcd_show_bnum(&ui->lcd, lk->locker_ID[2] - '0', 494, 142);
            }
            else
            {
                no_available_box = 1;
                lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/havent_box_.jpg", 494, 142);
                printf("[存件] 警告：没有可用的小柜(A)\n");
            }
        }
        /* 中箱 */
        else if (ts_x >= 378 && ts_x <= 407 && ts_y >= 48 && ts_y <= 124)
        {
            sel_box = 2;
            sel_letter = 'B';
            no_available_box = 0;
            locker_node_t *lk = locker_find_first_empty_by_prefix(
                                    ui_get_locker_head(), "B");
            if (lk)
            {
                sel_d1 = lk->locker_ID[1];
                sel_d2 = lk->locker_ID[2];
                lcd_show_letter(&ui->lcd, 'B', 494, 186);
                lcd_show_bnum(&ui->lcd, lk->locker_ID[1] - '0', 494, 164);
                lcd_show_bnum(&ui->lcd, lk->locker_ID[2] - '0', 494, 142);
            }
            else
            {
                no_available_box = 1;
                lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/havent_box_.jpg", 494, 142);
                printf("[存件] 警告：没有可用的中柜(B)\n");
            }
        }
        /* 大箱 */
        else if (ts_x >= 457 && ts_x <= 489 && ts_y >= 250 && ts_y <= 325)
        {
            sel_box = 3;
            sel_letter = 'C';
            no_available_box = 0;
            locker_node_t *lk = locker_find_first_empty_by_prefix(
                                    ui_get_locker_head(), "C");
            if (lk)
            {
                sel_d1 = lk->locker_ID[1];
                sel_d2 = lk->locker_ID[2];
                lcd_show_letter(&ui->lcd, 'C', 494, 186);
                lcd_show_bnum(&ui->lcd, lk->locker_ID[1] - '0', 494, 164);
                lcd_show_bnum(&ui->lcd, lk->locker_ID[2] - '0', 494, 142);
            }
            else
            {
                no_available_box = 1;
                lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/havent_box_.jpg", 494, 142);
                printf("[存件] 警告：没有可用的大柜(C)\n");
            }
        }
        /* 存放时长 1h */
        else if (ts_x >= 581 && ts_x <= 621 && ts_y >= 334 && ts_y <= 429)
        {
            if (sel_duration != 1)
            {
                if (sel_duration == 2)
                {
                    lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/2hours.jpg", 579, 222);
                }
                else if (sel_duration == 4)
                {
                    lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/4hours.jpg", 641, 333);
                }
                else if (sel_duration == 8)
                {
                    lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/8hours.jpg", 641, 223);
                }

                lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/1hours_chos.jpg", 581, 333);
                sel_duration = 1;
                printf("[存件] 选择存放时长：1小时\n");
            }
            usleep(50000);
            continue;
        }
        /* 存放时长 2h */
        else if (ts_x >= 581 && ts_x <= 621 && ts_y >= 222 && ts_y <= 318)
        {
            if (sel_duration != 2)
            {
                if (sel_duration == 1)
                {
                    lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/1hours.jpg", 581, 333);
                }
                else if (sel_duration == 4)
                {
                    lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/4hours.jpg", 641, 333);
                }
                else if (sel_duration == 8)
                {
                    lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/8hours.jpg", 641, 223);
                }

                lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/2hours_chos.jpg", 579, 222);
                sel_duration = 2;
                printf("[存件] 选择存放时长：2小时\n");
            }
            usleep(50000);
            continue;
        }
        /* 存放时长 4h */
        else if (ts_x >= 641 && ts_x <= 681 && ts_y >= 334 && ts_y <= 429)
        {
            if (sel_duration != 4)
            {
                if (sel_duration == 1)
                {
                    lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/1hours.jpg", 581, 333);
                }
                else if (sel_duration == 2)
                {
                    lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/2hours.jpg", 579, 222);
                }
                else if (sel_duration == 8)
                {
                    lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/8hours.jpg", 641, 223);
                }

                lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/4hours_chos.jpg", 641, 333);
                sel_duration = 4;
                printf("[存件] 选择存放时长：4小时\n");
            }
            usleep(50000);
            continue;
        }
        /* 存放时长 8h */
        else if (ts_x >= 641 && ts_x <= 681 && ts_y >= 222 && ts_y <= 318)
        {
            if (sel_duration != 8)
            {
                if (sel_duration == 1)
                {
                    lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/1hours.jpg", 581, 333);
                }
                else if (sel_duration == 2)
                {
                    lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/2hours.jpg", 579, 222);
                }
                else if (sel_duration == 4)
                {
                    lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/4hours.jpg", 641, 333);
                }

                lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/8hours_chos.jpg", 641, 223);
                sel_duration = 8;
                printf("[存件] 选择存放时长：8小时\n");
            }
            usleep(50000);
            continue;
        }
        else
        {
            usleep(50000);
            continue;
        }

        /* 键盘输入循环 */
        while (active_field != 0)
        {
            if ((time(NULL) - start_time) >= PAGE_TIMEOUT_SEC)
            {
                printf("[超时] 存物信息界面操作超时(%d秒)，返回主页\n", PAGE_TIMEOUT_SEC);
                return RET_TIMEOUT;
            }

            if (touchpad_get_coord(&ui->touch, &ts_x, &ts_y) != 0)
            {
                usleep(50000);
                continue;
            }

            key = get_key_from_touch(ts_x, ts_y);

            if (key == UI_KEY_BACK)
            {
                return RET_TAKEOUT_BACK;
            }

            if (key == UI_KEY_CONFIRM || key == -1)
            {
                if (key == -1)
                {
                    int is_func_button = 0;

                    if (ts_x >= 34 && ts_x <= 84 && ts_y >= 425 && ts_y <= 471)
                    {
                        is_func_button = 1;
                    }
                    else if (ts_x >= 716 && ts_x <= 776 && ts_y >= 25 && ts_y <= 458)
                    {
                        is_func_button = 1;
                    }

                    if (is_func_button)
                    {
                        pending_click = 1;
                    }
                    else
                    {
                        printf("[存件] 用户点击键盘外区域，收起键盘\n");
                    }
                }
                active_field = 0;

                if (phone_count == 11 && !code_auto_generated)
                {
                    generate_random_code_int(code_digits, 4);
                    code_auto_generated = 1;

                    printf("[存件] 自动生成随机取件码：");
                    for (int i = 0; i < 4; i++)
                        printf("%d", code_digits[i]);
                    printf("\n");
                }

                redraw_store_info(ui, phone_digits, phone_count,
                                  code_digits, code_auto_generated,
                                  sel_box, sel_letter, sel_d1, sel_d2,
                                  no_available_box, sel_duration);
                break;
            }

            if (key >= 0 && key <= 9)
            {
                if (active_field == 1 && phone_count < 11)
                {
                    phone_digits[phone_count] = key;
                    int dy = 408 - phone_count * 20;
                    lcd_show_digit(&ui->lcd, key, 181, dy);
                    phone_count++;
                }
            }
            else if (key == UI_KEY_DELETE)
            {
                if (active_field == 1 && phone_count > 0)
                {
                    phone_count--;
                    phone_digits[phone_count] = 0;

                    if (phone_count < 11 && code_auto_generated)
                    {
                        code_auto_generated = 0;
                        memset(code_digits, 0, sizeof(code_digits));
                        printf("[存件] 手机号未完整，已清除随机取件码\n");
                    }

                    redraw_store_info(ui, phone_digits, phone_count,
                                      code_digits, code_auto_generated,
                                      sel_box, sel_letter, sel_d1, sel_d2,
                                      no_available_box, sel_duration);
                    lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/keyboard.jpg", 493, 0);
                }
            }
            else if (key == UI_KEY_QUERY)
            {
                /* 忽略查询键 */
            }

            usleep(50000);
        }

        usleep(50000);
    }

    return RET_TAKEOUT_OK;
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
    int ts_x, ts_y;
    time_t start_time;

    if (ui == NULL)
    {
        return 0;
    }

    lcd_show_menu(&ui->lcd, "pay_info.jpg");
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
            printf("[超时] 支付界面操作超时(%d秒)，取消支付返回主页\n", PAGE_TIMEOUT_SEC);
            return 0;
        }

        if (touchpad_get_coord(&ui->touch, &ts_x, &ts_y) == 0)
        {
            if (ts_x >= 581 && ts_x <= 636 && ts_y >= 40 && ts_y <= 422)
            {
                printf("[支付] 用户选择: 我已支付\n");
                return 1;
            }
            if (ts_x >= 668 && ts_x <= 721 && ts_y >= 40 && ts_y <= 422)
            {
                printf("[支付] 用户选择: 取消支付\n");
                return 0;
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
 *   @retval: 1  继续存件
 *            0  返回首页（或超时）
 *   @note  : 显示 send_success.jpg 背景
 *            继续存件按钮坐标: (610,37)-(661,442)
 *            返回首页按钮坐标: (686,38)-(734,440)
 *
 ***************************************************************************/
int show_send_success(ui_context_t *ui)
{
    int ts_x, ts_y;
    time_t start_time;

    if (ui == NULL)
    {
        return 0;
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
            return 0;
        }

        if (touchpad_get_coord(&ui->touch, &ts_x, &ts_y) == 0)
        {
            if (ts_x >= 610 && ts_x <= 661 && ts_y >= 37 && ts_y <= 442)
            {
                printf("[存件成功] 继续存件\n");
                return 1;
            }
            if (ts_x >= 686 && ts_x <= 734 && ts_y >= 38 && ts_y <= 440)
            {
                printf("[存件成功] 返回首页\n");
                return 0;
            }
        }
        usleep(50000);
    }
}