#ifndef VERIFY_GATE_H_
#define VERIFY_GATE_H_

#include <stddef.h>

#define VG_TOKEN_LEN   64
#define VG_URL_LEN     160
#define VG_PHONE_LEN   16

#define VG_STATUS_PENDING   0
#define VG_STATUS_VERIFIED  1
#define VG_STATUS_OPENED    2
#define VG_STATUS_INVALID  -1
#define VG_STATUS_ERROR    -2

typedef struct
{
    int  status;
    char bound_phone[VG_PHONE_LEN];
    char verified_phone[VG_PHONE_LEN];
    long verified_at;
} vg_status_t;

/**************************************************************************
 *
 *   @brief : 创建取件任务（存件时调用）
 *   @arg   : out_token     输出取件令牌（UUID），需至少 VG_TOKEN_LEN 字节
 *   @arg   : out_url       输出二维码对应的网页 URL，需至少 VG_URL_LEN 字节
 *
 *   @retval: 0  成功
 *           -1 参数错误
 *   @note  : 生成随机 UUID 作为取件令牌；有效期由网关端决定
 *
 ***************************************************************************/
int vg_create_pickup(char *out_token, char *out_url);

/**************************************************************************
 *
 *   @brief : 查询取件任务状态（轮询用，不消费）
 *   @arg   : token   取件令牌
 *   @arg   : status  输出状态详情，可为 NULL
 *
 *   @retval: VG_STATUS_PENDING   待验证
 *           VG_STATUS_VERIFIED   已验证，可开箱
 *           VG_STATUS_OPENED     已开箱
 *           VG_STATUS_INVALID    预留（当前实现不返回该值）
 *           VG_STATUS_ERROR      请求错误
 *   @note  : 可重复调用，不影响票据
 *
 ***************************************************************************/
int vg_query_status(const char *token, vg_status_t *status);

/**************************************************************************
 *
 *   @brief : 消费验证票据并开箱（一次性，防重放）
 *   @arg   : token        取件令牌
 *   @arg   : out_phone    输出验证通过的手机号，可为 NULL
 *   @arg   : phone_size   out_phone 缓冲区大小
 *
 *   @retval: 1  消费成功，可开箱
 *           0  未验证或已被消费
 *          -1 参数错误
 *          -2 网络请求失败
 *          -3 响应解析失败
 *   @note  : 全局只能成功一次，第二次调用返回 0
 *
 ***************************************************************************/
int vg_consume_ticket(const char *token, char *out_phone, size_t phone_size);

/**************************************************************************
 *
 *   @brief : 校验手机号是否在云端快递员名单中
 *   @arg   : phone  快递员手机号
 *   @arg   : valid  输出是否在名单中（1=在，0=不在）
 *
 *   @retval: 0  校验成功（valid 有效）
 *           -1 参数错误
 *           -2 网络请求失败
 *           -3 响应解析失败
 *   @note  : 快递员账号存放于云端（/opt/mail-box-api/courier_data.json），
 *            嵌入式端登录时通过 GET /api/courier/check 校验名单
 *
 ***************************************************************************/
int vg_check_courier(const char *phone, int *valid);

/**************************************************************************
 *
 *   @brief : 校验手机号是否在云端用户名单中
 *   @arg   : phone  用户手机号
 *   @arg   : valid  输出是否在名单中（1=在，0=不在）
 *
 *   @retval: 0  校验成功（valid 有效）
 *           -1 参数错误
 *           -2 网络请求失败
 *           -3 响应解析失败
 *   @note  : 用户账号存放于云端（/opt/mail-box-api/user_data.json），
 *            嵌入式端登录时通过 GET /api/user/check 校验名单
 *
 ***************************************************************************/
int vg_check_user(const char *phone, int *valid);

/**************************************************************************
 *
 *   @brief : 异步上报登录记录到云端（后台线程执行，不阻塞调用方）
 *   @arg   : phone     登录手机号
 *   @arg   : role      登录角色（如 "user" / "courier"）
 *   @arg   : timestamp 登录时间戳（unix 秒），0 表示使用当前时间
 *
 *   @retval: 0  已派发异步上报任务
 *           -1 参数错误或线程创建失败
 *   @note  : 创建分离线程 POST /api/login/record（记录存云端
 *            user_data.json 的 records 数组），调用方立即返回
 *
 ***************************************************************************/
int vg_report_login_async(const char *phone, const char *role, long timestamp);

/**************************************************************************
 *
 *   @brief : 更新取件令牌的剩余包裹信息
 *   @arg   : token      取件令牌
 *   @arg   : remaining  剩余包裹JSON数组字符串，如 "[{\"lockerId\":\"A01\",\"code\":\"1234\"}]"
 *
 *   @retval: 0  成功
 *           -1 参数错误
 *           -2 网络请求失败
 *   @note  : 开箱后调用，网页端通过 /api/status 轮询获取剩余包裹
 *
 ***************************************************************************/
int vg_update_remaining(const char *token, const char *remaining_json);

#endif