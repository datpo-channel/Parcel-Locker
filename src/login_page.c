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
 *   @arg   : dst_size 目标缓冲区大小（字节）
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
 *   @brief : 通用登录界面（快递员/用户共用）
 *   @arg   : ui         指向 ui_context_t 结构体的指针
 *   @arg   : login_jpg  登录界面背景图文件名
 *   @arg   : button_jpg 按钮背景图文件名（带键盘布局）
 *   @arg   : phone      输出参数，用户输入的手机号（需至少 12 字节）
 *
 *   @retval: RET_TAKEOUT_OK    验证成功
 *            RET_TIMEOUT       页面超时
 *            RET_LOGIN_CANCEL  用户取消（点击返回）
 *            RET_LOGIN_FAILED  验证码错误次数超限
 *   @note  : 分两步：先输入手机号，再输入短信验证码
 *            非 demo 手机号会发送真实短信，demo 号跳过
 *
 ***************************************************************************/
static int show_login_common(ui_context_t *ui, const char *login_jpg,
                              const char *button_jpg, char *phone)
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
    char input_code[7];

    if (ui == NULL || phone == NULL)
    {
        return RET_LOGIN_FAILED;
    }

    memset(phone_digits, 0, sizeof(phone_digits));
    memset(code_digits, 0, sizeof(code_digits));
    lcd_show_menu(&ui->lcd, login_jpg);
    start_time = time(NULL);

    while (1)
    {
        if ((time(NULL) - start_time) >= PAGE_TIMEOUT_SEC)
        {
            printf("[超时] 登录界面操作超时(%d秒)，返回主页\n", PAGE_TIMEOUT_SEC);
            return RET_TIMEOUT;
        }

        if (touchpad_get_coord(&ui->touch, &ts_x, &ts_y) == 0)
        {
            if (ts_x >= 17 && ts_x <= 62 && ts_y >= 427 && ts_y <= 472)
            {
                return RET_LOGIN_CANCEL;
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
                        lcd_show_menu(&ui->lcd, login_jpg);
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

                        lcd_show_menu(&ui->lcd, button_jpg);
                        step = 2;
                        printf("[登录] 手机号: %s, 进入验证码输入\n", phone);

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
                                printf("[登录] 短信验证码 %s 已发送到 %s\n", sms_code, phone);
                            }
                            else
                            {
                                printf("[登录] 短信发送失败(ret=%d)，使用 demo 验证码\n", sms_ret);
                                safe_strcpy(g_last_sms_phone, sizeof(g_last_sms_phone), phone);
                                safe_strcpy(g_last_sms_code, sizeof(g_last_sms_code), DEMO_CODE);
                                g_last_sms_time = time(NULL);
                            }
                        }
                        else
                        {
                            printf("[登录] Demo 手机号，跳过短信发送\n");
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
                        lcd_show_menu(&ui->lcd, button_jpg);
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
                            printf("[登录] 验证成功\n");
                            return RET_TAKEOUT_OK;
                        }
                        else
                        {
                            fail_count++;
                            printf("[登录] 验证失败(%d/%d)\n", fail_count, MAX_VERIFY_FAIL);
                            if (fail_count >= MAX_VERIFY_FAIL)
                            {
                                return RET_LOGIN_FAILED;
                            }
                            code_len = 0;
                            memset(code_digits, 0, sizeof(code_digits));
                            lcd_show_menu(&ui->lcd, button_jpg);
                        }
                    }
                }
            }
        }
        usleep(50000);
    }
}

/**************************************************************************
 *
 *   @brief : 快递员登录界面
 *   @arg   : ui    指向 ui_context_t 结构体的指针
 *   @arg   : phone 输出参数，登录成功的手机号（需至少 12 字节）
 *
 *   @retval: RET_TAKEOUT_OK    验证成功
 *            RET_TIMEOUT       页面超时
 *            RET_LOGIN_CANCEL  用户取消
 *            RET_LOGIN_FAILED  验证码错误次数超限
 *
 ***************************************************************************/
int show_sendman_login(ui_context_t *ui, char *phone)
{
    return show_login_common(ui, "sendman_login.jpg", "sendman_button.jpg", phone);
}

/**************************************************************************
 *
 *   @brief : 普通用户登录界面
 *   @arg   : ui    指向 ui_context_t 结构体的指针
 *   @arg   : phone 输出参数，登录成功的手机号（需至少 12 字节）
 *
 *   @retval: RET_TAKEOUT_OK    验证成功
 *            RET_TIMEOUT       页面超时
 *            RET_LOGIN_CANCEL  用户取消
 *            RET_LOGIN_FAILED  验证码错误次数超限
 *
 ***************************************************************************/
int show_user_login(ui_context_t *ui, char *phone)
{
    return show_login_common(ui, "user_login.jpg", "user_button.jpg", phone);
}

/**************************************************************************
 *
 *   @brief : 验证手机号和短信验证码是否匹配
 *   @arg   : phone 用户输入的手机号
 *   @arg   : code  用户输入的验证码
 *
 *   @retval: 1  验证通过
 *            0  验证失败
 *   @note  : 先检查最近一次发送的短信验证码（有效期 SMS_CODE_EXPIRE_SEC 秒），
 *            再检查 demo 测试账号（DEMO_PHONE + DEMO_CODE）
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