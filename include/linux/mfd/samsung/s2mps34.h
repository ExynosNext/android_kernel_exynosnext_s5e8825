/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Samsung S2MPS34 PMIC register definitions and structures.
 *
 * The S2MPS34 is the PMIC for the Samsung Exynos 1280 (s5e8825) SoC.
 * It exposes 39 LDOs, 9 BUCK regulators, RTC, and watchdog via I2C.
 *
 * Register map based on Samsung's 5.10 kernel source, adapted for the
 * standard Linux MFD/regulator framework.
 *
 * Copyright (C) 2026 ExynosNext Project
 */

#ifndef __LINUX_MFD_S2MPS34_H
#define __LINUX_MFD_S2MPS34_H

#include <linux/regmap.h>

enum s2mps34_device_type {
	S2MPS34X,
};

/* ── Register addresses ─────────────────────────────────────────────────── */

#define S2MPS34_REG_PMIC_ID		0x00
#define S2MPS34_REG_PMIC_REV		0x01
#define S2MPS34_REG_MAIN_INT		0x02
#define S2MPS34_REG_INT1		0x03
#define S2MPS34_REG_INT2		0x04
#define S2MPS34_REG_INT3		0x05
#define S2MPS34_REG_INT1M		0x06
#define S2MPS34_REG_INT2M		0x07
#define S2MPS34_REG_INT3M		0x08
#define S2MPS34_REG_CTRL1		0x0C
#define S2MPS34_REG_CTRL2		0x0D

/* LDO configuration registers (LDO1-LDO39) */
#define S2MPS34_REG_LDO1_CTRL		0x20
#define S2MPS34_REG_LDO2_CTRL		0x21
#define S2MPS34_REG_LDO3_CTRL		0x22
/* ... contiguous up to LDO39 */
#define S2MPS34_REG_LDO_CTRL(i)	(S2MPS34_REG_LDO1_CTRL + ((i) - 1))

#define S2MPS34_REG_LDO1_VSEL		0x40
#define S2MPS34_REG_LDO_VSEL(i)		(S2MPS34_REG_LDO1_VSEL + ((i) - 1))

/* BUCK configuration registers (BUCK1-BUCK9) */
#define S2MPS34_REG_BUCK1_CTRL		0x68
#define S2MPS34_REG_BUCK2_CTRL		0x69
#define S2MPS34_REG_BUCK3_CTRL		0x6A
#define S2MPS34_REG_BUCK4_CTRL		0x6B
#define S2MPS34_REG_BUCK5_CTRL		0x6C
#define S2MPS34_REG_BUCK6_CTRL		0x6D
#define S2MPS34_REG_BUCK7_CTRL		0x6E
#define S2MPS34_REG_BUCK8_CTRL		0x6F
#define S2MPS34_REG_BUCK9_CTRL		0x70
#define S2MPS34_REG_BUCK_CTRL(i)	(S2MPS34_REG_BUCK1_CTRL + ((i) - 1))

#define S2MPS34_REG_BUCK1_VSEL		0x78
#define S2MPS34_REG_BUCK_VSEL(i)	(S2MPS34_REG_BUCK1_VSEL + ((i) - 1))

/* RTC registers */
#define S2MPS34_REG_RTC_CTRL		0x80
#define S2MPS34_REG_RTC_UPDATE		0x81
#define S2MPS34_REG_RTC_SEC		0x82
#define S2MPS34_REG_RTC_MIN		0x83
#define S2MPS34_REG_RTC_HOUR		0x84
#define S2MPS34_REG_RTC_DAY		0x85
#define S2MPS34_REG_RTC_MONTH		0x86
#define S2MPS34_REG_RTC_YEAR		0x87
#define S2MPS34_REG_RTC_WDAY		0x88

#define S2MPS34_REG_MAX			0xFF

/* ── Control register bits ───────────────────────────────────────────────── */

#define S2MPS34_ENABLE			BIT(6)
#define S2MPS34_ENABLE_SHIFT		6
#define S2MPS34_LDO_VSEL_MASK		0x3F
#define S2MPS34_BUCK_VSEL_MASK		0xFF

/* ── Device structure ────────────────────────────────────────────────────── */

struct s2mps34_dev {
	struct device	*dev;
	struct i2c_client *i2c;
	struct regmap	*regmap;
	enum s2mps34_device_type device_type;
	int		irq;
};

#endif /* __LINUX_MFD_S2MPS34_H */
