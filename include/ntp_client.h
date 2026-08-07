#ifndef _NTP_CLIENT_H_
#define _NTP_CLIENT_H_

#define NTP_HOST     "ntp.aliyun.com"
#define NTP_PORT     123
#define NTP_TIMEOUT   5

int ntp_sync(void);

#endif