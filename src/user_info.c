#include "user_info.h"

user_node_t *user_create_node(const char *username,
                              const char *password,
                              const char *small_phone)
{
    user_node_t *node = (user_node_t *)malloc(sizeof(user_node_t));
    if (node == NULL)
    {
        return NULL;
    }

    memset(node, 0, sizeof(user_node_t));

    if (username != NULL)
    {
        strncpy(node->username, username, USER_NAME_LEN - 1);
    }
    if (password != NULL)
    {
        strncpy(node->password, password, USER_PASSWD_LEN - 1);
    }
    if (small_phone != NULL)
    {
        strncpy(node->small_phone, small_phone, USER_PHONE_LEN - 1);
    }

    node->next = NULL;
    return node;
}

int user_insert_head(user_node_t **head, const char *username,
                     const char *password, const char *small_phone)
{
    user_node_t *node;

    if (head == NULL)
    {
        return -1;
    }

    node = user_create_node(username, password, small_phone);
    if (node == NULL)
    {
        return -1;
    }

    node->next = *head;
    *head = node;
    return 0;
}

user_node_t *user_find_by_phone(user_node_t *head, const char *small_phone)
{
    user_node_t *current = head;

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

user_node_t *user_verify_login(user_node_t *head, const char *small_phone,
                               const char *password)
{
    user_node_t *node;

    if (small_phone == NULL || password == NULL)
    {
        return NULL;
    }

    node = user_find_by_phone(head, small_phone);
    if (node == NULL)
    {
        return NULL;
    }

    if (strcmp(node->password, password) != 0)
    {
        return NULL;
    }

    return node;
}

int user_remove(user_node_t **head, user_node_t *node)
{
    user_node_t *current;
    user_node_t *prev;

    if (head == NULL || *head == NULL || node == NULL)
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

void user_print_all(user_node_t *head)
{
    user_node_t *current = head;
    int index = 0;

    while (current != NULL)
    {
        printf("[%d] User:%s Phone:%s\n",
               index,
               current->username,
               current->small_phone);
        current = current->next;
        index++;
    }
}

void user_free_list(user_node_t **head)
{
    user_node_t *current;
    user_node_t *next;

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

int user_load_from_file(user_node_t **head, const char *filepath)
{
    FILE *fp;
    const char *path;
    char line[USER_PHONE_LEN + 2];
    int count = 0;

    if (head == NULL)
    {
        return -1;
    }

    path = (filepath != NULL) ? filepath : COURIER_DATA_PATH;

    fp = fopen(path, "r");
    if (fp == NULL)
    {
        return 0;
    }

    *head = NULL;

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            line[len - 1] = '\0';
            len--;
        }

        if (len == 0)
        {
            continue;
        }

        if (user_insert_head(head, "", "", line) != 0)
        {
            fclose(fp);
            user_free_list(head);
            return -1;
        }
        count++;
    }

    fclose(fp);
    return count;
}

int user_save_to_file(user_node_t *head, const char *filepath)
{
    FILE *fp;
    const char *path;
    user_node_t *current;

    path = (filepath != NULL) ? filepath : COURIER_DATA_PATH;

    fp = fopen(path, "w");
    if (fp == NULL)
    {
        return -1;
    }

    current = head;
    while (current != NULL)
    {
        if (current->small_phone[0] != '\0')
        {
            fprintf(fp, "%s\n", current->small_phone);
        }
        current = current->next;
    }

    fclose(fp);
    return 0;
}