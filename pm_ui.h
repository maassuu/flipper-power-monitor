/**
 * @file pm_ui.h
 * Drawing of all views on the 128x64 display.
 * Called from the GUI thread with the application mutex held. The GUI draws
 * into a frame buffer and commits it at once, so there is no flicker.
 */
#pragma once

#include <gui/canvas.h>
#include "power_monitor_i.h"

/** Number of text rows visible in list views. */
#define PM_UI_LIST_ROWS (5u)

/**
 * Draw the current view. List views clamp app->scroll[view] to the valid
 * range, so input handling may simply increment/decrement it.
 */
void pm_ui_draw(Canvas* canvas, PowerMonitorApp* app, uint32_t now_ms);
