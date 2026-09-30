#ifndef LCD_UI_DISPLAY_H_
#define LCD_UI_DISPLAY_H_

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <jpeglib.h>

#define LCD_WIDTH       800
#define LCD_HEIGHT      480
#define LCD_BPP         32
#define LCD_PATH        "/dev/fb0"

#define MAX_CACHE_SIZE  50

typedef struct
{
    int fd;
    unsigned int *fb;
} lcd_context_t;

typedef struct
{
    char path[256];
    unsigned int *pixels;
    int width;
    int height;
    int is_valid;
    int last_used;
} image_cache_t;

/**************************************************************************
 *
 *   @brief : 初始化 LCD 屏幕，打开帧缓冲设备并映射显存
 *   @arg   : lcd  指向 lcd_context_t 结构体的指针
 *
 *   @retval: 成功返回 0
 *            失败返回 -1
 *   @note  : 调用者负责最终调用 lcd_close 释放资源
 *
 ***************************************************************************/
int lcd_init(lcd_context_t *lcd);

/**************************************************************************
 *
 *   @brief : 关闭 LCD 屏幕，解除显存映射并关闭设备文件
 *   @arg   : lcd  指向 lcd_context_t 结构体的指针
 *
 *   @retval: 无
 *   @note  : 仅关闭有效资源，可安全重复调用
 *
 ***************************************************************************/
void lcd_close(lcd_context_t *lcd);

/**************************************************************************
 *
 *   @brief : 解码 JPEG 图片并显示到 LCD 指定位置
 *   @arg   : lcd       指向 lcd_context_t 结构体的指针
 *   @arg   : jpg_path  JPEG 图片文件路径
 *   @arg   : start_x   图片左上角 X 坐标
 *   @arg   : start_y   图片左上角 Y 坐标
 *
 *   @retval: 成功返回 0
 *            失败返回 -1
 *   @note  : 使用 libjpeg 解码，自动处理 RGB888 颜色转换
 *
 ***************************************************************************/
int lcd_show_jpg(lcd_context_t *lcd, const char *jpg_path, int start_x, int start_y);

/**************************************************************************
 *
 *   @brief : 在指定位置显示单个数字图片(0-9)
 *   @arg   : lcd    指向 lcd_context_t 结构体的指针
 *   @arg   : digit  要显示的数字 (0-9)
 *   @arg   : x      数字图片左上角 X 坐标
 *   @arg   : y      数字图片左上角 Y 坐标
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 数字图片路径为 resource/num_pic/X.jpg，大小 16x11
 *
 ***************************************************************************/
int lcd_show_digit(lcd_context_t *lcd, int digit, int x, int y);

/**************************************************************************
 *
 *   @brief : 显示菜单背景图片
 *   @arg   : lcd       指向 lcd_context_t 结构体的指针
 *   @arg   : pic_name  菜单图片文件名(位于 resource/menu_pic/jpg/)
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 图片全屏显示 (0,0)
 *
 ***************************************************************************/
int lcd_show_menu(lcd_context_t *lcd, const char *pic_name);

/**************************************************************************
 *
 *   @brief : 显示 b_num 系列数字图片 (22x18)
 *   @arg   : lcd    指向 lcd_context_t 结构体的指针
 *   @arg   : digit  要显示的数字 (0-9)
 *   @arg   : x      图片左上角 X 坐标
 *   @arg   : y      图片左上角 Y 坐标
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 图片路径 resource/b_num_pic/X.jpg，大小 22x18
 *
 ***************************************************************************/
int lcd_show_bnum(lcd_context_t *lcd, int digit, int x, int y);

/**************************************************************************
 *
 *   @brief : 显示字母图片 (22x18)
 *   @arg   : lcd     指向 lcd_context_t 结构体的指针
 *   @arg   : letter  字母字符 ('A','B','C')
 *   @arg   : x       图片左上角 X 坐标
 *   @arg   : y       图片左上角 Y 坐标
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 图片路径 resource/letter_pic/X.jpg，大小 22x18
 *
 ***************************************************************************/
int lcd_show_letter(lcd_context_t *lcd, char letter, int x, int y);

int lcd_cache_init(void);
void lcd_cache_cleanup(void);
int lcd_cache_load(const char *jpg_path);
int lcd_cache_show(lcd_context_t *lcd, const char *jpg_path, int start_x, int start_y);
int lcd_cache_show_menu(lcd_context_t *lcd, const char *pic_name);
int lcd_cache_show_digit(lcd_context_t *lcd, int digit, int x, int y);
int lcd_cache_show_bnum(lcd_context_t *lcd, int digit, int x, int y);
int lcd_cache_show_letter(lcd_context_t *lcd, char letter, int x, int y);
void lcd_preload_common_images(void);

#endif