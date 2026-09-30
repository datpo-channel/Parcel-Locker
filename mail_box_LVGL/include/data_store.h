#ifndef DATA_STORE_H_
#define DATA_STORE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "verify_gate.h"

/* ==================== 储物柜链表（原 locker_info.h） ==================== */

#define LOCKER_ID_LEN    11
#define LOCKER_CODE_LEN   5
#define USERNAME_LEN     10
#define PHONE_LEN        14

#define LOCKER_EMPTY     0
#define LOCKER_OCCUPIED  1

/* 各型号柜数量（locker_init_all 按此初始化，改柜数只需改这里） */
#define LOCKER_C_COUNT   5
#define LOCKER_B_COUNT   10
#define LOCKER_A_COUNT   15
#define LOCKER_TOTAL     (LOCKER_C_COUNT + LOCKER_B_COUNT + LOCKER_A_COUNT)

typedef struct locker_node
{
    int  loc_data;
    char locker_ID[LOCKER_ID_LEN];
    char locker_getID[LOCKER_CODE_LEN];
    char username[USERNAME_LEN];
    char small_phone[PHONE_LEN];
    char pickup_token[VG_TOKEN_LEN];
    struct locker_node *next;
} locker_node_t;

locker_node_t *locker_create_node(int loc_data, const char *locker_ID,
                                  const char *locker_getID,
                                  const char *username,
                                  const char *small_phone);

int locker_insert_head(locker_node_t **head, int loc_data,
                       const char *locker_ID, const char *locker_getID,
                       const char *username, const char *small_phone);

locker_node_t *locker_find_by_id(locker_node_t *head, const char *locker_ID);

locker_node_t *locker_find_by_code(locker_node_t *head, const char *locker_getID);

int locker_update_status(locker_node_t *node, int loc_data);

void locker_free_list(locker_node_t **head);

int locker_init_all(locker_node_t **head);

locker_node_t *locker_find_first_empty_by_prefix(locker_node_t *head,
                                                  const char *prefix);

int locker_clean_by_id(locker_node_t *head, const char *locker_ID);

/* ==================== 链表并发访问锁 ==================== */

/* 储物柜链表被主线程与扫码监控线程(pickup_monitor)并发访问，
 * 读写节点字段前需持锁；锁内禁止进行网络等耗时操作 */
void locker_lock(void);
void locker_unlock(void);

#endif /* DATA_STORE_H_ */
