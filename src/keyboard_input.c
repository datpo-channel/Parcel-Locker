#include "keyboard_input.h"

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
int get_key_from_touch(int x, int y)
{
    if (x >= 496 && x <= 527)
    {
        if (y >= 312 && y <= 401)
        {
            return 1;
        }
        if (y >= 196 && y <= 286)
        {
            return 2;
        }
        if (y >= 81 && y <= 171)
        {
            return 3;
        }
    }
    else if (x >= 538 && x <= 568)
    {
        if (y >= 312 && y <= 401)
        {
            return 4;
        }
        if (y >= 196 && y <= 286)
        {
            return 5;
        }
        if (y >= 81 && y <= 171)
        {
            return 6;
        }
    }
    else if (x >= 580 && x <= 610)
    {
        if (y >= 312 && y <= 401)
        {
            return 7;
        }
        if (y >= 196 && y <= 286)
        {
            return 8;
        }
        if (y >= 81 && y <= 171)
        {
            return 9;
        }
    }
    else if (x >= 621 && x <= 652)
    {
        if (y >= 301 && y <= 401)
        {
            return UI_KEY_CONFIRM;
        }
        if (y >= 196 && y <= 286)
        {
            return 0;
        }
        if (y >= 81 && y <= 171)
        {
            return UI_KEY_DELETE;
        }
    }

    if (x >= 732 && x <= 779 && y >= 25 && y <= 458)
    {
        return UI_KEY_QUERY;
    }

    if (x >= 17 && x <= 62 && y >= 427 && y <= 472)
    {
        return UI_KEY_BACK;
    }

    return -1;
}