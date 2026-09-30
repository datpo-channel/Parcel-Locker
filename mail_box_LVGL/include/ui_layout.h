#ifndef UI_LAYOUT_H_
#define UI_LAYOUT_H_

#include "display.h"

/**************************************************************************
 *
 * 屏幕布局常量：屏幕适配的唯一改动点
 *
 * 设计原则：
 *  - 屏幕基准尺寸引用 display.h 的 LCD_WIDTH/LCD_HEIGHT，
 *    换屏时只需修改 display.h，各页面布局随之适配；
 *  - 键盘区（3×4 数字键盘）贴屏幕右缘，宽度固定 319px，
 *    水平位置随屏幕宽度自适应（UI_KEYPAD_X = 宽 - 键盘宽）；
 *  - 各页面内的具体控件坐标基于背景图（800×480 设计图）对齐，
 *    属于业务布局坐标，不在屏幕适配范围；控件相对于屏幕边缘/键盘区的
 *    坐标一律使用本头文件的宏推导。
 *
 ***************************************************************************/

/* 屏幕基准尺寸（与 lv_disp_drv 注册分辨率保持一致） */
#define UI_SCREEN_W  LCD_WIDTH
#define UI_SCREEN_H  LCD_HEIGHT

/* 右侧键盘区：宽 319px（3 列按键 + 边距），高度铺满，贴右缘 */
#define UI_KEYPAD_W   319
#define UI_KEYPAD_H   UI_SCREEN_H
#define UI_KEYPAD_X   (UI_SCREEN_W - UI_KEYPAD_W)
#define UI_KEYPAD_Y   0

#endif /* UI_LAYOUT_H_ */
