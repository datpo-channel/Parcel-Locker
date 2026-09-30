#include "main.h"

#define MAX_RETRY_COUNT  3
#define DISP_BUF_SIZE    (LCD_WIDTH * LCD_HEIGHT)

char g_pickup_token[VG_TOKEN_LEN];

/**************************************************************************
 *
 *   @brief : 程序主入口，初始化系统并循环处理用户操作
 *
 *   @retval: 0  正常退出
 *           -1  初始化失败
 *   @note  : 初始化触摸屏和UI上下文，启动扫码取件后台监控线程，
 *            循环显示主菜单并根据用户选择进入取件/存件/快递员/管理界面；
 *            后台检测到扫码取件验证完成时，中断当前界面执行开箱
 *
 ***************************************************************************/

uint32_t custom_tick_get(void)
{
    struct timespec ts;
    /* 用单调时钟：墙钟被 NTP/手动调整时 tick 不会跳变或回退 */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* 生成取件令牌写入全局 g_pickup_token：对扫码监控线程持 locker_lock，
 * 避免主线程无锁写与监控线程锁内读取产生数据竞争
 * @retval: 0 成功 / -1 失败 */
static int regen_pickup_token(char *token, char *url)
{
    int ret;

    locker_lock();
    ret = vg_create_pickup(token, url);
    locker_unlock();
    return ret;
}

int main(void)
{
    ui_context_t ui;
    char phone[14];
    int ret;

    srand((unsigned int)time(NULL));

    printf("\n========================================\n");
    printf("  GEC6818 Mail Box System v2.0\n");
    printf("========================================\n\n");

    if (ui_init(&ui) != 0)
    {
        printf("UI init failed\n");
        return -1;
    }

    if (ui_start(&ui) != 0)
    {
        printf("UI start failed\n");
        ui_stop(&ui);
        return -1;
    }

    printf("正在同步网络时间...\n");
    if (ntp_sync() != 0)
    {
        printf("[警告] NTP 时间同步失败，系统时间可能不准确\n");
    }

    ui_start_pickup_watcher();

    printf("System ready! Waiting for user interaction...\n\n");

    {
        time_t _lv_time_last = 0;
        time_t _lv_time_now;

        lv_init();
        fbdev_init();

        static lv_color_t _lv_buf1[DISP_BUF_SIZE];
        static lv_color_t _lv_buf2[DISP_BUF_SIZE];
        static lv_disp_draw_buf_t _lv_disp_buf;
        lv_disp_draw_buf_init(&_lv_disp_buf, _lv_buf1, _lv_buf2, DISP_BUF_SIZE);

        static lv_disp_drv_t _lv_disp_drv;
        lv_disp_drv_init(&_lv_disp_drv);
        _lv_disp_drv.draw_buf = &_lv_disp_buf;
        _lv_disp_drv.flush_cb  = fbdev_flush;
        _lv_disp_drv.hor_res   = LCD_WIDTH;
        _lv_disp_drv.ver_res   = LCD_HEIGHT;
        lv_disp_drv_register(&_lv_disp_drv);

        evdev_init();

        static lv_indev_drv_t _lv_indev_drv;
        lv_indev_drv_init(&_lv_indev_drv);
        _lv_indev_drv.type    = LV_INDEV_TYPE_POINTER;
        _lv_indev_drv.read_cb = evdev_read;
        lv_indev_drv_register(&_lv_indev_drv);

        lv_main_menu_create();
        weather_start();

        while (1)
        {
            lv_timer_handler();
            /* 与各页面 get_action 轮询节拍保持一致，降低空转功耗 */
            usleep(20000);

            time(&_lv_time_now);
            if (_lv_time_now != _lv_time_last)
            {
                /* 使用本地时间（时区由系统 TZ 环境变量决定），避免硬编码时区偏移 */
                struct tm *tm_info;
                tm_info = localtime(&_lv_time_now);
                char time_str[32];
                strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
                lv_main_menu_update_time(time_str);

                const weather_info_t *winfo = weather_get_info();
                if (winfo != NULL && winfo->valid) {
                    lv_main_menu_update_weather_info(winfo->city, winfo->weather_desc);
                } else {
                    lv_main_menu_update_weather_info("获取失败", "");
                }
                _lv_time_last = _lv_time_now;
            }

            if (PICKUP_NOTIFY_GET())
            {
                PICKUP_NOTIFY_CLR();
                printf("[扫码取件] 主菜单检测到验证完成，执行开箱\n");
                int wc = 0;
                check_scan_pickup(&ui, &wc);
                lv_main_menu_destroy();
                lv_main_menu_create();
            }

            int action = lv_main_menu_get_action();
            if (action == 0)
            {
                continue;
            }

            lv_main_menu_destroy();

            ret = action;

            if (ret == 1)
            {
                char pickup_url[VG_URL_LEN];
                char qr_path[256] = "/Workspace/mail_box_pic/resource/qrcode/pickup_qr.jpg";

                if (regen_pickup_token(g_pickup_token, pickup_url) != 0)
                {
                    printf("[错误] 取件令牌生成失败，返回主页\n");
                    lv_main_menu_create();
                    continue;
                }
                if (generate_qr(pickup_url, qr_path, 3, 2, 85) != 0)
                {
                    printf("[错误] 二维码生成失败，请检查 qrcode 目录是否存在\n");
                }
                printf("[扫码取件] 二维码已生成，token: %.6s...(共%zu位)\n",
                       g_pickup_token, strlen(g_pickup_token));

                lvgl_takeout_create(&ui);
                lvgl_takeout_set_qr(qr_path);

                int takeout_retry = 0;

                while (takeout_retry < MAX_RETRY_COUNT)
                {
                    int wc = 0;
                    if (check_scan_pickup(&ui, &wc))
                    {
                        if (wc)
                        {
                            /* 用户选择继续取件：重新生成令牌与二维码，回到取件界面 */
                            printf("[扫码取件] 用户选择继续取件\n");
                            /* 成功页已销毁，但取件页仍在屏上，先销毁再重建，防止对象泄漏 */
                            lvgl_takeout_destroy();
                            if (regen_pickup_token(g_pickup_token, pickup_url) != 0)
                            {
                                printf("[错误] 取件令牌生成失败，返回主页\n");
                                break;
                            }
                            if (generate_qr(pickup_url, qr_path, 3, 2, 85) != 0)
                            {
                                printf("[错误] 二维码生成失败，请检查 qrcode 目录是否存在\n");
                            }
                            lvgl_takeout_create(&ui);
                            lvgl_takeout_set_qr(qr_path);
                            takeout_retry = 0;
                            continue;
                        }
                        printf("[扫码取件] 取件成功，返回主页\n");
                        break;
                    }

                    int key = lvgl_takeout_get_action(&ui);

                    if (PICKUP_NOTIFY_GET())
                    {
                        PICKUP_NOTIFY_CLR();
                        printf("[扫码取件] 取件界面检测到验证完成，执行开箱\n");
                        int wc2 = 0;
                        if (check_scan_pickup(&ui, &wc2) && wc2)
                        {
                            /* 用户选择继续取件：重新生成令牌与二维码，回到取件界面 */
                            lvgl_takeout_destroy();
                            if (regen_pickup_token(g_pickup_token, pickup_url) != 0)
                            {
                                printf("[错误] 取件令牌生成失败，返回主页\n");
                                break;
                            }
                            if (generate_qr(pickup_url, qr_path, 3, 2, 85) != 0)
                            {
                                printf("[错误] 二维码生成失败，请检查 qrcode 目录是否存在\n");
                            }
                            lvgl_takeout_create(&ui);
                            lvgl_takeout_set_qr(qr_path);
                            takeout_retry = 0;
                            continue;
                        }
                        break;
                    }

                    if (key == UI_KEY_BACK)
                    {
                        printf("用户从取件界面返回\n");
                        break;
                    }

                    if (key == -1)
                    {
                        printf("取件界面超时，返回主页\n");
                        break;
                    }

                    if (key == UI_KEY_QUERY)
                    {
                        printf("用户查询取件信息...\n");

                        lvgl_takeout_destroy();

                        int query_ret = show_received_query(&ui);
                        if (query_ret == RET_SCAN_PICKUP)
                        {
                            PICKUP_NOTIFY_CLR();
                            printf("[扫码取件] 查询界面检测到验证完成，执行开箱\n");
                            int wc3 = 0;
                            if (!check_scan_pickup(&ui, &wc3) || !wc3)
                            {
                                printf("[扫码取件] 用户选择不继续取件，返回主页\n");
                                break;
                            }
                            /* 用户选择继续取件：重新生成令牌与二维码 */
                            if (regen_pickup_token(g_pickup_token, pickup_url) != 0)
                            {
                                printf("[错误] 取件令牌生成失败，返回主页\n");
                                break;
                            }
                            if (generate_qr(pickup_url, qr_path, 3, 2, 85) != 0)
                            {
                                printf("[错误] 二维码生成失败，请检查 qrcode 目录是否存在\n");
                            }
                            lvgl_takeout_create(&ui);
                            lvgl_takeout_set_qr(qr_path);
                            continue;
                        }
                        else if (query_ret == RET_QUERY_OK)
                        {
                            printf("查询取件信息完成\n");
                        }
                        else if (query_ret == RET_QUERY_BACK)
                        {
                            printf("用户返回取件界面\n");
                        }

                        lvgl_takeout_create(&ui);
                        lvgl_takeout_set_qr(qr_path);
                        continue;
                    }

                    /* key == UI_KEY_CONFIRM */
                    char code[5];
                    if (lvgl_takeout_get_code(code, sizeof(code)) != 0)
                    {
                        /* 未输满 4 位, 忽略 */
                        continue;
                    }

                    printf("输入的取件码为: %s\n", code);

                    int valid = 0;
                    locker_lock();
                    locker_node_t *locker = locker_find_by_code(ui_get_locker_head(), code);
                    if (locker != NULL)
                    {
                        valid = locker_clean_by_id(ui_get_locker_head(), locker->locker_ID);
                        if (valid)
                        {
                            printf("取件成功！储物柜%s已弹出并清空\n", locker->locker_ID);
                        }
                    }
                    locker_unlock();

                    if (valid)
                    {
                        lvgl_takeout_destroy();
                        int takeout_next = show_takeout_success(&ui);
                        takeout_retry = 0;
                        if (takeout_next == 1)
                        {
                            /* 继续取件需重新生成令牌与二维码，否则旧 token 失效后扫码取件不可用 */
                            if (regen_pickup_token(g_pickup_token, pickup_url) != 0)
                            {
                                printf("[错误] 取件令牌生成失败，返回主页\n");
                                break;
                            }
                            if (generate_qr(pickup_url, qr_path, 3, 2, 85) != 0)
                            {
                                printf("[错误] 二维码生成失败，请检查 qrcode 目录是否存在\n");
                            }
                            lvgl_takeout_create(&ui);
                            lvgl_takeout_set_qr(qr_path);
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
                        /* 清空输入 */
                        lvgl_takeout_destroy();
                        lvgl_takeout_create(&ui);
                        lvgl_takeout_set_qr(qr_path);
                        continue;
                    }
                }

                lvgl_takeout_destroy();
                locker_lock();
                memset(g_pickup_token, 0, sizeof(g_pickup_token));
                locker_unlock();

                lv_main_menu_create();
                continue;
            }

            lv_main_menu_destroy();

            switch (ret)
            {
            case 2:
                printf("用户选择: 存件\n");
            {
                int logged_in = 0;

                phone[0] = '\0';

                while (1)
                {
                    if (!logged_in)
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
                            printf("[安全] 验证码错误次数过多，返回主页\n");
                            break;
                        }

                        if (ret == RET_SCAN_PICKUP)
                        {
                            PICKUP_NOTIFY_CLR();
                            printf("[扫码取件] 登录界面检测到验证完成，执行开箱\n");
                            int wc_scan = 0;
                            check_scan_pickup(&ui, &wc_scan);
                            break;
                        }

                        if (ret != 0)
                        {
                            printf("未知登录错误: %d\n", ret);
                            break;
                        }

                        printf("普通用户登录成功，手机号: %s\n", phone);
                        logged_in = 1;
                    }

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

                    /* 链表分配与写入持锁，防止与扫码监控线程并发读写 */
                    locker_lock();
                    /* 取件码全局查重：碰撞则重新生成，避免取件时弹错柜门 */
                    while (locker_find_by_code(ui_get_locker_head(), custom_code) != NULL)
                    {
                        generate_random_code(custom_code, 4);
                    }
                    locker_node_t *locker = locker_find_first_empty_by_prefix(
                        ui_get_locker_head(), LOCKER_PREFIXES[box_size]);
                    if (locker == NULL)
                    {
                        locker_unlock();
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

                    {
                        char pickup_url[VG_URL_LEN];
                        if (vg_create_pickup(locker->pickup_token, pickup_url) != 0)
                        {
                            printf("[错误] 取件令牌生成失败，释放储物柜\n");
                            locker->loc_data = LOCKER_EMPTY;
                            memset(locker->locker_getID, 0, sizeof(locker->locker_getID));
                            memset(locker->small_phone, 0, sizeof(locker->small_phone));
                            memset(locker->username, 0, sizeof(locker->username));
                            locker_unlock();
                            break;
                        }
                        printf("[存件] 已生成取件令牌: %s\n", locker->pickup_token);
                    }
                    locker_unlock();

                    int pay_ret = show_pay_info(&ui);
                    if (pay_ret == 1)
                    {
                        printf("[存件] 用户已支付，显示成功页面\n");
                        int succ_ret = show_send_success(&ui);
                        if (succ_ret == 1)
                        {
                            printf("[存件] 用户选择继续存件\n");
                            phone[0] = '\0';
                            continue;
                        }
                        printf("[存件] 用户选择返回首页\n");
                    }
                    else
                    {
                        printf("[存件] 用户取消支付或超时，释放储物柜\n");
                        locker_lock();
                        locker->loc_data = LOCKER_EMPTY;
                        memset(locker->locker_getID, 0, sizeof(locker->locker_getID));
                        memset(locker->small_phone, 0, sizeof(locker->small_phone));
                        memset(locker->pickup_token, 0, sizeof(locker->pickup_token));
                        memset(locker->username, 0, sizeof(locker->username));
                        locker_unlock();
                    }

                    break;
                }
            }
            break;

            case 3:
                printf("用户选择: 登录快递员\n");
            {
                int logged_in = 0;

                phone[0] = '\0';

                while (1)
                {
                    if (!logged_in)
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
                            printf("[安全] 验证码错误次数过多，返回主页\n");
                            break;
                        }

                        if (ret == RET_SCAN_PICKUP)
                        {
                            PICKUP_NOTIFY_CLR();
                            printf("[扫码取件] 登录界面检测到验证完成，执行开箱\n");
                            int wc_scan = 0;
                            check_scan_pickup(&ui, &wc_scan);
                            break;
                        }

                        if (ret != 0)
                        {
                            printf("未知登录错误: %d\n", ret);
                            break;
                        }

                        printf("用户登录快递员成功！手机号为: %s\n", phone);
                        logged_in = 1;
                    }

                    char custom_code[5];
                    int box_size = 0;
                    int duration = 0;

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

                    /* 链表分配与写入持锁，防止与扫码监控线程并发读写 */
                    locker_lock();
                    /* 取件码全局查重：碰撞则重新生成，避免取件时弹错柜门 */
                    while (locker_find_by_code(ui_get_locker_head(), custom_code) != NULL)
                    {
                        generate_random_code(custom_code, 4);
                    }
                    locker_node_t *locker = locker_find_first_empty_by_prefix(
                        ui_get_locker_head(), LOCKER_PREFIXES[box_size]);
                    if (locker == NULL)
                    {
                        locker_unlock();
                        printf("没有大小为 %d 的空储物柜！\n", box_size);
                        phone[0] = '\0';
                        break;
                    }

                    locker->loc_data = LOCKER_OCCUPIED;
                    strncpy(locker->locker_getID, custom_code, LOCKER_CODE_LEN - 1);
                    locker->locker_getID[LOCKER_CODE_LEN - 1] = '\0';
                    strncpy(locker->small_phone, phone, PHONE_LEN - 1);
                    locker->small_phone[PHONE_LEN - 1] = '\0';

                    printf("已分配储物柜为: %s, 取件码为: %s\n",
                           locker->locker_ID, custom_code);

                    {
                        char pickup_url[VG_URL_LEN];
                        if (vg_create_pickup(locker->pickup_token, pickup_url) != 0)
                        {
                            printf("[错误] 取件令牌生成失败，释放储物柜\n");
                            locker->loc_data = LOCKER_EMPTY;
                            memset(locker->locker_getID, 0, sizeof(locker->locker_getID));
                            memset(locker->small_phone, 0, sizeof(locker->small_phone));
                            memset(locker->username, 0, sizeof(locker->username));
                            locker_unlock();
                            break;
                        }
                        printf("[存件] 已生成取件令牌: %s\n", locker->pickup_token);
                    }
                    locker_unlock();

                    int succ_ret = show_send_success(&ui);
                    if (succ_ret == 1)
                    {
                        printf("[存件] 快递员选择继续存件\n");
                        phone[0] = '\0';
                        continue;
                    }
                    printf("[存件] 快递员选择返回首页\n");
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

                    if (ret == RET_SCAN_PICKUP)
                    {
                        PICKUP_NOTIFY_CLR();
                        printf("[扫码取件] 查询界面检测到验证完成，执行开箱\n");
                        int wc_query = 0;
                        check_scan_pickup(&ui, &wc_query);
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

            lv_main_menu_create();
        }
    }

    weather_stop();
    ui_stop_pickup_watcher();
    ui_stop(&ui);

    return 0;
}