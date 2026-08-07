#ifndef PICKUP_MONITOR_H_
#define PICKUP_MONITOR_H_

extern volatile int g_pickup_notify_flag;

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

#endif