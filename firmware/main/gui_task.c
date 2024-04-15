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
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_adc_cal.h"
#include "esp_freertos_hooks.h"
#include "esp_log.h"
#include "esp_lcd_backlight.h"
#include "esp_timer.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "disp_spi.h"
#include "disp_driver.h"
#include "config.h"
#include "cnt_task.h"
#include "gui_task.h"
#include "lvgl/lvgl.h"
#include <math.h>
#include <stdio.h>


//
// Constants
//
#define GUI_EVAL_MSEC              10

// LVGL sub-task indicies
#define LVGL_ST_EVENT              0
#define LVGL_ST_BTN_DEBOUNCE       1
#define LVGL_ST_BATT_CHECK         2
#define LVGL_ST_NUM                3

// Operating mode state
#define MODE_MEASURE               0
#define MODE_ACCUMULATE            1
#define NUM_MODES                  2

// Backlight brightness levels
#define BACKLIGHT_FULL_PERCENT     100
#define BACKLIGHT_HALF_PERCENT     33

// Power states
#define POWER_ST_GOOD_BATT         0
#define POWER_ST_LOW_BATT          1
#define POWER_ST_CHARGE            2

// Button state
#define BUTTON_ST_NOT_PRESSED      0
#define BUTTON_ST_PRESSED          1
#define BUTTON_ST_LONG_PRESSED     2

// Button state array indidies
#define BUTTON_LEFT_INDEX          0
#define BUTTON_RIGHT_INDEX         1
#define BUTTON_COUNT               2

//
// Global variables
//
TaskHandle_t task_handle_gui;



//
// Typedefs
//
typedef struct {
	bool button_prev;
	bool button_down;
	int button_pin;
	int button_state;
	uint32_t button_timestamp;
	uint32_t button_long_msec;
} button_state_t;



//
// Private variables
//
static const char* TAG = "gui_task";

// Dual display update buffers to allow DMA/SPI transfer of one while the other is updated
static lv_color_t lvgl_disp_buf1[CONFIG_LVGL_DISP_BUF_SIZE];
static lv_color_t lvgl_disp_buf2[CONFIG_LVGL_DISP_BUF_SIZE];
static lv_disp_buf_t lvgl_disp_buf;

// Display driver
static lv_disp_drv_t lvgl_disp_drv;

// LVGL sub-task array
static lv_task_t* lvgl_tasks[LVGL_ST_NUM];

// LVGL display objects
static lv_obj_t* screen;
static lv_obj_t* lbl_mute;
static lv_obj_t* lbl_batt;
static lv_obj_t* gauge;
static lv_obj_t* lbl_rt_count;
static lv_obj_t* lbl_rt_dose;

static lv_obj_t* container;
static lv_obj_t* lbl_cum_info;
static lv_obj_t* lbl_cum_time;
static lv_obj_t* lbl_cum_counts;
static lv_obj_t* lbl_cum_dose;

static lv_obj_t* lbl_l_btn;
static lv_obj_t* lbl_r_btn;

// Accumulated values
static int cum_count;
static float cum_dose;
static uint32_t cum_start_timestamp;
static int accum_interval_index;

#define NUM_ACCUM_INTERVALS 5
static const uint32_t accum_int_min[NUM_ACCUM_INTERVALS] = {10, 30, 60, 360, 1440};

// Backlight
static const disp_backlight_config_t backlight_config = {
	true,                     // Use PWM
	false,                    // Don't invert
	CONFIG_LCD_DISP_PIN_BL,   // GPIO
	0,                        // Timer 0
	0                         // LEDC 0
};
static disp_backlight_h disp_backlight_handle;

// Button state
static button_state_t button_state[BUTTON_COUNT];

// Gauge ranges
#define NUM_GAUGE_RANGES 5
static int cur_gauge_range;
static const uint32_t gauge_cpm_threshold[NUM_GAUGE_RANGES-1] = {100,  1000, 10000,  60000};
static const int16_t  gauge_max_value[NUM_GAUGE_RANGES]       = {100,  1000, 10000,  1000,  10000};
static const bool     gauge_is_cpm[NUM_GAUGE_RANGES]          = {true, true, true,   false, false};

