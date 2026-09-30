#ifndef PICKUP_MONITOR_H_
#define PICKUP_MONITOR_H_

#include "ui_context.h"

extern volatile int g_pickup_notify_flag;

/* 跨线程标志访问统一走 GCC 原子内建（单字原子 + 内存序），
 * 消除监控线程写与主线程读写之间的数据竞争 */
#define PICKUP_NOTIFY_SET() __atomic_store_n(&g_pickup_notify_flag, 1, __ATOMIC_RELEASE)
#define PICKUP_NOTIFY_CLR() __atomic_store_n(&g_pickup_notify_flag, 0, __ATOMIC_RELEASE)
#define PICKUP_NOTIFY_GET() __atomic_load_n(&g_pickup_notify_flag, __ATOMIC_ACQUIRE)

/**************************************************************************
 *
 *   @brief : 启动扫码取件后台监控线程
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 后台线程每 3 秒轮询所有有取件令牌的占用柜，
 *            发现网页端验证完成后设置通知标志，由主线程开箱
 *
 ***************************************************************************/
int ui_start_pickup_watcher(void);
void ui_stop_pickup_watcher(void);

/**************************************************************************
 *
 *   @brief : 检查扫码取件状态并执行开箱操作
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *   @arg   : want_continue 输出参数：取件成功页用户选择"继续取件"时为 1
 *
 *   @retval: 1  扫码取件成功，已开箱并显示成功界面
 *            0  未取件或无匹配包裹
 *           -1  清理柜门失败
 *   @note  : 开箱成功后内部调用 show_takeout_success 显示成功界面
 *
 ***************************************************************************/
int check_scan_pickup(ui_context_t *ui, int *want_continue);

#endif