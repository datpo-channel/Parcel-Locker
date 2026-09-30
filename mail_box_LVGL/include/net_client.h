#ifndef NET_CLIENT_H_
#define NET_CLIENT_H_

/* ==================== HTTP 客户端（原 http_client.h） ==================== */

#define WEATHER_HOST    "wttr.in"
#define WEATHER_PORT    80
#define HTTP_TIMEOUT    5
#define HTTP_BUF_SIZE   (64 * 1024)

int http_get(const char *host, int port, const char *path, char *resp_buf, int buf_size);

/* ==================== NTP 时间同步（原 ntp_client.h） ==================== */

#define NTP_HOST     "ntp.aliyun.com"
#define NTP_PORT     123
#define NTP_TIMEOUT   5

int ntp_sync(void);

#endif /* NET_CLIENT_H_ */