// Mode
static int mode;

// Mute state
static bool audio_muted;

// Power state
static int power_state;
static int power_state_prev;

// GPIO to ADC1 Channel mapping
static const adc1_channel_t gpio_2_adc_ch[40] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1,  4,  5,  6,  7,  0,  1,  2,  3
};
static 	int batt_adc_ch;

// ADC Characterization
static esp_adc_cal_characteristics_t adc_cal_chars;



//
// Forward declarations
//
static void gui_init_gpios();
static bool gui_lvgl_init();
static void gui_state_init();
static void gui_screen_init();
static void gui_add_subtasks();
static void gui_task_event_handler_task(lv_task_t * task);
static void gui_task_btn_handler_task(lv_task_t * task);
static void gui_task_batt_handler_task(lv_task_t * task);
static void gui_update_button_info();
static void gui_update_count_info();
static void gui_update_mute_info();
static void gui_update_power_info();
static void IRAM_ATTR lv_tick_callback();
static void eval_button(button_state_t* bs, bool* short_press, bool* long_press);
static uint32_t get_uptime_msec();
static int batt_v_to_power_state(float v);
static float get_batt_v();
static void reset_accumulation();



//
// API
//
void gui_task()
{
	ESP_LOGI(TAG, "Start task");

	// Initialize the IO that this task uses
	gui_init_gpios();
	
	// Initialize LVGL
	if (!gui_lvgl_init()) {
		vTaskDelete(NULL);
	}
	gui_state_init();
	gui_screen_init();
	gui_add_subtasks();
	
	while (1) {
		// This task runs every GUI_EVAL_MSEC mSec
		vTaskDelay(pdMS_TO_TICKS(GUI_EVAL_MSEC));
		lv_task_handler();
	}
}



