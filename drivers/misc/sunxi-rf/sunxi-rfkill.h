/*
 * drivers/misc/sunxi-rf/sunxi-rfkill.h
 *
 * Copyright (c) 2014 softwinner.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SUNXI_RFKILL_H
#define __SUNXI_RFKILL_H

#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/rfkill.h>
#define EXTENDED_VCCWIFI_GPIO 1
struct sunxi_bt_platdata {
	int power_num;
	int bt_power_voltage;
	int bt_io_voltage;
	struct regulator **bt_power;
	struct regulator *io_regulator;
	struct clk *lpo;
	int gpio_bt_rst;
	char **bt_power_name;
	char *io_regulator_name;
	char *clk_name;

	int power_state;
	struct rfkill *rfkill;
	struct platform_device *pdev;
};

struct sunxi_wlan_platdata {
	unsigned int wakeup_enable;

	int bus_index;
	int wlan_power_voltage;
	int wlan_io_voltage;

	struct regulator **wlan_power;
	struct regulator *io_regulator;
	struct clk *lpo;

	int gpio_wlan_regon;
#ifdef EXTENDED_VCCWIFI_GPIO
	int gpio_wlan_vccwifi_ctrl;
#endif
	int gpio_wlan_hostwake;
	int gpio_chip_en;
	int power_num;

	char **wlan_power_name;
	char *io_regulator_name;
	char *clk_name;

	int power_state;
	struct platform_device *pdev;
	int gpio_chip_en_invert;
};

extern void sunxi_wl_chipen_set(int dev, int on_off);
extern void sunxi_wlan_set_power(bool on_off);
extern int  sunxi_wlan_get_bus_index(void);
extern int  sunxi_wlan_get_oob_irq(void);
extern int  sunxi_wlan_get_oob_irq_flags(void);
extern int  enable_gpio_wakeup_src(int para);
extern void sunxi_mmc_rescan_card(unsigned ids);
extern int  sunxi_get_soc_chipid(uint8_t *chipid);

#endif /* SUNXI_RFKILL_H */
