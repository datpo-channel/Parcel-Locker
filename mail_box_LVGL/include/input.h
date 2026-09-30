#ifndef INPUT_H
#define INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __linux__
#include <linux/input.h>
#include <sys/time.h>
#else
/* Windows 交叉开发环境下的兼容性定义（加守卫避免与其它头冲突） */

#ifndef EV_ABS
#define EV_ABS 0x03
#endif

#ifndef ABS_X
#define ABS_X 0x00
#define ABS_Y 0x01
#endif

struct input_event
{
    unsigned long tv_sec;
    unsigned long tv_usec;
    unsigned short type;
    unsigned short code;
    int value;
};

#endif

#ifndef LV_DRV_NO_CONF
#ifdef LV_CONF_INCLUDE_SIMPLE
#include "lv_drv_conf.h"
#else
#include "../../lv_drv_conf.h"
#endif
#endif

/* ==================== LVGL evdev 输入驱动接口 ==================== */

#if USE_EVDEV || USE_BSD_EVDEV

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/**
 * Initialize the evdev
 */
void evdev_init(void);
/**
 * reconfigure the device file for evdev
 * @param dev_name set the evdev device filename
 * @return true: the device file set complete
 *         false: the device file doesn't exist current system
 */
bool evdev_set_file(char* dev_name);
/**
 * Get the current position and state of the evdev
 * @param data store the evdev data here
 */
void evdev_read(lv_indev_drv_t * drv, lv_indev_data_t * data);

/**
 * 页面切换时清空触摸输入：重置 LVGL 输入状态、排空内核 evdev 队列、
 * 复位驱动侧按压标志，防止上一页残留的按压/坐标事件传递到新页面。
 * 必须在每个页面创建(create)时调用。
 */
void evdev_reset_for_page_switch(void);

#endif /* USE_EVDEV */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* INPUT_H */
