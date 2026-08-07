#include <setjmp.h>
#include <string.h>
#include "lcd_ui_display.h"

static image_cache_t g_image_cache[MAX_CACHE_SIZE];
static int g_cache_initialized = 0;
static int g_cache_counter = 0;

struct my_error_mgr
{
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

/**************************************************************************
 *
 *   @brief : libjpeg 错误处理回调函数
 *   @arg   : cinfo  JPEG 解压上下文指针
 *
 *   @retval: 无
 *   @note  : 通过 longjmp 跳回 setjmp 位置进行错误恢复
 *
 ***************************************************************************/
static void my_error_exit(j_common_ptr cinfo)
{
    struct my_error_mgr *err = (struct my_error_mgr *)cinfo->err;
    longjmp(err->setjmp_buffer, 1);
}

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
int lcd_init(lcd_context_t *lcd)
{
    if (lcd == NULL)
    {
        return -1;
    }

    lcd->fd = open(LCD_PATH, O_RDWR);
    if (lcd->fd < 0)
    {
        perror("lcd_init: open LCD failed");
        return -1;
    }

    lcd->fb = mmap(NULL, LCD_WIDTH * LCD_HEIGHT * 4, PROT_READ | PROT_WRITE, MAP_SHARED, lcd->fd, 0);
    if (lcd->fb == MAP_FAILED)
    {
        perror("lcd_init: mmap failed");
        close(lcd->fd);
        return -1;
    }

    return 0;
}

/**************************************************************************
 *
 *   @brief : 关闭 LCD 屏幕，解除显存映射并关闭设备文件
 *   @arg   : lcd  指向 lcd_context_t 结构体的指针
 *
 *   @retval: 无
 *   @note  : 仅关闭有效资源，可安全重复调用
 *
 ***************************************************************************/
void lcd_close(lcd_context_t *lcd)
{
    if (lcd == NULL)
    {
        return;
    }

    if (lcd->fb != NULL && lcd->fb != MAP_FAILED)
    {
        munmap(lcd->fb, LCD_WIDTH * LCD_HEIGHT * 4);
    }

    if (lcd->fd >= 0)
    {
        close(lcd->fd);
    }
}

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
int lcd_show_jpg(lcd_context_t *lcd, const char *jpg_path, int start_x, int start_y)
{
    struct jpeg_decompress_struct cinfo;
    struct my_error_mgr jerr;
    FILE *infile;
    JSAMPROW buffer;
    int row_stride;
    int x, y;

    if (lcd == NULL || lcd->fb == NULL || jpg_path == NULL)
    {
        return -1;
    }

    infile = fopen(jpg_path, "rb");
    if (infile == NULL)
    {
        fprintf(stderr, "can't open %s\n", jpg_path);
        return -1;
    }

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    if (setjmp(jerr.setjmp_buffer))
    {
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return -1;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (JSAMPROW)calloc(row_stride, 1);
    if (buffer == NULL)
    {
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return -1;
    }

    while (cinfo.output_scanline < cinfo.output_height)
    {
        jpeg_read_scanlines(&cinfo, &buffer, 1);

        y = (int)(cinfo.output_scanline - 1) + start_y;
        for (x = 0; x < (int)cinfo.output_width; x++)
        {
            if (y >= 0 && y < LCD_HEIGHT && (start_x + x) >= 0 && (start_x + x) < LCD_WIDTH)
            {
                unsigned char r = buffer[x * 3 + 0];
                unsigned char g = buffer[x * 3 + 1];
                unsigned char b = buffer[x * 3 + 2];

                unsigned int argb8888 = (0xFF << 24) | (r << 16) | (g << 8) | b;
                lcd->fb[y * LCD_WIDTH + start_x + x] = argb8888;
            }
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    free(buffer);

    return 0;
}

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
int lcd_show_digit(lcd_context_t *lcd, int digit, int x, int y)
{
    if (g_cache_initialized)
    {
        return lcd_cache_show_digit(lcd, digit, x, y);
    }

    {
        char path[64];

        if (lcd == NULL || digit < 0 || digit > 9)
        {
            return -1;
        }

        snprintf(path, sizeof(path), "resource/num_pic/%d.jpg", digit);
        return lcd_show_jpg(lcd, path, x, y);
    }
}

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
int lcd_show_menu(lcd_context_t *lcd, const char *pic_name)
{
    if (g_cache_initialized)
    {
        return lcd_cache_show_menu(lcd, pic_name);
    }

    {
        char path[128];

        if (lcd == NULL || pic_name == NULL)
        {
            return -1;
        }

        snprintf(path, sizeof(path), "resource/menu_pic/jpg/%s", pic_name);
        return lcd_show_jpg(lcd, path, 0, 0);
    }
}

int lcd_show_bnum(lcd_context_t *lcd, int digit, int x, int y)
{
    if (g_cache_initialized)
    {
        return lcd_cache_show_bnum(lcd, digit, x, y);
    }

    {
        char path[64];

        if (lcd == NULL || digit < 0 || digit > 9)
        {
            return -1;
        }

        snprintf(path, sizeof(path), "resource/num_pic/b_num_%d_w.jpg", digit);
        return lcd_show_jpg(lcd, path, x, y);
    }
}

int lcd_show_letter(lcd_context_t *lcd, char letter, int x, int y)
{
    if (g_cache_initialized)
    {
        return lcd_cache_show_letter(lcd, letter, x, y);
    }

    {
        char path[64];

        if (lcd == NULL)
        {
            return -1;
        }

        snprintf(path, sizeof(path), "resource/num_pic/%c_w.jpg", letter);
        return lcd_show_jpg(lcd, path, x, y);
    }
}

static int find_cache_slot(const char *path)
{
    int i;
    int oldest_slot = -1;
    int oldest_time = __INT_MAX__;

    if (!g_cache_initialized)
    {
        return -1;
    }

    for (i = 0; i < MAX_CACHE_SIZE; i++)
    {
        if (g_image_cache[i].is_valid &&
            strcmp(g_image_cache[i].path, path) == 0)
        {
            g_image_cache[i].last_used = ++g_cache_counter;
            return i;
        }
    }

    for (i = 0; i < MAX_CACHE_SIZE; i++)
    {
        if (!g_image_cache[i].is_valid)
        {
            return i;
        }
    }

    for (i = 0; i < MAX_CACHE_SIZE; i++)
    {
        if (g_image_cache[i].last_used < oldest_time)
        {
            oldest_time = g_image_cache[i].last_used;
            oldest_slot = i;
        }
    }

    return oldest_slot;
}

int lcd_cache_init(void)
{
    int i;

    memset(g_image_cache, 0, sizeof(g_image_cache));
    g_cache_initialized = 1;
    g_cache_counter = 0;

    for (i = 0; i < MAX_CACHE_SIZE; i++)
    {
        g_image_cache[i].is_valid = 0;
        g_image_cache[i].pixels = NULL;
        g_image_cache[i].last_used = 0;
    }

    printf("[CACHE] 图片缓存系统初始化完成 (容量: %d张)\n", MAX_CACHE_SIZE);
    return 0;
}

void lcd_cache_cleanup(void)
{
    int i;

    if (!g_cache_initialized)
    {
        return;
    }

    for (i = 0; i < MAX_CACHE_SIZE; i++)
    {
        if (g_image_cache[i].is_valid && g_image_cache[i].pixels != NULL)
        {
            free(g_image_cache[i].pixels);
            g_image_cache[i].pixels = NULL;
            g_image_cache[i].is_valid = 0;
        }
    }

    g_cache_initialized = 0;
    printf("[CACHE] 图片缓存已清理\n");
}

static unsigned int *decode_jpeg_to_memory(const char *jpg_path, int *out_width, int *out_height)
{
    struct jpeg_decompress_struct cinfo;
    struct my_error_mgr jerr;
    FILE *infile;
    JSAMPROW buffer;
    int row_stride;
    unsigned int *pixel_buffer = NULL;
    int x, y;

    infile = fopen(jpg_path, "rb");
    if (infile == NULL)
    {
        return NULL;
    }

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    if (setjmp(jerr.setjmp_buffer))
    {
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        if (pixel_buffer) free(pixel_buffer);
        return NULL;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    *out_width = cinfo.output_width;
    *out_height = cinfo.output_height;

    pixel_buffer = (unsigned int *)malloc(cinfo.output_width * cinfo.output_height * sizeof(unsigned int));
    if (pixel_buffer == NULL)
    {
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return NULL;
    }

    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (JSAMPROW)calloc(row_stride, 1);
    if (buffer == NULL)
    {
        free(pixel_buffer);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return NULL;
    }

    while (cinfo.output_scanline < cinfo.output_height)
    {
        jpeg_read_scanlines(&cinfo, &buffer, 1);

        y = (int)(cinfo.output_scanline - 1);
        for (x = 0; x < (int)cinfo.output_width; x++)
        {
            unsigned char r = buffer[x * 3 + 0];
            unsigned char g = buffer[x * 3 + 1];
            unsigned char b = buffer[x * 3 + 2];

            pixel_buffer[y * cinfo.output_width + x] =
                (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    free(buffer);

    return pixel_buffer;
}

int lcd_cache_load(const char *jpg_path)
{
    int slot;
    unsigned int *pixels;
    int width, height;

    if (!g_cache_initialized || jpg_path == NULL)
    {
        return -1;
    }

    slot = find_cache_slot(jpg_path);
    if (slot >= 0 && g_image_cache[slot].is_valid &&
        strcmp(g_image_cache[slot].path, jpg_path) == 0)
    {
        return 0;
    }

    if (slot < 0)
    {
        return -1;
    }

    if (g_image_cache[slot].is_valid && g_image_cache[slot].pixels != NULL)
    {
        free(g_image_cache[slot].pixels);
        g_image_cache[slot].pixels = NULL;
        g_image_cache[slot].is_valid = 0;
    }

    pixels = decode_jpeg_to_memory(jpg_path, &width, &height);
    if (pixels == NULL)
    {
        return -1;
    }

    strncpy(g_image_cache[slot].path, jpg_path, sizeof(g_image_cache[slot].path) - 1);
    g_image_cache[slot].path[sizeof(g_image_cache[slot].path) - 1] = '\0';
    g_image_cache[slot].pixels = pixels;
    g_image_cache[slot].width = width;
    g_image_cache[slot].height = height;
    g_image_cache[slot].is_valid = 1;
    g_image_cache[slot].last_used = ++g_cache_counter;

    return 0;
}

static void fast_blit_to_lcd(lcd_context_t *lcd, const unsigned int *pixels,
                              int img_w, int img_h, int start_x, int start_y)
{
    int y, copy_width, copy_height;
    int src_offset, dst_offset;

    if (start_x >= LCD_WIDTH || start_y >= LCD_HEIGHT)
    {
        return;
    }

    copy_width = img_w;
    if (start_x + copy_width > LCD_WIDTH)
    {
        copy_width = LCD_WIDTH - start_x;
    }

    copy_height = img_h;
    if (start_y + copy_height > LCD_HEIGHT)
    {
        copy_height = LCD_HEIGHT - start_y;
    }

    if (start_x < 0)
    {
        copy_width += start_x;
        start_x = 0;
    }

    if (start_y < 0)
    {
        copy_height += start_y;
        start_y = 0;
    }

    if (copy_width <= 0 || copy_height <= 0)
    {
        return;
    }

    for (y = 0; y < copy_height; y++)
    {
        src_offset = y * img_w;
        dst_offset = (start_y + y) * LCD_WIDTH + start_x;
        memcpy(&lcd->fb[dst_offset], &pixels[src_offset], copy_width * sizeof(unsigned int));
    }
}

int lcd_cache_show(lcd_context_t *lcd, const char *jpg_path, int start_x, int start_y)
{
    int slot;

    if (lcd == NULL || lcd->fb == NULL || jpg_path == NULL)
    {
        return -1;
    }

    if (!g_cache_initialized)
    {
        return lcd_show_jpg(lcd, jpg_path, start_x, start_y);
    }

    slot = find_cache_slot(jpg_path);
    if (slot < 0 || !g_image_cache[slot].is_valid)
    {
        if (lcd_cache_load(jpg_path) != 0)
        {
            return lcd_show_jpg(lcd, jpg_path, start_x, start_y);
        }
        slot = find_cache_slot(jpg_path);
    }

    if (slot >= 0 && g_image_cache[slot].is_valid)
    {
        fast_blit_to_lcd(lcd, g_image_cache[slot].pixels,
                         g_image_cache[slot].width,
                         g_image_cache[slot].height,
                         start_x, start_y);
        return 0;
    }

    return -1;
}

int lcd_cache_show_menu(lcd_context_t *lcd, const char *pic_name)
{
    char path[128];

    if (lcd == NULL || pic_name == NULL)
    {
        return -1;
    }

    snprintf(path, sizeof(path), "resource/menu_pic/jpg/%s", pic_name);
    return lcd_cache_show(lcd, path, 0, 0);
}

int lcd_cache_show_digit(lcd_context_t *lcd, int digit, int x, int y)
{
    char path[64];

    if (lcd == NULL || digit < 0 || digit > 9)
    {
        return -1;
    }

    snprintf(path, sizeof(path), "resource/num_pic/%d.jpg", digit);
    return lcd_cache_show(lcd, path, x, y);
}

int lcd_cache_show_bnum(lcd_context_t *lcd, int digit, int x, int y)
{
    char path[64];

    if (lcd == NULL || digit < 0 || digit > 9)
    {
        return -1;
    }

    snprintf(path, sizeof(path), "resource/num_pic/b_num_%d_w.jpg", digit);
    return lcd_cache_show(lcd, path, x, y);
}

int lcd_cache_show_letter(lcd_context_t *lcd, char letter, int x, int y)
{
    char path[64];

    if (lcd == NULL)
    {
        return -1;
    }

    snprintf(path, sizeof(path), "resource/num_pic/%c_w.jpg", letter);
    return lcd_cache_show(lcd, path, x, y);
}

void lcd_preload_common_images(void)
{
    const char *common_images[] = {
        "resource/menu_pic/jpg/main_menu.jpg",
        "resource/menu_pic/jpg/takeout_code.jpg",
        "resource/menu_pic/jpg/save_info_set.jpg",
        "resource/menu_pic/jpg/sendman_login.jpg",
        "resource/menu_pic/jpg/saveuser_login.jpg",
        "resource/menu_pic/jpg/received.jpg",
        "resource/menu_pic/jpg/pay_info.jpg",
        "resource/menu_pic/jpg/send_success.jpg",
        "resource/menu_pic/jpg/takeout_success.jpg",
        "resource/menu_pic/jpg/keyboard.jpg",
        "resource/menu_pic/jpg/havent_box_.jpg",
        NULL
    };
    int i;
    char num_path[64];

    if (!g_cache_initialized)
    {
        return;
    }

    printf("[CACHE] 开始预加载常用图片...\n");

    for (i = 0; common_images[i] != NULL; i++)
    {
        if (lcd_cache_load(common_images[i]) == 0)
        {
            printf("[CACHE] 预加载: %s ✓\n", common_images[i]);
        }
        else
        {
            printf("[CACHE] 预加载失败: %s ✗\n", common_images[i]);
        }
    }

    for (i = 0; i <= 9; i++)
    {
        snprintf(num_path, sizeof(num_path), "resource/num_pic/%d.jpg", i);
        lcd_cache_load(num_path);

        snprintf(num_path, sizeof(num_path), "resource/num_pic/b_num_%d_w.jpg", i);
        lcd_cache_load(num_path);
    }

    lcd_cache_load("resource/num_pic/A_w.jpg");
    lcd_cache_load("resource/num_pic/B_w.jpg");
    lcd_cache_load("resource/num_pic/C_w.jpg");

    printf("[CACHE] 常用图片预加载完成\n");
}