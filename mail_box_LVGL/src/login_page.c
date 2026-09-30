#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "login_page.h"
#include "ret_codes.h"
#include "config.h"
#include "utils.h"
#include "pickup_monitor.h"
#include "sms.h"
#include "verify_gate.h"
#include "lvgl_login_page.h"

#define MAX_VERIFY_FAIL   3

/* 登录角色：决定短信验证后校验哪份云端名单 */
#define LOGIN_ROLE_COURIER  1
#define LOGIN_ROLE_USER     2

char   g_last_sms_phone[12] = {0};
char   g_last_sms_code[7]   = {0};
time_t g_last_sms_time      = 0;

/**************************************************************************
 *
 *   @brief : 通用登录界面（快递员/用户共用）- LVGL 实现
 *          使用 LVGL 按钮键盘替代原有的触摸板输入方式
 *   @arg   : ui       指向 ui_context_t 结构体的指针
 *   @arg   : bg_sjpg_path 背景 sjpg 路径
 *   @arg   : phone    输出参数，存储用户输入的手机号
 *   @arg   : role     LOGIN_ROLE_COURIER=快递员（短信验证后校验云端快递员名单）
 *                    LOGIN_ROLE_USER=普通用户（不做云端实时校验，
 *                    登录成功后异步上报登录记录）
 *
 *   @retval: RET_TAKEOUT_OK    登录成功
 *            RET_LOGIN_CANCEL  用户返回
 *            RET_TIMEOUT       页面超时
 *            RET_SCAN_PICKUP   后台检测到扫码取件
 *
 ***************************************************************************/
static int show_login_common(ui_context_t *ui, const char *bg_sjpg_path,
                             char *phone, int role)
{
    int verify_fail_count = 0;
    int ret;

    if (ui == NULL || phone == NULL || bg_sjpg_path == NULL)
    {
        return RET_LOGIN_FAILED;
    }

    /* 创建 LVGL 登录界面 */
    ret = lvgl_login_create(ui, bg_sjpg_path);
    if (ret != 0)
    {
        printf("[登录] 创建 LVGL 登录界面失败\n");
        return RET_LOGIN_FAILED;
    }

    while (1)
    {
        /* 检查扫码取件通知 */
        if (PICKUP_NOTIFY_GET())
        {
            printf("[登录] 检测到扫码取件，退出登录\n");
            lvgl_login_destroy();
            return RET_SCAN_PICKUP;
        }

        /* 获取用户操作 */
        int action = lvgl_login_get_action(ui);

        /* 超时 */
        if (action == -1)
        {
            printf("[登录] 登录界面操作超时(%d秒)，返回主页\n", PAGE_TIMEOUT_SEC);
            lvgl_login_destroy();
            return RET_TIMEOUT;
        }

        /* 返回键 */
        if (action == UI_KEY_BACK)
        {
            printf("[登录] 用户点击返回键，取消登录\n");
            lvgl_login_destroy();
            return RET_LOGIN_CANCEL;
        }

        /* 获取验证码 */
        if (action == UI_KEY_QUERY)
        {
            char phone_str[12];
            if (lvgl_login_get_phone(phone_str, sizeof(phone_str)) == 0)
            {
                request_sms_code(phone_str);
            }
            else
            {
                printf("[短信] 请先输入完整的手机号\n");
            }
            continue;
        }

        /* 登录确认 */
        if (action == UI_KEY_CONFIRM)
        {
            char phone_str[12];
            char code_str[5];

            int phone_ret = lvgl_login_get_phone(phone_str, sizeof(phone_str));
            int code_ret = lvgl_login_get_code(code_str, sizeof(code_str));

            if (phone_ret != 0 || code_ret != 0)
            {
                printf("[登录] 请先完整填写手机号和验证码\n");
                continue;
            }

            if (verify_phone_code(phone_str, code_str))
            {
                int login_ok = 1;

                /* 快递员登录：短信验证通过后还需校验云端名单，
                 * 账号不在名单中或云端不可达一律拒绝（fail-closed）；
                 * 用户登录不做云端实时校验，仅在成功后异步上报登录记录 */
                if (role == LOGIN_ROLE_COURIER)
                {
                    int valid = 0;
                    int c_ret = vg_check_courier(phone_str, &valid);
                    if (c_ret != 0 || !valid)
                    {
                        printf("[登录] 手机号不在云端快递员名单(ret=%d, valid=%d)\n",
                               c_ret, valid);
                        login_ok = 0;
                    }
                }

                if (login_ok)
                {
                    printf("[登录] 登录成功: %s\n", phone_str);

                    /* 用户登录成功后异步上报登录记录（后台线程，不阻塞） */
                    if (role == LOGIN_ROLE_USER)
                    {
                        vg_report_login_async(phone_str, "user", (long)time(NULL));
                    }

                    strncpy(phone, phone_str, 12);
                    phone[11] = '\0';
                    lvgl_login_destroy();
                    return RET_TAKEOUT_OK;
                }
            }

            verify_fail_count++;
            printf("[登录] 验证失败（第%d次）\n", verify_fail_count);

            if (verify_fail_count >= MAX_VERIFY_FAIL)
            {
                printf("[登录] 验证已连续失败%d次，返回主页\n", verify_fail_count);
                lvgl_login_destroy();
                return RET_LOGIN_FAILED;
            }

            /* 验证失败，清空验证码 */
            lvgl_login_clear_code();
            continue;
        }
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
    return show_login_common(ui, "/Workspace/mail_box_pic/resource/sjpeg/menu_pic/sendman_login.sjpg",
                             phone, LOGIN_ROLE_COURIER);
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
    return show_login_common(ui, "/Workspace/mail_box_pic/resource/sjpeg/menu_pic/saveuser_login.sjpg",
                             phone, LOGIN_ROLE_USER);
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

    /* 时钟回拨防护：now 早于发送时间时直接判定失效，避免回拨后验证码不过期 */
    if (now >= g_last_sms_time &&
        strcmp(phone, g_last_sms_phone) == 0 &&
        (now - g_last_sms_time) < SMS_CODE_EXPIRE_SEC &&
        strcmp(code, g_last_sms_code) == 0)
    {
        return 1;
    }

    /* DEMO 账号只做本地放行，不写入 g_last_sms_* 冷却/验证状态，
     * 避免污染该号码 60 秒内发送短信的冷却逻辑 */
    if (strcmp(phone, DEMO_PHONE) == 0 && strcmp(code, DEMO_CODE) == 0)
    {
        return 1;
    }

    return 0;
}