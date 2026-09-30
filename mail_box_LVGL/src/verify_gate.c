#include "verify_gate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

#define VG_BUFSIZE   8192
#define VG_CMD_SIZE  8192

static const char *VG_API_KEY = "";
static const char *vg_api_base_url = "http://8.148.211.45:3000";

/**************************************************************************
 *
 *   @brief : 获取验证服务API基础地址
 *
 *   @retval: API基础地址字符串
 *   @note  : 优先读取环境变量 VG_API_BASE，未设置则使用默认地址
 *
 ***************************************************************************/
static const char *vg_api_base(void)
{
    const char *env = getenv("VG_API_BASE");
    return (env && *env) ? env : vg_api_base_url;
}

/**************************************************************************
 *
 *   @brief : 获取验证服务API密钥
 *
 *   @retval: API密钥字符串
 *   @note  : 优先读取环境变量 VG_API_KEY，未设置则使用默认密钥
 *
 ***************************************************************************/
static const char *vg_api_key(void)
{
    const char *env = getenv("VG_API_KEY");
    return (env && *env) ? env : VG_API_KEY;
}

/**************************************************************************
 *
 *   @brief : 从JSON字符串中提取指定键的字符串值
 *   @arg   : json     JSON响应字符串
 *   @arg   : key      要查找的键名
 *   @arg   : out      输出缓冲区
 *   @arg   : out_size 输出缓冲区大小
 *
 *   @retval: 0  成功提取
 *           -1 未找到键或格式错误
 *   @note  : 简易JSON解析，支持转义字符处理
 *
 ***************************************************************************/
static int json_extract_string(const char *json, const char *key,
                               char *out, size_t out_size)
{
    char pattern[64];
    const char *p;
    const char *start;
    size_t len;

    if (json == NULL || key == NULL || out == NULL || out_size == 0)
    {
        return -1;
    }

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    p = strstr(json, pattern);
    if (p == NULL)
    {
        return -1;
    }

    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == ':')
    {
        p++;
    }

    if (*p != '"')
    {
        return -1;
    }
    p++;

    start = p;
    while (*p != '\0' && *p != '"')
    {
        if (*p == '\\' && *(p + 1) != '\0')
        {
            p++;
        }
        p++;
    }

    len = (size_t)(p - start);
    if (len >= out_size)
    {
        len = out_size - 1;
    }

    memcpy(out, start, len);
    out[len] = '\0';

    return 0;
}

/**************************************************************************
 *
 *   @brief : 从JSON字符串中提取指定键的长整数值
 *   @arg   : json JSON响应字符串
 *   @arg   : key  要查找的键名
 *
 *   @retval: 提取到的数值，未找到或格式错误返回 0
 *
 ***************************************************************************/
static long json_extract_long(const char *json, const char *key)
{
    char pattern[64];
    const char *p;
    char *end;
    long val;

    if (json == NULL || key == NULL)
    {
        return 0;
    }

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    p = strstr(json, pattern);
    if (p == NULL)
    {
        return 0;
    }

    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == ':')
    {
        p++;
    }

    val = strtol(p, &end, 10);
    if (end == p)
    {
        return 0;
    }

    return val;
}

/**************************************************************************
 *
 *   @brief : 从JSON字符串中提取指定键的布尔值
 *   @arg   : json JSON响应字符串
 *   @arg   : key  要查找的键名
 *
 *   @retval: 1  true
 *            0  false
 *           -1 未找到或格式错误
 *
 ***************************************************************************/
static int json_find_bool(const char *json, const char *key)
{
    char pattern[64];
    const char *p;

    if (json == NULL || key == NULL)
    {
        return -1;
    }

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    p = strstr(json, pattern);
    if (p == NULL)
    {
        return -1;
    }

    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == ':')
    {
        p++;
    }

    if (strncmp(p, "true", 4) == 0)
    {
        return 1;
    }
    if (strncmp(p, "false", 5) == 0)
    {
        return 0;
    }

    return -1;
}

/**************************************************************************
 *
 *   @brief : 检测系统是否安装了curl命令
 *
 *   @retval: 1  已安装curl
 *            0  未安装curl
 *   @note  : 首次检测后缓存结果，避免重复执行which命令
 *
 ***************************************************************************/
static int has_curl(void)
{
    static int checked = -1;
    if (checked == -1)
    {
        FILE *f = popen("which curl 2>/dev/null", "r");
        if (f)
        {
            char buf[16] = {0};
            if (fgets(buf, sizeof(buf), f) && buf[0] != '\0')
                checked = 1;
            else
                checked = 0;
            pclose(f);
        }
        else
        {
            checked = 0;
        }
    }
    return checked;
}