//
// Internal Functions
//
static void gui_init_gpios()
{
	esp_adc_cal_value_t val_type;
	
	// LCD backlight
	disp_backlight_handle = disp_backlight_new(&backlight_config);
	if (disp_backlight_handle) {
		disp_backlight_set(disp_backlight_handle, BACKLIGHT_FULL_PERCENT);
	}
	
	// Button inputs
	gpio_reset_pin(CONFIG_L_BTN_PIN);
    gpio_set_direction(CONFIG_L_BTN_PIN, GPIO_MODE_INPUT);
    gpio_reset_pin(CONFIG_R_BTN_PIN);
    gpio_set_direction(CONFIG_R_BTN_PIN, GPIO_MODE_INPUT);
	
	// Audio mute output
	gpio_reset_pin(CONFIG_MUTEL_PIN);
    gpio_set_direction(CONFIG_MUTEL_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(CONFIG_MUTEL_PIN, 1);       // Mute disabled (active low)
	
	// Power sense output
	gpio_reset_pin(CONFIG_PWR_SNS_EN_PIN);
    gpio_set_direction(CONFIG_PWR_SNS_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(CONFIG_PWR_SNS_EN_PIN, 1);  // Enable power sense circuitry
	
	// Power sense input ADC configuration
	batt_adc_ch = gpio_2_adc_ch[CONFIG_PWR_SNS_ADC_PIN];
	adc1_config_width(ADC_WIDTH_BIT_12);
	adc1_config_channel_atten(batt_adc_ch, CONFIG_ESP_ADC_ATTEN);
	
	// Characterize ADC1 for highest accuracy
	val_type = esp_adc_cal_characterize(ADC_UNIT_1, CONFIG_ESP_ADC_ATTEN, ADC_WIDTH_BIT_12, 1100, &adc_cal_chars);
	if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI(TAG, "ADC Cal: eFuse Vref");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI(TAG, "ADC Cal: Two Point");
    } else {
        ESP_LOGI(TAG, "ADC Cal: Default");
    }
}


static bool gui_lvgl_init()
{
	// Initialize lvgl
	lv_init();
	
	//
	// Interface and driver initialization
	//
	disp_driver_init(true);
	
	// Install the display driver
	lv_disp_buf_init(&lvgl_disp_buf, lvgl_disp_buf1, lvgl_disp_buf2, CONFIG_LVGL_DISP_BUF_SIZE);
	lv_disp_drv_init(&lvgl_disp_drv);
	lvgl_disp_drv.flush_cb = disp_driver_flush;
	lvgl_disp_drv.buffer = &lvgl_disp_buf;
	lv_disp_drv_register(&lvgl_disp_drv);
    
    // Hook LittleVGL's timebase to its CPU system tick so it can keep track of time
    esp_register_freertos_tick_hook(lv_tick_callback);
    
    return true;
}


static void gui_state_init()
{
	mode = MODE_MEASURE;
	audio_muted = false;
	
	// Force power state evaluation by setting different values
	power_state = POWER_ST_GOOD_BATT;
	power_state_prev = -1;
	
	button_state[BUTTON_LEFT_INDEX].button_prev = false;
	button_state[BUTTON_LEFT_INDEX].button_down = false;
	button_state[BUTTON_LEFT_INDEX].button_pin = CONFIG_L_BTN_PIN;
	button_state[BUTTON_LEFT_INDEX].button_state = BUTTON_ST_NOT_PRESSED;
	button_state[BUTTON_LEFT_INDEX].button_timestamp = 0;
	button_state[BUTTON_LEFT_INDEX].button_long_msec = CONFIG_LONG_PRESS_MSEC;
	
	button_state[BUTTON_RIGHT_INDEX].button_prev = false;
	button_state[BUTTON_RIGHT_INDEX].button_down = false;
	button_state[BUTTON_RIGHT_INDEX].button_pin = CONFIG_R_BTN_PIN;
	button_state[BUTTON_RIGHT_INDEX].button_state = BUTTON_ST_NOT_PRESSED;
	button_state[BUTTON_RIGHT_INDEX].button_timestamp = 0;
	button_state[BUTTON_RIGHT_INDEX].button_long_msec = CONFIG_LONG_PRESS_MSEC;
	
	cum_count = 0;
	cum_dose = 0;
	cum_start_timestamp = 0;
	accum_interval_index = 0;
	
	// Force update to gauge to draw correct range values
	cur_gauge_range = -1;
}


static void gui_screen_init()
{
	static lv_style_t style_rt_12pt;
	static lv_style_t style_16pt;
	static lv_style_t style_rt_16pt;
	static lv_style_t style_cnt;
	static lv_style_t style_cum_12pt;
	static lv_style_t style_cum_16pt;
	
	// Setup a global theme and Initialize the underlying screen object
	lv_theme_t *theme = lv_theme_night_init(CONFIG_GUI_THEME_HUE, NULL);
	lv_theme_set_current(theme);
	
	// Setup the 12pt and 16pt sized label style
	lv_style_copy(&style_rt_12pt, theme->style.bg);
	style_rt_12pt.text.color = CONFIG_REAL_TIME_DISP_COLOR;
	
	lv_style_copy(&style_16pt, theme->style.bg);
	style_16pt.text.font = &lv_font_roboto_16;
	
	lv_style_copy(&style_rt_16pt, theme->style.bg);
	style_rt_16pt.text.color = CONFIG_REAL_TIME_DISP_COLOR;
	style_rt_16pt.text.font = &lv_font_roboto_16;
	
	// Create the main screen object
	screen = lv_obj_create(NULL, NULL);
	lv_obj_set_size(screen, LV_HOR_RES_MAX, LV_VER_RES_MAX);
	lv_scr_load(screen);
	
	// Top row
	//
	// Battery/Charge icon
	lbl_batt = lv_label_create(screen, NULL);
	lv_obj_set_pos(lbl_batt, 5, 2);
	lv_label_set_style(lbl_batt, LV_LABEL_STYLE_MAIN, &style_16pt);
	
	// Mute label
	lbl_mute = lv_label_create(screen, NULL);
	lv_obj_set_pos(lbl_mute, 113, 2);
	lv_label_set_style(lbl_mute, LV_LABEL_STYLE_MAIN, &style_16pt);
	
	// Gauge
	static lv_color_t needle_colors[1];
    needle_colors[0] = CONFIG_REAL_TIME_DISP_COLOR;
	gauge = lv_gauge_create(screen, NULL);
	lv_obj_set_size(gauge, 130, 130);
	lv_obj_set_pos(gauge, 2, 15);
	lv_gauge_set_needle_count(gauge, 1, needle_colors);
	
	// Real-time count
	lbl_rt_count = lv_label_create(screen, NULL);
	lv_obj_set_pos(lbl_rt_count, 5, 100);
	lv_label_set_long_mode(lbl_rt_count, LV_LABEL_LONG_BREAK);
	lv_label_set_align(lbl_rt_count,  LV_LABEL_ALIGN_CENTER);
	lv_label_set_style(lbl_rt_count, LV_LABEL_STYLE_MAIN, &style_rt_12pt);
	lv_obj_set_width(lbl_rt_count, 125);
	
	// Real-time dose
	lbl_rt_dose = lv_label_create(screen, NULL);
	lv_obj_set_pos(lbl_rt_dose, 5, 117);
	lv_label_set_long_mode(lbl_rt_dose, LV_LABEL_LONG_BREAK);
	lv_label_set_align(lbl_rt_dose,  LV_LABEL_ALIGN_CENTER);
	lv_label_set_style(lbl_rt_dose, LV_LABEL_STYLE_MAIN, &style_rt_16pt);
	lv_obj_set_width(lbl_rt_dose, 125);
	
	// Container for the cumulative information
	//
	// Container style
	lv_style_copy(&style_cnt, theme->style.bg);
	style_cnt.body.main_color = CONFIG_CUMULATIVE_DISP_BG_COLOR;
	style_cnt.body.grad_color = CONFIG_CUMULATIVE_DISP_BG_COLOR;
	style_cnt.body.radius = 5;
	style_cnt.body.padding.top = 2;
	style_cnt.body.padding.inner = 2;
	
	// Container
	container = lv_cont_create(screen, NULL);
	lv_obj_set_pos(container, 3, 140);
	lv_obj_set_size(container, 129, 80);
	lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &style_cnt);
	lv_cont_set_layout(container, LV_LAYOUT_COL_M);
	
	lv_style_copy(&style_cum_12pt, theme->style.bg);
	style_cum_12pt.body.main_color = CONFIG_CUMULATIVE_DISP_BG_COLOR;
	style_cum_12pt.body.grad_color = CONFIG_CUMULATIVE_DISP_BG_COLOR;
	style_cum_12pt.text.color = CONFIG_CUMULATIVE_DISP_COLOR;
	
	lv_style_copy(&style_cum_16pt, theme->style.bg);
	style_cum_16pt.body.main_color = CONFIG_CUMULATIVE_DISP_BG_COLOR;
	style_cum_16pt.body.grad_color = CONFIG_CUMULATIVE_DISP_BG_COLOR;
	style_cum_16pt.text.color = CONFIG_CUMULATIVE_DISP_COLOR;
	style_cum_16pt.text.font = &lv_font_roboto_16;
	
	
	// Cumulative information label
	lbl_cum_info = lv_label_create(container, NULL);
	lv_label_set_style(lbl_cum_info, LV_LABEL_STYLE_MAIN, &style_cum_12pt);
	lv_label_set_static_text(lbl_cum_info, "Cumulative Dose");
	
	// Cumulative info time display
	lbl_cum_time = lv_label_create(container, NULL);
	lv_label_set_style(lbl_cum_time, LV_LABEL_STYLE_MAIN, &style_cum_16pt);
	
	// Cumulative count
	lbl_cum_counts = lv_label_create(container, NULL);
	lv_label_set_style(lbl_cum_counts, LV_LABEL_STYLE_MAIN, &style_cum_16pt);
	lv_label_set_static_text(lbl_cum_counts, "-");
	
	// Cumulative dose
	lbl_cum_dose = lv_label_create(container, NULL);
	lv_label_set_style(lbl_cum_dose, LV_LABEL_STYLE_MAIN, &style_cum_16pt);
	lv_label_set_static_text(lbl_cum_dose, "-");
	
	
	// Buttons
	//
	// Left
	lbl_l_btn = lv_label_create(screen, NULL);
	lv_obj_set_pos(lbl_l_btn, 5, 227);
	
	// Right
	lbl_r_btn = lv_label_create(screen, NULL);
	lv_obj_set_pos(lbl_r_btn, 68, 227);
	lv_label_set_long_mode(lbl_r_btn, LV_LABEL_LONG_BREAK);
	lv_label_set_align(lbl_r_btn,  LV_LABEL_ALIGN_RIGHT);
	lv_obj_set_width(lbl_r_btn, 62);
	
	// Initial values
	gui_update_button_info();
	gui_update_count_info();
	gui_update_mute_info();
	gui_update_power_info();
}


