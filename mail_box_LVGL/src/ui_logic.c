#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "ui_logic.h"
#include "utils.h"
#include "config.h"
#include "lvgl.h"
#include "pickup_monitor.h"
#include "lvgl_overlay_page.h"

static locker_node_t *g_locker_head = NULL;

const char *LOCKER_PREFIXES[4] = {"", "A", "B", "C"};

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

void safe_strcpy(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0) return;
    if (src == NULL)
    {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

int ui_init(ui_context_t *ui)
{
    if (ui == NULL)
    {
        return -1;
    }

    if (lcd_init(&ui->lcd) != 0)
    {
        return -1;
    }

    if (locker_init_all(&g_locker_head) != 0)
    {
        lcd_close(&ui->lcd);
        return -1;
    }

    return 0;
}

int ui_start(ui_context_t *ui)
{
    /* 触摸输入统一由 LVGL evdev 驱动消费，无需额外启动线程 */
    return (ui != NULL) ? 0 : -1;
}

int ui_stop(ui_context_t *ui)
{
    if (ui == NULL)
    {
        return -1;
    }

    /* 必须先停止扫码监控线程，再释放链表，避免监控线程访问已释放节点 */
    ui_stop_pickup_watcher();

    lcd_close(&ui->lcd);

    locker_lock();
    locker_free_list(&g_locker_head);
    locker_unlock();

    return 0;
}

locker_node_t *ui_get_locker_head(void)
{
    return g_locker_head;
}

int ui_wait_overlay(ui_context_t *ui, void (*create_fn)(void),
                    void (*destroy_fn)(void), const char *page_name)
{
    time_t start_time;

    if (ui == NULL || create_fn == NULL || destroy_fn == NULL)
    {
        return 0;
    }

    lvgl_overlay_reset();
    create_fn();
    start_time = time(NULL);

    while (1)
    {
        lv_timer_handler();
        usleep(5000);

        if ((time(NULL) - start_time) >= PAGE_TIMEOUT_SEC)
        {
            printf("[超时] %s 显示超时(%d秒)，返回主页\n", page_name, PAGE_TIMEOUT_SEC);
            destroy_fn();
            return 0;
        }

        if (PICKUP_NOTIFY_GET())
        {
            printf("[扫码取件] %s 检测到扫码通知，中断\n", page_name);
            destroy_fn();
            return 0;
        }

        int result = lvgl_overlay_get_result();
        if (result == 1)
        {
            destroy_fn();
            return 1;
        }
        if (result == 0)
        {
            destroy_fn();
            return 0;
        }
    }
}