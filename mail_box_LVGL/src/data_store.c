/**
 * @file data_store.c
 * 数据存储模块：储物柜链表（locker_*）
 */

#include "data_store.h"

#include <pthread.h>

/* 储物柜链表全局互斥锁：主线程与扫码监控线程共享访问保护 */
static pthread_mutex_t g_locker_mutex = PTHREAD_MUTEX_INITIALIZER;

void locker_lock(void)
{
    pthread_mutex_lock(&g_locker_mutex);
}

void locker_unlock(void)
{
    pthread_mutex_unlock(&g_locker_mutex);
}

/* ====================================================================
 * 储物柜链表管理（原 locker_info.c）
 * ==================================================================== */

locker_node_t *locker_create_node(int loc_data, const char *locker_ID,
                                  const char *locker_getID,
                                  const char *username,
                                  const char *small_phone)
{
    locker_node_t *node = (locker_node_t *)malloc(sizeof(locker_node_t));
    if (node == NULL)
    {
        return NULL;
    }

    memset(node, 0, sizeof(locker_node_t));
    node->loc_data = loc_data;

    if (locker_ID != NULL)
    {
        strncpy(node->locker_ID, locker_ID, LOCKER_ID_LEN - 1);
    }
    if (locker_getID != NULL)
    {
        strncpy(node->locker_getID, locker_getID, LOCKER_CODE_LEN - 1);
    }
    if (username != NULL)
    {
        strncpy(node->username, username, USERNAME_LEN - 1);
    }
    if (small_phone != NULL)
    {
        strncpy(node->small_phone, small_phone, PHONE_LEN - 1);
    }

    node->next = NULL;
    return node;
}

int locker_insert_head(locker_node_t **head, int loc_data,
                       const char *locker_ID, const char *locker_getID,
                       const char *username, const char *small_phone)
{
    locker_node_t *node;

    if (head == NULL)
    {
        return -1;
    }

    node = locker_create_node(loc_data, locker_ID, locker_getID,
                              username, small_phone);
    if (node == NULL)
    {
        return -1;
    }

    node->next = *head;
    *head = node;
    return 0;
}

locker_node_t *locker_find_by_id(locker_node_t *head, const char *locker_ID)
{
    locker_node_t *current = head;

    if (locker_ID == NULL)
    {
        return NULL;
    }

    while (current != NULL)
    {
        if (strcmp(current->locker_ID, locker_ID) == 0)
        {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

locker_node_t *locker_find_by_code(locker_node_t *head, const char *locker_getID)
{
    locker_node_t *current = head;

    if (locker_getID == NULL)
    {
        return NULL;
    }

    while (current != NULL)
    {
        if (current->loc_data == LOCKER_OCCUPIED &&
            strcmp(current->locker_getID, locker_getID) == 0)
        {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

int locker_update_status(locker_node_t *node, int loc_data)
{
    if (node == NULL)
    {
        return -1;
    }

    node->loc_data = loc_data;

    if (loc_data == LOCKER_EMPTY)
    {
        memset(node->username, 0, USERNAME_LEN);
        memset(node->small_phone, 0, PHONE_LEN);
        memset(node->locker_getID, 0, LOCKER_CODE_LEN);
        memset(node->pickup_token, 0, VG_TOKEN_LEN);
    }

    return 0;
}

void locker_free_list(locker_node_t **head)
{
    locker_node_t *current;
    locker_node_t *next;

    if (head == NULL)
    {
        return;
    }

    current = *head;
    while (current != NULL)
    {
        next = current->next;
        free(current);
        current = next;
    }

    *head = NULL;
}

int locker_init_all(locker_node_t **head)
{
    char id[LOCKER_ID_LEN];
    int i;

    if (head == NULL)
    {
        return -1;
    }

    *head = NULL;

    /* 固定初始化 30 个柜：C 大柜 LOCKER_C_COUNT 个、B 中柜 LOCKER_B_COUNT 个、
     * A 小柜 LOCKER_A_COUNT 个（数量在 data_store.h 统一维护） */
    for (i = LOCKER_C_COUNT; i >= 1; i--)
    {
        snprintf(id, sizeof(id), "C%02d", i);
        if (locker_insert_head(head, LOCKER_EMPTY, id, "", "", "") != 0)
        {
            locker_free_list(head);
            return -1;
        }
    }

    /* B 中柜: B01-B10 */
    for (i = LOCKER_B_COUNT; i >= 1; i--)
    {
        snprintf(id, sizeof(id), "B%02d", i);
        if (locker_insert_head(head, LOCKER_EMPTY, id, "", "", "") != 0)
        {
            locker_free_list(head);
            return -1;
        }
    }

    /* A 小柜: A01-A15 */
    for (i = LOCKER_A_COUNT; i >= 1; i--)
    {
        snprintf(id, sizeof(id), "A%02d", i);
        if (locker_insert_head(head, LOCKER_EMPTY, id, "", "", "") != 0)
        {
            locker_free_list(head);
            return -1;
        }
    }

    return 0;
}

locker_node_t *locker_find_first_empty_by_prefix(locker_node_t *head,
                                                  const char *prefix)
{
    locker_node_t *current = head;
    size_t prefix_len;

    if (prefix == NULL)
    {
        return NULL;
    }

    prefix_len = strlen(prefix);

    while (current != NULL)
    {
        if (current->loc_data == LOCKER_EMPTY &&
            strncmp(current->locker_ID, prefix, prefix_len) == 0)
        {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

int locker_clean_by_id(locker_node_t *head, const char *locker_ID)
{
    locker_node_t *node;

    if (head == NULL)
    {
        printf("链表为空\n");
        return 0;
    }

    node = locker_find_by_id(head, locker_ID);
    if (node == NULL)
    {
        printf("没有此储物柜编号\n");
        return 0;
    }

    locker_update_status(node, LOCKER_EMPTY);
    printf("删除成功，储物柜%s已弹出\n", locker_ID);
    return 1;
}
