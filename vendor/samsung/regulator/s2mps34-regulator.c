// SPDX-License-Identifier: GPL-2.0+
/*
 * Samsung S2MPS34 regulator driver.
 *
 * Provides 39 LDOs and 9 BUCK regulators via the standard Linux
 * regulator subsystem. Uses regmap for register access and the
 * standard regulator_desc/regulator_config framework — fully
 * mainline-compatible.
 *
 * LDO voltage ranges: 0.8V - 3.95V (step 25mV)
 * BUCK voltage ranges: 0.5V - 2.3V (step 6.25mV)
 *
 * Copyright (C) 2026 ExynosNext Project
 */

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/mfd/samsung/s2mps34.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#define S2MPS34_LDO_MIN_UV		800000
#define S2MPS34_LDO_STEP_UV		25000
#define S2MPS34_LDO_N_VOLTAGES		64
#define S2MPS34_LDO_VSEL_MASK		0x3F

#define S2MPS34_BUCK_MIN_UV		500000
#define S2MPS34_BUCK_STEP_UV		6250
#define S2MPS34_BUCK_N_VOLTAGES		256
#define S2MPS34_BUCK_VSEL_MASK		0xFF

/* ── LDO operations ─────────────────────────────────────────────────────── */

static const struct regulator_ops s2mps34_ldo_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
};

static const struct regulator_ops s2mps34_buck_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
};

/* ── Regulator descriptors ────────────────────────────────────────────────── */

#define S2MPS34_LDO_DESC(_id, _name, _reg_ctrl, _reg_vsel)	\
	{							\
		.id = S2MPS34_LDO##_id,				\
		.name = _name,					\
		.of_match = of_match_ptr(_name),		\
		.regulators_node = of_match_ptr("regulators"),	\
		.ops = &s2mps34_ldo_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.owner = THIS_MODULE,				\
		.min_uV = S2MPS34_LDO_MIN_UV,			\
		.uV_step = S2MPS34_LDO_STEP_UV,		\
		.n_voltages = S2MPS34_LDO_N_VOLTAGES,		\
		.vsel_reg = _reg_vsel,				\
		.vsel_mask = S2MPS34_LDO_VSEL_MASK,		\
		.enable_reg = _reg_ctrl,			\
		.enable_mask = S2MPS34_ENABLE,			\
	}

#define S2MPS34_BUCK_DESC(_id, _name, _reg_ctrl, _reg_vsel)	\
	{							\
		.id = S2MPS34_BUCK##_id,			\
		.name = _name,					\
		.of_match = of_match_ptr(_name),		\
		.regulators_node = of_match_ptr("regulators"),	\
		.ops = &s2mps34_buck_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.owner = THIS_MODULE,				\
		.min_uV = S2MPS34_BUCK_MIN_UV,			\
		.uV_step = S2MPS34_BUCK_STEP_UV,		\
		.n_voltages = S2MPS34_BUCK_N_VOLTAGES,		\
		.vsel_reg = _reg_vsel,				\
		.vsel_mask = S2MPS34_BUCK_VSEL_MASK,		\
		.enable_reg = _reg_ctrl,			\
		.enable_mask = S2MPS34_ENABLE,			\
	}

enum s2mps34_regulator_id {
	S2MPS34_LDO1, S2MPS34_LDO2, S2MPS34_LDO3, S2MPS34_LDO4,
	S2MPS34_LDO5, S2MPS34_LDO6, S2MPS34_LDO7, S2MPS34_LDO8,
	S2MPS34_LDO9, S2MPS34_LDO10, S2MPS34_LDO11, S2MPS34_LDO12,
	S2MPS34_LDO13, S2MPS34_LDO14, S2MPS34_LDO15, S2MPS34_LDO16,
	S2MPS34_LDO17, S2MPS34_LDO18, S2MPS34_LDO19, S2MPS34_LDO20,
	S2MPS34_LDO21, S2MPS34_LDO22, S2MPS34_LDO23, S2MPS34_LDO24,
	S2MPS34_LDO25, S2MPS34_LDO26, S2MPS34_LDO27, S2MPS34_LDO28,
	S2MPS34_LDO29, S2MPS34_LDO30, S2MPS34_LDO31, S2MPS34_LDO32,
	S2MPS34_LDO33, S2MPS34_LDO34, S2MPS34_LDO35, S2MPS34_LDO36,
	S2MPS34_LDO37, S2MPS34_LDO38, S2MPS34_LDO39,
	S2MPS34_BUCK1, S2MPS34_BUCK2, S2MPS34_BUCK3, S2MPS34_BUCK4,
	S2MPS34_BUCK5, S2MPS34_BUCK6, S2MPS34_BUCK7, S2MPS34_BUCK8,
	S2MPS34_BUCK9,
	S2MPS34_REGULATOR_MAX,
};

