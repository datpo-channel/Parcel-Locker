#include "locker_info.h"

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

locker_node_t *locker_find_by_phone(locker_node_t *head, const char *small_phone)
{
    locker_node_t *current = head;

    if (small_phone == NULL)
    {
        return NULL;
    }

    while (current != NULL)
    {
        if (strcmp(current->small_phone, small_phone) == 0)
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

int locker_remove(locker_node_t **head, locker_node_t *node)
{
    locker_node_t *current;
    locker_node_t *prev;

    if (head == NULL || *head == NULL || node == NULL)
    {
        return -1;
    }

    if (node->loc_data == LOCKER_OCCUPIED)
    {
        return -1;
    }

    if (*head == node)
    {
        *head = node->next;
        free(node);
        return 0;
    }

    prev = NULL;
    current = *head;
    while (current != NULL && current != node)
    {
        prev = current;
        current = current->next;
    }

    if (current == NULL)
    {
        return -1;
    }

    prev->next = current->next;
    free(current);
    return 0;
}

int locker_count_by_status(locker_node_t *head, int loc_data)
{
    locker_node_t *current = head;
    int count = 0;

    while (current != NULL)
    {
        if (current->loc_data == loc_data)
        {
            count++;
        }
        current = current->next;
    }

    return count;
}

void locker_print_all(locker_node_t *head)
{
    locker_node_t *current = head;
    int index = 0;

    while (current != NULL)
    {
        printf("[%d] ID:%s Code:%s User:%s Phone:%s Status:%d\n",
               index,
               current->locker_ID,
               current->locker_getID,
               current->username,
               current->small_phone,
               current->loc_data);
        current = current->next;
        index++;
    }
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

int locker_init_all(locker_node_t **head, int count)
{
    char id[LOCKER_ID_LEN];
    int i;

    if (head == NULL || count <= 0)
    {
        return -1;
    }

    *head = NULL;

    /* C 大柜 5个: C01-C05 */
    for (i = 5; i >= 1; i--)
    {
        snprintf(id, sizeof(id), "C%02d", i);
        if (locker_insert_head(head, LOCKER_EMPTY, id, "", "", "") != 0)
        {
            locker_free_list(head);
            return -1;
        }
    }

    /* B 中柜 10个: B01-B10 */
    for (i = 10; i >= 1; i--)
    {
        snprintf(id, sizeof(id), "B%02d", i);
        if (locker_insert_head(head, LOCKER_EMPTY, id, "", "", "") != 0)
        {
            locker_free_list(head);
            return -1;
        }
    }

    /* A 小柜 15个: A01-A15 */
    for (i = 15; i >= 1; i--)
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

locker_node_t *locker_find_first_empty(locker_node_t *head)
{
    locker_node_t *current = head;

    while (current != NULL)
    {
        if (current->loc_data == LOCKER_EMPTY)
        {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

locker_node_t *locker_assign(locker_node_t *head,
                             const char *locker_getID,
                             const char *small_phone)
{
    locker_node_t *node = locker_find_first_empty(head);
    if (node == NULL)
    {
        return NULL;
    }

    node->loc_data = LOCKER_OCCUPIED;

    if (locker_getID != NULL)
    {
        strncpy(node->locker_getID, locker_getID, LOCKER_CODE_LEN - 1);
        node->locker_getID[LOCKER_CODE_LEN - 1] = '\0';
    }
    if (small_phone != NULL)
    {
        strncpy(node->small_phone, small_phone, PHONE_LEN - 1);
        node->small_phone[PHONE_LEN - 1] = '\0';
    }

    return node;
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