/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#else
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#endif
#endif
#include "lcm_drv.h"

#ifndef BUILD_LK
static unsigned int GPIO_LCD_RST;
static unsigned int GPIO_LCD_PWR_ENP;
static unsigned int GPIO_LCD_PWR_ENN;
static struct regulator *lcm_vgp;

void lcm_request_gpio_control(struct device *dev)
{
	GPIO_LCD_RST = of_get_named_gpio(dev->of_node, "gpio_lcm_rst", 0);
	gpio_request(GPIO_LCD_RST, "GPIO_LCD_RST");
	GPIO_LCD_PWR_ENP = of_get_named_gpio(dev->of_node,
		"gpio_lcm_pwr", 0);
	gpio_request(GPIO_LCD_PWR_ENP, "GPIO_LCD_PWR_ENP");
	GPIO_LCD_PWR_ENN = of_get_named_gpio(dev->of_node,
		"gpio_lcm_bl", 0);
	gpio_request(GPIO_LCD_PWR_ENN, "GPIO_LCD_PWR_ENN");
}

/* get LDO supply */
static int lcm_get_vgp_supply(struct device *dev)
{
	int ret;
	struct regulator *lcm_vgp_ldo;

	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	lcm_vgp_ldo = devm_regulator_get(dev, "reg-lcm");
	if (IS_ERR(lcm_vgp_ldo)) {
		ret = PTR_ERR(lcm_vgp_ldo);
		pr_debug("failed to get reg-lcm LDO\n");
		return ret;
	}

	pr_notice("LCM: lcm get supply ok.\n");

	/* get current voltage settings */
	ret = regulator_get_voltage(lcm_vgp_ldo);
	pr_notice("lcm LDO voltage = %d in LK stage\n", ret);

	lcm_vgp = lcm_vgp_ldo;

	return ret;
}

int lcm_vgp_supply_enable(void)
{
	int ret;
	unsigned int volt;

	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	if (lcm_vgp == NULL)
		return 0;

	/* get voltage settings again */
	volt = regulator_get_voltage(lcm_vgp);
	if (volt == 1800000)
		pr_err("LCM: check voltage=1.8V pass!\n");
	else
		pr_err("LCM: check voltage=1.8V fail! (voltage: %d)\n", volt);

	ret = regulator_enable(lcm_vgp);
	if (ret != 0) {
		pr_err("LCM: Failed to enable lcm_vgp: %d\n", ret);
		return ret;
	}

	return ret;
}

int lcm_vgp_supply_disable(void)
{
	int ret = 0;
	unsigned int isenable;

	if (lcm_vgp == NULL)
		return 0;

	/* disable regulator */
	isenable = regulator_is_enabled(lcm_vgp);

	pr_notice("LCM: lcm query regulator enable status[0x%x]\n", isenable);

	if (isenable) {
		ret = regulator_disable(lcm_vgp);
		if (ret != 0) {
			pr_err("LCM: lcm failed to disable lcm_vgp: %d\n", ret);
			return ret;
		}
		/* verify */
		isenable = regulator_is_enabled(lcm_vgp);
		if (!isenable)
			pr_err("LCM: lcm regulator disable pass\n");
	}

	return ret;
}

static int lcm_driver_probe(struct device *dev, void const *data)
{
	lcm_get_vgp_supply(dev);
	lcm_request_gpio_control(dev);
	lcm_vgp_supply_enable();

	return 0;
}

static const struct of_device_id lcm_platform_of_match[] = {
	{
		.compatible = "mediatek,mt6580-lcm",
		.data = 0,
	}, {
		/* sentinel */
	}
};

MODULE_DEVICE_TABLE(of, platform_of_match);

static int lcm_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;

	id = of_match_node(lcm_platform_of_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;

	return lcm_driver_probe(&pdev->dev, id->data);
}

static struct platform_driver lcm_driver = {
	.probe = lcm_platform_probe,
	.driver = {
		.name = "lcm",
		.of_match_table = lcm_platform_of_match,
	},
};

