#include "ntp_client.h"
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

#define NTP_TIMESTAMP_DELTA 2208988800ULL

int ntp_sync(void)
{
    int sockfd;
    struct sockaddr_in addr;
    struct timeval tv;
    unsigned char buf[48];
    ssize_t n;
    time_t ntp_time;

    memset(buf, 0, sizeof(buf));
    buf[0] = 0x1B;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    tv.tv_sec = NTP_TIMEOUT;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(NTP_PORT);

    {
        struct hostent *he = gethostbyname(NTP_HOST);
        if (!he) {
            fprintf(stderr, "gethostbyname %s failed\n", NTP_HOST);
            close(sockfd);
            return -1;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
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

    if (n < 48) {
        fprintf(stderr, "ntp recv failed or incomplete\n");
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
        if (clock_settime(CLOCK_REALTIME, &ts) == -1) {
            stime(&ntp_time);
        }
    }

    printf("NTP同步成功: %s", ctime(&ntp_time));
    return 0;
}