/*
 * Geiger circuitry pulse counting task.
 *   - Counts pulses from geiger muller tube circuitry
 *   - Generates instantaneous Counts Per Second (CPS) values
 *   - Generates dynamically sized average Counts Per Minute (CPM) values
 *   - Controls 3 LED output channels
 *      - Blue LED pulsed for ~20mSec for each geiger tube pulse
 *      - Red/Green PWM intensity controlled LEDs for power indication
 *        (Green - Batt OK, Red - Batt Low)
 *   - Sends notification to gui_task once/second with updated CPS/CPM information
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
#ifndef CNT_TASK_H
#define CNT_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/task.h"

//
// Constants
//

// Task notifications
#define CNT_NOTIFY_GOOD_BATT_MASK 0x00000001
#define CNT_NOTIFY_LOW_BATT_MASK  0x00000002



//
// Typedefs
//
typedef struct {
	uint32_t cpm;
	uint32_t cps;
} count_status_t;



//
// Global variables
//
extern TaskHandle_t task_handle_cnt;



//
// API
//
void cnt_task();
void get_counts(count_status_t* s);

#endif /* CNT_TASK_H */
