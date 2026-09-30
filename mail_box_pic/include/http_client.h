#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

#define WEATHER_HOST    "wttr.in"
#define WEATHER_PORT    80
#define HTTP_TIMEOUT    5
#define HTTP_BUF_SIZE   (64 * 1024)

int http_get(const char *host, int port, const char *path, char *resp_buf, int buf_size);

#endif