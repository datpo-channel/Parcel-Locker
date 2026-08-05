#include "touchpad.h"

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
int touch_open(const char *dev_path)
{
    const char *path;

    if (dev_path != NULL)
    {
        path = dev_path;
    }
    else
    {
        path = TOUCHPAD_PATH;
    }

    int fd = open(path, O_RDWR);
    if (fd == -1)
    {
        fprintf(stderr, "touch_open: open %s error: %s\n", path, strerror(errno));
        return -1;
    }

    return fd;
}

/**************************************************************************
 *
 *   @brief : 关闭触摸屏设备
 *   @arg   : fd  设备文件描述符
 *
 *   @retval: 无
 *   @note  : 仅关闭有效的文件描述符 (fd >= 0)
 *
 ***************************************************************************/
void touch_close(int fd)
{
    if (fd >= 0)
    {
        close(fd);
    }
}

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
int pack_coord(int x, int y, int *totalxy)
{
    *totalxy = y * COORD_PACK_BASE + x;
    return *totalxy;
}

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
void unpack_coord(int totalxy, int *x, int *y)
{
    *x = totalxy % COORD_PACK_BASE;
    *y = totalxy / COORD_PACK_BASE;
}

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
int touch_read(int fd, int *xy)
{
    struct input_event event;
    unsigned int touch_x = 0;
    unsigned int touch_y = 0;
    int x_ready = 0;
    int y_ready = 0;

    if (xy == NULL)
    {
        return -1;
    }

    memset(&event, 0, sizeof(event));

    while (!x_ready || !y_ready)
    {
        ssize_t ret = read(fd, &event, sizeof(event));
        if (ret != (ssize_t)sizeof(event))
        {
            return -1;
        }

        if (event.type == EV_ABS)
        {
            if (event.code == ABS_X)
            {
                touch_x = TOUCH_X_TO_LCD(event.value);
                x_ready = 1;
            }
            else if (event.code == ABS_Y)
            {
                touch_y = TOUCH_Y_TO_LCD(event.value);
                y_ready = 1;
            }
        }
    }

    pack_coord((int)touch_x, (int)touch_y, xy);

    return 0;
}

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
 *
 ***************************************************************************/
int touch_get_val(int fd, int *ts_x, int *ts_y)
{
    struct input_event event;
    int x_ready = 0;
    int y_ready = 0;

    if (ts_x == NULL || ts_y == NULL)
    {
        return -1;
    }

    memset(&event, 0, sizeof(event));

    while (!x_ready || !y_ready)
    {
        ssize_t ret = read(fd, &event, sizeof(event));
        if (ret != (ssize_t)sizeof(event))
        {
            return -1;
        }

        if (event.type == EV_ABS)
        {
            if (event.code == ABS_X)
            {
                *ts_x = TOUCH_X_TO_LCD(event.value);
                x_ready = 1;
            }
            else if (event.code == ABS_Y)
            {
                *ts_y = TOUCH_Y_TO_LCD(event.value);
                y_ready = 1;
            }
        }
    }

    return 0;
}

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
int touchpad_init(touchpad_context_t *ctx, int fd)
{
    if (ctx == NULL || fd < 0)
    {
        return -1;
    }

    ctx->fd = fd;
    ctx->ts_x = 0;
    ctx->ts_y = 0;
    ctx->touched = 0;
    ctx->coord_ready = 0;
    ctx->running = 0;

    if (pthread_mutex_init(&ctx->mutex, NULL) != 0)
    {
        fprintf(stderr, "touchpad_init: mutex init failed\n");
        return -1;
    }

    return 0;
}

/**************************************************************************
 *
 *   @brief : 启动触摸屏轮询线程
 *   @arg   : ctx  指向 touchpad_context_t 结构体的指针
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 线程启动后会持续读取触摸坐标并更新上下文
 *
 ***************************************************************************/
int touchpad_start(touchpad_context_t *ctx)
{
    if (ctx == NULL || ctx->fd < 0)
    {
        return -1;
    }

    ctx->running = 1;
    if (pthread_create(&ctx->tid, NULL, touchpad_task, ctx) != 0)
    {
        fprintf(stderr, "touchpad_start: pthread_create failed\n");
        ctx->running = 0;
        return -1;
    }

    return 0;
}

/**************************************************************************
 *
 *   @brief : 停止触摸屏轮询线程
 *   @arg   : ctx  指向 touchpad_context_t 结构体的指针
 *
 *   @retval: 成功返回 0，失败返回 -1
 *   @note  : 等待线程退出并释放资源
 *
 ***************************************************************************/
int touchpad_stop(touchpad_context_t *ctx)
{
    if (ctx == NULL)
    {
        return -1;
    }

    ctx->running = 0;

    if (ctx->tid != 0)
    {
        pthread_join(ctx->tid, NULL);
        ctx->tid = 0;
    }

    pthread_mutex_destroy(&ctx->mutex);

    return 0;
}

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
int touchpad_get_coord(touchpad_context_t *ctx, int *x, int *y)
{
    if (ctx == NULL || x == NULL || y == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&ctx->mutex);

    if (ctx->coord_ready)
    {
        *x = ctx->ts_x;
        *y = ctx->ts_y;
        ctx->coord_ready = 0;
        pthread_mutex_unlock(&ctx->mutex);
        return 0;
    }
    else
    {
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }
}

/**************************************************************************
 *
 *   @brief : 触摸屏轮询线程入口
 *   @arg   : arg  指向 touchpad_context_t 结构体的指针
 *
 *   @retval: NULL
 *   @note  : 循环读取触摸坐标，使用互斥锁保护共享数据
 *
 ***************************************************************************/
void *touchpad_task(void *arg)
{
    touchpad_context_t *ctx = (touchpad_context_t *)arg;
    struct input_event event;
    int x = 0, y = 0;
    int x_ready = 0, y_ready = 0;

    if (ctx == NULL)
    {
        return NULL;
    }

    while (ctx->running)
    {
        ssize_t ret = read(ctx->fd, &event, sizeof(event));
        if (ret != (ssize_t)sizeof(event))
        {
            usleep(10000);
            continue;
        }

        pthread_mutex_lock(&ctx->mutex);

        if (event.type == EV_ABS)
        {
            if (event.code == ABS_X)
            {
                x = TOUCH_X_TO_LCD(event.value);
                x_ready = 1;
            }
            else if (event.code == ABS_Y)
            {
                y = TOUCH_Y_TO_LCD(event.value);
                y_ready = 1;
            }

            if (x_ready && y_ready)
            {
                ctx->ts_x = x;
                ctx->ts_y = y;
            }
        }
        else if (event.type == EV_KEY && event.code == BTN_TOUCH)
        {
            ctx->touched = event.value;

            if (event.value == 0 && x_ready && y_ready)
            {
                ctx->coord_ready = 1;
            }
        }

        pthread_mutex_unlock(&ctx->mutex);
    }

    return NULL;
}