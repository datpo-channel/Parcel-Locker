#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "login_page.h"
#include "ret_codes.h"
#include "config.h"
#include "utils.h"
#include "keyboard_input.h"
#include "lcd_ui_display.h"
#include "pickup_monitor.h"
#include "sms.h"
#include "touchpad.h"

#define MAX_VERIFY_FAIL   3

char   g_last_sms_phone[12] = {0};
char   g_last_sms_code[7]   = {0};
time_t g_last_sms_time      = 0;

/**************************************************************************
 *
 *   @brief : 安全的字符串拷贝，确保目标缓冲区以 '\0' 结尾
 *   @arg   : dst      目标缓冲区
 *   @arg   : dst_size 目标缓冲区大小
 *   @arg   : src      源字符串
 *
 *   @retval: 无
 *
 ***************************************************************************/
void safe_strcpy(char *dst, size_t dst_size, const char *src)
{
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

/**************************************************************************
 *
 *   @brief : 重绘登录界面，恢复已输入的数字显示
 *   @arg   : ui         指向 ui_context_t 结构体的指针
 *   @arg   : bg_image   背景图片文件名
 *   @arg   : digits     手机号数字数组
 *   @arg   : count      手机号已输入位数
 *   @arg   : code_digits 验证码数字数组
 *   @arg   : code_count  验证码已输入位数
 *   @arg   : phone_x/y/step 手机号显示坐标和步长
 *   @arg   : code_x/y/step  验证码显示坐标和步长
 *
 *   @retval: 无
 *
 ***************************************************************************/
static void redraw_login(ui_context_t *ui, const char *bg_image,
                         const int *digits, int count,
                         const int *code_digits, int code_count,
                         int phone_x, int phone_y, int phone_step,
                         int code_x, int code_y, int code_step)
{
    lcd_show_menu(&ui->lcd, bg_image);

    for (int i = 0; i < count; i++)
    {
        lcd_show_digit(&ui->lcd, digits[i], phone_x, phone_y - i * phone_step);
    }
    for (int i = 0; i < code_count; i++)
    {
        lcd_show_digit(&ui->lcd, code_digits[i], code_x, code_y - i * code_step);
    }
}

/**************************************************************************
 *
 *   @brief : 通用登录界面（快递员/用户共用）
 *          点击输入框弹出键盘，点击确认/键盘外收起键盘
 *   @arg   : ui       指向 ui_context_t 结构体的指针
 *   @arg   : bg_image 背景图片文件名
 *   @arg   : phone    输出参数，存储用户输入的手机号
 *
 *   @retval: RET_TAKEOUT_OK    登录成功
 *            RET_LOGIN_CANCEL  用户返回
 *            RET_TIMEOUT       页面超时
 *            RET_SCAN_PICKUP   后台检测到扫码取件
 *
 *   坐标布局：
 *     返回键          (45-100, 425-475)
 *     获取验证码按钮   (278-328, 56-236)
 *     登录键          (352-403, 55-425)
 *     手机号输入框    (198-245, 54-425)
 *     验证码输入框    (280-328, 244-425)
 *     键盘显示位置    (493, 0)
 *     手机号显示      (215, 403 - i*28)
 *     验证码显示      (296, 403 - i*45)
 *
 ***************************************************************************/
static int show_login_common(ui_context_t *ui, const char *bg_image, char *phone)
{
    int ts_x, ts_y;
    int digits[11];
    int code_digits[4];
    int count = 0;
    int code_count = 0;
    int key;
    int active_field = 0;
    int verify_fail_count = 0;
    int pending_click = 0;
    time_t start_time;

    const int phone_x = 215;
    const int phone_y = 403;
    const int phone_step = 28;
    const int code_x = 296;
    const int code_y = 403;
    const int code_step = 45;

    if (ui == NULL || phone == NULL || bg_image == NULL)
    {
        return RET_LOGIN_FAILED;
    }

    memset(digits, 0, sizeof(digits));
    memset(code_digits, 0, sizeof(code_digits));

    lcd_show_menu(&ui->lcd, bg_image);
    start_time = time(NULL);

    for (int i = 0; i < 5; i++)
    {
        touchpad_get_coord(&ui->touch, &ts_x, &ts_y);
        usleep(20000);
    }

    while (1)
    {
        if (g_pickup_notify_flag)
        {
            return RET_SCAN_PICKUP;
        }

        if ((time(NULL) - start_time) >= PAGE_TIMEOUT_SEC)
        {
            printf("[登录] 登录界面操作超时(%d秒)，返回主页\n", PAGE_TIMEOUT_SEC);
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

        if (ts_x >= 45 && ts_x <= 100 && ts_y >= 425 && ts_y <= 475)
        {
            printf("[登录] 用户点击返回键，取消登录\n");
            return RET_LOGIN_CANCEL;
        }

        if (ts_x >= 278 && ts_x <= 328 && ts_y >= 56 && ts_y <= 236)
        {
            if (count == 11)
            {
                char phone_str[12];
                for (int i = 0; i < 11; i++)
                    phone_str[i] = '0' + digits[i];
                phone_str[11] = '\0';

                time_t now = time(NULL);
                if (strcmp(g_last_sms_phone, phone_str) == 0 &&
                    (now - g_last_sms_time) < SMS_COOLDOWN_SEC)
                {
                    printf("[短信] 请 %d 秒后再试\n",
                           SMS_COOLDOWN_SEC - (int)(now - g_last_sms_time));
                }
                else
                {
                    char sms_code[7];
                    generate_random_code(sms_code, 4);

                    int ret = send_sms_code(phone_str, sms_code);
                    if (ret == 0)
                    {
                        safe_strcpy(g_last_sms_phone, sizeof(g_last_sms_phone), phone_str);
                        safe_strcpy(g_last_sms_code, sizeof(g_last_sms_code), sms_code);
                        g_last_sms_time = now;
                        printf("[短信] 验证码已发送到 %s: %s\n", phone_str, sms_code);
                    }
                    else
                    {
                        printf("[短信] 验证码发送失败，错误码: %d\n", ret);
                    }
                }
            }
            else
            {
                printf("[短信] 请先输入完整的手机号\n");
            }
            usleep(50000);
            continue;
        }

        if (ts_x >= 352 && ts_x <= 403 && ts_y >= 55 && ts_y <= 425)
        {
            if (count == 11 && code_count == 4)
            {
                for (int i = 0; i < 11; i++)
                    phone[i] = '0' + digits[i];
                phone[11] = '\0';

                char code[5];
                for (int i = 0; i < 4; i++)
                    code[i] = '0' + code_digits[i];
                code[4] = '\0';

                if (verify_phone_code(phone, code))
                {
                    printf("[登录] 登录成功: %s\n", phone);
                    return RET_TAKEOUT_OK;
                }

                verify_fail_count++;
                printf("[登录] 验证失败（第%d次）\n", verify_fail_count);

                if (verify_fail_count >= MAX_VERIFY_FAIL)
                {
                    printf("[登录] 验证已连续失败%d次，返回主页\n", verify_fail_count);
                    return RET_LOGIN_FAILED;
                }

                memset(code_digits, 0, sizeof(code_digits));
                code_count = 0;
                redraw_login(ui, bg_image, digits, count,
                             code_digits, code_count,
                             phone_x, phone_y, phone_step,
                             code_x, code_y, code_step);
            }
            else
            {
                printf("[登录] 请先完整填写手机号和验证码\n");
            }
            usleep(50000);
            continue;
        }

        if (ts_x >= 198 && ts_x <= 245 && ts_y >= 54 && ts_y <= 425)
        {
            active_field = 1;
            lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/keyboard.jpg", 493, 0);
        }
        else if (ts_x >= 280 && ts_x <= 328 && ts_y >= 244 && ts_y <= 425)
        {
            active_field = 2;
            lcd_show_jpg(&ui->lcd, "resource/menu_pic/jpg/keyboard.jpg", 493, 0);
        }
        else
        {
            usleep(50000);
            continue;
        }

        while (active_field != 0)
        {
            if (g_pickup_notify_flag)
            {
                return RET_SCAN_PICKUP;
            }

            if ((time(NULL) - start_time) >= PAGE_TIMEOUT_SEC)
            {
                printf("[登录] 登录界面操作超时(%d秒)，返回主页\n", PAGE_TIMEOUT_SEC);
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

                if (ts_x >= 45 && ts_x <= 100 && ts_y >= 425 && ts_y <= 475)
                {
                    is_func_button = 1;
                }
                else if (ts_x >= 278 && ts_x <= 328 && ts_y >= 56 && ts_y <= 236)
                {
                    is_func_button = 1;
                }
                else if (ts_x >= 352 && ts_x <= 403 && ts_y >= 55 && ts_y <= 425)
                {
                    is_func_button = 1;
                }

                if (is_func_button)
                {
                    pending_click = 1;
                }
                else
                {
                    printf("[登录] 用户点击键盘外区域，收起键盘\n");
                }

                active_field = 0;
                redraw_login(ui, bg_image, digits, count,
                             code_digits, code_count,
                             phone_x, phone_y, phone_step,
                             code_x, code_y, code_step);
                break;
            }

            key = get_key_from_touch(ts_x, ts_y);

            if (key == UI_KEY_BACK)
            {
                printf("[登录] 用户点击键盘返回键，取消登录\n");
                return RET_LOGIN_CANCEL;
            }

            if (key == UI_KEY_CONFIRM || key == -1)
            {
                active_field = 0;
                redraw_login(ui, bg_image, digits, count,
                             code_digits, code_count,
                             phone_x, phone_y, phone_step,
                             code_x, code_y, code_step);
                break;
            }

            if (active_field == 1)
            {
                if (key >= 0 && key <= 9 && count < 11)
                {
                    digits[count] = key;
                    lcd_show_digit(&ui->lcd, key, phone_x,
                                   phone_y - count * phone_step);
                    count++;
                }
                else if (key == UI_KEY_DELETE)
                {
                    if (count > 0)
                    {
                        count--;
                        digits[count] = 0;
                        redraw_login(ui, bg_image, digits, count,
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
                        redraw_login(ui, bg_image, digits, count,
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

/**************************************************************************
 *
 *   @brief : 快递员登录界面
 *   @arg   : ui    指向 ui_context_t 结构体的指针
 *   @arg   : phone 输出参数，存储用户输入的手机号
 *
 *   @retval: 同 show_login_common
 *
 ***************************************************************************/
int show_sendman_login(ui_context_t *ui, char *phone)
{
    return show_login_common(ui, "sendman_login.jpg", phone);
}

/**************************************************************************
 *
 *   @brief : 普通用户登录界面
 *   @arg   : ui    指向 ui_context_t 结构体的指针
 *   @arg   : phone 输出参数，存储用户输入的手机号
 *
 *   @retval: 同 show_login_common
 *
 ***************************************************************************/
int show_user_login(ui_context_t *ui, char *phone)
{
    return show_login_common(ui, "saveuser_login.jpg", phone);
}

/**************************************************************************
 *
 *   @brief : 验证手机号和短信验证码是否匹配
 *   @arg   : phone 用户输入的手机号
 *   @arg   : code  用户输入的验证码
 *
 *   @retval: 1  验证通过
 *            0  验证失败或参数无效
 *   @note  : 检查手机号、验证码、发送时间是否与最近一次发送的短信匹配
 *
 ***************************************************************************/
int verify_phone_code(const char *phone, const char *code)
{
    if (phone == NULL || code == NULL)
    {
        return 0;
    }

    time_t now = time(NULL);

    if (strcmp(phone, g_last_sms_phone) == 0 &&
        (now - g_last_sms_time) < SMS_CODE_EXPIRE_SEC &&
        strcmp(code, g_last_sms_code) == 0)
    {
        return 1;
    }

    if (strcmp(phone, DEMO_PHONE) == 0 && strcmp(code, DEMO_CODE) == 0)
    {
        safe_strcpy(g_last_sms_phone, sizeof(g_last_sms_phone), phone);
        safe_strcpy(g_last_sms_code, sizeof(g_last_sms_code), code);
        g_last_sms_time = now;
        return 1;
    }

    return 0;
}