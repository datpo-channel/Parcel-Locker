#ifndef WEATHER_UTILS_H_
#define WEATHER_UTILS_H_

#include <stdint.h>

#define WEATHER_CITY_MAX    64
#define WEATHER_DESC_MAX    64
#define WEATHER_INTERVAL    30

typedef struct {
    char city[WEATHER_CITY_MAX];
    char temp[16];
    char feels_like[16];
    char humidity[16];
    char weather_desc[WEATHER_DESC_MAX];
    int valid;
} weather_info_t;

int weather_start(void);
void weather_stop(void);
const weather_info_t *weather_get_info(void);

#endif