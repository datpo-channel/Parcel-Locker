#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "pickup_monitor.h"
#include "locker_info.h"
#include "verify_gate.h"
#include "ui_logic.h"

extern char g_pickup_token[VG_TOKEN_LEN];

volatile int g_pickup_notify_flag = 0;
static pthread_t g_pickup_tid = 0;
static volatile int g_pickup_running = 0;

#define PICKUP_WATCHER_INTERVAL_SEC 3

/**************************************************************************
 *
 *   @brief : 后台扫码取件监控线程
 *   @arg   : arg 线程参数（当前未使用，留 NULL）
 *
 *   @retval: NULL
 *   @note  : 每 3 秒轮询取件令牌和各柜子的存件令牌，
 *            发现网页端验证完成后仅设置通知标志，由主线程执行开箱
 *
 ***************************************************************************/
static void *pickup_monitor_thread(void *arg)
{
    (void)arg;

    while (g_pickup_running)
    {
        sleep(PICKUP_WATCHER_INTERVAL_SEC);

        if (g_pickup_token[0] != '\0' && !g_pickup_notify_flag)
        {
            int st = vg_query_status(g_pickup_token, NULL);
            if (st == VG_STATUS_VERIFIED || st == VG_STATUS_OPENED)
            {
                printf("[扫码监控] 取件token已验证，通知主线程处理\n");
                g_pickup_notify_flag = 1;
            }
        }

        if (!g_pickup_notify_flag)
        {
            locker_node_t *node = ui_get_locker_head();
            while (node != NULL && !g_pickup_notify_flag)
            {
                if (node->loc_data == LOCKER_OCCUPIED &&
                    node->pickup_token[0] != '\0')
                {
                    int st = vg_query_status(node->pickup_token, NULL);
                    if (st == VG_STATUS_VERIFIED || st == VG_STATUS_OPENED)
                    {
                        printf("[扫码监控] 检测到柜号:%s 状态:%d，通知主线程处理\n",
                               node->locker_ID, st);
                        g_pickup_notify_flag = 1;
                    }
                }
                node = node->next;
            }
        }
    }

    return NULL;
}

/**************************************************************************
 *
 *   @brief : 启动扫码取件后台监控线程
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 后台线程每 3 秒轮询，检测到验证完成后设置通知标志
 *
 ***************************************************************************/
int ui_start_pickup_watcher(void)
{
    g_pickup_running = 1;
    if (pthread_create(&g_pickup_tid, NULL, pickup_monitor_thread, NULL) != 0)
    {
        g_pickup_running = 0;
        return -1;
    }
    return 0;
}