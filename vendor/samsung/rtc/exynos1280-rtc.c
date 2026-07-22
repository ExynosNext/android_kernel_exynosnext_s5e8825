// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos 1280 RTC driver.
 *
 * Uses the standard Linux RTC subsystem (rtc_device + rtc_class_ops).
 * The RTC is part of the S2MPS34 PMIC but accessed via memory-mapped
 * registers at 0x11a20000 on the Exynos 1280.
 *
 * Register layout (standard Samsung RTC):
 *   RTCCON   (+0x00): control register
 *   RTCUPDATE (+0x01): update trigger
 *   RTCSEC   (+0x02): seconds (BCD)
 *   RTCMIN   (+0x03): minutes (BCD)
 *   RTCHOUR  (+0x04): hours (BCD)
 *   RTCDAY   (+0x05): day of month (BCD)
 *   RTCMON   (+0x06): month (BCD)
 *   RTCYEAR  (+0x07): year (BCD)
 *   RTCWDAY  (+0x08): day of week (BCD)
 *
 * Copyright (C) 2026 ExynosNext Project
 */

#include <linux/bcd.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>

#define RTCCON		0x00
#define RTCUPDATE	0x01
#define RTCSEC		0x02
#define RTCMIN		0x03
#define RTCHOUR		0x04
#define RTCDATE		0x05
#define RTCMON		0x06
#define RTCYEAR		0x07
#define RTCWDAY		0x08

#define RTCCON_RTCEN	BIT(0)
#define RTCCON_WRST	BIT(3)

#define RTC_UPDATE_WAIT_MS 50

struct exynos1280_rtc {
	struct rtc_device	*rtc_dev;
	void __iomem		*base;
	struct mutex		lock;
};

static inline void exynos1280_rtc_wait_update(struct exynos1280_rtc *rtc)
{
	writel(1, rtc->base + RTCUPDATE);
	usleep_range(RTC_UPDATE_WAIT_MS * 1000, RTC_UPDATE_WAIT_MS * 1000 + 100);
}

static int exynos1280_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct exynos1280_rtc *rtc = dev_get_drvdata(dev);
	unsigned int bcd;

	mutex_lock(&rtc->lock);

	writel(readl(rtc->base + RTCCON) | RTCCON_RTCEN, rtc->base + RTCCON);

	bcd = readb(rtc->base + RTCSEC);
	tm->tm_sec = bcd2bin(bcd);
	bcd = readb(rtc->base + RTCMIN);
	tm->tm_min = bcd2bin(bcd);
	bcd = readb(rtc->base + RTCHOUR);
	tm->tm_hour = bcd2bin(bcd);
	bcd = readb(rtc->base + RTCDATE);
	tm->tm_mday = bcd2bin(bcd);
	bcd = readb(rtc->base + RTCMON);
	tm->tm_mon = bcd2bin(bcd) - 1;
	bcd = readb(rtc->base + RTCYEAR);
	tm->tm_year = bcd2bin(bcd) + 100;

	writel(readl(rtc->base + RTCCON) & ~RTCCON_RTCEN, rtc->base + RTCCON);

	mutex_unlock(&rtc->lock);

	rtc_tm_to_time(tm, NULL);
	return 0;
}

static int exynos1280_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct exynos1280_rtc *rtc = dev_get_drvdata(dev);

	mutex_lock(&rtc->lock);

	writel(readl(rtc->base + RTCCON) | RTCCON_RTCEN, rtc->base + RTCCON);

	writeb(bin2bcd(tm->tm_sec), rtc->base + RTCSEC);
	writeb(bin2bcd(tm->tm_min), rtc->base + RTCMIN);
	writeb(bin2bcd(tm->tm_hour), rtc->base + RTCHOUR);
	writeb(bin2bcd(tm->tm_mday), rtc->base + RTCDATE);
	writeb(bin2bcd(tm->tm_mon + 1), rtc->base + RTCMON);
	writeb(bin2bcd(tm->tm_year - 100), rtc->base + RTCYEAR);

	writel(readl(rtc->base + RTCCON) & ~RTCCON_RTCEN, rtc->base + RTCCON);

	exynos1280_rtc_wait_update(rtc);

	mutex_unlock(&rtc->lock);
	return 0;
}

static const struct rtc_class_ops exynos1280_rtc_ops = {
	.read_time	= exynos1280_rtc_read_time,
	.set_time	= exynos1280_rtc_set_time,
};

static int exynos1280_rtc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct exynos1280_rtc *rtc;
	struct resource *res;
	int ret;

	rtc = devm_kzalloc(dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rtc->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(rtc->base))
		return dev_err_probe(dev, PTR_ERR(rtc->base),
				     "failed to map RTC registers\n");

	mutex_init(&rtc->lock);

	platform_set_drvdata(pdev, rtc);

	/* Clear reset bit */
	writel(readl(rtc->base + RTCCON) & ~RTCCON_WRST, rtc->base + RTCCON);

	rtc->rtc_dev = devm_rtc_allocate_device(dev);
	if (IS_ERR(rtc->rtc_dev))
		return PTR_ERR(rtc->rtc_dev);

	rtc->rtc_dev->ops = &exynos1280_rtc_ops;
	rtc->rtc_dev->range_min = RTC_TIMESTAMP_BEGIN_2000;
	rtc->rtc_dev->range_max = RTC_TIMESTAMP_END_2099;

	ret = devm_rtc_register_device(rtc->rtc_dev);
	if (ret)
		return dev_err_probe(dev, ret, "failed to register RTC\n");

	dev_info(dev, "Exynos 1280 RTC @ 0x%lx\n", (unsigned long)res->start);
	return 0;
}

static const struct of_device_id exynos1280_rtc_match[] = {
	{ .compatible = "samsung,exynos8-rtc" },
	{ .compatible = "samsung,exynos1280-rtc" },
	{ }
};
MODULE_DEVICE_TABLE(of, exynos1280_rtc_match);

static struct platform_driver exynos1280_rtc_driver = {
	.probe	= exynos1280_rtc_probe,
	.driver = {
		.name = "exynos1280-rtc",
		.of_match_table = exynos1280_rtc_match,
	},
};
module_platform_driver(exynos1280_rtc_driver);

MODULE_DESCRIPTION("Samsung Exynos 1280 RTC driver");
MODULE_AUTHOR("ExynosNext Project");
MODULE_LICENSE("GPL v2");
