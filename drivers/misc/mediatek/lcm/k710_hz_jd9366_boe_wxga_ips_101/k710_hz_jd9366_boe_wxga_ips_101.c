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
#include <platform/upmu_common.h>
#include <string.h>
#include <cust_gpio_usage.h>
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

#ifdef BUILD_LK
void pr_notice(const char* fmt, ...){}
#endif

#ifndef BUILD_LK
static unsigned int GPIO_LCM_RST;
static unsigned int GPIO_LCD_ENP;
static unsigned int GPIO_LCD_ENN;
static struct regulator *lcm_vgp;

void lcm_request_gpio_control(struct device *dev)
{
	GPIO_LCM_RST = of_get_named_gpio(dev->of_node, "gpio_lcm_rst", 0);
	gpio_request(GPIO_LCM_RST, "gpio_lcm_rst");
	GPIO_LCD_ENP = of_get_named_gpio(dev->of_node,
		"gpio_lcm_pwr", 0);
	gpio_request(GPIO_LCD_ENP, "gpio_lcm_pwr");
	GPIO_LCD_ENN = of_get_named_gpio(dev->of_node,
		"gpio_lcm_bl", 0);
	gpio_request(GPIO_LCD_ENN, "gpio_lcm_bl");
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

static void __exit lcm_drv_exit(void)
{
	platform_driver_unregister(&lcm_driver);
	pr_notice("LCM: Unregister lcm driver done\n");
}

late_initcall(lcm_drv_init);
module_exit(lcm_drv_exit);
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
#define SET_GPIO_OUT(n, v)      (lcm_util.set_gpio_out(n, v))

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


enum LCM_CMD_TYPE { LCM_SET_CMDQ, LCM_MDELAY, LCM_END };

struct _LCM_CMD{
	enum LCM_CMD_TYPE type;
	uint32_t data;
};

typedef struct _LCM_CMD LCM_CMD;
typedef LCM_CMD LCM_CMD_TABLE[];



static LCM_CMD_TABLE lcm_init_table = {
/*	{LCM_GPIO, 0xb << 1 | 1},
	{LCM_MDELAY, 50},
	{LCM_GPIO, 0xc << 1 | 1},
	{LCM_MDELAY, 30},
	{LCM_MDELAY, 0x32},
	{LCM_GPIO, 0x46 << 1 | 1},
	{LCM_MDELAY, 0x32},
	{LCM_GPIO, 0x46 << 1 | 0},
	{LCM_MDELAY, 0x32},
	{LCM_GPIO, 0x46 << 1 | 1},
	{LCM_MDELAY, 80},*/
	{LCM_SET_CMDQ, 0xe01500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x93e11500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x65e21500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0xf8e31500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x2801500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1e01500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x66011500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x10e1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x171500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0xbf181500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x191500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1a1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0xbf1b1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1c1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x3e1f1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x28201500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x28211500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0xe221500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x9371500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x4381500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x8391500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x123a1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x783c1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0xff3d1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0xff3e1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x7f3f1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x6401500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0xa0411500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1551500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1561500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x69571500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0xa581500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0xa591500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x295a1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x155b1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x7c5d1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x655e1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x555f1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x49601500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x44611500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x35621500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x3a631500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x23641500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x3d651500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x3c661500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x3d671500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x5d681500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x4d691500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x566a1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x486b1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x456c1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x386d1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x256e1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x6f1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x7c701500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x65711500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x55721500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x49731500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x44741500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x35751500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x3a761500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x23771500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x3d781500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x3c791500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x3d7a1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x5d7b1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x4d7c1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x567d1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x487e1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x457f1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x38801500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x25811500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x821500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x2e01500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1e001500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1e011500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x41021500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x41031500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x43041500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x43051500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1f061500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1f071500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1f081500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1f091500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1e0a1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1e0b1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1f0c1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x470d1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x470e1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x450f1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x45101500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x4b111500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x4b121500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x49131500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x49141500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1f151500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1e161500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1e171500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x40181500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x40191500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x421a1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x421b1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1f1c1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1f1d1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1f1e1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1f1f1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1e201500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1e211500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1f221500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x46231500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x46241500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x44251500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x44261500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x4a271500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x4a281500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x48291500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x482a1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1f2b1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x10581500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x591500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x5a1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x305b1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x25c1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x405d1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x15e1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x25f1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x30601500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1611500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x2621500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x6a631500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x6a641500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x5651500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x12661500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x74671500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x4681500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x6a691500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x6a6a1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x86b1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x6c1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x66d1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x6e1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x886f1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x701500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x711500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x6721500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x7b731500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x741500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x7751500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x761500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x5d771500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x17781500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x1f791500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x7a1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x7b1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x7c1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x37d1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x7b7e1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x4e01500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x32d1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x442e1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x11091500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x32d1500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x3e01500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x3f981500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0xe01500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x2e61500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x2e71500},
	{LCM_MDELAY, 1},
	{LCM_SET_CMDQ, 0x110500},
	{LCM_MDELAY, 120},
	{LCM_SET_CMDQ, 0x290500},
	{LCM_MDELAY, 20},
	{LCM_MDELAY, 100},
//	{LCM_GPIO, 0xc << 1 | 1},
	{LCM_END, 0}
};


static LCM_CMD_TABLE suspend_table = {
	{LCM_SET_CMDQ, 0x00280500},
	{LCM_MDELAY, 5},
	{LCM_SET_CMDQ, 0x00100500},
	{LCM_MDELAY, 5},
	{LCM_END, 0}
};

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


void push_table(LCM_CMD_TABLE table){
	uint32_t dtable[16];
	int i;
	for(i = 0; table[i].type != LCM_END; i++){
		LCM_CMD cmd = table[i];
		switch(cmd.type){
			case LCM_MDELAY:
				MDELAY(cmd.data);
				break;
			case LCM_SET_CMDQ:
				dtable[0] = cmd.data;
				dsi_set_cmdq(dtable, 1, 1);
				break;
			/*case LCM_GPIO:
				// custom format
				// 0baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab
				// a - gpio num
				// b - enable or disable
				if(cmd.data >> 1 == GPIO_LCM_RST)
					SET_RESET_PIN((int)(cmd.data & 1));
				else
					//lcm_set_gpio_output(
					SET_GPIO_OUT(
						cmd.data >> 1,
						(int)(cmd.data & 1));
				break;*/
			default:
				break;
		}
	}
}

#define lcm_power_en(v) lcm_set_gpio_output(GPIO_LCD_ENP, v)
#define lcm_bl_en(v) lcm_set_gpio_output(GPIO_LCD_ENN, v)
#define lcm_rst(v) lcm_set_gpio_output(GPIO_LCM_RST, v)

static void init_karnak_fiti_kd_lcm(void)
{
	lcm_power_en(1);
        MDELAY(50);
	lcm_bl_en(1);
#ifdef BUILD_LK
	pmic_set_register_value(PMIC_RG_VGP3_VOSEL, 3);
	pmic_set_register_value(PMIC_RG_VGP3_EN, 1);
#else
	lcm_vgp_supply_enable();
#endif
        MDELAY(30);
	lcm_set_gpio_output(0xffffffff,1);
	MDELAY(50);
	lcm_rst(1);
        MDELAY(50);
	lcm_rst(0);
        MDELAY(50);
	lcm_rst(1);
        MDELAY(80);

	push_table(lcm_init_table);

	lcm_bl_en(1);
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
  params->dsi.mode = SYNC_PULSE_VDO_MODE;
  params->dsi.LANE_NUM = LCM_THREE_LANE;
  params->dsi.vertical_sync_active = 4;
  params->dsi.vertical_backporch = 8;
  params->dsi.vertical_frontporch = 0x18;
  params->dsi.PLL_CLOCK = 0x118;
  params->type = LCM_TYPE_DSI;
  params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;
  params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
  params->width = 800;
  params->dsi.horizontal_active_pixel = 800;
  params->height = 0x500;
  params->dsi.vertical_active_line = 0x500;
  params->dsi.horizontal_sync_active = 0x12;
  params->dsi.horizontal_backporch = 0x12;
  params->dsi.horizontal_frontporch = 0x12;
}


static void lcm_init(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	init_karnak_fiti_kd_lcm();
}

static void lcm_resume(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);
	init_karnak_fiti_kd_lcm();

}



static void lcm_suspend(void)
{
	lcm_bl_en(0);
	MDELAY(250);
	push_table(suspend_table);
	lcm_rst(0);
	MDELAY(10);
	lcm_power_en(0);
	MDELAY(20);
	lcm_bl_en(0);
#ifdef BUILD_LK
	pmic_set_register_value(PMIC_RG_VGP3_VOSEL, 0);
	pmic_set_register_value(PMIC_RG_VGP3_EN, 0);
#else
	lcm_vgp_supply_disable();
#endif
	MDELAY(80);
}

LCM_DRIVER k710_hz_jd9366_boe_wxga_ips_101_lcm_drv = {
	.name			= "k710_hz_jd9366_boe_wxga_ips_101",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.resume         = lcm_resume,
	.suspend        = lcm_suspend,
};


