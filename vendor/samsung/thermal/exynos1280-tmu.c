// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos 1280 TMU (Thermal Management Unit) driver.
 *
 * Uses the standard Linux thermal subsystem (thermal_zone_device +
 * thermal_zone_device_ops). The Exynos 1280 has multiple TMU sensors
 * monitoring CPU clusters, GPU, and ISP.
 *
 * Register layout (standard Samsung TMU v2):
 *   TMU_TRIMINFO   (+0x00): trim info (calibration data)
 *   TMU_CON         (+0x20): control register
 *   TMU_STATUS      (+0x28): status register
 *   TMU_CURRENT_TEMP (+0x40): current temperature (8 bits, 2's complement)
 *   TMU_THRESHOLD   (+0x50): threshold for interrupt
 *   TMU_THRESHOLD_RISE0 (+0x54): rising threshold 0
 *   TMU_THRESHOLD_RISE1 (+0x58): rising threshold 1
 *   TMU_THRESHOLD_FALL  (+0x5C): falling threshold
 *
 * Temperature: value in degrees Celsius (2's complement, resolution 1°C)
 *
 * Copyright (C) 2026 ExynosNext Project
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>

#define TMU_TRIMINFO		0x00
#define TMU_CON		0x20
#define TMU_STATUS		0x28
#define TMU_CURRENT_TEMP	0x40
#define TMU_THRESHOLD		0x50

#define TMU_CON_EN		BIT(0)
#define TMU_CON_BUF_VREF_SEL	(0x3 << 2)
#define TMU_CON_BUF_INT_EN	BIT(7)

#define TMU_TEMP_MASK		0xFF
#define TMU_TEMP_SHIFT		0

struct exynos1280_tmu {
	void __iomem		*base;
	struct thermal_zone_device *tzd;
	int			temp_offset;
};

static int exynos1280_tmu_get_temp(struct thermal_zone_device *tz, int *temp)
{
	struct exynos1280_tmu *tmu = thermal_zone_device_priv(tz);
	u8 code;

	code = readb(tmu->base + TMU_CURRENT_TEMP);
	/* Temperature is in 2's complement, 1°C resolution */
	*temp = (s8)code + tmu->temp_offset;

	return 0;
}

static struct thermal_zone_device_ops exynos1280_tmu_ops = {
	.get_temp	= exynos1280_tmu_get_temp,
};

static int exynos1280_tmu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct exynos1280_tmu *tmu;
	struct resource *res;
	u32 trim;
	int ret;

	tmu = devm_kzalloc(dev, sizeof(*tmu), GFP_KERNEL);
	if (!tmu)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tmu->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(tmu->base))
		return dev_err_probe(dev, PTR_ERR(tmu->base),
				     "failed to map TMU registers\n");

	/* Read trim info for calibration offset */
	trim = readl(tmu->base + TMU_TRIMINFO);
	tmu->temp_offset = (trim & 0xFF) - 50; /* approximate calibration */

	/* Enable TMU */
	writel(readl(tmu->base + TMU_CON) | TMU_CON_EN | TMU_CON_BUF_INT_EN,
	       tmu->base + TMU_CON);

	platform_set_drvdata(pdev, tmu);

	tmu->tzd = devm_thermal_of_zone_register(dev, 0, tmu,
						 &exynos1280_tmu_ops);
	if (IS_ERR(tmu->tzd))
		return dev_err_probe(dev, PTR_ERR(tmu->tzd),
				     "failed to register thermal zone\n");

	dev_info(dev, "Exynos 1280 TMU @ 0x%lx (offset=%d)\n",
		 (unsigned long)res->start, tmu->temp_offset);
	return 0;
}

static const struct of_device_id exynos1280_tmu_match[] = {
	{ .compatible = "samsung,exynos-tmu-v2" },
	{ .compatible = "samsung,exynos1280-tmu" },
	{ }
};
MODULE_DEVICE_TABLE(of, exynos1280_tmu_match);

static struct platform_driver exynos1280_tmu_driver = {
	.probe	= exynos1280_tmu_probe,
	.driver = {
		.name = "exynos1280-tmu",
		.of_match_table = exynos1280_tmu_match,
	},
};
module_platform_driver(exynos1280_tmu_driver);

MODULE_DESCRIPTION("Samsung Exynos 1280 TMU thermal driver");
MODULE_AUTHOR("ExynosNext Project");
MODULE_LICENSE("GPL v2");
