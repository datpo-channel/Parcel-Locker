#ifndef LOGIN_PAGE_H_
#define LOGIN_PAGE_H_

#include <time.h>
#include "ui_context.h"

#define SMS_COOLDOWN_SEC    60
#define SMS_CODE_EXPIRE_SEC 300
#define DEMO_PHONE          "13011110000"
#define DEMO_CODE           "1234"

extern char   g_last_sms_phone[12];
extern char g_last_sms_code[7];
extern time_t g_last_sms_time;

void safe_strcpy(char *dst, size_t dst_size, const char *src);

/**************************************************************************
 *
 *   @brief : 显示快递员登录界面，等待用户输入手机号和验证码
 *   @arg   : ui    指向 ui_context_t 结构体的指针
 *   @arg   : phone 输出缓冲区，用于存储输入的手机号
 *
 *   @retval: RET_TAKEOUT_OK    验证成功
 *            RET_TIMEOUT       页面超时
 *            RET_LOGIN_CANCEL  用户取消
 *            RET_LOGIN_FAILED  验证码错误次数超限
 *
 ***************************************************************************/
int show_sendman_login(ui_context_t *ui, char *phone);

/**************************************************************************
 *
 *   @brief : 显示普通用户登录界面，等待用户输入手机号和验证码
 *   @arg   : ui    指向 ui_context_t 结构体的指针
 *   @arg   : phone 输出缓冲区，用于存储输入的手机号
 *
 *   @retval: RET_TAKEOUT_OK    验证成功
 *            RET_TIMEOUT       页面超时
 *            RET_LOGIN_CANCEL  用户取消
 *            RET_LOGIN_FAILED  验证码错误次数超限
 *
 ***************************************************************************/
int show_user_login(ui_context_t *ui, char *phone);

/**************************************************************************
 *
 *   @brief : 验证手机号和验证码（短信验证接口）
 *   @arg   : phone  用户手机号
 *   @arg   : code   用户输入的验证码
 *
 *   @retval: 1  验证成功
 *            0  验证失败
 *   @note  : 验证手机号合法性：11位、以1开头
 *
 ***************************************************************************/
int verify_phone_code(const char *phone, const char *code);

#endif