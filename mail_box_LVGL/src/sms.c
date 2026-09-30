#include "sms.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "login_page.h"
#include "utils.h"

#define SMS_HOST_IP       "118.31.68.22"
#define SMS_PORT          80
#define SMS_BUFSIZE       4096
#define SMS_TIMEOUT_SEC   5

#define KEY_SERVER_HOST   "8.148.211.45"
#define KEY_SERVER_PORT   3000
#define KEY_SERVER_TOKEN  "mailbox_internal_token_2024"
#define KEY_TIMEOUT_SEC   3

static const char *SMS_ACCOUNT = "C81028909";

/* 全局发码节流时间：防任意号码频繁触发真实短信（跨号码轰炸） */
static time_t g_last_any_sms_time = 0;

/**************************************************************************
 *
 *   @brief : 从密钥服务器获取 SMS API 密码
 *   @arg   : password      输出缓冲区
 *   @arg   : password_size 缓冲区大小
 *
 *   @retval: 0  获取成功
 *           -1  连接/通信失败
 *           -2  认证被拒
 *   @note  : 发送 POST /sms_key 带 token 认证，服务器返回 JSON
 *            {"ok":true,"password":"xxx"}，解析出密码
 *
 ***************************************************************************/
static int fetch_sms_password(char *password, size_t password_size)
{
    int sockfd;
    struct sockaddr_in addr;
    char request[512];
    char response[1024];
    char *body, *p, *start, *end;

    if (password == NULL || password_size == 0)
        return -1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        printf("[SMS_KEY] socket create failed: %s\n", strerror(errno));
        return -1;
    }

    {
        struct timeval tv = { KEY_TIMEOUT_SEC, 0 };
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(KEY_SERVER_PORT);
    addr.sin_addr.s_addr = inet_addr(KEY_SERVER_HOST);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf("[SMS_KEY] connect to %s:%d failed: %s\n",
               KEY_SERVER_HOST, KEY_SERVER_PORT, strerror(errno));
        close(sockfd);
        return -1;
    }

    {
        char json_body[256];
        int body_len = snprintf(json_body, sizeof(json_body),
                                "{\"token\":\"%s\"}", KEY_SERVER_TOKEN);
        snprintf(request, sizeof(request),
                 "POST /sms_key HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %d\r\n"
                 "\r\n"
                 "%s",
                 KEY_SERVER_HOST, KEY_SERVER_PORT,
                 body_len, json_body);
    }

    if (write(sockfd, request, strlen(request)) < 0)
    {
        printf("[SMS_KEY] send request failed: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }

    memset(response, 0, sizeof(response));
    {
        int total = 0;
        int n;
        while (total < (int)sizeof(response) - 1)
        {
            n = read(sockfd, response + total, sizeof(response) - 1 - total);
            if (n <= 0)
                break;
            total += n;
            response[total] = '\0';
            /* 不提前 break：需读完整个响应体（含 HTTP body），
             * 依赖服务器发送完毕后关闭连接 + SO_RCVTIMEO 兜底 */
        }
        if (total == 0)
        {
            printf("[SMS_KEY] read response failed: %s\n", strerror(errno));
            close(sockfd);
            return -1;
        }
    }

    close(sockfd);

    body = strstr(response, "\r\n\r\n");
    if (body == NULL)
    {
        printf("[SMS_KEY] bad response format\n");
        return -1;
    }
    body += 4;

    if (strncmp(response, "HTTP/1.", 7) == 0)
    {
        const char *space = strchr(response, ' ');
        if (space != NULL)
        {
            int status_code = atoi(space + 1);
            if (status_code != 200)
            {
                printf("[SMS_KEY] server returned HTTP %d\n", status_code);
                return -1;
            }
        }
    }

    p = strstr(body, "\"password\":\"");
    if (p == NULL)
    {
        printf("[SMS_KEY] password not found in response: %s\n", body);
        return -1;
    }
    start = p + 12;  /* strlen("\"password\":\"") */

    end = strchr(start, '\"');
    if (end == NULL)
    {
        printf("[SMS_KEY] malformed password field\n");
        return -1;
    }

    {
        size_t len = (size_t)(end - start);
        if (len >= password_size)
            len = password_size - 1;
        memcpy(password, start, len);
        password[len] = '\0';
    }

    printf("[SMS_KEY] password fetched from server\n");
    return 0;
}

/**************************************************************************
 *
 *   @brief : 通过互亿无线短信 API 发送短信验证码
 *   @arg   : phone 目标手机号（11 位）
 *   @arg   : code  验证码（4-6 位数字）
 *
 *   @retval: 0   发送成功
 *            -1  参数无效（NULL 或长度错误）
 *            -2  socket 连接失败
 *            -3  发送数据失败
 *            -4  读取响应失败
 *            -5  从密钥服务器获取密码失败
 *   @note  : 使用 HTTP POST 方式调用 api.ihuyi.com 短信接口，
 *            超时时间 5 秒；密码从密钥服务器动态获取，不硬编码
 *
 ***************************************************************************/
int send_sms_code(const char *phone, const char *code)
{
    int sockfd, ret;
    struct sockaddr_in servaddr;
    char sms_password[64];
    char msg[512];
    char buf[SMS_BUFSIZE];
    char recvbuf[SMS_BUFSIZE];

    if (phone == NULL || code == NULL)
    {
        printf("[SMS] Error: phone or code is NULL\n");
        return -1;
    }

    if (strlen(phone) != 11 || strlen(code) < 4 || strlen(code) > 6)
    {
        printf("[SMS] Error: invalid phone length or code length\n");
        return -1;
    }

    if (fetch_sms_password(sms_password, sizeof(sms_password)) != 0)
    {
        printf("[SMS] Error: failed to fetch SMS password from server\n");
        return -5;
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("[SMS] Error: create socket failed - %s\n", strerror(errno));
        return -2;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SMS_PORT);
    servaddr.sin_addr.s_addr = inet_addr(SMS_HOST_IP);

    {
        struct timeval tv;
        tv.tv_sec = SMS_TIMEOUT_SEC;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        printf("[SMS] Error: connect failed - %s\n", strerror(errno));
        close(sockfd);
        return -2;
    }

    printf("[SMS] Connect success\n");

    snprintf(msg, sizeof(msg),
             "account=%s&password=%s&mobile=%s&content=您的验证码是：%s。请不要把验证码泄露给其他人。",
             SMS_ACCOUNT, sms_password, phone, code);

    snprintf(buf, sizeof(buf),
             "POST /sms/Submit.json HTTP/1.1\r\n"
             "Host: api.ihuyi.com\r\n"
             "Content-Type: application/x-www-form-urlencoded\r\n"
             "Content-Length: %zu\r\n"
             "\r\n"
             "%s", strlen(msg), msg);

    /* 不打印完整请求：其中包含短信密码与验证码明文，避免日志泄露凭据 */
    printf("[SMS] Request %zu bytes to api.ihuyi.com\n", strlen(buf));

    ret = write(sockfd, buf, strlen(buf));
    if (ret < 0)
    {
        printf("[SMS] Error: send data failed - errno:%d, errmsg:'%s'\n",
               errno, strerror(errno));
        close(sockfd);
        return -3;
    }

    printf("[SMS] Send data success, length:%d bytes\n", ret);

    memset(recvbuf, 0, sizeof(recvbuf));
    {
        int total = 0;
        int n;
        while (total < (int)sizeof(recvbuf) - 1)
        {
            n = read(sockfd, recvbuf + total, sizeof(recvbuf) - 1 - total);
            if (n <= 0)
                break;
            total += n;
            recvbuf[total] = '\0';
            /* 不提前 break：需读完整个响应体（含 HTTP body），
             * 依赖服务器发送完毕后关闭连接 + SO_RCVTIMEO 兜底 */
        }
        if (total == 0)
        {
            printf("[SMS] Error: read response failed - %s\n", strerror(errno));
            close(sockfd);
            memset(sms_password, 0, sizeof(sms_password));
            return -4;
        }
    }
    printf("[SMS] Response: %s\n", recvbuf);

    /* 解析互亿无线业务码：code==2 才表示提交成功；
     * 欠费/号码无效/内容审核等失败时 code != 2，必须区分，否则界面误报"验证码已发送" */
    
        char *cp = strstr(recvbuf, "\"code\"");
        int biz_code = -1;

        if (cp != NULL)
        {
            cp += 6;
            while (*cp == ' ' || *cp == ':' || *cp == '\t')
            {
                cp++;
            }
            if (*cp == '"')
            {
                cp++;
            }
            biz_code = atoi(cp);
        }

        memset(sms_password, 0, sizeof(sms_password));
        close(sockfd);

        if (biz_code != 2)
        {
            printf("[SMS] 业务提交失败, code=%d\n", biz_code);
            return -6;
        }
        return 0;
    
}

/**************************************************************************
 *
 *   @brief : 向手机号发送短信验证码（带 60 秒冷却控制）
 *   @arg   : phone  目标手机号（11位）
 *
 *   @retval: 0  发送成功
 *           1  冷却中（60 秒内对同一号码重复请求）
 *           其他  send_sms_code 返回的错误码
 *   @note  : 供登录/查询页共用；成功后将手机号/验证码/时间记入 g_last_sms_*
 *
 ***************************************************************************/
int request_sms_code(const char *phone)
{
    time_t now;
    char sms_code[7];

    if (phone == NULL || strlen(phone) != 11 || phone[0] != '1')
    {
        return -1;
    }

    now = time(NULL);
    if (strcmp(g_last_sms_phone, phone) == 0 &&
        (now - g_last_sms_time) < SMS_COOLDOWN_SEC)
    {
        printf("[短信] 请 %d 秒后再试\n",
               SMS_COOLDOWN_SEC - (int)(now - g_last_sms_time));
        return 1;
    }

    /* 跨号码频率限制：任意两次发码之间至少间隔 60 秒，防短信轰炸 */
    if ((now - g_last_any_sms_time) < SMS_COOLDOWN_SEC)
    {
        printf("[短信] 发送过于频繁，请稍后再试\n");
        return 1;
    }

    /* Demo 账号不真实发短信：固定验证码本地放行（与 verify_phone_code 的
     * DEMO 分支保持一致），避免测试号触发真实短信 */
    if (strcmp(phone, DEMO_PHONE) == 0)
    {
        safe_strcpy(g_last_sms_phone, sizeof(g_last_sms_phone), phone);
        safe_strcpy(g_last_sms_code, sizeof(g_last_sms_code), DEMO_CODE);
        g_last_sms_time = now;
        g_last_any_sms_time = now;
        printf("[短信] Demo 账号免短信，验证码: %s\n", DEMO_CODE);
        return 0;
    }

    generate_random_code(sms_code, 4);

    int sms_ret = send_sms_code(phone, sms_code);
    if (sms_ret != 0)
    {
        printf("[短信] 验证码发送失败，错误码: %d\n", sms_ret);
        return sms_ret;
    }

    safe_strcpy(g_last_sms_phone, sizeof(g_last_sms_phone), phone);
    safe_strcpy(g_last_sms_code, sizeof(g_last_sms_code), sms_code);
    g_last_sms_time = now;
    g_last_any_sms_time = now;
    /* 不打印明文验证码，避免日志泄露 */
    printf("[短信] 验证码已发送到 %s\n", phone);
    return 0;
}