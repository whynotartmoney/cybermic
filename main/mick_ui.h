#pragma once

#include "mick_field.h"

// Own screens for the moving-art application. Call only from the LVGL task
// or while holding bsp_lvgl_lock().
void mick_ui_create(const mick_field_t *field, int battery_soc);
void mick_ui_sync(const mick_field_t *field, int battery_soc);
