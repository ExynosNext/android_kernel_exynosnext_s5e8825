// SPDX-License-Identifier: GPL-2.0+
/*
 * Samsung S2MPS34 PMIC core driver (MFD).
 *
 * The S2MPS34 is Samsung's PMIC for the Exynos 1280 (s5e8825) SoC.
 * It provides 39 LDOs, 9 BUCK regulators, RTC, and watchdog.
 *
 * This driver uses the standard Linux MFD subsystem — it registers
 * the I2C device, creates regmap, and spawns child devices (regulator,
 * clk, rtc) via devm_mfd_add_devices. This is fully mainline-compatible
 * and follows the same pattern as the existing s2mps11 driver.
 *
 * Communication: I2C via the ACPM MFD bus (acpm-mfd-bus), which is an
 * I2C-over-ACPM-IPC tunnel. The standard I2C subsystem handles this
 * transparently — the PMIC appears as a normal I2C device at address 0x66.
 *
 * Register map (partial, from Samsung 5.10 source):
 *   0x00-0x0F: PMIC ID, version, interrupt status
 *   0x10-0x1F: interrupt masks, configuration
 *   0x20-0x3F: LDO1-LDO39 configuration
 *   0x40-0x5F: BUCK1-BUCK9 configuration
 *   0x60-0x7F: RTC, watchdog
 *
 * Copyright (C) 2026 ExynosNext Project
 */

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/mfd/core.h>
#include <linux/mfd/samsung/s2mps34.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

static const struct mfd_cell s2mps34_devs[] = {
	{ .name = "s2mps34-regulator", },
	{ .name = "s2mps34-clk", },
	{ .name = "s2mps34-rtc", },
};

static const struct regmap_config s2mps34_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = S2MPS34_REG_MAX,
};

static int s2mps34_probe(struct i2c_client *i2c,
			 const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct s2mps34_dev *s2mps34;
	unsigned int reg;
	int ret;

	s2mps34 = devm_kzalloc(&i2c->dev, sizeof(*s2mps34), GFP_KERNEL);
	if (!s2mps34)
		return -ENOMEM;

	s2mps34->dev = &i2c->dev;
	s2mps34->i2c = i2c;
	i2c_set_clientdata(i2c, s2mps34);

	regmap = devm_regmap_init_i2c(i2c, &s2mps34_regmap_config);
	if (IS_ERR(regmap))
		return dev_err_probe(&i2c->dev, PTR_ERR(regmap),
				     "failed to allocate regmap\n");
	s2mps34->regmap = regmap;

	/* Read PMIC ID */
	ret = regmap_read(regmap, S2MPS34_REG_PMIC_ID, &reg);
	if (ret)
		return dev_err_probe(&i2c->dev, ret,
				     "failed to read PMIC ID\n");

	s2mps34->device_type = S2MPS34X;

	dev_info(&i2c->dev, "Samsung S2MPS34 PMIC (ID: 0x%02x)\n", reg);

	ret = devm_mfd_add_devices(&i2c->dev, PLATFORM_DEVID_AUTO,
				    s2mps34_devs, ARRAY_SIZE(s2mps34_devs),
				    NULL, 0, NULL);
	if (ret)
		return dev_err_probe(&i2c->dev, ret,
				     "failed to add MFD devices\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id s2mps34_of_match[] = {
	{ .compatible = "samsung,s2mps34-pmic" },
	{ }
};
MODULE_DEVICE_TABLE(of, s2mps34_of_match);
#endif

static const struct i2c_device_id s2mps34_i2c_id[] = {
	{ "s2mps34-pmic", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mps34_i2c_id);

static struct i2c_driver s2mps34_driver = {
	.driver = {
		.name = "s2mps34",
		.of_match_table = of_match_ptr(s2mps34_of_match),
	},
	.probe = s2mps34_probe,
	.id_table = s2mps34_i2c_id,
};
module_i2c_driver(s2mps34_driver);

MODULE_DESCRIPTION("Samsung S2MPS34 PMIC core driver");
MODULE_AUTHOR("ExynosNext Project");
MODULE_LICENSE("GPL");
