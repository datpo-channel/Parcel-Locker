#ifndef KEYBOARD_INPUT_H_
#define KEYBOARD_INPUT_H_

#define UI_KEY_DELETE    10
#define UI_KEY_CONFIRM   11
#define UI_KEY_QUERY     12
#define UI_KEY_BACK      13

/**************************************************************************
 *
 *   @brief : 根据触摸坐标判断用户点击的按键
 *   @arg   : x  触摸点 X 坐标
 *   @arg   : y  触摸点 Y 坐标
 *
 *   @retval: 返回 0-9  对应数字键
 *            返回 UI_KEY_DELETE  删除键
 *            返回 UI_KEY_CONFIRM 确认键
 *            返回 -1 无效区域
 *   @note  : 删除键位于数字键盘右上，确认键位于数字键盘右下
 *
 ***************************************************************************/
int get_key_from_touch(int x, int y);

#endif