static void gui_add_subtasks()
{
	// Event handler sub-task runs every 40 mSec
	lvgl_tasks[LVGL_ST_EVENT] = lv_task_create(gui_task_event_handler_task, 40, LV_TASK_PRIO_MID, NULL);
	
	// Button debounce runs every 20 mSec
	lvgl_tasks[LVGL_ST_BTN_DEBOUNCE] = lv_task_create(gui_task_btn_handler_task, 20, LV_TASK_PRIO_MID, NULL);
	
	// Battery monitor runs every 1000 mSec
	lvgl_tasks[LVGL_ST_BATT_CHECK] = lv_task_create(gui_task_batt_handler_task, 1000, LV_TASK_PRIO_MID, NULL);
}


static void gui_task_event_handler_task(lv_task_t * task)
{
	uint32_t notification_value;
	
	// Look for incoming notifications (clear them upon reading)
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
		if (Notification(notification_value, GUI_NOTIFY_NEW_COUNT_INFO)) {
			gui_update_count_info();
		}
	}
}


static void gui_task_btn_handler_task(lv_task_t * task)
{
	bool btn_l_short, btn_l_long, btn_r_short, btn_r_long;
	// Get button activity
	eval_button(&(button_state[BUTTON_LEFT_INDEX]), &btn_l_short, &btn_l_long);
	eval_button(&(button_state[BUTTON_RIGHT_INDEX]), &btn_r_short, &btn_r_long);
	
	// Evaluate button presses
	if (btn_l_short) {
		// New mode
		if (++mode == NUM_MODES) mode = MODE_MEASURE;
		reset_accumulation();
	}
		
	if (btn_l_long) {
		// Toggle mute
		audio_muted = !audio_muted;
		gpio_set_level(CONFIG_MUTEL_PIN, audio_muted ? 0 : 1);
		gui_update_mute_info();
	}
	
	if (btn_r_short) {
		reset_accumulation();
	}
	
	if (btn_r_long) {
		// Change accumulation interval (and reset accumulation) if in accumulation mode (otherwise ignore)
		if (mode == MODE_ACCUMULATE) {
			if (++accum_interval_index == NUM_ACCUM_INTERVALS) accum_interval_index = 0;
			reset_accumulation();
		}
	}
	
	// Update button labels if necessary
	if (btn_l_short || btn_r_long) {
		gui_update_button_info();
	}
	
	// Update displayed information
	if (btn_l_short || btn_r_short || btn_r_long) {
		gui_update_count_info();
	}
}


