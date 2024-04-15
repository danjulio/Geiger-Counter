/**
 * @file disp_driver.c
 */

#include "disp_driver.h"
#include "disp_spi.h"
#include "st7789.h"



void disp_driver_init(bool init_spi)
{
	if (init_spi) {
		disp_spi_init();
	}

	st7789_init();
}

void disp_driver_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map)
{
	st7789_flush(drv, area, color_map);
}
