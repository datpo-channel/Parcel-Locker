#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "ui_logic.h"
#include "takeout_page.h"
#include "store_page.h"
#include "login_page.h"
#include "query_page.h"
#include "pickup_monitor.h"
#include "locker_info.h"
#include "verify_gate.h"
#include "qr_jpeg.h"

#define MAX_RETRY_COUNT  3
#define SCAN_PICKUP_POLL_INTERVAL_SEC  3
#define SCAN_PICKUP_POLL_TIMEOUT_SEC   300

char g_pickup_token[VG_TOKEN_LEN];

static volatile int g_qr_running = 0;
static pthread_t g_qr_tid;

static void *qr_thread(void *arg)
{
    ui_context_t *ui = (ui_context_t *)arg;
    char path[64] = "resource/qrcode/pickup_qr.jpg";

    while (g_qr_running)
    {
        lcd_show_jpg(&ui->lcd, path, 188, 188);
        usleep(100000);
    }
    return NULL;
}

static int check_scan_pickup(ui_context_t *ui)
{
    locker_node_t *head = ui_get_locker_head();
    locker_node_t *node;

    if (g_pickup_token[0] != '\0')
    {
        vg_status_t status;
        int st = vg_query_status(g_pickup_token, &status);
        if (st == VG_STATUS_VERIFIED && status.verified_phone[0] != '\0')
        {
            node = head;
            int opened = 0;
            while (node != NULL)
            {
                if (node->loc_data == LOCKER_OCCUPIED &&
                    strcmp(node->small_phone, status.verified_phone) == 0)
                {
                    printf("[扫码取件] 验证通过，柜号:%s 手机:%s\n",
                           node->locker_ID, status.verified_phone);
                    locker_clean_by_id(head, node->locker_ID);
                    opened++;
                }
                node = node->next;
            }
            if (opened > 0)
            {
                show_takeout_success(ui);
                memset(g_pickup_token, 0, sizeof(g_pickup_token));
                return 1;
            }
            printf("[扫码取件] 手机号 %s 无匹配包裹\n", status.verified_phone);
            memset(g_pickup_token, 0, sizeof(g_pickup_token));
            return 0;
        }
    }

    node = head;
    while (node != NULL)
    {
        if (node->loc_data == LOCKER_OCCUPIED && node->pickup_token[0] != '\0')
        {
            int st = vg_query_status(node->pickup_token, NULL);
            if (st == VG_STATUS_VERIFIED)
            {
                char vphone[VG_PHONE_LEN] = {0};
                int cr = vg_consume_ticket(node->pickup_token, vphone, sizeof(vphone));
                if (cr == 1)
                {
                    printf("[扫码取件] 验证通过，柜号:%s 手机:%s\n",
                           node->locker_ID, vphone[0] ? vphone : "未知");
                    locker_clean_by_id(head, node->locker_ID);
                    show_takeout_success(ui);
                    return 1;
                }
            }
            else if (st == VG_STATUS_OPENED)
            {
                printf("[扫码取件] 柜号:%s 已被开箱，本地同步清空\n", node->locker_ID);
                locker_clean_by_id(head, node->locker_ID);
            }
        }
        node = node->next;
    }
    return 0;
}