static void gui_task_batt_handler_task(lv_task_t * task)
{
	gui_update_power_info();
}


static void gui_update_button_info()
{
	static char accum_dur_buf[16];
	
	switch (mode) {
		case MODE_ACCUMULATE:
			lv_label_set_static_text(lbl_l_btn, "ACCUM");
			if (accum_int_min[accum_interval_index] <= 60) {
				sprintf(accum_dur_buf, "%d Min", accum_int_min[accum_interval_index]);
			} else {
				sprintf(accum_dur_buf, "%d Hour", accum_int_min[accum_interval_index]/60);
			}
			lv_label_set_static_text(lbl_r_btn, accum_dur_buf);
			break;
		default: // MODE_MEASURE
			lv_label_set_static_text(lbl_l_btn, "MEASURE");
			lv_label_set_static_text(lbl_r_btn, "RESET");
	}
}


static void gui_update_count_info()
{
	// Statically allocated buffers for each lv object
	static char rt_count_buf[11];     // "XXXXX CPX" + null + 1 extra
	static char rt_dose_buf[15];      // "XXX.XX uSv/Hr" + null + 1
	static char cum_time_buf[15];     // "XXXXX:XX:XX" + null + 1 (where m/s are 8-bit)
	static char cum_count_buf[20];    // "Counts: XXXXXX" or "Cnts: XXXXXXXX"
	static char cum_dose_buf[18];     // "Dose: XXX.XX uSv" + null + 1
	
	// Stack variables
	count_status_t cnts;
	int range;
	int t;
	uint16_t h;
	uint8_t m, s;
	uint32_t adj_cpm;
	uint32_t adj_cps;
	float f;
	
	// Get the count values and adjust for the tube's dead time
	get_counts(&cnts);
	f = ((float) cnts.cpm) / (1.0 - (((float) cnts.cpm) * CONFIG_DEAD_TIME_SEC));
	adj_cpm = round(f);
	f = ((float) cnts.cps) / (1.0 - (((float) cnts.cps) * CONFIG_DEAD_TIME_SEC));
	adj_cps = round(f);
	
	// Look to see if we have to change the gauge range
	for (range=0; range<(NUM_GAUGE_RANGES-1); range++) {
		if (adj_cpm < gauge_cpm_threshold[range]) {
			break;
		}
	}
	if (range != cur_gauge_range) {
		cur_gauge_range = range;
		lv_gauge_set_range(gauge, 0, gauge_max_value[range]);
		lv_gauge_set_critical_value(gauge, gauge_max_value[range]);
	}
	
	// Update the gauge and real-time counts
	if (gauge_is_cpm[range]) {
		lv_gauge_set_value(gauge, 0, adj_cpm);
		sprintf(rt_count_buf, "%d CPM", (int) adj_cpm);
	} else {
		lv_gauge_set_value(gauge, 0, adj_cps);
		sprintf(rt_count_buf, "%d CPS", (int) adj_cps);
	}
	lv_label_set_static_text(lbl_rt_count, rt_count_buf);
	
	// Update real-time dose
	f = ((float) adj_cpm) * CONFIG_CPM_TO_USVHR;
	if (f < 1000.0) {
		sprintf(rt_dose_buf, "%1.2f uSv/hr", f);
	} else if (f < 1000000.0) {
		sprintf(rt_dose_buf, "%1.2f mSv/hr", f / 1000.0);
	} else {
		sprintf(rt_dose_buf, "%1.2f Sv/hr", f / 1000000.0);
	}
	lv_label_set_static_text(lbl_rt_dose, rt_dose_buf);
	
	// Update cumulative time stamp
	if (mode == MODE_MEASURE) {
		t = get_uptime_msec() - cum_start_timestamp;
	} else {
		t = (cum_start_timestamp + accum_int_min[accum_interval_index]*60000) - get_uptime_msec();
		if (t < 0) t = 0;
	}
	t = t / 1000;
	s = t % 60;
	m = (t/60) % 60;
	h = t/3600;
	sprintf(cum_time_buf, "%02d:%02d:%02d", h, m, s);
	lv_label_set_static_text(lbl_cum_time, cum_time_buf);
	
	// Update cumulative statistics
	if ((mode == MODE_MEASURE) || (t > 0)) {
		// Update cumulative dose
		cum_count += adj_cps;
		cum_dose = ((float) cum_count) * CONFIG_CPM_TO_USVHR / 60.0;
	}
	
	if (cum_count < 10000) {
		sprintf(cum_count_buf, "Counts: %d", cum_count);
	} else {
		sprintf(cum_count_buf, "Cnts: %d", cum_count);
	}
	lv_label_set_static_text(lbl_cum_counts, cum_count_buf);
	
	if (cum_dose < 1000.0) {
		sprintf(cum_dose_buf, "Dose: %1.2f uSv", cum_dose);
	} else if (cum_dose < 1000000.0) {
		sprintf(cum_dose_buf, "Dose: %1.2f mSv", cum_dose / 1000.0);
	} else {
		sprintf(cum_dose_buf, "Dose: %1.2f Sv", cum_dose / 1000000.0);
	}
	lv_label_set_static_text(lbl_cum_dose, cum_dose_buf);
}


