#ifndef DISPLAY_H
#define DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#ifndef LV_DRV_NO_CONF
#ifdef LV_CONF_INCLUDE_SIMPLE
#include "lv_drv_conf.h"
#else
#include "../../lv_drv_conf.h"
#endif
#endif

/* ==================== LCD framebuffer 初始化接口 ==================== */

#ifndef LCD_WIDTH
#define LCD_WIDTH       800
#define LCD_HEIGHT      480
#endif
#define LCD_BPP         16
#define LCD_PATH        "/dev/fb0"

typedef struct
{
    int fd;
    unsigned int *fb;
} lcd_context_t;

int lcd_init(lcd_context_t *lcd);
void lcd_close(lcd_context_t *lcd);

/* ==================== LVGL framebuffer 显示驱动接口 ==================== */

#if USE_FBDEV || USE_BSD_FBDEV

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/**
 * Initialize the framebuffer device
 */
void fbdev_init(void);

/**
 * Flush a buffer to the marked area
 * @param drv pointer to driver where this function belongs
 * @param area an area where to copy `color_p`
 * @param color_p an array of pixels to copy to the `area` part of the screen
 */
void fbdev_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_p);

#endif /* USE_FBDEV || USE_BSD_FBDEV */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* DISPLAY_H */
