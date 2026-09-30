#ifndef UI_LOGIC_H_
#define UI_LOGIC_H_

#include "ui_context.h"
#include "ret_codes.h"
#include "data_store.h"

/**************************************************************************
 *
 *   @brief : 初始化 UI 上下文
 *   @arg   : ui       指向 ui_context_t 结构体的指针
 *
 *   @retval: 成功返回 0，失败返回 -1
 *
 ***************************************************************************/
int ui_init(ui_context_t *ui);

/**************************************************************************
 *
 *   @brief : 启动 UI 系统
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *
 *   @retval: 成功返回 0，失败返回 -1
 *
 ***************************************************************************/
int ui_start(ui_context_t *ui);

/**************************************************************************
 *
 *   @brief : 停止 UI 系统
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *
 *   @retval: 成功返回 0，失败返回 -1
 *
 ***************************************************************************/
int ui_stop(ui_context_t *ui);

locker_node_t *ui_get_locker_head(void);

/**************************************************************************
 *
 *   @brief : 轮询等待 overlay 结果页（支付/取件成功/存件成功共用）
 *   @arg   : ui          指向 ui_context_t 结构体的指针
 *   @arg   : create_fn   创建 overlay 页面（无参回调）
 *   @arg   : destroy_fn  销毁 overlay 页面
 *   @arg   : page_name   页面名称（仅用于日志）
 *
 *   @retval: 1  用户点击确认（已支付/继续存件）
 *            0  用户取消 / 页面超时 / 扫码取件中断
 *   @note  : 内部轮询 lv_timer_handler 与 lvgl_overlay_get_result，
 *            超时 PAGE_TIMEOUT_SEC 秒或检测到扫码通知时自动退出
 *
 ***************************************************************************/
int ui_wait_overlay(ui_context_t *ui, void (*create_fn)(void),
                    void (*destroy_fn)(void), const char *page_name);

#endif