static void gui_update_mute_info()
{
	if (audio_muted) {
		lv_label_set_static_text(lbl_mute, LV_SYMBOL_MUTE);
	} else {
		lv_label_set_static_text(lbl_mute, LV_SYMBOL_VOLUME_MAX);
	}
}


static void gui_update_power_info()
{
	float v;
	
	// Get the current battery level
	v = get_batt_v();
//	ESP_LOGI(TAG, "V = %1.2f", v);
	
	// Update power state
	power_state = batt_v_to_power_state(v);
	if (power_state != power_state_prev) {
		power_state_prev = power_state;
		
		// Notify cnt_task if necessary
		if (power_state == POWER_ST_LOW_BATT) {
			xTaskNotify(task_handle_cnt, CNT_NOTIFY_LOW_BATT_MASK, eSetBits);
		} else {
			xTaskNotify(task_handle_cnt, CNT_NOTIFY_GOOD_BATT_MASK, eSetBits);
		}
		
		// Update the GUI
		if (power_state == POWER_ST_CHARGE) {
			lv_label_set_static_text(lbl_batt, LV_SYMBOL_CHARGE);
		} else {
			if (v < 3.5) {
				lv_label_set_static_text(lbl_batt, LV_SYMBOL_BATTERY_EMPTY);
			} else if (v < 3.66) {
				lv_label_set_static_text(lbl_batt, LV_SYMBOL_BATTERY_1);
			} else if (v < 3.72) {
				lv_label_set_static_text(lbl_batt, LV_SYMBOL_BATTERY_2);
			} else if (v < 3.9) {
				lv_label_set_static_text(lbl_batt, LV_SYMBOL_BATTERY_3);
			} else {			
				lv_label_set_static_text(lbl_batt, LV_SYMBOL_BATTERY_FULL);
			}
		}
	}
}


