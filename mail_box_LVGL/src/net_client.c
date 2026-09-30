/**
 * @file net_client.c
 * 网络客户端模块：合并 HTTP GET 客户端（原 http_client.c）
 * 与 NTP 时间同步（原 ntp_client.c）
 */

#include "net_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>

/* ====================================================================
 * HTTP GET 客户端（原 http_client.c）
 * ==================================================================== */

int http_get(const char *host, int port, const char *path,
             char *resp_buf, int buf_size)
{
    int sockfd, ret;
    struct sockaddr_in addr;
    struct timeval tv;
    char request[1024];
    char *body_start;
    int total = 0, n;

    if (resp_buf == NULL || buf_size <= 1 ||
        host == NULL || path == NULL || port <= 0)
    {
        return -1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    tv.tv_sec  = HTTP_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0 ||
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0)
    {
        perror("setsockopt timeout");
        close(sockfd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);

    {
        struct addrinfo hints, *res = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        /* getaddrinfo 线程安全且支持超时能力（配合 hints），替代 gethostbyname */
        if (getaddrinfo(host, NULL, &hints, &res) != 0 || res == NULL)
        {
            fprintf(stderr, "getaddrinfo %s failed\n", host);
            close(sockfd);
            return -1;
        }
        addr.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }

    {
        /* 非阻塞 connect + select 超时：SO_SNDTIMEO 不影响 connect，
         * 不可达地址时避免阻塞到内核 SYN 重传超时（可达数十秒） */
        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags >= 0)
            fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        ret = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
        if (ret == -1 && errno != EINPROGRESS)
        {
            perror("connect");
            close(sockfd);
            return -1;
        }
        if (ret != 0)
        {
            fd_set wset;
            struct timeval ctv;
            FD_ZERO(&wset);
            FD_SET(sockfd, &wset);
            ctv.tv_sec = HTTP_TIMEOUT;
            ctv.tv_usec = 0;
            if (select(sockfd + 1, NULL, &wset, NULL, &ctv) <= 0)
            {
                perror("connect timeout");
                close(sockfd);
                return -1;
            }
            int soerr = 0;
            socklen_t slen = sizeof(soerr);
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &soerr, &slen) < 0 || soerr != 0)
            {
                fprintf(stderr, "connect failed: %s\n", strerror(soerr));
                close(sockfd);
                return -1;
            }
        }
        if (flags >= 0)
            fcntl(sockfd, F_SETFL, flags);
    }

    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: WeatherClock/1.0\r\n"
             "Accept: application/json\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, host);

    /* 循环发送直至全部送出，避免部分发送导致服务端一直等待 */
    {
        size_t req_len = strlen(request);
        size_t off = 0;
        while (off < req_len)
        {
            ssize_t sent = send(sockfd, request + off, req_len - off, 0);
            if (sent <= 0)
            {
                perror("send");
                close(sockfd);
                return -1;
            }
            off += (size_t)sent;
        }
    }

    while (total < buf_size - 1) {
        n = recv(sockfd, resp_buf + total, buf_size - 1 - total, 0);
        if (n <= 0) break;
        total += n;
    }
    resp_buf[total] = '\0';
    close(sockfd);

    /* 校验 HTTP 状态码，避免 301/404 等响应被当作成功解析 */
    if (strncmp(resp_buf, "HTTP/1.1 200", 12) != 0 &&
        strncmp(resp_buf, "HTTP/1.0 200", 12) != 0)
    {
        fprintf(stderr, "[HTTP] unexpected status, first line: %.60s\n", resp_buf);
        return -1;
    }

    body_start = strstr(resp_buf, "\r\n\r\n");
    if (!body_start) return -1;
    body_start += 4;

    n = strlen(body_start);
    memmove(resp_buf, body_start, n + 1);
    return n;
}

/* ====================================================================
 * NTP 时间同步（原 ntp_client.c）
 * ==================================================================== */

#define NTP_TIMESTAMP_DELTA 2208988800ULL

int ntp_sync(void)
{
    int sockfd;
    struct sockaddr_in addr;
    struct timeval tv;
    unsigned char buf[48];
    ssize_t n;
    time_t ntp_time;

    /* 时区设置不依赖网络同步结果：无论 NTP 成败都保持北京时间显示，
     * 避免断网启动时 localtime 退化为 UTC（相差 8 小时）。
     * CST-8 为 POSIX TZ 格式，无需 tzdata 即可生效 */
    setenv("TZ", "CST-8", 1);
    tzset();

    memset(buf, 0, sizeof(buf));
    buf[0] = 0x1B;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    tv.tv_sec = NTP_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0 ||
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0)
    {
        perror("setsockopt timeout");
        close(sockfd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(NTP_PORT);

    {
        struct addrinfo hints, *res = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        if (getaddrinfo(NTP_HOST, NULL, &hints, &res) != 0 || res == NULL)
        {
            fprintf(stderr, "getaddrinfo %s failed\n", NTP_HOST);
            close(sockfd);
            return -1;
        }
        addr.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }

    n = sendto(sockfd, buf, sizeof(buf), 0,
               (struct sockaddr *)&addr, sizeof(addr));
    if (n != sizeof(buf)) {
        perror("sendto");
        close(sockfd);
        return -1;
    }

    n = recvfrom(sockfd, buf, sizeof(buf), 0, NULL, NULL);
    close(sockfd);

    /* 校验 NTP 响应报文有效性，拒绝伪造/异常报文写入系统时钟：
     * - 长度不足 48 字节
     * - NTP 版本号(VN) < 3
     * - 模式(Mode)非法
     * - stratum >= 16（表示时钟未同步）
     * - LI == 3（时钟不同步告警） */
    if (n < 48 ||
        ((buf[0] >> 3) & 0x07) < 3 ||
        (buf[0] & 0x07) == 0 ||
        buf[1] >= 16 ||
        (buf[0] >> 6) == 3)
    {
        fprintf(stderr, "ntp invalid response\n");
        return -1;
    }

    ntp_time = ((unsigned int)buf[40] << 24) |
               ((unsigned int)buf[41] << 16) |
               ((unsigned int)buf[42] << 8)  |
               (unsigned int)buf[43];

    ntp_time -= (time_t)NTP_TIMESTAMP_DELTA;

    {
        struct timespec ts;
        ts.tv_sec = ntp_time;
        ts.tv_nsec = 0;
        if (clock_settime(CLOCK_REALTIME, &ts) == -1)
        {
            /* clock_settime 与 stime 权限要求相同，stime 在较新 glibc 已移除，直接报错 */
            perror("clock_settime failed");
            return -1;
        }
    }

    printf("NTP同步成功: %s", ctime(&ntp_time));
    return 0;
}