static const struct regulator_desc s2mps34_regulators[] = {
	/* LDOs 1-39 */
	S2MPS34_LDO_DESC(1,  "LDO1",  S2MPS34_REG_LDO_CTRL(1),  S2MPS34_REG_LDO_VSEL(1)),
	S2MPS34_LDO_DESC(2,  "LDO2",  S2MPS34_REG_LDO_CTRL(2),  S2MPS34_REG_LDO_VSEL(2)),
	S2MPS34_LDO_DESC(3,  "LDO3",  S2MPS34_REG_LDO_CTRL(3),  S2MPS34_REG_LDO_VSEL(3)),
	S2MPS34_LDO_DESC(4,  "LDO4",  S2MPS34_REG_LDO_CTRL(4),  S2MPS34_REG_LDO_VSEL(4)),
	S2MPS34_LDO_DESC(5,  "LDO5",  S2MPS34_REG_LDO_CTRL(5),  S2MPS34_REG_LDO_VSEL(5)),
	S2MPS34_LDO_DESC(6,  "LDO6",  S2MPS34_REG_LDO_CTRL(6),  S2MPS34_REG_LDO_VSEL(6)),
	S2MPS34_LDO_DESC(7,  "LDO7",  S2MPS34_REG_LDO_CTRL(7),  S2MPS34_REG_LDO_VSEL(7)),
	S2MPS34_LDO_DESC(8,  "LDO8",  S2MPS34_REG_LDO_CTRL(8),  S2MPS34_REG_LDO_VSEL(8)),
	S2MPS34_LDO_DESC(9,  "LDO9",  S2MPS34_REG_LDO_CTRL(9),  S2MPS34_REG_LDO_VSEL(9)),
	S2MPS34_LDO_DESC(10, "LDO10", S2MPS34_REG_LDO_CTRL(10), S2MPS34_REG_LDO_VSEL(10)),
	S2MPS34_LDO_DESC(11, "LDO11", S2MPS34_REG_LDO_CTRL(11), S2MPS34_REG_LDO_VSEL(11)),
	S2MPS34_LDO_DESC(12, "LDO12", S2MPS34_REG_LDO_CTRL(12), S2MPS34_REG_LDO_VSEL(12)),
	S2MPS34_LDO_DESC(13, "LDO13", S2MPS34_REG_LDO_CTRL(13), S2MPS34_REG_LDO_VSEL(13)),
	S2MPS34_LDO_DESC(14, "LDO14", S2MPS34_REG_LDO_CTRL(14), S2MPS34_REG_LDO_VSEL(14)),
	S2MPS34_LDO_DESC(15, "LDO15", S2MPS34_REG_LDO_CTRL(15), S2MPS34_REG_LDO_VSEL(15)),
	S2MPS34_LDO_DESC(16, "LDO16", S2MPS34_REG_LDO_CTRL(16), S2MPS34_REG_LDO_VSEL(16)),
	S2MPS34_LDO_DESC(17, "LDO17", S2MPS34_REG_LDO_CTRL(17), S2MPS34_REG_LDO_VSEL(17)),
	S2MPS34_LDO_DESC(18, "LDO18", S2MPS34_REG_LDO_CTRL(18), S2MPS34_REG_LDO_VSEL(18)),
	S2MPS34_LDO_DESC(19, "LDO19", S2MPS34_REG_LDO_CTRL(19), S2MPS34_REG_LDO_VSEL(19)),
	S2MPS34_LDO_DESC(20, "LDO20", S2MPS34_REG_LDO_CTRL(20), S2MPS34_REG_LDO_VSEL(20)),
	S2MPS34_LDO_DESC(21, "LDO21", S2MPS34_REG_LDO_CTRL(21), S2MPS34_REG_LDO_VSEL(21)),
	S2MPS34_LDO_DESC(22, "LDO22", S2MPS34_REG_LDO_CTRL(22), S2MPS34_REG_LDO_VSEL(22)),
	S2MPS34_LDO_DESC(23, "LDO23", S2MPS34_REG_LDO_CTRL(23), S2MPS34_REG_LDO_VSEL(23)),
	S2MPS34_LDO_DESC(24, "LDO24", S2MPS34_REG_LDO_CTRL(24), S2MPS34_REG_LDO_VSEL(24)),
	S2MPS34_LDO_DESC(25, "LDO25", S2MPS34_REG_LDO_CTRL(25), S2MPS34_REG_LDO_VSEL(25)),
	S2MPS34_LDO_DESC(26, "LDO26", S2MPS34_REG_LDO_CTRL(26), S2MPS34_REG_LDO_VSEL(26)),
	S2MPS34_LDO_DESC(27, "LDO27", S2MPS34_REG_LDO_CTRL(27), S2MPS34_REG_LDO_VSEL(27)),
	S2MPS34_LDO_DESC(28, "LDO28", S2MPS34_REG_LDO_CTRL(28), S2MPS34_REG_LDO_VSEL(28)),
	S2MPS34_LDO_DESC(29, "LDO29", S2MPS34_REG_LDO_CTRL(29), S2MPS34_REG_LDO_VSEL(29)),
	S2MPS34_LDO_DESC(30, "LDO30", S2MPS34_REG_LDO_CTRL(30), S2MPS34_REG_LDO_VSEL(30)),
	S2MPS34_LDO_DESC(31, "LDO31", S2MPS34_REG_LDO_CTRL(31), S2MPS34_REG_LDO_VSEL(31)),
	S2MPS34_LDO_DESC(32, "LDO32", S2MPS34_REG_LDO_CTRL(32), S2MPS34_REG_LDO_VSEL(32)),
	S2MPS34_LDO_DESC(33, "LDO33", S2MPS34_REG_LDO_CTRL(33), S2MPS34_REG_LDO_VSEL(33)),
	S2MPS34_LDO_DESC(34, "LDO34", S2MPS34_REG_LDO_CTRL(34), S2MPS34_REG_LDO_VSEL(34)),
	S2MPS34_LDO_DESC(35, "LDO35", S2MPS34_REG_LDO_CTRL(35), S2MPS34_REG_LDO_VSEL(35)),
	S2MPS34_LDO_DESC(36, "LDO36", S2MPS34_REG_LDO_CTRL(36), S2MPS34_REG_LDO_VSEL(36)),
	S2MPS34_LDO_DESC(37, "LDO37", S2MPS34_REG_LDO_CTRL(37), S2MPS34_REG_LDO_VSEL(37)),
	S2MPS34_LDO_DESC(38, "LDO38", S2MPS34_REG_LDO_CTRL(38), S2MPS34_REG_LDO_VSEL(38)),
	S2MPS34_LDO_DESC(39, "LDO39", S2MPS34_REG_LDO_CTRL(39), S2MPS34_REG_LDO_VSEL(39)),
	/* BUCKs 1-9 */
	S2MPS34_BUCK_DESC(1, "BUCK1", S2MPS34_REG_BUCK_CTRL(1), S2MPS34_REG_BUCK_VSEL(1)),
	S2MPS34_BUCK_DESC(2, "BUCK2", S2MPS34_REG_BUCK_CTRL(2), S2MPS34_REG_BUCK_VSEL(2)),
	S2MPS34_BUCK_DESC(3, "BUCK3", S2MPS34_REG_BUCK_CTRL(3), S2MPS34_REG_BUCK_VSEL(3)),
	S2MPS34_BUCK_DESC(4, "BUCK4", S2MPS34_REG_BUCK_CTRL(4), S2MPS34_REG_BUCK_VSEL(4)),
	S2MPS34_BUCK_DESC(5, "BUCK5", S2MPS34_REG_BUCK_CTRL(5), S2MPS34_REG_BUCK_VSEL(5)),
	S2MPS34_BUCK_DESC(6, "BUCK6", S2MPS34_REG_BUCK_CTRL(6), S2MPS34_REG_BUCK_VSEL(6)),
	S2MPS34_BUCK_DESC(7, "BUCK7", S2MPS34_REG_BUCK_CTRL(7), S2MPS34_REG_BUCK_VSEL(7)),
	S2MPS34_BUCK_DESC(8, "BUCK8", S2MPS34_REG_BUCK_CTRL(8), S2MPS34_REG_BUCK_VSEL(8)),
	S2MPS34_BUCK_DESC(9, "BUCK9", S2MPS34_REG_BUCK_CTRL(9), S2MPS34_REG_BUCK_VSEL(9)),
};

