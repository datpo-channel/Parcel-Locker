#ifndef LOCKER_INFO_H_
#define LOCKER_INFO_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "verify_gate.h"

#define LOCKER_ID_LEN    11
#define LOCKER_CODE_LEN   5
#define USERNAME_LEN     10
#define PHONE_LEN        14

#define LOCKER_EMPTY     0
#define LOCKER_OCCUPIED  1

#define LOCKER_TOTAL     30

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

/**************************************************************************
 *
 *   @brief : 创建储物柜节点
 *   @arg   : loc_data    储物柜状态 (LOCKER_EMPTY / LOCKER_OCCUPIED)
 *   @arg   : locker_ID   储物柜编号
 *   @arg   : locker_getID 取件码
 *   @arg   : username    用户名
 *   @arg   : small_phone 手机号码
 *
 *   @retval: 成功返回新节点指针，失败返回 NULL
 *   @note  : 调用者负责最终调用 locker_free_list 释放
 *
 ***************************************************************************/
locker_node_t *locker_create_node(int loc_data, const char *locker_ID,
                                  const char *locker_getID,
                                  const char *username,
                                  const char *small_phone);

/**************************************************************************
 *
 *   @brief : 头插法插入节点
 *   @arg   : head        指向链表头指针的指针
 *   @arg   : loc_data    储物柜状态
 *   @arg   : locker_ID   储物柜编号
 *   @arg   : locker_getID 取件码
 *   @arg   : username    用户名
 *   @arg   : small_phone 手机号码
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 新节点插在链表最前面
 *
 ***************************************************************************/
int locker_insert_head(locker_node_t **head, int loc_data,
                       const char *locker_ID, const char *locker_getID,
                       const char *username, const char *small_phone);

/**************************************************************************
 *
 *   @brief : 根据储物柜编号查找节点
 *   @arg   : head       链表头指针
 *   @arg   : locker_ID  要查找的储物柜编号
 *
 *   @retval: 找到返回节点指针，未找到返回 NULL
 *   @note  : 返回的是链表中的节点，不要直接释放
 *
 ***************************************************************************/
locker_node_t *locker_find_by_id(locker_node_t *head, const char *locker_ID);

/**************************************************************************
 *
 *   @brief : 根据手机号码查找节点
 *   @arg   : head        链表头指针
 *   @arg   : small_phone 要查找的手机号码
 *
 *   @retval: 找到返回节点指针，未找到返回 NULL
 *   @note  : 返回第一个匹配的节点
 *
 ***************************************************************************/
locker_node_t *locker_find_by_phone(locker_node_t *head, const char *small_phone);

/**************************************************************************
 *
 *   @brief : 根据取件码查找节点
 *   @arg   : head         链表头指针
 *   @arg   : locker_getID 要查找的取件码
 *
 *   @retval: 找到返回节点指针，未找到返回 NULL
 *   @note  : 用于取件时根据验证码定位储物柜
 *
 ***************************************************************************/
locker_node_t *locker_find_by_code(locker_node_t *head, const char *locker_getID);

/**************************************************************************
 *
 *   @brief : 更新储物柜状态
 *   @arg   : node      指向储物柜节点的指针
 *   @arg   : loc_data  新状态 (LOCKER_EMPTY / LOCKER_OCCUPIED)
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 同时清空用户信息（设为空状态时）
 *
 ***************************************************************************/
int locker_update_status(locker_node_t *node, int loc_data);

/**************************************************************************
 *
 *   @brief : 从链表中移除指定节点并释放
 *   @arg   : head  指向链表头指针的指针
 *   @arg   : node  要移除的节点指针
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 仅移除有货物的储物柜时需先取走货物
 *
 ***************************************************************************/
int locker_remove(locker_node_t **head, locker_node_t *node);

/**************************************************************************
 *
 *   @brief : 统计指定状态的储物柜数量
 *   @arg   : head      链表头指针
 *   @arg   : loc_data  储物柜状态
 *
 *   @retval: 符合条件的储物柜数量
 *   @note  : 可用于判断是否有空闲柜
 *
 ***************************************************************************/
int locker_count_by_status(locker_node_t *head, int loc_data);

/**************************************************************************
 *
 *   @brief : 打印链表中所有储物柜信息
 *   @arg   : head  链表头指针
 *
 *   @retval: 无
 *   @note  : 仅用于调试输出
 *
 ***************************************************************************/
void locker_print_all(locker_node_t *head);

/**************************************************************************
 *
 *   @brief : 释放整个链表
 *   @arg   : head  指向链表头指针的指针
 *
 *   @retval: 无
 *   @note  : 释放后 *head 置 NULL，可安全重复调用
 *
 ***************************************************************************/
void locker_free_list(locker_node_t **head);

/**************************************************************************
 *
 *   @brief : 初始化所有储物柜节点
 *   @arg   : head   指向链表头指针的指针
 *   @arg   : count  储物柜总数
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 创建 count 个空柜节点，编号为 A01-A15, B01-B10, C01-C05
 *            调用前 *head 应为 NULL
 *
 ***************************************************************************/
int locker_init_all(locker_node_t **head, int count);

/**************************************************************************
 *
 *   @brief : 查找第一个空闲储物柜
 *   @arg   : head  链表头指针
 *
 *   @retval: 找到返回节点指针，未找到返回 NULL
 *   @note  : 返回第一个 loc_data == LOCKER_EMPTY 的节点
 *
 ***************************************************************************/
locker_node_t *locker_find_first_empty(locker_node_t *head);

/**************************************************************************
 *
 *   @brief : 分配空闲储物柜给用户
 *   @arg   : head        链表头指针
 *   @arg   : locker_getID 取件码
 *   @arg   : small_phone 手机号码
 *
 *   @retval: 成功返回分配的节点指针，失败返回 NULL
 *   @note  : 查找第一个空闲柜，设置为 LOCKER_OCCUPIED
 *            并填入取件码和手机号
 *
 ***************************************************************************/
locker_node_t *locker_assign(locker_node_t *head,
                             const char *locker_getID,
                             const char *small_phone);

/**************************************************************************
 *
 *   @brief : 根据ID前缀查找第一个空闲储物柜
 *   @arg   : head    链表头指针
 *   @arg   : prefix  ID前缀 ("A"/"B"/"C")
 *
 *   @retval: 找到返回节点指针，未找到返回 NULL
 *   @note  : 用于按箱体大小分配柜子
 *
 ***************************************************************************/
locker_node_t *locker_find_first_empty_by_prefix(locker_node_t *head,
                                                  const char *prefix);

/**************************************************************************
 *
 *   @brief : 根据储物柜编号清空指定储物柜
 *   @arg   : head      链表头指针
 *   @arg   : locker_ID 储物柜编号
 *
 *   @retval: 成功返回 1，失败返回 0
 *   @note  : 查找指定编号的储物柜并清空除编号外的所有信息
 *            状态置为 LOCKER_EMPTY，用户名、手机号、取件码清空
 *
 ***************************************************************************/
int locker_clean_by_id(locker_node_t *head, const char *locker_ID);

#endif