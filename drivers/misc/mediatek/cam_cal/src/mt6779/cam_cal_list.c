/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

#ifndef VENDOR_EDIT
#define VENDOR_EDIT
#endif

#ifdef VENDOR_EDIT
/*Longyuan.Yang@Camera.Driver 20190109 add for P90 PDAF porting*/
#define IMX586_MAX_EEPROM_SIZE 0x2B00
#else
#define IMX586_MAX_EEPROM_SIZE 0x24D0
#endif

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	#ifdef VENDOR_EDIT
	/*Henry.Chang@Camera.Driver 20181217 modify for sensor eeprom*/
	{IMX586_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3P9SP_SENSOR_ID, 0xA8, Common_read_region},
	{S5KGD1SP_SENSOR_ID, 0xA8, Common_read_region},
	{GC5035_SENSOR_ID, 0xA0, Common_read_region},
	{HI846_SENSOR_ID, 0xA2, Common_read_region},
	#else
	{IMX519_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2T7SP_SENSOR_ID, 0xA4, Common_read_region},
	{IMX386_SENSOR_ID, 0xA0, Common_read_region},
	{IMX386_MONO_SENSOR_ID, 0xA0, Common_read_region},
	{IMX398_SENSOR_ID, 0xA0, Common_read_region},
	{IMX350_SENSOR_ID, 0xA0, Common_read_region},
	{IMX586_SENSOR_ID, 0xA0, Common_read_region, IMX586_MAX_EEPROM_SIZE},
	{IMX576_SENSOR_ID, 0xA2, Common_read_region},
	{S5K2L7_SENSOR_ID, 0xA0, Common_read_region},
	{IMX230_SENSOR_ID, 0xA0, Common_read_region},
	{IMX338_SENSOR_ID, 0xA0, Common_read_region},
	{IMX318_SENSOR_ID, 0xA0, Common_read_region},
	{IMX258_SENSOR_ID, 0xA0, Common_read_region},
	{S5K4E6_SENSOR_ID, 0xA8, Common_read_region},
	#endif
	/*  ADD before this line */
	{0, 0, 0}	/*end of list */
};

unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


