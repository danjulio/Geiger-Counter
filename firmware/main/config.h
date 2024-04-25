/*
 * System Configuration File
 *
 * Contains system definition and configurable items.
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
 *
 */
#ifndef _CONFIG_H
#define _CONFIG_H

// ======================================================================================
// System hardware definitions
//

//
// IO Pins
//   LCD uses VSPI (no MISO)
//

#define CONFIG_L_BTN_PIN       0
#define CONFIG_R_BTN_PIN       35

#define CONFIG_LCD_CSN_IO      5
#define CONFIG_LCD_SCK_IO      18
#define CONFIG_LCD_MOSI_IO     19
#define CONFIG_LCD_PIN_DC      16
#define CONFIG_LCD_PIN_RST     23
#define CONFIG_LCD_DISP_PIN_BL 4

#define CONFIG_PULSE_IN_PIN    36

#define CONFIG_R_LED_PIN       12
#define CONFIG_G_LED_PIN       13
#define CONFIG_B_LED_PIN       15

#define CONFIG_CLICK_PIN       2
#define CONFIG_MUTEL_PIN       17

#define CONFIG_PWR_SNS_EN_PIN  14
#define CONFIG_PWR_SNS_ADC_PIN 34

//
// SPI Interface
//
#define CONFIG_LCD_SPI_HOST    VSPI_HOST
#define CONFIG_LCD_SPI_FREQ_HZ 20000000

//
// Battery sense input multiplier (based on resistor divider network)
// and ADC attenuation to match the input voltage range
//
#define CONFIG_BATT_ADC_MULT   2.0
#define CONFIG_ESP_ADC_ATTEN   ADC_ATTEN_DB_11

//
// Red/Green LED PWM duty cycles (selected to create appropriate brightness levels)
//
#define CONFIG_RED_PWM_PERCENT    33
#define CONFIG_GREEN_PWM_PERCENT  20



// ======================================================================================
// LVGL definitions
//

//
// Display buffer
//
#define CONFIG_LVGL_DISP_BUF_SIZE               (135 * 10)


//
// Display orientation
//
#define CONFIG_LV_DISPLAY_ORIENTATION           2
#define CONFIG_LV_DISPLAY_ORIENTATION_LANDSCAPE 1
#define CONFIG_LV_INVERT_COLORS                 1

//
// GUI color schemes
//
// Theme hue (0-360)
#define CONFIG_GUI_THEME_HUE                    240

// Text areas
#define CONFIG_REAL_TIME_DISP_COLOR             LV_COLOR_MAKE(0xF0, 0xF0, 0x00)
#define CONFIG_CUMULATIVE_DISP_BG_COLOR         LV_COLOR_MAKE(0x00, 0x1A, 0x40)
#define CONFIG_CUMULATIVE_DISP_COLOR            LV_COLOR_MAKE(0x00, 0x60, 0xF0)



// ======================================================================================
// System Configuration
//

// Click output high period
#define CONFIG_PULSE_CLICK_MSEC      2.5

// Pulse blink period
#define CONFIG_PULSE_BLINK_MSEC      20

// Button long-press mSec
#define CONFIG_LONG_PRESS_MSEC       2000

// LND712 Tube CPM to uSv/Hr conversion
//   From: https://sites.google.com/site/diygeigercounter/technical/gm-tubes-supported
//         https://www.utsunomia.com/y.utsunomia/Kansan.html
//         http://einstlab.web.fc2.com/geiger/geiger3.html
#define CONFIG_CPM_TO_USVHR          0.00833

// LND712 Tube dead time
#define CONFIG_DEAD_TIME_SEC         0.00009

// Number of previous samples to analyze to detect high rates of change
// in order to reduce the sample size used to compute CPM.  This has the
// effect of making the display more responsive.
#define CONFIG_DELTA_DET_SAMPLES     5

// Percentages for detect sample average to differ from long-term average
// to trigger reduction in sample size used to compute CPM.
#define CONFIG_LOW_DET_PERCENT       80
#define CONFIG_HIGH_DET_PERCENT      125



// ======================================================================================
// System Utilities macros
//
#define Notification(var, mask) ((var & mask) == mask)


#endif // _CONFIG_H