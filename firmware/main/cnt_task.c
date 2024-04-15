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
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "config.h"
#include "cnt_task.h"
#include "gui_task.h"
#include <math.h>

//
// Constants
//
#define CNT_EVAL_MSEC 50

// Red/Green LED PWM settings
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_HIGH_SPEED_MODE
#define LEDC_R_CHANNEL  LEDC_CHANNEL_0
#define LEDC_G_CHANNEL  LEDC_CHANNEL_1
#define LEDC_DUTY_RES   LEDC_TIMER_13_BIT
#define LEDC_R_ON_DUTY  (8191 * CONFIG_RED_PWM_PERCENT / 100)
#define LEDC_G_ON_DUTY  (8191 * CONFIG_GREEN_PWM_PERCENT / 100)
#define LEDC_FREQUENCY  5000

//
// Global variables
//
TaskHandle_t task_handle_cnt;


//
// Private variables
//
static const char* TAG = "cnt_task";

// Pulse counting
static volatile uint32_t pulse_count;
static volatile uint32_t history_buffer[60];
static volatile int history_buffer_index = 0;
static volatile int history_buffer_count = 0;

// Battery level flag - determines Green or Red LED on when no pulse indicated
static volatile bool low_batt = false;

// Timer handles
static esp_timer_handle_t periodic_timer;
static esp_timer_handle_t oneshot_timer;

// Shared data structure for gui_task to obtain current values
static count_status_t count_info;
static SemaphoreHandle_t count_info_mutex;


//
// Forward declarations
//
static void init_gpios();
static void handle_notifications();
static void IRAM_ATTR gpio_isr_handler(void* arg);
static void IRAM_ATTR periodic_timer_callback(void* arg);
static void IRAM_ATTR oneshot_timer_callback(void* arg);



//
// API
//
void cnt_task()
{
	ESP_LOGI(TAG, "Start task");
	
	init_gpios();
	
	// Access mutex
	count_info_mutex = xSemaphoreCreateMutex();
	
	// Once per second timer
	const esp_timer_create_args_t periodic_timer_args = {
		.callback = &periodic_timer_callback,
		.name = "periodic"
    };
	ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
	
	// Pulse LED blink timer
	const esp_timer_create_args_t oneshot_timer_args = {
		.callback = &oneshot_timer_callback,
		.name = "oneshot"
    };
	ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args, &oneshot_timer));
	
	// Start the periodic timer to evaluate once per second
	esp_timer_start_periodic(periodic_timer, (1000 * 1000));
	
	// Start catching pulses from the geiger-muller tube circuitry
	gpio_install_isr_service(/*ESP_INTR_FLAG_LEVEL4 | ESP_INTR_FLAG_EDGE | */ESP_INTR_FLAG_IRAM);
	gpio_isr_handler_add(CONFIG_PULSE_IN_PIN, gpio_isr_handler, (void*) CONFIG_PULSE_IN_PIN);
	
	while (1) {
		handle_notifications();
		
		vTaskDelay(pdMS_TO_TICKS(CNT_EVAL_MSEC));
	}
}


void get_counts(count_status_t* s)
{
	xSemaphoreTake(count_info_mutex, portMAX_DELAY);
	s->cpm = count_info.cpm;
	s->cps = count_info.cps;
	xSemaphoreGive(count_info_mutex);
}



//
// Internal Functions
//
static void init_gpios()
{
	// Pulse input
	gpio_reset_pin(CONFIG_PULSE_IN_PIN);
    gpio_set_direction(CONFIG_PULSE_IN_PIN, GPIO_MODE_INPUT);
    gpio_set_intr_type(CONFIG_PULSE_IN_PIN, GPIO_INTR_POSEDGE);
    
    // LEDC PWM timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
	
	// Red LED
    ledc_channel_config_t ledc_r_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_R_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = CONFIG_R_LED_PIN,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_r_channel));
	
	// Green LED - turned on by default assuming good battery
    ledc_channel_config_t ledc_g_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_G_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = CONFIG_G_LED_PIN,
        .duty           = LEDC_G_ON_DUTY,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_g_channel));
	
	// Blue LED
	gpio_reset_pin(CONFIG_B_LED_PIN);
    gpio_set_direction(CONFIG_B_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(CONFIG_B_LED_PIN, 0);
}


static void handle_notifications()
{
	uint32_t notification_value;
	
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
		if (Notification(notification_value, CNT_NOTIFY_GOOD_BATT_MASK)) {
			(void) ledc_set_duty(LEDC_MODE, LEDC_R_CHANNEL, 0);
			(void) ledc_set_duty(LEDC_MODE, LEDC_G_CHANNEL, LEDC_G_ON_DUTY);
			(void) ledc_update_duty(LEDC_MODE, LEDC_R_CHANNEL);
			(void) ledc_update_duty(LEDC_MODE, LEDC_G_CHANNEL);
			low_batt = false;
		}
		
		if (Notification(notification_value, CNT_NOTIFY_LOW_BATT_MASK)) {
			(void) ledc_set_duty(LEDC_MODE, LEDC_R_CHANNEL, LEDC_R_ON_DUTY);
			(void) ledc_set_duty(LEDC_MODE, LEDC_G_CHANNEL, 0);
			(void) ledc_update_duty(LEDC_MODE, LEDC_R_CHANNEL);
			(void) ledc_update_duty(LEDC_MODE, LEDC_G_CHANNEL);
			low_batt = true;
		}
	}
}


