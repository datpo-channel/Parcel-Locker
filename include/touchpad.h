#ifndef TOUCHPAD_H_
#define TOUCHPAD_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>

#ifdef __linux__
#include <linux/input.h>
#include <sys/time.h>
#else
/* Windows 交叉开发环境下的兼容性定义 */

#define EV_ABS 0x03

#define ABS_X 0x00
#define ABS_Y 0x01

struct input_event
{
    unsigned long tv_sec;
    unsigned long tv_usec;
    unsigned short type;
    unsigned short code;
    int value;
};

#endif

/* ==================== 可配置参数 ==================== */

/* 触摸屏设备文件路径 (可按需修改) */
#define TOUCHPAD_PATH "/dev/input/event0"

/* LCD 屏幕分辨率 (可按实际屏幕修改) */
#define LCD_WIDTH 800
#define LCD_HEIGHT 480

/* 触摸屏原始分辨率 (通常为 1024x600) */
#define TOUCH_WIDTH 1024
#define TOUCH_HEIGHT 600

/* 触摸坐标转换宏: 将触摸屏原始坐标映射到 LCD 坐标 */
#define TOUCH_X_TO_LCD(x) ((x) * LCD_WIDTH / TOUCH_WIDTH)
#define TOUCH_Y_TO_LCD(y) ((y) * LCD_HEIGHT / TOUCH_HEIGHT)

/* 坐标打包/解包基数 (pack: y*1000+x, unpack: x%1000, y/1000) */
#define COORD_PACK_BASE 1000

/* ==================== 数据结构 ==================== */

/* 触摸屏线程上下文: 在线程和主线程间共享坐标数据 */
typedef struct
{
    int fd;            /* 触摸屏设备文件描述符 */
    int ts_x;          /* 当前 X 坐标 (LCD 坐标) */
    int ts_y;          /* 当前 Y 坐标 (LCD 坐标) */
    volatile int touched;     /* 手指是否按下 (1=按下, 0=释放) */
    volatile int coord_ready; /* 新坐标是否可用 (1=有新坐标, 0=已读取) */
    volatile int running;     /* 线程运行标志 */
    pthread_t tid;     /* 线程 ID */
    pthread_mutex_t mutex; /* 坐标数据互斥锁 */
} touchpad_context_t;

/* ==================== 函数声明 ==================== */

/**************************************************************************
 *
 *   @brief : 打开触摸屏设备
 *   @arg   : dev_path  设备文件路径, 传 NULL 则使用默认路径 TOUCHPAD_PATH
 *
 *   @retval: 成功返回设备文件描述符 (>=0)
 *            失败返回 -1, 并输出错误信息到 stderr
 *   @note  : 以 O_RDWR 方式打开, 调用者负责最终调用 touch_close 关闭
 *
 ***************************************************************************/
int touch_open(const char *dev_path);

/**************************************************************************
 *
 *   @brief : 关闭触摸屏设备
 *   @arg   : fd  设备文件描述符
 *
 *   @retval: 无
 *   @note  : 仅关闭有效的文件描述符 (fd >= 0)
 *
 ***************************************************************************/
void touch_close(int fd);

/**************************************************************************
 *
 *   @brief : 读取一次触摸坐标 (阻塞方式, 打包输出)
 *   @arg   : fd  设备文件描述符
 *   @arg   : xy  打包坐标输出指针 (格式: y * COORD_PACK_BASE + x)
 *
 *   @retval: 成功返回 0
 *            失败返回 -1 (参数无效 或 read 出错)
 *   @note  : 内部阻塞等待 X 和 Y 坐标都就绪后才返回
 *            坐标已自动转换为 LCD 坐标
 *
 ***************************************************************************/
int touch_read(int fd, int *xy);

/**************************************************************************
 *
 *   @brief : 读取一次触摸坐标 (阻塞方式, 分别输出 x, y)
 *   @arg   : fd    设备文件描述符
 *   @arg   : ts_x  X 坐标输出指针
 *   @arg   : ts_y  Y 坐标输出指针
 *
 *   @retval: 成功返回 0
 *            失败返回 -1
 *   @note  : 坐标已自动转换为 LCD 坐标
 *            与 touch_read 功能等价, 仅输出格式不同
 *
 ***************************************************************************/
int touch_get_val(int fd, int *ts_x, int *ts_y);