/**************************************************************************
 *
 *   @brief : 将 src 中的单引号转义为 '\'' 序列
 *   @note  : vg_http_request 用 popen 执行 curl/wget 命令，url/body 含
 *            用户可控内容（如手机号）时单引号可逃逸并注入 shell 命令；
 *            此函数在拼入 shell 前做转义，防止命令注入
 *
 ***************************************************************************/
static void shell_escape_single_quote(char *dst, size_t dst_size, const char *src)
{
    size_t i = 0, o = 0;

    if (dst == NULL || src == NULL || dst_size == 0)
    {
        return;
    }

    while (src[i] != '\0' && o + 4 < dst_size)
    {
        if (src[i] == '\'')
        {
            dst[o++] = '\'';
            dst[o++] = '\\';
            dst[o++] = '\'';
            dst[o++] = '\'';
        }
        else
        {
            dst[o++] = src[i];
        }
        i++;
    }
    dst[o] = '\0';
}

/**************************************************************************
 *
 *   @brief : 发送HTTP请求并获取响应
 *   @arg   : method  HTTP方法（"GET"/"POST"等）
 *   @arg   : url_path API路径（相对于基础地址）
 *   @arg   : body    POST请求体，GET请求传NULL
 *
 *   @retval: 成功返回响应字符串（需调用者free），失败返回NULL
 *   @note  : 优先使用curl，未安装则回退到wget；超时15秒
 *
 ***************************************************************************/
static char *vg_http_request(const char *method, const char *url_path,
                             const char *body)
{
    char cmd[VG_CMD_SIZE];
    char url[512];
    char esc_url[512];
    char esc_body[VG_CMD_SIZE];
    char buf[VG_BUFSIZE];
    size_t total = 0;
    char *response = NULL;
    char *new_resp;
    FILE *fp;
    const char *api_key;
    int n;

    snprintf(url, sizeof(url), "%s%s", vg_api_base(), url_path);
    shell_escape_single_quote(esc_url, sizeof(esc_url), url);
    if (body != NULL)
    {
        shell_escape_single_quote(esc_body, sizeof(esc_body), body);
    }

    api_key = vg_api_key();

    if (has_curl())
    {
        if (body != NULL)
        {
            n = snprintf(cmd, sizeof(cmd),
                         "curl -s -m 15 --connect-timeout 2 -X %s '%s' "
                         "-H 'Content-Type: application/json' "
                         "-H 'x-api-key: %s' "
                         "-d '%s'",
                         method, esc_url, api_key, esc_body);
        }
        else
        {
            n = snprintf(cmd, sizeof(cmd),
                         "curl -s -m 15 --connect-timeout 2 -X %s '%s' "
                         "-H 'x-api-key: %s'",
                         method, esc_url, api_key);
        }
    }
    else
    {
        if (body != NULL)
        {
            n = snprintf(cmd, sizeof(cmd),
                         "wget -q -O - -T 15 "
                         "--header='Content-Type: application/json' "
                         "--header='x-api-key: %s' "
                         "--post-data='%s' '%s'",
                         api_key, esc_body, esc_url);
        }
        else
        {
            n = snprintf(cmd, sizeof(cmd),
                         "wget -q -O - -T 15 "
                         "--header='x-api-key: %s' '%s'",
                         api_key, esc_url);
        }
    }

    if (n < 0 || (size_t)n >= sizeof(cmd))
    {
        return NULL;
    }

    fp = popen(cmd, "r");
    if (fp == NULL)
    {
        return NULL;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL)
    {
        size_t len = strlen(buf);
        /* 响应大小上限：防止异常/恶意服务器输出无限增长耗尽内存 */
        if (total + len >= (64 * 1024))
        {
            printf("[网关] 响应超过 64KB 上限，截断\n");
            break;
        }
        new_resp = (char *)realloc(response, total + len + 1);
        if (new_resp == NULL)
        {
            free(response);
            response = NULL;
            break;
        }
        response = new_resp;
        memcpy(response + total, buf, len);
        total += len;
        response[total] = '\0';
    }

    pclose(fp);

    if (response == NULL)
    {
        response = (char *)malloc(1);
        if (response != NULL)
        {
            response[0] = '\0';
        }
    }

    return response;
}

/**************************************************************************
 *
 *   @brief : 生成扫码取件令牌和对应的扫码URL
 *   @arg   : out_token 输出缓冲区，存储生成的令牌（需VG_TOKEN_LEN字节）
 *   @arg   : out_url   输出缓冲区，存储扫码URL（需VG_URL_LEN字节）
 *
 *   @retval: 0  成功
 *           -1 参数无效
 *   @note  : 令牌基于时间戳和计数器生成，格式为UUID风格
 *
 ***************************************************************************/