static void IRAM_ATTR gpio_isr_handler(void* arg)
{
	pulse_count += 1;
	
	// Trigger the LED blink if possible
	if (!esp_timer_is_active(oneshot_timer)) {
		// Turn off the current power LED
		if (low_batt) {
			(void) ledc_set_duty(LEDC_MODE, LEDC_R_CHANNEL, 0);
			(void) ledc_update_duty(LEDC_MODE, LEDC_R_CHANNEL);
		} else {
			(void) ledc_set_duty(LEDC_MODE, LEDC_G_CHANNEL, 0);
			(void) ledc_update_duty(LEDC_MODE, LEDC_G_CHANNEL);
		}
		
		// Turn on the blue LED
		gpio_set_level(CONFIG_B_LED_PIN, 1);
		
		// Trigger the oneshot timer
		esp_timer_start_once(oneshot_timer, (CONFIG_PULSE_BLINK_MSEC * 1000));
	}
}


static void IRAM_ATTR periodic_timer_callback(void* arg)
{
	static uint32_t prev_pulse_count = 0;
	int i;
	int n;
	int idx;
	int sum = 0;
	uint32_t cps;
	
	// Atomically get the count over the last second ("atomically" because we make one 32-bit
	// access to the ISR updated pulse_count)
	cps = pulse_count - prev_pulse_count;
	prev_pulse_count += cps;
	
	// Add the current count to 60 second history buffer
	history_buffer[history_buffer_index++] = cps;
	if (history_buffer_index >= 60) history_buffer_index = 0;
	if (history_buffer_count < 60) history_buffer_count += 1;
	
	// Determine how much of the history buffer to use in computing the CPM
	if (history_buffer_count < CONFIG_DELTA_DET_SAMPLES) {
		// Just starting up so do a quick and dirty computation for the first few seconds of runtime
		n = history_buffer_count;
	} else {
		// Get the trend over the last CONFIG_DELTA_DET_SAMPLES seconds.  We're looking for
		// big changes over this period which would indicate a high delta rate in radiation
		// levels (e.g. we're approaching or moving away from a radioactive source).
		idx = history_buffer_index - CONFIG_DELTA_DET_SAMPLES;
		if (idx < 0) idx += 60;
		sum = 0;
		for (i=0; i<(CONFIG_DELTA_DET_SAMPLES-1); i++) {
			n = idx + 1;
			if (n >= 60) n = 0;
			sum += history_buffer[n] - history_buffer[idx++];
			if (idx >= 60) idx = 0;
		}
//		printf("%d : ", sum);
		if (sum < -CONFIG_DELTA_DET_SAMPLES) {
			// Moving away: Use very small number of previous samples to quickly ignore previously large values
			n = CONFIG_DELTA_DET_SAMPLES - 2;
			history_buffer_count = n;
//			printf("-");
		} else if (sum > CONFIG_DELTA_DET_SAMPLES) {
			// Approaching: Use small number of samples to quickly start showing larger values
			n = CONFIG_DELTA_DET_SAMPLES;
			history_buffer_count = n;
//			printf("*");
		} else {
			// Not much change: Use the full history buffer for the most stable results
			n = history_buffer_count;
		}
	}
	
	// Set the starting entry
	idx = history_buffer_index - n;
	if (idx < 0) idx += 60;
//	printf("%d %d : ", n, idx);
	
	// Compute the CPS
	sum = 0;
	for (i=0; i<n; i++) {
		sum += history_buffer[idx++];
		if (idx >= 60) idx = 0;
	}
	sum *= 60 / n;
//	printf("%d\n", sum);

	// Update the shared information
	xSemaphoreTake(count_info_mutex, portMAX_DELAY);
	count_info.cpm = sum;   // Total over the past 60 seconds
	count_info.cps = cps;
	xSemaphoreGive(count_info_mutex);
	
	// Notify GUI task of updated values
	xTaskNotify(task_handle_gui, GUI_NOTIFY_NEW_COUNT_INFO, eSetBits);

}


static void IRAM_ATTR oneshot_timer_callback(void* arg)
{
	// Turn the Blue LED off
	gpio_set_level(CONFIG_B_LED_PIN, 0);
	
	// Turn the current power LED on
	if (low_batt) {
		(void) ledc_set_duty(LEDC_MODE, LEDC_R_CHANNEL, LEDC_R_ON_DUTY);
		(void) ledc_update_duty(LEDC_MODE, LEDC_R_CHANNEL);
	} else {
		(void) ledc_set_duty(LEDC_MODE, LEDC_G_CHANNEL, LEDC_G_ON_DUTY);
		(void) ledc_update_duty(LEDC_MODE, LEDC_G_CHANNEL);
	}
}