static void IRAM_ATTR lv_tick_callback()
{
	lv_tick_inc(portTICK_RATE_MS);
}


static void eval_button(button_state_t* bs, bool* short_press, bool* long_press)
{
	bool cur_press;
	
	// Read the current state
	cur_press = gpio_get_level(bs->button_pin) == 0;
	
	// Presses will be set if necessary
	*short_press = false;
	*long_press = false;
	
	// Evaluate button state
	if (cur_press && bs->button_prev && !bs->button_down) {
		// Button just pressed
		bs->button_down = true;
		bs->button_state = BUTTON_ST_PRESSED;
		bs->button_timestamp = get_uptime_msec();
	} else if (!cur_press && !bs->button_prev && bs->button_down) {
		// Button just released
		bs->button_down = false;
		if (bs->button_state == BUTTON_ST_PRESSED) {
			// Short press detected
			*short_press = true;
		}
		bs->button_state = BUTTON_ST_NOT_PRESSED;
	} else if (bs->button_state == BUTTON_ST_PRESSED) {
		if ((get_uptime_msec() - bs->button_timestamp) >= bs->button_long_msec) {
			// Long press detected
			*long_press = true;
			bs->button_state = BUTTON_ST_LONG_PRESSED;
		}
	}
	bs->button_prev = cur_press;
}


static uint32_t get_uptime_msec()
{
	return (uint32_t) (esp_timer_get_time() / 1000);
}


static int batt_v_to_power_state(float v)
{
	// Implement slight hysteresis
	if (power_state == POWER_ST_LOW_BATT) {
		if (v > 4.3) {
			return POWER_ST_CHARGE;
		} else if (v < 3.6) {
			return POWER_ST_LOW_BATT;
		} else {
			return POWER_ST_GOOD_BATT;
		}
	} else {
		if (v > 4.3) {
			return POWER_ST_CHARGE;
		} else if (v < 3.5) {
			return POWER_ST_LOW_BATT;
		} else {
			return POWER_ST_GOOD_BATT;
		}
	}
}


static float get_batt_v()
{
	int adc_mv;
	
	adc_mv = esp_adc_cal_raw_to_voltage(adc1_get_raw(batt_adc_ch), &adc_cal_chars);
	
	return CONFIG_BATT_ADC_MULT * ((float) adc_mv) / 1000.0;
}


static void reset_accumulation()
{
	cum_count = 0;
	cum_dose = 0;
	cum_start_timestamp = get_uptime_msec();
}