int vg_create_pickup(char *out_token, char *out_url)
{
    static unsigned int counter = 0;
    unsigned int seed;

    if (out_token == NULL || out_url == NULL)
    {
        return -1;
    }

    out_token[0] = '\0';
    out_url[0] = '\0';

    seed = (unsigned int)time(NULL) + (++counter);

    snprintf(out_token, VG_TOKEN_LEN, "%04x%04x-%04x-%04x-%04x-%04x%04x%04x",
             (seed >> 16) & 0xFFFF,
             seed & 0xFFFF,
             (seed >> 12) & 0xFFFF,
             ((seed << 4) & 0xFFFF) | (counter & 0xFFFF),
             (seed >> 8) & 0xFFFF,
             (seed >> 4) & 0xFFFF,
             seed & 0xFFFF,
             (seed << 8) & 0xFFFF);

    snprintf(out_url, VG_URL_LEN, "%s/?token=%s",
             vg_api_base(), out_token);

    /* 不打印完整令牌：token 可开柜，明文落日志存在侧信道泄露风险，仅打印前缀与长度 */
    printf("[取件] 生成token: %.6s...(共%zu位)\n", out_token, strlen(out_token));
    printf("[取件] 扫码URL已生成(%zu字节)\n", strlen(out_url));

    return 0;
}

/**************************************************************************
 *
 *   @brief : 查询取件令牌的验证状态
 *   @arg   : token  取件令牌字符串
 *   @arg   : status 输出参数，存储验证状态和手机号，传NULL则不获取详情
 *
 *   @retval: VG_STATUS_VERIFIED  已验证
 *           VG_STATUS_PENDING    待验证
 *           VG_STATUS_ERROR      查询失败
 *   @note  : 通过GET /api/status接口查询，提取verified和phone字段
 *
 ***************************************************************************/
int vg_query_status(const char *token, vg_status_t *status)
{
    char url_path[VG_TOKEN_LEN + 32];
    char *resp;
    int result = VG_STATUS_PENDING;
    int verified;

    if (token == NULL || token[0] == '\0')
    {
        return VG_STATUS_ERROR;
    }

    if (status != NULL)
    {
        memset(status, 0, sizeof(vg_status_t));
        status->status = VG_STATUS_ERROR;
    }

    snprintf(url_path, sizeof(url_path), "/api/status?token=%s", token);

    resp = vg_http_request("GET", url_path, NULL);
    if (resp == NULL)
    {
        return VG_STATUS_ERROR;
    }

    /* 云端 /api/status 对已开箱票据返回 opened:true（verified 亦为 true），
     * 优先解析 opened 以返回 VG_STATUS_OPENED，否则已消费票据被误判 PENDING 持续轮询 */
    if (json_find_bool(resp, "opened") == 1)
    {
        result = VG_STATUS_OPENED;
    }
    else
    {
        verified = json_find_bool(resp, "verified");
        if (verified == 1)
        {
            result = VG_STATUS_VERIFIED;
        }
        else if (verified == 0)
        {
            result = VG_STATUS_PENDING;
        }
        else
        {
            result = VG_STATUS_ERROR;
        }
    }

    if (status != NULL)
    {
        status->status = result;
        json_extract_string(resp, "phone", status->verified_phone, VG_PHONE_LEN);
        status->verified_at = json_extract_long(resp, "verifiedAt");
    }

    free(resp);
    return result;
}

/**************************************************************************
 *
 *   @brief : 消费取件票据，标记令牌为已开箱并获取验证手机号
 *   @arg   : token      取件令牌字符串
 *   @arg   : out_phone  输出缓冲区，存储验证手机号
 *   @arg   : phone_size 输出缓冲区大小
 *
 *   @retval: 1  消费成功
 *            0  令牌已被消费或验证未通过
 *           -1 参数无效或请求失败
 *   @note  : 通过POST /api/consume接口消费，令牌只能被消费一次
 *
 ***************************************************************************/
int vg_consume_ticket(const char *token, char *out_phone, size_t phone_size)
{
    char body[VG_TOKEN_LEN + 32];
    char url_path[80];
    char *resp;
    int success;
    int ret = 0;

    if (token == NULL || token[0] == '\0')
    {
        return -1;
    }

    if (out_phone != NULL && phone_size > 0)
    {
        out_phone[0] = '\0';
    }

    snprintf(body, sizeof(body), "{\"token\":\"%s\"}", token);
    strcpy(url_path, "/api/pickup/consume");

    resp = vg_http_request("POST", url_path, body);
    if (resp == NULL)
    {
        return -2;
    }

    success = json_find_bool(resp, "success");
    if (success == 1)
    {
        ret = 1;
        if (out_phone != NULL && phone_size > 0)
        {
            json_extract_string(resp, "phone", out_phone, phone_size);
        }
    }
    else if (success == 0)
    {
        /* 业务明确拒绝（未验证或已被消费） */
        ret = 0;
    }
    else
    {
        /* 响应缺少 success 字段：视为解析失败，调用方据此保留本地令牌，避免误清 */
        ret = -3;
    }

    free(resp);
    return ret;
}

