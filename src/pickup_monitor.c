#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "pickup_monitor.h"
#include "locker_info.h"
#include "verify_gate.h"
#include "ui_logic.h"

static volatile int g_pickup_notify_flag = 0;
static pthread_t g_pickup_tid = 0;
static volatile int g_pickup_running = 0;

/**************************************************************************
 *
 *   @brief : 后台扫码取件监控线程
 *   @arg   : arg 线程参数（当前未使用，留 NULL）
 *
 *   @retval: NULL
 *   @note  : 每 3 秒轮询所有占用柜的取件令牌，
 *            发现网页端验证完成后设置通知标志，供界面主循环检查
 *
 ***************************************************************************/
static void *pickup_monitor_thread(void *arg)
{
    (void)arg;

    while (g_pickup_running)
    {
        sleep(3);

        locker_node_t *head = ui_get_locker_head();
        locker_node_t *node = head;

        while (node != NULL)
        {
            if (node->loc_data == LOCKER_OCCUPIED &&
                node->pickup_token[0] != '\0')
            {
                int st = vg_query_status(node->pickup_token, NULL);
                if (st == VG_STATUS_VERIFIED)
                {
                    printf("[监控] 检测到扫码取件验证通过，柜号:%s\n",
                           node->locker_ID);
                    g_pickup_notify_flag = 1;
                    break;
                }
            }
            node = node->next;
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

/**************************************************************************
 *
 *   @brief : 检查是否有扫码取件通知（非阻塞，读取后自动清除）
 *
 *   @retval: 1  有待处理的取件通知
 *            0  无通知
 *
 ***************************************************************************/
int ui_check_pickup_notify(void)
{
    int flag = g_pickup_notify_flag;
    g_pickup_notify_flag = 0;
    return flag;
}

/**************************************************************************
 *
 *   @brief : 清除扫码取件通知标志
 *
 *   @retval: 无
 *
 ***************************************************************************/
void ui_clear_pickup_notify(void)
{
    g_pickup_notify_flag = 0;
}