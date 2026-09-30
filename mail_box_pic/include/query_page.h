#ifndef QUERY_PAGE_H_
#define QUERY_PAGE_H_

#include "ui_context.h"

/**************************************************************************
 *
 *   @brief : 显示快件查询界面，等待用户输入手机号和验证码进行查询
 *   @arg   : ui  指向 ui_context_t 结构体的指针
 *
 *   @retval: RET_QUERY_OK(0)     查询完成，显示送达状态后返回
 *            RET_QUERY_BACK(1)   用户点击了返回键
 *            -1                  参数错误
 *
 ***************************************************************************/
int show_received_query(ui_context_t *ui);

#endif