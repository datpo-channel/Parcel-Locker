#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>

int http_get(const char *host, int port, const char *path,
             char *resp_buf, int buf_size)
{
    int sockfd, ret;
    struct sockaddr_in addr;
    struct timeval tv;
    char request[1024];
    char *body_start;
    int total = 0, n;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    tv.tv_sec  = HTTP_TIMEOUT;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);

    {
        struct hostent *he = gethostbyname(host);
        if (!he) {
            fprintf(stderr, "gethostbyname %s failed\n", host);
            close(sockfd);
            return -1;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    ret = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == -1) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: WeatherClock/1.0\r\n"
             "Accept: application/json\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, host);

    ret = send(sockfd, request, strlen(request), 0);
    if (ret == -1) {
        perror("send");
        close(sockfd);
        return -1;
    }

    while (total < buf_size - 1) {
        n = recv(sockfd, resp_buf + total, buf_size - 1 - total, 0);
        if (n <= 0) break;
        total += n;
    }
    resp_buf[total] = '\0';
    close(sockfd);

    body_start = strstr(resp_buf, "\r\n\r\n");
    if (!body_start) return -1;
    body_start += 4;

    n = strlen(body_start);
    memmove(resp_buf, body_start, n + 1);
    return n;
}