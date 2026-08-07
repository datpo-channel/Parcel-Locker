#include "weather_utils.h"
#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#define FONT_PATH           "/font/wqy.ttc"
#define WEATHER_CITY        "Guangzhou"

static FT_Library  ft_lib = NULL;
static FT_Face     ft_face = NULL;
static int         ft_ready = 0;

static weather_info_t g_weather_info;
static time_t         g_last_weather = 0;

static void ft_set_size(int size)
{
    FT_Set_Pixel_Sizes(ft_face, 0, size);
}

static void ft_draw_text_on_lcd(lcd_context_t *lcd, int x, int y,
                                 const char *text, uint32_t color, int font_size)
{
    int i, j, sy;
    FT_GlyphSlot slot;
    const wchar_t *wstr;
    wchar_t wbuf[256];
    int len;

    if (!ft_ready || lcd == NULL || lcd->fb == NULL)
        return;

    len = mbstowcs(wbuf, text, 256);
    if (len <= 0) return;
    wstr = wbuf;

    ft_set_size(font_size);
    slot = ft_face->glyph;

    sy = y;
    for (i = 0; i < len; i++) {
        if (FT_Load_Char(ft_face, wstr[i], FT_LOAD_RENDER))
            continue;

        for (j = 0; j < (int)slot->bitmap.rows; j++) {
            for (int k = 0; k < (int)slot->bitmap.width; k++) {
                unsigned char val = slot->bitmap.buffer[j * slot->bitmap.width + k];
                int rx = x - slot->bitmap_top + j;
                int ry = sy - slot->bitmap_left - k;

                if (rx < 0 || rx >= LCD_WIDTH || ry < 0 || ry >= LCD_HEIGHT)
                    continue;

                if (val > 128) {
                    lcd->fb[ry * LCD_WIDTH + rx] = color;
                } else if (val > 64) {
                    uint32_t bg = lcd->fb[ry * LCD_WIDTH + rx];
                    uint32_t r = ((color >> 16) & 0xFF) * val / 255;
                    uint32_t g = ((color >> 8)  & 0xFF) * val / 255;
                    uint32_t b = (color & 0xFF) * val / 255;
                    uint32_t br = ((bg >> 16) & 0xFF) * (255 - val) / 255;
                    uint32_t bg_ = ((bg >> 8)  & 0xFF) * (255 - val) / 255;
                    uint32_t bb = (bg & 0xFF) * (255 - val) / 255;
                    lcd->fb[ry * LCD_WIDTH + rx] =
                        ((r + br) << 16) | ((g + bg_) << 8) | (b + bb);
                }
            }
        }
        sy -= (font_size * 0.6);
    }
}

static int json_get_str(const char *json, const char *key, char *out, int out_size)
{
    char search[128];
    const char *p, *end;
    int len;

    snprintf(search, sizeof(search), "\"%s\":\"", key);
    p = strstr(json, search);
    if (!p) return -1;

    p += strlen(search);
    end = strchr(p, '"');
    if (!end) return -1;

    len = end - p;
    if (len >= out_size) len = out_size - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return 0;
}

