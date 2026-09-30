#ifndef _WEATHER_UTILS_H_
#define _WEATHER_UTILS_H_

#include <stdint.h>
#include "lcd_ui_display.h"

#define FONT_SIZE           17// 字体大小
#define FONT_LINE_SPACING   6// 字体间距
#define TEXT_GAP            15// 文本间距

#define WEATHER_CITY_MAX    64// 城市名称最大长度
#define WEATHER_DESC_MAX    64// 天气描述最大长度

#define COLOR_WHITE         0x00FFFFFF// 白色颜色
#define COLOR_GRAY          0x00888888// 灰色颜色
#define COLOR_ACCENT        0x0000BFFF// 青色颜色

#define WEATHER_INTERVAL    30// 天气更新间隔，单位秒

typedef struct {
    char city[WEATHER_CITY_MAX];// 城市名称
    char temp[16];// 温度
    char feels_like[16];// 感觉温度
    char humidity[16];// 湿度
    char weather_desc[WEATHER_DESC_MAX];// 天气描述
    int valid;// 是否有效
} weather_info_t;

int weather_time_init(void);
void weather_time_draw(lcd_context_t *lcd, int x, int y);
void weather_time_cleanup(void);

#endif