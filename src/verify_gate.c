#include "verify_gate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define VG_BUFSIZE   4096
#define VG_CMD_SIZE  1024

static const char *VG_API_KEY = "";
static const char *vg_api_base_url = "http://8.148.211.45:3000";

static const char *vg_api_base(void)
{
    const char *env = getenv("VG_API_BASE");
    return (env && *env) ? env : vg_api_base_url;
}

static const char *vg_api_key(void)
{
    const char *env = getenv("VG_API_KEY");
    return (env && *env) ? env : VG_API_KEY;
}

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

static char *vg_http_request(const char *method, const char *url_path,
                             const char *body)
{
    char cmd[VG_CMD_SIZE];
    char url[512];
    char buf[VG_BUFSIZE];
    size_t total = 0;
    char *response = NULL;
    char *new_resp;
    FILE *fp;
    const char *api_key;
    int n;

    snprintf(url, sizeof(url), "%s%s", vg_api_base(), url_path);

    api_key = vg_api_key();

    if (has_curl())
    {
        if (body != NULL)
        {
            n = snprintf(cmd, sizeof(cmd),
                         "curl -s -m 15 -X %s '%s' "
                         "-H 'Content-Type: application/json' "
                         "-H 'x-api-key: %s' "
                         "-d '%s'",
                         method, url, api_key, body);
        }
        else
        {
            n = snprintf(cmd, sizeof(cmd),
                         "curl -s -m 15 -X %s '%s' "
                         "-H 'x-api-key: %s'",
                         method, url, api_key);
        }
    }
    else
    {
        if (body != NULL)
        {
            n = snprintf(cmd, sizeof(cmd),
                         "wget -q -O - --timeout=15 "
                         "--header='Content-Type: application/json' "
                         "--header='x-api-key: %s' "
                         "--post-data='%s' '%s'",
                         api_key, body, url);
        }
        else
        {
            n = snprintf(cmd, sizeof(cmd),
                         "wget -q -O - --timeout=15 "
                         "--header='x-api-key: %s' '%s'",
                         api_key, url);
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

int vg_create_pickup(const char *phone, const char *box_id,
                     char *out_token, char *out_url)
{
    static unsigned int counter = 0;
    unsigned int seed;

    if (out_token == NULL || out_url == NULL)
    {
        return -1;
    }

    out_token[0] = '\0';
    out_url[0] = '\0';

    (void)phone;
    (void)box_id;

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
             vg_api_base_url, out_token);

    printf("[取件] 生成token: %s\n", out_token);
    printf("[取件] 扫码URL: %s\n", out_url);

    return 0;
}

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

    if (status != NULL)
    {
        status->status = result;
        json_extract_string(resp, "phone", status->verified_phone, VG_PHONE_LEN);
        json_extract_string(resp, "verifiedPhone", status->verified_phone, VG_PHONE_LEN);
        status->verified_at = json_extract_long(resp, "verifiedAt");
    }

    free(resp);
    return result;
}

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
    else
    {
        ret = 0;
    }

    free(resp);
    return ret;
}