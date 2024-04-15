/*
 * Geiger Counter for TTGO Lilygo ESP32 dev board
 *   - 135 x 240 pixel 16-bit TFT display
 *   - 2 buttons
 *   - LVGL based GUI
 *   - Audio mute control for external clicker circuit
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "cnt_task.h"
#include "gui_task.h"


static const char* TAG = "main";


void app_main(void)
{
    ESP_LOGI(TAG, "Geiger starting");
    
    // Start tasks
    //   Core 0 : PRO
    //   Core 1 : APP
	
	xTaskCreatePinnedToCore(&cnt_task,  "cnt_task",  2560, NULL, 1, &task_handle_cnt,  0);
    xTaskCreatePinnedToCore(&gui_task,  "gui_task",  2560, NULL, 3, &task_handle_gui,  1);
}
