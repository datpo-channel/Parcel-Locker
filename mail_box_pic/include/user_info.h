#ifndef USER_INFO_H_
#define USER_INFO_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USER_NAME_LEN    10
#define USER_PASSWD_LEN  10
#define USER_PHONE_LEN   14

#define COURIER_DATA_PATH "resource/couriers.txt"

typedef struct user_node
{
    char username[USER_NAME_LEN];
    char password[USER_PASSWD_LEN];
    char small_phone[USER_PHONE_LEN];
    struct user_node *next;
} user_node_t;

/**************************************************************************
 *
 *   @brief : 创建用户节点
 *   @arg   : username    用户名
 *   @arg   : password    密码
 *   @arg   : small_phone 手机号码
 *
 *   @retval: 成功返回新节点指针，失败返回 NULL
 *   @note  : 调用者负责最终调用 user_free_list 释放
 *
 ***************************************************************************/
user_node_t *user_create_node(const char *username,
                              const char *password,
                              const char *small_phone);

/**************************************************************************
 *
 *   @brief : 头插法插入用户节点
 *   @arg   : head        指向链表头指针的指针
 *   @arg   : username    用户名
 *   @arg   : password    密码
 *   @arg   : small_phone 手机号码
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 新节点插在链表最前面
 *
 ***************************************************************************/
int user_insert_head(user_node_t **head, const char *username,
                     const char *password, const char *small_phone);

/**************************************************************************
 *
 *   @brief : 根据手机号码查找用户
 *   @arg   : head        链表头指针
 *   @arg   : small_phone 要查找的手机号码
 *
 *   @retval: 找到返回节点指针，未找到返回 NULL
 *   @note  : 返回第一个匹配的节点
 *
 ***************************************************************************/
user_node_t *user_find_by_phone(user_node_t *head, const char *small_phone);

/**************************************************************************
 *
 *   @brief : 验证用户登录
 *   @arg   : head        链表头指针
 *   @arg   : small_phone 手机号码
 *   @arg   : password    密码
 *
 *   @retval: 验证成功返回用户节点指针，失败返回 NULL
 *   @note  : 手机号和密码都匹配才算成功
 *
 ***************************************************************************/
user_node_t *user_verify_login(user_node_t *head, const char *small_phone,
                               const char *password);

/**************************************************************************
 *
 *   @brief : 从链表中移除指定用户节点并释放
 *   @arg   : head  指向链表头指针的指针
 *   @arg   : node  要移除的节点指针
 *
 *   @retval: 成功返回 0，失败返回 -1
 *
 ***************************************************************************/
int user_remove(user_node_t **head, user_node_t *node);

/**************************************************************************
 *
 *   @brief : 打印链表中所有用户信息
 *   @arg   : head  链表头指针
 *
 *   @retval: 无
 *   @note  : 仅用于调试输出
 *
 ***************************************************************************/
void user_print_all(user_node_t *head);

/**************************************************************************
 *
 *   @brief : 释放整个链表
 *   @arg   : head  指向链表头指针的指针
 *
 *   @retval: 无
 *   @note  : 释放后 *head 置 NULL，可安全重复调用
 *
 ***************************************************************************/
void user_free_list(user_node_t **head);

/**************************************************************************
 *
 *   @brief : 从文件加载快递员手机号列表
 *   @arg   : head      指向链表头指针的指针
 *   @arg   : filepath  文件路径，传 NULL 使用默认 COURIER_DATA_PATH
 *
 *   @retval: 成功返回加载的数量，失败返回 -1
 *   @note  : 文件格式：每行一个手机号
 *            文件不存在时返回 0（空链表），不视为错误
 *
 ***************************************************************************/
int user_load_from_file(user_node_t **head, const char *filepath);

/**************************************************************************
 *
 *   @brief : 将快递员手机号列表保存到文件
 *   @arg   : head      链表头指针
 *   @arg   : filepath  文件路径，传 NULL 使用默认 COURIER_DATA_PATH
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 覆盖写入，每行一个手机号
 *
 ***************************************************************************/
int user_save_to_file(user_node_t *head, const char *filepath);

#endif