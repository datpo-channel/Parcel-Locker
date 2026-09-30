#ifndef QUERY_PAGE_H_
#define QUERY_PAGE_H_

#include "ui_context.h"

/**************************************************************************
 *
 *   @brief : 显示快件查询界面，等待用户输入手机号和验证码进行查询
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *
 *   @retval: RET_QUERY_OK(0)     查询完成，显示取件码/无包裹结果后返回
 *            RET_QUERY_BACK(1)   用户点击了返回键
 *            RET_TIMEOUT(-2)     操作超时
 *            RET_LOGIN_FAILED(-4) 验证码错误次数过多
 *            RET_SCAN_PICKUP(5)  扫码取件通知中断
 *            -1                  参数错误
 *
 ***************************************************************************/
int show_received_query(ui_context_t *ui);

#endif