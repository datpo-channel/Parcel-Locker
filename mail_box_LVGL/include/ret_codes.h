#ifndef RET_CODES_H_
#define RET_CODES_H_

/* 统一返回码：
 * - RET_TAKEOUT_OK/RET_QUERY_OK 数值同为 0，但分属取件/查询两个业务域，
 *   各自表示"该流程成功"，不会在同一调用点混淆；
 * - RET_TAKEOUT_BACK/RET_QUERY_BACK 同理（数值同为 1）；
 * - RET_SCAN_PICKUP 为正数 5，表示"扫码取件验证完成需中断当前页面"，非错误码 */
#define RET_TAKEOUT_OK       0
#define RET_TAKEOUT_BACK     1
#define RET_TAKEOUT_QUERY    2
#define RET_TIMEOUT          -2
#define RET_LOGIN_CANCEL     -3
#define RET_LOGIN_FAILED     -4
#define RET_SCAN_PICKUP      5

#define RET_QUERY_OK         0
#define RET_QUERY_BACK       1

#endif