/**************************************************************************
 *
 *   @brief : 将 XY 坐标打包为一个整数
 *   @arg   : x        X 坐标
 *   @arg   : y        Y 坐标
 *   @arg   : totalxy  打包结果输出指针
 *
 *   @retval: 打包后的坐标值 (y * COORD_PACK_BASE + x)
 *   @note  : 低三位为 X 坐标, 高位为 Y 坐标
 *
 ***************************************************************************/
int pack_coord(int x, int y, int *totalxy);

/**************************************************************************
 *
 *   @brief : 将打包坐标解包为 XY 坐标
 *   @arg   : totalxy  打包坐标值
 *   @arg   : x        X 坐标输出指针
 *   @arg   : y        Y 坐标输出指针
 *
 *   @retval: 无
 *   @note  : x = totalxy % COORD_PACK_BASE
 *            y = totalxy / COORD_PACK_BASE
 *
 ***************************************************************************/
void unpack_coord(int totalxy, int *x, int *y);

/**************************************************************************
 *
 *   @brief : 触摸屏轮询线程入口
 *   @arg   : arg  指向 touchpad_context_t 结构体的指针
 *
 *   @retval: NULL
 *   @note  : for(;;) 循环阻塞读取触摸坐标, 写入上下文结构体
 *            线程永不返回
 *
 ***************************************************************************/
void *touchpad_task(void *arg);

/**************************************************************************
 *
 *   @brief : 初始化触摸屏线程上下文
 *   @arg   : ctx  指向 touchpad_context_t 结构体的指针
 *   @arg   : fd   触摸屏设备文件描述符
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 初始化互斥锁和运行标志
 *
 ***************************************************************************/
int touchpad_init(touchpad_context_t *ctx, int fd);

/**************************************************************************
 *
 *   @brief : 启动触摸屏轮询线程
 *   @arg   : ctx  指向 touchpad_context_t 结构体的指针
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 线程启动后会持续读取触摸坐标并更新上下文
 *
 ***************************************************************************/
int touchpad_start(touchpad_context_t *ctx);

/**************************************************************************
 *
 *   @brief : 停止触摸屏轮询线程
 *   @arg   : ctx  指向 touchpad_context_t 结构体的指针
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 等待线程退出并释放资源
 *
 ***************************************************************************/
int touchpad_stop(touchpad_context_t *ctx);

/**************************************************************************
 *
 *   @brief : 线程安全地获取当前触摸坐标
 *   @arg   : ctx  指向 touchpad_context_t 结构体的指针
 *   @arg   : x    X 坐标输出指针
 *   @arg   : y    Y 坐标输出指针
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 使用互斥锁保护共享数据
 *
 ***************************************************************************/
int touchpad_get_coord(touchpad_context_t *ctx, int *x, int *y);

#endif /* T1_H */

/*
 * ==================== 使用示例 ====================
 *
 * // ---- 方式一: 单线程阻塞读取 ----
 * #include "t1.h"
 *
 * int main(void)
 * {
 *     int fd = touch_open(NULL);
 *     if (fd < 0) return -1;
 *
 *     int xy;
 *     while (1) {
 *         if (touch_read(fd, &xy) == 0) {
 *             int x, y;
 *             unpack_coord(xy, &x, &y);
 *             printf("touch: (%d, %d)\n", x, y);
 *         }
 *     }
 *
 *     touch_close(fd);
 *     return 0;
 * }
 *
 *
 * // ---- 方式二: 多线程方式 ----
 * #include "t1.h"
 *
 * int main(void)
 * {
 *     touchpad_context_t ctx;
 *     ctx.fd   = touch_open(NULL);
 *     if (ctx.fd < 0) return -1;
 *     ctx.ts_x = 0;
 *     ctx.ts_y = 0;
 *
 *     pthread_t tid;
 *     pthread_create(&tid, NULL, touchpad_task, &ctx);
 *
 *     while (1) {
 *         printf("touch: (%d, %d)\n", ctx.ts_x, ctx.ts_y);
 *         usleep(30000);
 *     }
 *
 *     touch_close(ctx.fd);
 *     return 0;
 * }
 *
 *
 * // ---- 方式三: 分别获取 x, y (不用打包) ----
 * #include "t1.h"
 *
 * int main(void)
 * {
 *     int fd = touch_open(NULL);
 *     if (fd < 0) return -1;
 *
 *     int x, y;
 *     while (1) {
 *         if (touch_get_val(fd, &x, &y) == 0) {
 *             printf("touch: (%d, %d)\n", x, y);
 *         }
 *     }
 *
 *     touch_close(fd);
 *     return 0;
 * }
 */