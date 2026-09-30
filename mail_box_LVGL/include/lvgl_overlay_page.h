#ifndef LVGL_OVERLAY_PAGE_H
#define LVGL_OVERLAY_PAGE_H

#ifdef __cplusplus
extern "C" {
#endif

void lvgl_pay_create(void);
void lvgl_pay_destroy(void);

void lvgl_success_create(int type);
void lvgl_success_destroy(void);

int  lvgl_overlay_get_result(void);
void lvgl_overlay_reset(void);

#ifdef __cplusplus
}
#endif

#endif