/**************************************************************************
 *
 *   @brief : 校验手机号是否在云端成员名单（快递员/用户）中
 *   @arg   : api_path 校验接口路径（如 "/api/courier/check"）
 *   @arg   : phone    手机号
 *   @arg   : valid    输出是否在名单中（1=在，0=不在）
 *
 *   @retval: 0  校验成功（valid 有效）
 *           -1 参数错误
 *           -2 网络请求失败
 *           -3 响应解析失败
 *
 ***************************************************************************/
static int vg_check_list_member(const char *api_path, const char *phone, int *valid)
{
    char url_path[VG_PHONE_LEN + 32];
    char *resp;
    int result;

    if (api_path == NULL || phone == NULL || phone[0] == '\0' || valid == NULL)
    {
        return -1;
    }

    *valid = 0;

    /* 手机号为纯数字，无需 URL 编码 */
    snprintf(url_path, sizeof(url_path), "%s?phone=%s", api_path, phone);

    resp = vg_http_request("GET", url_path, NULL);
    if (resp == NULL)
    {
        return -2;
    }

    result = json_find_bool(resp, "valid");
    free(resp);

    if (result < 0)
    {
        return -3;
    }

    *valid = result;
    return 0;
}

int vg_check_courier(const char *phone, int *valid)
{
    return vg_check_list_member("/api/courier/check", phone, valid);
}

int vg_check_user(const char *phone, int *valid)
{
    return vg_check_list_member("/api/user/check", phone, valid);
}

/* 异步上报任务的入参（堆上分配，由工作线程释放） */
typedef struct
{
    char phone[VG_PHONE_LEN];
    char role[16];
    long timestamp;
} vg_login_report_t;

/**************************************************************************
 *
 *   @brief : 登录记录上报工作线程（POST /api/login/record）
 *   @arg   : arg 指向 vg_login_report_t 的指针，用完释放
 *
 *   @retval: NULL（分离线程）
 *   @note  : 独立线程执行 curl/wget 网络请求，失败仅打印日志不重试，
 *            不阻塞登录主流程
 *
 ***************************************************************************/
static void *vg_login_report_thread(void *arg)
{
    vg_login_report_t *r = (vg_login_report_t *)arg;
    char body[VG_PHONE_LEN + 96];
    long ts = (r->timestamp > 0) ? r->timestamp : (long)time(NULL);
    char *resp;

    snprintf(body, sizeof(body),
             "{\"phone\":\"%s\",\"role\":\"%s\",\"loginTime\":%ld}",
             r->phone, r->role, ts);

    printf("[登录上报] 上传登录记录: %s\n", body);

    resp = vg_http_request("POST", "/api/login/record", body);
    if (resp == NULL)
    {
        printf("[登录上报] 网络请求失败\n");
    }
    else
    {
        printf("[登录上报] 服务器响应: %s\n", resp);
        free(resp);
    }

    free(r);
    return NULL;
}

int vg_report_login_async(const char *phone, const char *role, long timestamp)
{
    pthread_t tid;
    pthread_attr_t attr;
    vg_login_report_t *r;

    if (phone == NULL || phone[0] == '\0' ||
        role == NULL || role[0] == '\0')
    {
        return -1;
    }

    r = (vg_login_report_t *)malloc(sizeof(vg_login_report_t));
    if (r == NULL)
    {
        return -1;
    }

    strncpy(r->phone, phone, sizeof(r->phone) - 1);
    r->phone[sizeof(r->phone) - 1] = '\0';
    strncpy(r->role, role, sizeof(r->role) - 1);
    r->role[sizeof(r->role) - 1] = '\0';
    r->timestamp = timestamp;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&tid, &attr, vg_login_report_thread, r) != 0)
    {
        pthread_attr_destroy(&attr);
        free(r);
        printf("[登录上报] 线程创建失败\n");
        return -1;
    }
    pthread_attr_destroy(&attr);

    return 0;
}

int vg_update_remaining(const char *token, const char *remaining_json)
{
    char body[VG_TOKEN_LEN + 2048];
    char *resp;
    int success;
    int ret = -2;

    if (token == NULL || token[0] == '\0' || remaining_json == NULL)
    {
        return -1;
    }

    snprintf(body, sizeof(body),
             "{\"token\":\"%s\",\"remaining\":%s}",
             token, remaining_json);

    resp = vg_http_request("POST", "/api/update-remaining", body);
    if (resp == NULL)
    {
        return -2;
    }

    success = json_find_bool(resp, "success");
    if (success == 1)
    {
        ret = 0;
    }

    free(resp);
    return ret;
}