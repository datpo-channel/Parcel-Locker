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
 *   @note  : 生成随机 UUID 作为取件令牌，令牌默认 30 分钟有效
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
 *           VG_STATUS_INVALID    令牌无效/过期
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

#endif