static int weather_fetch(weather_info_t *info, const char *city)
{
    char path[256];
    char buf[HTTP_BUF_SIZE];
    int ret;

    if (!info || !city) return -1;
    memset(info, 0, sizeof(*info));

    snprintf(path, sizeof(path), "/%s?format=j1", city);
    ret = http_get(WEATHER_HOST, WEATHER_PORT, path, buf, sizeof(buf));
    if (ret < 0) return -1;

    json_get_str(buf, "temp_C", info->temp, sizeof(info->temp));
    json_get_str(buf, "FeelsLikeC", info->feels_like, sizeof(info->feels_like));
    json_get_str(buf, "humidity", info->humidity, sizeof(info->humidity));

    {
        const char *desc = strstr(buf, "\"weatherDesc\"");
        if (desc) {
            desc = strstr(desc, "\"value\"");
            if (desc) {
                desc = strchr(desc + 7, '"');
                if (desc) {
                    desc++;
                    const char *end = strchr(desc, '"');
                    int len = end ? (end - desc) : 0;
                    if (len > 0 && len < WEATHER_DESC_MAX) {
                        memcpy(info->weather_desc, desc, len);
                        info->weather_desc[len] = '\0';
                    }
                }
            }
        }
    }

    {
        const char *area = strstr(buf, "\"nearest_area\"");
        if (area) {
            area = strstr(area, "\"region\"");
            if (area) {
                area = strstr(area, "\"value\"");
                if (area) {
                    area = strchr(area + 7, '"');
                    if (area) {
                        area++;
                        const char *end = strchr(area, '"');
                        int len = end ? (end - area) : 0;
                        if (len > 0 && len < WEATHER_CITY_MAX) {
                            memcpy(info->city, area, len);
                            info->city[len] = '\0';
                        }
                    }
                }
            }
        }
    }

    if (info->temp[0] == '\0' && info->weather_desc[0] == '\0') {
        return -1;
    }

    info->valid = 1;
    return 0;
}

int weather_time_init(void)
{
    if (FT_Init_FreeType(&ft_lib)) {
        fprintf(stderr, "FT_Init_FreeType failed\n");
        return -1;
    }
    if (FT_New_Face(ft_lib, FONT_PATH, 0, &ft_face)) {
        fprintf(stderr, "FT_New_Face failed: %s\n", FONT_PATH);
        FT_Done_FreeType(ft_lib);
        ft_lib = NULL;
        return -1;
    }
    FT_Select_Charmap(ft_face, FT_ENCODING_UNICODE);
    ft_ready = 1;

    memset(&g_weather_info, 0, sizeof(g_weather_info));
    return 0;
}

void weather_time_draw(lcd_context_t *lcd, int x, int y)
{
    time_t now;
    struct tm *tm_info;
    char time_str[32];
    const char *weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};

    if (!ft_ready || lcd == NULL || lcd->fb == NULL)
        return;

    time(&now);
    now += 8 * 3600;
    tm_info = gmtime(&now);

    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    ft_draw_text_on_lcd(lcd, x, y, time_str, COLOR_WHITE, FONT_SIZE);

    strftime(time_str, sizeof(time_str), "%Y-%m-%d", tm_info);
    ft_draw_text_on_lcd(lcd, x + TEXT_GAP, y, time_str, COLOR_WHITE, FONT_SIZE);

    ft_draw_text_on_lcd(lcd, x + TEXT_GAP * 2, y,
                         weekdays[tm_info->tm_wday], COLOR_WHITE, FONT_SIZE);

    if (now - g_last_weather >= WEATHER_INTERVAL || !g_weather_info.valid) {
        g_last_weather = now;

        if (weather_fetch(&g_weather_info, WEATHER_CITY) == 0) {
            ft_draw_text_on_lcd(lcd, x + TEXT_GAP * 3, y,
                                 g_weather_info.city, COLOR_WHITE, FONT_SIZE);
            ft_draw_text_on_lcd(lcd, x + TEXT_GAP * 4, y,
                                 g_weather_info.weather_desc, COLOR_ACCENT, FONT_SIZE);
        } else {
            ft_draw_text_on_lcd(lcd, x + TEXT_GAP * 3, y,
                                 "获取失败", COLOR_GRAY, FONT_SIZE);
        }
    } else if (g_weather_info.valid) {
        ft_draw_text_on_lcd(lcd, x + TEXT_GAP * 3, y,
                             g_weather_info.city, COLOR_WHITE, FONT_SIZE);
        ft_draw_text_on_lcd(lcd, x + TEXT_GAP * 4, y,
                             g_weather_info.weather_desc, COLOR_ACCENT, FONT_SIZE);
    }
}

void weather_time_cleanup(void)
{
    if (ft_face) {
        FT_Done_Face(ft_face);
        ft_face = NULL;
    }
    if (ft_lib) {
        FT_Done_FreeType(ft_lib);
        ft_lib = NULL;
    }
    ft_ready = 0;
}