int main(void)
{
    ui_context_t ui;
    int touch_fd;
    char code[5];
    char phone[14];
    int ret;

    srand((unsigned int)time(NULL));

    printf("\n========================================\n");
    printf("  GEC6818 Mail Box System v2.0\n");
    printf("========================================\n\n");

    touch_fd = open(TOUCHPAD_PATH, O_RDWR);
    if (touch_fd < 0)
    {
        perror("open touchpad failed");
        return -1;
    }

    if (ui_init(&ui, touch_fd) != 0)
    {
        printf("UI init failed\n");
        close(touch_fd);
        return -1;
    }

    if (ui_start(&ui) != 0)
    {
        printf("UI start failed\n");
        ui_stop(&ui);
        close(touch_fd);
        return -1;
    }

    lcd_cache_init();
    lcd_preload_common_images();

    ui_start_pickup_watcher();

    printf("System ready! Waiting for user interaction...\n\n");

    while (1)
    {
        ret = show_main_menu(&ui);

        if (ret == RET_SCAN_PICKUP)
        {
            ui_clear_pickup_notify();
            printf("[扫码取件] 主菜单检测到验证完成，执行开箱\n");
            check_scan_pickup(&ui);
            continue;
        }

        switch (ret)
        {
        case 1:
            printf("用户选择: 取件\n");
            {
                char pickup_url[VG_URL_LEN];
                char qr_path[64] = "resource/qrcode/pickup_qr.jpg";

                vg_create_pickup(NULL, NULL, g_pickup_token, pickup_url);
                generate_qr(pickup_url, qr_path, 3, 2, 85);
                printf("[扫码取件] 二维码已生成，token: %s\n", g_pickup_token);

                g_qr_running = 1;
                pthread_create(&g_qr_tid, NULL, qr_thread, &ui);

                int takeout_retry = 0;

                while (takeout_retry < MAX_RETRY_COUNT)
                {
                    if (check_scan_pickup(&ui))
                    {
                        printf("[扫码取件] 取件成功，返回主页\n");
                        break;
                    }

                    ret = show_takeout_code(&ui, code);

                    if (ret == RET_SCAN_PICKUP)
                    {
                        ui_clear_pickup_notify();
                        printf("[扫码取件] 取件界面检测到验证完成，执行开箱\n");
                        check_scan_pickup(&ui);
                        break;
                    }

                    if (ret == RET_TAKEOUT_BACK || ret == RET_TIMEOUT)
                    {
                        if (ret == RET_TIMEOUT)
                            printf("取件界面超时，返回主页\n");
                        else
                            printf("用户从取件界面返回\n");
                        break;
                    }

                    if (ret == RET_TAKEOUT_QUERY)
                    {
                        printf("用户查询取件信息...\n");

                        int query_ret = show_received_query(&ui);
                        if (query_ret == RET_SCAN_PICKUP)
                        {
                            ui_clear_pickup_notify();
                            printf("[扫码取件] 查询界面检测到验证完成，执行开箱\n");
                            check_scan_pickup(&ui);
                            break;
                        }
                        else if (query_ret == RET_QUERY_OK)
                        {
                            printf("查询取件信息完成\n");
                        }
                        else if (query_ret == RET_QUERY_BACK)
                        {
                            printf("用户返回取件界面\n");
                        }

                        continue;
                    }

                    if (ret != RET_TAKEOUT_OK)
                    {
                        continue;
                    }

                    printf("输入的取件码为: %s\n", code);

                    int valid = 0;
                    locker_node_t *locker = locker_find_by_code(ui_get_locker_head(), code);
                    if (locker != NULL)
                    {
                        valid = locker_clean_by_id(ui_get_locker_head(), locker->locker_ID);
                    }

                    if (valid)
                    {
                        printf("取件成功！储物柜%s已弹出并清空\n", locker->locker_ID);
                        int takeout_next = show_takeout_success(&ui);
                        takeout_retry = 0;
                        if (takeout_next == 1)
                        {
                            continue;
                        }
                        break;
                    }
                    else
                    {
                        takeout_retry++;
                        if (takeout_retry >= MAX_RETRY_COUNT)
                        {
                            printf("[错误] 取件码验证已失败%d次，达到最大重试次数，返回主页\n", MAX_RETRY_COUNT);
                            break;
                        }
                        printf("验证失败（第%d/%d次），请重新输入\n", takeout_retry, MAX_RETRY_COUNT);
                        continue;
                    }
                }

                g_qr_running = 0;
                pthread_join(g_qr_tid, NULL);
            }
            memset(g_pickup_token, 0, sizeof(g_pickup_token));
            break;

        case 2:
            printf("用户选择: 存件\n");
            {
                int user_login_retry = 0;

                while (user_login_retry < MAX_RETRY_COUNT)
                {
                    ret = show_user_login(&ui, phone);

                    if (ret == RET_TIMEOUT)
                    {
                        printf("[超时] 用户登录超时，返回主页\n");
                        break;
                    }

                    if (ret == RET_LOGIN_CANCEL)
                    {
                        printf("用户取消登录，返回主页\n");
                        break;
                    }

                    if (ret == RET_LOGIN_FAILED)
                    {
                        user_login_retry++;
                        if (user_login_retry >= MAX_RETRY_COUNT)
                        {
                            printf("[错误] 用户登录流程已重试%d次，验证码多次错误，返回主页\n", MAX_RETRY_COUNT);
                            break;
                        }
                        printf("验证码错误次数过多（第%d次登录流程），请重新开始\n", user_login_retry + 1);
                        continue;
                    }

                    if (ret != 0)
                    {
                        printf("未知登录错误: %d\n", ret);
                        break;
                    }

                    printf("普通用户登录成功，手机号: %s\n", phone);

                    char custom_code[5];
                    int box_size = 0;
                    int duration = 0;

                    ret = show_store_info(&ui, phone, custom_code, &box_size, &duration);
                    if (ret == RET_TIMEOUT)
                    {
                        printf("[超时] 存物信息填写超时，返回主页\n");
                        break;
                    }
                    else if (ret == RET_TAKEOUT_BACK)
                    {
                        printf("用户从存件界面返回，退出登录流程\n");
                        break;
                    }
                    else if (ret != 0)
                    {
                        printf("[错误] 存物信息填写返回未知错误(%d)，退出登录流程\n", ret);
                        break;
                    }

                    printf("收件人手机号为: %s, 取件码为: %s, 存储大小: %d, 存储时间长: %dh\n",
                           phone, custom_code, box_size, duration);

                    const char *prefixes[] = {"", "A", "B", "C"};
                    locker_node_t *locker = locker_find_first_empty_by_prefix(
                        ui_get_locker_head(), prefixes[box_size]);
                    if (locker == NULL)
                    {
                        printf("没有大小为 %d 的空储物柜！\n", box_size);
                        break;
                    }

                    locker->loc_data = LOCKER_OCCUPIED;
                    strncpy(locker->locker_getID, custom_code, LOCKER_CODE_LEN - 1);
                    locker->locker_getID[LOCKER_CODE_LEN - 1] = '\0';
                    strncpy(locker->small_phone, phone, PHONE_LEN - 1);
                    locker->small_phone[PHONE_LEN - 1] = '\0';

                    printf("已分配储物柜为: %s, 取件码为: %s\n",
                           locker->locker_ID, custom_code);

                    int pay_ret = show_pay_info(&ui);
                    if (pay_ret == 1)
                    {
                        printf("[存件] 用户已支付，显示成功页面\n");
                        show_send_success(&ui);
                    }
                    else
                    {
                        printf("[存件] 用户取消支付或超时，释放储物柜\n");
                        locker->loc_data = LOCKER_EMPTY;
                        memset(locker->locker_getID, 0, sizeof(locker->locker_getID));
                        memset(locker->small_phone, 0, sizeof(locker->small_phone));
                        memset(locker->pickup_token, 0, sizeof(locker->pickup_token));
                    }

                    user_login_retry = 0;
                    break;
                }
            }
            break;

        case 3:
            printf("用户选择: 登录快递员\n");
            {
                int courier_login_retry = 0;

                while (courier_login_retry < MAX_RETRY_COUNT)
                {
                    ret = show_sendman_login(&ui, phone);

                    if (ret == RET_TIMEOUT)
                    {
                        printf("[超时] 快递员登录超时，返回主页\n");
                        break;
                    }

                    if (ret == RET_LOGIN_CANCEL)
                    {
                        printf("快递员取消登录，返回主页\n");
                        break;
                    }

                    if (ret == RET_LOGIN_FAILED)
                    {
                        courier_login_retry++;
                        if (courier_login_retry >= MAX_RETRY_COUNT)
                        {
                            printf("[错误] 快递员登录流程已重试%d次，验证码多次错误，返回主页\n", MAX_RETRY_COUNT);
                            break;
                        }
                        printf("验证码错误次数过多（第%d次登录流程），请重新开始\n", courier_login_retry + 1);
                        continue;
                    }

                    if (ret != 0)
                    {
                        printf("未知登录错误: %d\n", ret);
                        break;
                    }

                    char custom_code[5];
                    int box_size = 0;
                    int duration = 0;

                    printf("用户登录快递员成功！手机号为: %s\n", phone);
                    ret = show_store_info(&ui, phone, custom_code, &box_size, &duration);
                    if (ret == RET_TIMEOUT)
                    {
                        printf("[超时] 快递员存物信息填写超时，返回主页\n");
                        break;
                    }
                    else if (ret == RET_TAKEOUT_BACK)
                    {
                        printf("快递员从存件界面返回，退出登录流程\n");
                        break;
                    }
                    else if (ret != 0)
                    {
                        printf("[错误] 存物信息填写返回未知错误(%d)，退出登录流程\n", ret);
                        break;
                    }

                    printf("收件人手机号为: %s, 取件码为: %s, 存储大小为: %d, 存储时间长为: %dh\n",
                           phone, custom_code, box_size, duration);

                    const char *prefixes[] = {"", "A", "B", "C"};
                    locker_node_t *locker = locker_find_first_empty_by_prefix(
                        ui_get_locker_head(), prefixes[box_size]);
                    if (locker == NULL)
                    {
                        printf("没有大小为 %d 的空储物柜！\n", box_size);
                        break;
                    }

                    locker->loc_data = LOCKER_OCCUPIED;
                    strncpy(locker->locker_getID, custom_code, LOCKER_CODE_LEN - 1);
                    locker->locker_getID[LOCKER_CODE_LEN - 1] = '\0';
                    strncpy(locker->small_phone, phone, PHONE_LEN - 1);
                    locker->small_phone[PHONE_LEN - 1] = '\0';

                    printf("已分配储物柜为: %s, 取件码为: %s\n",
                           locker->locker_ID, custom_code);

                    show_send_success(&ui);

                    courier_login_retry = 0;
                    break;
                }
            }
            break;

        case 4:
            printf("用户选择: 查询\n");
            {
                int query_retry = 0;

                while (query_retry < MAX_RETRY_COUNT)
                {
                    ret = show_received_query(&ui);

                    if (ret == RET_TIMEOUT)
                    {
                        printf("[超时] 查询界面操作超时，返回主页\n");
                        break;
                    }

                    if (ret == RET_LOGIN_FAILED)
                    {
                        printf("[安全] 验证码错误次数过多，返回主页\n");
                        break;
                    }

                    if (ret == RET_QUERY_BACK)
                    {
                        printf("用户从查询界面返回\n");
                        break;
                    }

                    if (ret == RET_QUERY_OK)
                    {
                        printf("查询取件信息完成\n");
                        query_retry = 0;
                        break;
                    }

                    query_retry++;
                    if (query_retry >= MAX_RETRY_COUNT)
                    {
                        printf("[错误] 查询操作已失败%d次，达到最大重试次数，返回主页\n", MAX_RETRY_COUNT);
                        break;
                    }
                    printf("查询失败（第%d/%d次），请重新尝试\n", query_retry, MAX_RETRY_COUNT);
                }
            }
            break;

        default:
            printf("未知选择: %d\n", ret);
            break;
        }

        usleep(100000);
    }

    ui_stop(&ui);
    lcd_cache_cleanup();
    close(touch_fd);

    return 0;
}