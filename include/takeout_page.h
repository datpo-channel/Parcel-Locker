#ifndef TAKEOUT_PAGE_H_
#define TAKEOUT_PAGE_H_

#include "ui_context.h"

/**************************************************************************
 *
 *   @brief : 显示取件码输入界面，等待用户输入4位取件码
 *   @arg   : ui   指向 ui_context_t 结构体的指针
 *   @arg   : code 输出缓冲区，用于存储输入的4位取件码
 *
 *   @retval: RET_TAKEOUT_OK(0)    成功输入4位取件码
 *            RET_TAKEOUT_BACK(1)  用户点击返回键
 *            RET_TAKEOUT_QUERY(2) 用户点击查询键
 *            -1                   参数错误
 *   @note  : 阻塞等待，输入完成或用户点击侧边键后返回
 *            code 需至少5字节空间，仅在返回 RET_TAKEOUT_OK 时有效
 *
 ***************************************************************************/
int show_takeout_code(ui_context_t *ui, char *code);

/**************************************************************************
 *
 *   @brief : 在LCD上显示取件码，等待用户确认
 *   @arg   : ui    指向 ui_context_t 结构体的指针
 *   @arg   : code  4位取件码字符串
 *
 *   @retval: 无
 *   @note  : 显示 takeout_code.jpg 背景 + 4位数字
 *            等待用户触摸任意位置后返回
 *
 ***************************************************************************/
void show_pickup_code(ui_context_t *ui, const char *code);

/**************************************************************************
 *
 *   @brief : 取件成功后显示成功页面，等待用户选择继续取件或返回首页
 *   @arg   : ui    指向 ui_context_t 结构体的指针
 *
 *   @retval: 1  用户点击"继续取件"
 *            0  用户点击"返回首页"
 *   @note  : 显示 takeout_success.jpg 背景
 *
 ***************************************************************************/
int show_takeout_success(ui_context_t *ui);

#endif