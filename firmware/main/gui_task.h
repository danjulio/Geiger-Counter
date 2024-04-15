/*
 * GUI control task
 *  - Controls display on LCD
 *  - Debounces push buttons (short/long press capable)
 *  - Battery voltage monitoring
 *    - Low Battery detection
 *    - Charge detection
 *  - Sends Batt OK/Batt Low notifications to cnt_task to control power LEDs
 *
 * Copyright 2024 Dan Julio
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * It is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef GUI_TASK_H
#define GUI_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/task.h"


//
// Constants
//

// Task notifications
#define GUI_NOTIFY_NEW_COUNT_INFO 0x00000001



//
// Global variables
//
extern TaskHandle_t task_handle_gui;



//
// API
//
void gui_task();

#endif /* GUI_TASK_H */