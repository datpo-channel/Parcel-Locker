#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "pickup_monitor.h"
#include "data_store.h"
#include "verify_gate.h"
#include "ui_logic.h"
#include "takeout_page.h"

extern char g_pickup_token[VG_TOKEN_LEN];

volatile int g_pickup_notify_flag = 0;
static pthread_t g_pickup_tid = 0;
static volatile int g_pickup_running = 0;

#define PICKUP_WATCHER_INTERVAL_SEC 3

/* 单周期最多查询次数：限制 fork curl 子进程频率，避免网络差时监控周期被拖长 */
#define MAX_QUERY_PER_CYCLE 5

/**************************************************************************
 *
 *   @brief : 后台扫码取件监控线程
 *   @arg   : arg 线程参数（当前未使用，留 NULL）
 *
 *   @retval: NULL
 *   @note  : 每 3 秒轮询取件令牌和各柜子的存件令牌，
 *            检测到网页端验证完成后仅设置通知标志，由主线程执行开箱；
 *            链表字段访问持 locker_lock，网络查询放在锁外
 *
 ***************************************************************************/
static void *pickup_monitor_thread(void *arg)
{
    (void)arg;
    int consecutive_fail = 0;

    nice(10);

    while (g_pickup_running)
    {
        int query_budget;
        int cycle_fail = 0;
        char token[VG_TOKEN_LEN] = {0};
        int st;

        sleep(PICKUP_WATCHER_INTERVAL_SEC);
        query_budget = MAX_QUERY_PER_CYCLE;

        /* 1) 全局取件令牌 */
        if (!PICKUP_NOTIFY_GET() && query_budget > 0)
        {
            locker_lock();
            if (g_pickup_token[0] != '\0')
                strncpy(token, g_pickup_token, VG_TOKEN_LEN - 1);
            locker_unlock();

            if (token[0] != '\0')
            {
                query_budget--;
                st = vg_query_status(token, NULL);
                if (st == VG_STATUS_ERROR) cycle_fail = 1;
                if (st == VG_STATUS_VERIFIED || st == VG_STATUS_OPENED)
                {
                    printf("[扫码监控] 取件token已验证，通知主线程处理\n");
                    PICKUP_NOTIFY_SET();
                }
            }
        }

        /* 2) 各柜子存件令牌（查询预算用尽则本周期跳过，留待下周期） */
        if (!PICKUP_NOTIFY_GET() && query_budget > 0)
        {
            locker_node_t *node = ui_get_locker_head();

            while (node != NULL && !PICKUP_NOTIFY_GET() && query_budget > 0)
            {
                memset(token, 0, sizeof(token));

                locker_lock();
                if (node->loc_data == LOCKER_OCCUPIED &&
                    node->pickup_token[0] != '\0')
                {
                    strncpy(token, node->pickup_token, VG_TOKEN_LEN - 1);
                }
                locker_unlock();

                if (token[0] != '\0')
                {
                    query_budget--;
                    st = vg_query_status(token, NULL);
                    if (st == VG_STATUS_ERROR) cycle_fail = 1;
                    if (st == VG_STATUS_VERIFIED || st == VG_STATUS_OPENED)
                    {
                        /* locker_ID 创建后不可变，锁外读取仅用于日志，安全 */
                        printf("[扫码监控] 检测到柜号:%s 状态:%d，通知主线程处理\n",
                               node->locker_ID, st);
                        PICKUP_NOTIFY_SET();
                    }
                }
                node = node->next;
            }
        }

        /* 网络不可用退避：连续 3 个周期查询全部失败时拉长休眠，
         * 减少断网期间频繁 fork curl 造成的系统抖动与触摸响应延迟 */
        if (cycle_fail)
        {
            if (++consecutive_fail >= 3)
            {
                printf("[扫码监控] 网络查询连续失败，暂停轮询 30 秒\n");
                for (int i = 0; i < 30 && g_pickup_running; i++) sleep(1);
                consecutive_fail = 0;
            }
        }
        else
        {
            consecutive_fail = 0;
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

void ui_stop_pickup_watcher(void)
{
    if (g_pickup_running)
    {
        g_pickup_running = 0;
        if (g_pickup_tid != 0)
        {
            pthread_join(g_pickup_tid, NULL);
            g_pickup_tid = 0;
        }
        printf("[扫码监控] 后台监控线程已停止\n");
    }
}

/**************************************************************************
 *
 *   @brief : 检查扫码取件状态并执行开箱操作
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *   @arg   : want_continue 输出参数：取件成功页用户选择"继续取件"时为 1
 *
 *   @retval: 1  扫码取件成功，已开箱并显示成功界面
 *            0  未取件或无匹配包裹
 *           -1  清理柜门失败
 *   @note  : 优先检查全局取件令牌 g_pickup_token，按手机号匹配柜子开箱；
 *            其次遍历各柜子的存件令牌，验证通过后消费票据并开箱；
 *            开箱成功后内部调用 show_takeout_success 显示成功界面
 *
 ***************************************************************************/
int check_scan_pickup(ui_context_t *ui, int *want_continue)
{
    locker_node_t *head = ui_get_locker_head();
    locker_node_t *node;

    if (want_continue) *want_continue = 0;

    /* 1) 全局取件令牌（扫码进入取件界面时生成） */
    if (g_pickup_token[0] != '\0')
    {
        vg_status_t status;
        int st = vg_query_status(g_pickup_token, &status);
        if (st == VG_STATUS_VERIFIED && status.verified_phone[0] != '\0')
        {
            char remaining_json[2048] = "[";
            int has_remaining = 0;
            int opened = 0;

            /* 链表读写持锁，网络调用放锁外 */
            locker_lock();
            node = head;
            while (node != NULL)
            {
                if (node->loc_data == LOCKER_OCCUPIED &&
                    strcmp(node->small_phone, status.verified_phone) == 0)
                {
                    printf("[扫码取件] 验证通过，柜号:%s 手机:%s\n",
                           node->locker_ID, status.verified_phone);
                    if (locker_clean_by_id(head, node->locker_ID))
                    {
                        opened = 1;
                    }
                    break;
                }
                node = node->next;
            }

            if (opened > 0)
            {
                node = head;
                while (node != NULL)
                {
                    if (node->loc_data == LOCKER_OCCUPIED &&
                        strcmp(node->small_phone, status.verified_phone) == 0)
                    {
                        char entry[128];
                        snprintf(entry, sizeof(entry),
                                 "%s{\"lockerId\":\"%s\",\"code\":\"%s\"}",
                                 has_remaining ? "," : "",
                                 node->locker_ID, node->locker_getID);
                        strncat(remaining_json, entry,
                                sizeof(remaining_json) - strlen(remaining_json) - 1);
                        has_remaining = 1;
                    }
                    node = node->next;
                }
                strncat(remaining_json, "]",
                        sizeof(remaining_json) - strlen(remaining_json) - 1);
            }
            locker_unlock();

            if (opened > 0)
            {
                /* 先上报剩余包裹（票据仍有效），再消费票据，最后清空本地令牌 */
                if (has_remaining)
                {
                    vg_update_remaining(g_pickup_token, remaining_json);
                }
                {
                    char vphone[VG_PHONE_LEN] = {0};
                    if (vg_consume_ticket(g_pickup_token, vphone, sizeof(vphone)) != 1)
                    {
                        printf("[扫码取件] 票据消费失败，服务器端状态可能未同步\n");
                    }
                }
                /* 先清空令牌再显示成功页，避免监控线程再次通知打断成功页 */
                locker_lock();
                memset(g_pickup_token, 0, sizeof(g_pickup_token));
                locker_unlock();
                if (want_continue) *want_continue = show_takeout_success(ui);
                return 1;
            }
            printf("[扫码取件] 手机号 %s 无匹配包裹\n", status.verified_phone);
            /* 无匹配包裹时也清理令牌，避免监控线程反复通知造成界面重建 */
            locker_lock();
            memset(g_pickup_token, 0, sizeof(g_pickup_token));
            locker_unlock();
            return 0;
        }
        if (st == VG_STATUS_OPENED)
        {
            printf("[扫码取件] 取件令牌已开箱，本地清理\n");
            locker_lock();
            memset(g_pickup_token, 0, sizeof(g_pickup_token));
            locker_unlock();
            return 0;
        }
    }

    /* 2) 各柜子存件令牌（单周期限次，避免网络故障时逐个查询拖垮主界面） */
    int query_budget = MAX_QUERY_PER_CYCLE;
    node = head;
    while (node != NULL && query_budget > 0)
    {
        char token[VG_TOKEN_LEN] = {0};
        int st;

        locker_lock();
        if (node->loc_data == LOCKER_OCCUPIED && node->pickup_token[0] != '\0')
        {
            strncpy(token, node->pickup_token, VG_TOKEN_LEN - 1);
        }
        locker_unlock();

        if (token[0] == '\0')
        {
            node = node->next;
            continue;
        }

        query_budget--;
        st = vg_query_status(token, NULL);
        if (st == VG_STATUS_VERIFIED)
        {
            char vphone[VG_PHONE_LEN] = {0};
            int cr = vg_consume_ticket(token, vphone, sizeof(vphone));
            if (cr == 1)
            {
                char saved_token[VG_TOKEN_LEN];
                char remaining_json[2048] = "[";
                int has_remaining = 0;
                int cleaned = 0;

                strncpy(saved_token, token, VG_TOKEN_LEN - 1);
                saved_token[VG_TOKEN_LEN - 1] = '\0';

                printf("[扫码取件] 验证通过，柜号:%s 手机:%s\n",
                       node->locker_ID, vphone[0] ? vphone : "未知");

                locker_lock();
                cleaned = locker_clean_by_id(head, node->locker_ID);
                if (vphone[0] != '\0')
                {
                    locker_node_t *rn = head;
                    while (rn != NULL)
                    {
                        if (rn->loc_data == LOCKER_OCCUPIED &&
                            strcmp(rn->small_phone, vphone) == 0)
                        {
                            char entry[128];
                            snprintf(entry, sizeof(entry),
                                     "%s{\"lockerId\":\"%s\",\"code\":\"%s\"}",
                                     has_remaining ? "," : "",
                                     rn->locker_ID, rn->locker_getID);
                            strncat(remaining_json, entry,
                                    sizeof(remaining_json) - strlen(remaining_json) - 1);
                            has_remaining = 1;
                        }
                        rn = rn->next;
                    }
                    strncat(remaining_json, "]",
                            sizeof(remaining_json) - strlen(remaining_json) - 1);
                }
                locker_unlock();

                if (!cleaned)
                {
                    printf("[扫码取件] 清理柜门失败，跳过\n");
                    locker_lock();
                    memset(node->pickup_token, 0, VG_TOKEN_LEN);
                    locker_unlock();
                    return -1;
                }

                if (vphone[0] != '\0' && has_remaining)
                {
                    vg_update_remaining(saved_token, remaining_json);
                }

                if (want_continue) *want_continue = show_takeout_success(ui);
                return 1;
            }
            else
            {
                if (cr == 0)
                {
                    /* 业务明确拒绝（未验证/已消费），清理本地令牌 */
                    printf("[扫码取件] 票据消费失败(返回%d)，柜号:%s，清理令牌\n",
                           cr, node->locker_ID);
                    locker_lock();
                    memset(node->pickup_token, 0, VG_TOKEN_LEN);
                    locker_unlock();
                }
                else
                {
                    /* 网络/解析失败(-2/-3)：保留令牌，下次轮询重试 */
                    printf("[扫码取件] 票据消费网络失败(返回%d)，柜号:%s，保留令牌待重试\n",
                           cr, node->locker_ID);
                }
            }
        }
        else if (st == VG_STATUS_OPENED)
        {
            printf("[扫码取件] 柜号:%s 已被开箱，本地同步清空\n", node->locker_ID);
            locker_lock();
            if (!locker_clean_by_id(head, node->locker_ID))
            {
                printf("[扫码取件] 警告: 本地未找到柜号:%s\n", node->locker_ID);
            }
            locker_unlock();
        }
        node = node->next;
    }
    return 0;
}