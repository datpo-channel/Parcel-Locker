#ifndef LVGL_QUERY_PAGE_H_
#define LVGL_QUERY_PAGE_H_

#include "lvgl.h"
#include "ui_context.h"

int lvgl_query_create(ui_context_t *ui, const char *bg_sjpg_path);
void lvgl_query_destroy(void);
void lvgl_query_clear_code(void);
int lvgl_query_get_action(ui_context_t *ui);
int lvgl_query_has_action(void);
int lvgl_query_get_phone(char *phone, size_t phone_size);
int lvgl_query_get_code(char *code, size_t code_size);

#endif /* LVGL_QUERY_PAGE_H_ */