#include "weather_utils.h"
#include "net_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define WEATHER_CITY        "Guangzhou"

static weather_info_t g_weather_info;

static pthread_t       g_weather_tid = 0;
static volatile int    g_weather_running = 0;
static pthread_mutex_t g_weather_mutex = PTHREAD_MUTEX_INITIALIZER;

static int json_get_str(const char *json, const char *key, char *out, int out_size)
{
    char pattern[128];
    const char *p, *start, *end;
    int len;

    if (json == NULL || key == NULL || out == NULL || out_size <= 1)
        return -1;

    /* 匹配 "key" 后跳过空格/冒号再取引号值，
     * 兼容 wttr.in 返回的 "key": "value"（冒号后带空格）格式 */
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (!p) return -1;

    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == ':')
        p++;
    if (*p != '"') return -1;
    p++;

    start = p;
    end = strchr(p, '"');
    if (!end) return -1;

    len = end - start;
    if (len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return 0;
}

/* wttr.in(Yahoo 天气码) → 中文描述。
 * j1 的 weatherDesc 始终为英文（lang=zh 对 JSON 格式不生效），
 * 故用稳定的 weatherCode 在本地映射，配合 msyh.ttc 渲染中文 */
static const char *weather_code_to_zh(const char *code)
{
    static const struct { const char *code; const char *zh; } MAP[] = {
        {"113", "晴"},      {"116", "多云"},   {"119", "阴云"},   {"122", "阴"},
        {"143", "雾"},      {"248", "雾"},     {"260", "冻雾"},   {"149", "霾"},
        {"176", "零星小雨"}, {"263", "零星小雨"}, {"266", "小雨"},
        {"281", "冻毛雨"},  {"284", "强冻雨"}, {"293", "零星小雨"},
        {"296", "小雨"},    {"299", "中雨"},   {"302", "中雨"},
        {"305", "大雨"},    {"308", "大雨"},   {"311", "冻雨"},   {"314", "强冻雨"},
        {"317", "零星雨夹雪"}, {"320", "雨夹雪"},
        {"353", "阵雨"},    {"356", "阵雨"},   {"359", "强阵雨"},
        {"200", "雷雨"},    {"386", "雷阵雨"}, {"389", "雷阵雨"},
        {"227", "吹雪"},    {"230", "暴雪"},   {"323", "零星小雪"},
        {"326", "小雪"},    {"329", "小到中雪"}, {"332", "中雪"},
        {"335", "大雪"},    {"338", "大雪"},   {"350", "冰粒"},
        {"362", "雨夹雪阵"}, {"365", "雨夹雪阵"}, {"368", "小雪阵"},
        {"371", "中雪阵"},  {"374", "冰粒阵"}, {"377", "冰粒阵"},
        {"392", "雷雪"},    {"395", "雷雪"},
    };
    size_t i;

    for (i = 0; i < sizeof(MAP) / sizeof(MAP[0]); i++)
    {
        if (strcmp(MAP[i].code, code) == 0) return MAP[i].zh;
    }
    return NULL;
}

/* 省级行政区英文名 → 中文（wttr.in region 字段恒为英文，本地映射显示中文；
 * 未收录的地区回退英文原文） */
static const char *region_to_zh(const char *region)
{
    static const struct { const char *en; const char *zh; } MAP[] = {
        {"Guangdong", "广东"}, {"Guangxi", "广西"}, {"Guangzhou", "广州"},
        {"Beijing", "北京"}, {"Shanghai", "上海"}, {"Tianjin", "天津"},
        {"Chongqing", "重庆"}, {"Jiangsu", "江苏"}, {"Zhejiang", "浙江"},
        {"Fujian", "福建"}, {"Hunan", "湖南"}, {"Hubei", "湖北"},
        {"Sichuan", "四川"}, {"Yunnan", "云南"}, {"Guizhou", "贵州"},
        {"Shandong", "山东"}, {"Henan", "河南"}, {"Hebei", "河北"},
        {"Anhui", "安徽"}, {"Jiangxi", "江西"}, {"Shaanxi", "陕西"},
        {"Shanxi", "山西"}, {"Gansu", "甘肃"}, {"Liaoning", "辽宁"},
        {"Jilin", "吉林"}, {"Heilongjiang", "黑龙江"}, {"Hainan", "海南"},
        {"Qinghai", "青海"}, {"Ningxia", "宁夏"}, {"Xinjiang", "新疆"},
        {"Tibet", "西藏"}, {"Inner Mongolia", "内蒙古"},
        {"Hong Kong", "香港"}, {"Macau", "澳门"}, {"Taiwan", "台湾"},
    };
    size_t i;

    if (region == NULL) return NULL;
    for (i = 0; i < sizeof(MAP) / sizeof(MAP[0]); i++)
    {
        if (strcmp(MAP[i].en, region) == 0) return MAP[i].zh;
    }
    return NULL;
}

static int weather_fetch(weather_info_t *info, const char *city)
{
    char path[256];
    static char buf[HTTP_BUF_SIZE];  /* 仅供 weather 线程使用，避免大缓冲占用线程栈 */
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

    /* 用 weatherCode 本地翻译成中文；未收录的码回退英文 weatherDesc */
    {
        char wcode[8] = {0};
        if (json_get_str(buf, "weatherCode", wcode, sizeof(wcode)) == 0) {
            const char *zh = weather_code_to_zh(wcode);
            if (zh) {
                strncpy(info->weather_desc, zh, WEATHER_DESC_MAX - 1);
                info->weather_desc[WEATHER_DESC_MAX - 1] = '\0';
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
                            /* 本地映射为中文，未收录则保留英文原文 */
                            char region[WEATHER_CITY_MAX];
                            const char *zh;
                            memcpy(region, area, len);
                            region[len] = '\0';
                            zh = region_to_zh(region);
                            if (zh) {
                                strncpy(info->city, zh, WEATHER_CITY_MAX - 1);
                                info->city[WEATHER_CITY_MAX - 1] = '\0';
                            } else {
                                memcpy(info->city, region, len);
                                info->city[len] = '\0';
                            }
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

static void *weather_thread_func(void *arg)
{
    (void)arg;
    while (g_weather_running) {
        weather_info_t info;
        memset(&info, 0, sizeof(info));

        if (weather_fetch(&info, WEATHER_CITY) == 0) {
            pthread_mutex_lock(&g_weather_mutex);
            memcpy(&g_weather_info, &info, sizeof(g_weather_info));
            pthread_mutex_unlock(&g_weather_mutex);
        }

        for (int i = 0; i < WEATHER_INTERVAL && g_weather_running; i++) {
            sleep(1);
        }
    }
    return NULL;
}

int weather_start(void)
{
    if (g_weather_running) return 0;
    g_weather_running = 1;
    if (pthread_create(&g_weather_tid, NULL, weather_thread_func, NULL) != 0) {
        g_weather_running = 0;
        return -1;
    }
    return 0;
}

void weather_stop(void)
{
    g_weather_running = 0;
    if (g_weather_tid) {
        pthread_join(g_weather_tid, NULL);
        g_weather_tid = 0;
    }
}

const weather_info_t *weather_get_info(void)
{
    static weather_info_t cached;
    pthread_mutex_lock(&g_weather_mutex);
    memcpy(&cached, &g_weather_info, sizeof(cached));
    pthread_mutex_unlock(&g_weather_mutex);
    return &cached;
}