/* ── Platform driver ──────────────────────────────────────────────────────── */

static int s2mps34_regulator_probe(struct platform_device *pdev)
{
	struct s2mps34_dev *s2mps34 = dev_get_drvdata(pdev->dev.parent);
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	int i;

	config.dev = pdev->dev.parent;
	config.regmap = s2mps34->regmap;
	config.driver_data = s2mps34;

	for (i = 0; i < ARRAY_SIZE(s2mps34_regulators); i++) {
		rdev = devm_regulator_register(&pdev->dev,
					       &s2mps34_regulators[i],
					       &config);
		if (IS_ERR(rdev))
			return dev_err_probe(&pdev->dev, PTR_ERR(rdev),
				   "failed to register regulator %s\n",
				   s2mps34_regulators[i].name);
	}

	dev_info(&pdev->dev, "S2MPS34: %zu regulators registered\n",
		 ARRAY_SIZE(s2mps34_regulators));
	return 0;
}

static const struct platform_device_id s2mps34_regulator_id[] = {
	{ "s2mps34-regulator", 0 },
	{ }
};

static struct platform_driver s2mps34_regulator_driver = {
	.driver = {
		.name = "s2mps34-regulator",
	},
	.probe = s2mps34_regulator_probe,
	.id_table = s2mps34_regulator_id,
};
module_platform_driver(s2mps34_regulator_driver);

MODULE_DESCRIPTION("Samsung S2MPS34 regulator driver");
MODULE_AUTHOR("ExynosNext Project");
MODULE_LICENSE("GPL");
