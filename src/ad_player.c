#include <unistd.h>
#include "ad_player.h"
#include "lcd_ui_display.h"

/**************************************************************************
 *
 *   @brief : 广告播放线程入口
 *   @arg   : arg  指向 ui_context_t 结构体的指针
 *
 *   @retval: NULL
 *   @note  : 每3秒轮换一次广告图片，显示位置 (96,19)
 *
 ***************************************************************************/
static void *ad_player_task(void *arg)
{
    ui_context_t *ui = (ui_context_t *)arg;
    const char *ad_files[] = {"resource/AD_pic/AD1.jpg",
                             "resource/AD_pic/AD2.jpg",
                             "resource/AD_pic/AD3.jpg"};
    int current_ad = 0;
    int ad_count = sizeof(ad_files) / sizeof(ad_files[0]);

    while (ui->ad_running)
    {
        lcd_show_jpg(&ui->lcd, ad_files[current_ad], 96, 19);
        current_ad = (current_ad + 1) % ad_count;

        for (int i = 0; i < 30 && ui->ad_running; i++)
        {
            usleep(100000);
        }
    }

    return NULL;
}

/**************************************************************************
 *
 *   @brief : 启动广告播放线程
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 在主菜单显示时调用
 *
 ***************************************************************************/
int start_ad_player(ui_context_t *ui)
{
    ui->ad_running = 1;
    if (pthread_create(&ui->ad_tid, NULL, ad_player_task, ui) != 0)
    {
        ui->ad_running = 0;
        return -1;
    }
    return 0;
}

/**************************************************************************
 *
 *   @brief : 停止广告播放线程
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 在用户选择后调用，等待线程结束并回收资源
 *
 ***************************************************************************/
int stop_ad_player(ui_context_t *ui)
{
    ui->ad_running = 0;
    if (ui->ad_tid != 0)
    {
        pthread_join(ui->ad_tid, NULL);
        ui->ad_tid = 0;
    }
    return 0;
}