static int __init lcm_drv_init(void)
{
	pr_notice("LCM: Register lcm driver\n");
	if (platform_driver_register(&lcm_driver)) {
		pr_notice("LCM: failed to register disp driver\n");
		return -ENODEV;
	}

	return 0;
}

/*static void __exit lcm_drv_exit(void)
{
	platform_driver_unregister(&lcm_driver);
	pr_notice("LCM: Unregister lcm driver done\n");
}*/

late_initcall(lcm_drv_init);
//module_exit(lcm_drv_exit);
MODULE_AUTHOR("mediatek");
MODULE_DESCRIPTION("Display subsystem Driver");
MODULE_LICENSE("GPL");
#endif

/* ----------------------------------------------------------------- */
/* Local Constants */
/* ----------------------------------------------------------------- */

#define FRAME_WIDTH		(800)
#define FRAME_HEIGHT		(1280)
#define GPIO_OUT_ONE		1
#define GPIO_OUT_ZERO		0

#define REGFLAG_DELAY		0xFE
#define REGFLAG_END_OF_TABLE		0x00

/* ----------------------------------------------------------------- */
/*  Local Variables */
/* ----------------------------------------------------------------- */
static LCM_UTIL_FUNCS lcm_util = { 0 };
#define SET_RESET_PIN(v)		(lcm_util.set_reset_pin((v)))
#define UDELAY(n)				(lcm_util.udelay(n))
#define MDELAY(n)				(lcm_util.mdelay(n))

/* ----------------------------------------------------------------- */
/* Local Functions */
/* ----------------------------------------------------------------- */
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
		 (lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update))
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		 (lcm_util.dsi_set_cmdq(pdata, queue_size, force_update))
#define wrtie_cmd(cmd) \
		 (lcm_util.dsi_write_cmd(cmd))
#define write_regs(addr, pdata, byte_nums) \
		 (lcm_util.dsi_write_regs(addr, pdata, byte_nums))
#define read_reg \
		 (lcm_util.dsi_read_reg())
#define read_reg_v2(cmd, buffer, buffer_size) \
		 (lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size))

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO, output);
#else
	gpio_direction_output(GPIO, output);
	gpio_set_value(GPIO, output);
#endif
}

static void init_karnak_fiti_kd_lcm(void)
{
	unsigned int data_array [16];

	data_array[0] = 0xe01500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x93e11500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x65e21500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0xf8e31500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x2801500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1e01500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x66011500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x10e1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x171500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0xbf181500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x191500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1a1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0xbf1b1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1c1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x3e1f1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x28201500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x28211500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0xe221500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x9371500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x4381500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x8391500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x123a1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x783c1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0xff3d1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0xff3e1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x7f3f1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x6401500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0xa0411500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1551500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1561500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x69571500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0xa581500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0xa591500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x295a1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x155b1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x7c5d1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x655e1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x555f1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x49601500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x44611500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x35621500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x3a631500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x23641500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x3d651500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x3c661500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x3d671500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x5d681500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x4d691500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x566a1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x486b1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x456c1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x386d1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x256e1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x6f1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x7c701500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x65711500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x55721500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x49731500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x44741500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x35751500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x3a761500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x23771500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x3d781500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x3c791500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x3d7a1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x5d7b1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x4d7c1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x567d1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x487e1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x457f1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x38801500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x25811500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x821500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x2e01500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1e001500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1e011500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x41021500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x41031500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x43041500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x43051500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1f061500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1f071500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1f081500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1f091500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1e0a1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1e0b1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1f0c1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x470d1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x470e1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x450f1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x45101500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x4b111500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x4b121500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x49131500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x49141500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1f151500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1e161500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1e171500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x40181500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x40191500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x421a1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x421b1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1f1c1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1f1d1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1f1e1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1f1f1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1e201500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1e211500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1f221500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x46231500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x46241500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x44251500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x44261500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x4a271500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x4a281500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x48291500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x482a1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1f2b1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x10581500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x591500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x5a1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x305b1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x25c1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x405d1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x15e1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x25f1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x30601500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1611500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x2621500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x6a631500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x6a641500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x5651500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x12661500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x74671500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x4681500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x6a691500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x6a6a1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x86b1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x6c1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x66d1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x6e1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x886f1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x701500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x711500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x6721500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x7b731500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x741500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x7751500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x761500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x5d771500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x17781500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x1f791500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x7a1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x7b1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x7c1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x37d1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x7b7e1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x4e01500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x32d1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x442e1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x11091500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x32d1500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x3e01500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x3f981500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0xe01500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x2e61500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x2e71500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(1);
	data_array[0] = 0x110500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(120);
	data_array[0] = 0x290500;
	dsi_set_cmdq(data_array,1,1);
	MDELAY(120);
}


