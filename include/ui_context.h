#ifndef UI_CONTEXT_H_
#define UI_CONTEXT_H_

#include <pthread.h>
#include "touchpad.h"
#include "lcd_ui_display.h"

typedef struct
{
    lcd_context_t lcd;
    touchpad_context_t touch;
    volatile int current_page;
    pthread_mutex_t mutex;
    pthread_t ad_tid;
    volatile int ad_running;
} ui_context_t;

#endif