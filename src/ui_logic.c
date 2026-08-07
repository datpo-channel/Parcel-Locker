#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "ui_logic.h"
#include "utils.h"
#include "ad_player.h"
#include "pickup_monitor.h"

static locker_node_t *g_locker_head = NULL;
static user_node_t   *g_user_head   = NULL;

/**************************************************************************
 *
 *   @brief : 生成随机验证码字符串
 *   @arg   : code    输出缓冲区
 *   @arg   : length  验证码长度 (1-6)
 *
 *   @retval: 无
 *
 ***************************************************************************/
void generate_random_code(char *code, int length)
{
    if (code == NULL || length <= 0 || length > 6)
    {
        return;
    }

    for (int i = 0; i < length; i++)
    {
        code[i] = '0' + (rand() % 10);
    }
    code[length] = '\0';
}

/**************************************************************************
 *
 *   @brief : 初始化 UI 上下文
 *   @arg   : ui       指向 ui_context_t 结构体的指针
 *   @arg   : touch_fd 触摸屏设备文件描述符
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 初始化 LCD 和触摸屏上下文
 *
 ***************************************************************************/
int ui_init(ui_context_t *ui, int touch_fd)
{
    if (ui == NULL || touch_fd < 0)
    {
        return -1;
    }

    if (lcd_init(&ui->lcd) != 0)
    {
        return -1;
    }

    if (touchpad_init(&ui->touch, touch_fd) != 0)
    {
        lcd_close(&ui->lcd);
        return -1;
    }

    ui->current_page = 0;
    ui->ad_running = 0;
    ui->ad_tid = 0;

    if (pthread_mutex_init(&ui->mutex, NULL) != 0)
    {
        lcd_close(&ui->lcd);
        touchpad_stop(&ui->touch);
        return -1;
    }

    if (locker_init_all(&g_locker_head, LOCKER_TOTAL) != 0)
    {
        lcd_close(&ui->lcd);
        touchpad_stop(&ui->touch);
        pthread_mutex_destroy(&ui->mutex);
        return -1;
    }

    user_load_from_file(&g_user_head, NULL);

    return 0;
}

/**************************************************************************
 *
 *   @brief : 启动 UI 系统
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 启动触摸屏轮询线程
 *
 ***************************************************************************/
int ui_start(ui_context_t *ui)
{
    if (ui == NULL)
    {
        return -1;
    }

    return touchpad_start(&ui->touch);
}

/**************************************************************************
 *
 *   @brief : 停止 UI 系统
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 停止触摸屏线程并释放资源
 *
 ***************************************************************************/
int ui_stop(ui_context_t *ui)
{
    if (ui == NULL)
    {
        return -1;
    }

    stop_ad_player(ui);
    touchpad_stop(&ui->touch);
    lcd_close(&ui->lcd);
    pthread_mutex_destroy(&ui->mutex);

    user_save_to_file(g_user_head, NULL);
    locker_free_list(&g_locker_head);
    user_free_list(&g_user_head);

    return 0;
}

/**************************************************************************
 *
 *   @brief : 显示主菜单界面，等待用户触摸选择
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *
 *   @retval: 1  取件
 *            2  存物
 *            3  快递员登录
 *            4  查询
 *   @note  : 阻塞等待，触摸对应区域后返回，同时播放广告
 *
 ***************************************************************************/
int show_main_menu(ui_context_t *ui)
{
    int ts_x, ts_y;
    int ret = 0;

    if (ui == NULL)
    {
        return -1;
    }

    lcd_show_menu(&ui->lcd, "main_menu.jpg");
    start_ad_player(ui);

    while (1)
    {
        if (g_pickup_notify_flag)
        {
            stop_ad_player(ui);
            return RET_SCAN_PICKUP;
        }

        if (touchpad_get_coord(&ui->touch, &ts_x, &ts_y) == 0)
        {
            if (ts_x >= 282 && ts_x <= 479 && ts_y >= 251 && ts_y <= 437)
            {
                stop_ad_player(ui);
                lcd_show_menu(&ui->lcd, "takeout_code.jpg");
                return 1;
            }
            else if (ts_x >= 282 && ts_x <= 479 && ts_y >= 43 && ts_y <= 228)
            {
                stop_ad_player(ui);
                lcd_show_menu(&ui->lcd, "save_info_set.jpg");
                return 2;
            }
            else if (ts_x >= 501 && ts_x <= 700 && ts_y >= 251 && ts_y <= 437)
            {
                stop_ad_player(ui);
                lcd_show_menu(&ui->lcd, "sendman_login.jpg");
                return 3;
            }
            else if (ts_x >= 501 && ts_x <= 700 && ts_y >= 43 && ts_y <= 228)
            {
                stop_ad_player(ui);
                return 4;
            }
        }
        usleep(50000);
    }

    return ret;
}

/**************************************************************************
 *
 *   @brief : 获取储物柜链表头指针
 *
 *   @retval: 储物柜链表头指针
 *   @note  : 用于外部模块查询储物柜信息
 *
 ***************************************************************************/
locker_node_t *ui_get_locker_head(void)
{
    return g_locker_head;
}

/**************************************************************************
 *
 *   @brief : 获取用户链表头指针
 *
 *   @retval: 用户链表头指针
 *   @note  : 用于外部模块查询用户信息
 *
 ***************************************************************************/
user_node_t *ui_get_user_head(void)
{
    return g_user_head;
}

/**************************************************************************
 *
 *   @brief : 生成4位随机取件码，确保不与已有取件码重复
 *   @arg   : code  输出缓冲区，需至少5字节
 *
 *   @retval: 无
 *   @note  : 取件码范围 1000-9999，最多尝试100次避免死循环
 *
 ***************************************************************************/
void generate_pickup_code(char *code)
{
    int attempts = 0;

    while (attempts < 100)
    {
        int num = 1000 + rand() % 9000;
        snprintf(code, LOCKER_CODE_LEN, "%04d", num);

        if (locker_find_by_code(g_locker_head, code) == NULL)
        {
            return;
        }
        attempts++;
    }

    snprintf(code, LOCKER_CODE_LEN, "%04d", 1000 + rand() % 9000);
}