/* ----------------------------------------------------------------- */
/* LCM Driver Implementations */
/* ----------------------------------------------------------------- */
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->dsi.mode   = SYNC_PULSE_VDO_MODE;

	params->dsi.LANE_NUM				= LCM_THREE_LANE;
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;

	/* Video mode setting */
	params->dsi.intermediat_buffer_num = 0;

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active		= 4;//4;
	params->dsi.vertical_backporch			= 8;//15;
	params->dsi.vertical_frontporch			= 24;//21;
	params->dsi.vertical_active_line		= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active		= 18;//26;
	params->dsi.horizontal_backporch		= 18;//28;
	params->dsi.horizontal_frontporch		= 18;//28;
	params->dsi.horizontal_active_pixel		= FRAME_WIDTH;

	params->dsi.PLL_CLOCK = 280;
	params->dsi.clk_lp_per_line_enable = 1;

	params->dsi.ssc_disable = 1;
	params->dsi.cont_clock = 0;
	params->dsi.DA_HS_EXIT = 1;
	params->dsi.CLK_ZERO = 16;
	params->dsi.HS_ZERO = 9;
	params->dsi.HS_TRAIL = 5;
	params->dsi.CLK_TRAIL = 5;
	params->dsi.CLK_HS_POST = 8;
	params->dsi.CLK_HS_EXIT = 6;
	/* params->dsi.CLK_HS_PRPR = 1; */

	params->dsi.TA_GO = 8;
	params->dsi.TA_GET = 10;

	params->physical_width = 108;
	params->physical_height = 172;
}


static void lcm_init(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	init_karnak_fiti_kd_lcm();
}

static void lcm_resume(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	init_karnak_fiti_kd_lcm(); /* TPV panel */

}

static void lcm_init_power(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_PWR_ENP, GPIO_OUT_ONE);
	MDELAY(50);
	lcm_set_gpio_output(GPIO_LCD_PWR_ENN, GPIO_OUT_ONE);
	MDELAY(30);
	lcm_vgp_supply_enable();
	MDELAY(50);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(50);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(50);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(80);
}

static void lcm_resume_power(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	/*lcm_vgp_supply_enable();
	lcm_set_gpio_output(GPIO_LCD_PWR_ENP, GPIO_OUT_ONE);
	MDELAY(1);
	lcm_set_gpio_output(GPIO_LCD_PWR_ENN, GPIO_OUT_ONE);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(10);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(2);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(5);*/
	lcm_init_power();
}

static void lcm_suspend(void)
{
	unsigned int data_array[16];

	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	data_array[0] = 0x00280500; // Display Off
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10);

	data_array[0] = 0x00100500; // Sleep In
	dsi_set_cmdq(data_array, 1, 1);

	MDELAY(120);
}

static void lcm_suspend_power(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_PWR_ENN, GPIO_OUT_ZERO);
	MDELAY(3);
	lcm_set_gpio_output(GPIO_LCD_PWR_ENP, GPIO_OUT_ZERO);
	MDELAY(5);
	lcm_vgp_supply_disable();
	MDELAY(10);
}

LCM_DRIVER k710_hz_jd9366_boe_wxga_ips_101_lcm_drv = {
	.name			= "1-k710_hz_jd9366_boe_wxga_ips_101",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.init_power     = lcm_init_power,
	.resume         = lcm_resume,
	.resume_power	= lcm_resume_power,
	.suspend        = lcm_suspend,
	.suspend_power	= lcm_suspend_power,
};


