#ifndef MAIN_H_
#define MAIN_H_

/* main.c 集中引用头文件：标准库、项目模块、LVGL 与板级驱动 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "ui_logic.h"
#include "takeout_page.h"
#include "store_page.h"
#include "login_page.h"
#include "query_page.h"
#include "pickup_monitor.h"
#include "data_store.h"
#include "verify_gate.h"
#include "qr_jpeg.h"
#include "net_client.h"
#include "weather_utils.h"
#include "utils.h"
#include "lvgl.h"
#include "display.h"
#include "input.h"
#include "lvgl_main_menu.h"
#include "lvgl_takeout_page.h"
#include "ret_codes.h"
#include "config.h"

#endif /* _MAIN_H_ */
