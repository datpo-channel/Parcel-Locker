#ifndef LVGL_STORE_PAGE_H_
#define LVGL_STORE_PAGE_H_

#include "lvgl.h"
#include "ui_context.h"

int lvgl_store_create(ui_context_t *ui, const char *bg_sjpg_path);
void lvgl_store_destroy(void);
int lvgl_store_get_action(ui_context_t *ui);
int lvgl_store_get_phone(char *phone, size_t phone_size);
int lvgl_store_get_code(char *code, size_t code_size);
int lvgl_store_get_box_size(void);
int lvgl_store_get_duration(void);

#endif