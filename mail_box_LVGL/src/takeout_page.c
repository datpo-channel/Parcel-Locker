#include <stdio.h>
#include "takeout_page.h"
#include "pickup_monitor.h"
#include "lvgl.h"
#include "lvgl_overlay_page.h"
#include "ui_logic.h"

static void _takeout_success_create(void)
{
    lvgl_success_create(1);
}

int show_takeout_success(ui_context_t *ui)
{
    return ui_wait_overlay(ui, _takeout_success_create,
                           lvgl_success_destroy, "取件成功页面");
}
