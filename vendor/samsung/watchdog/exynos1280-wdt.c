// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos 1280 watchdog timer driver.
 *
 * Uses the standard Linux watchdog subsystem (watchdog_device + watchdog_ops).
 * The Exynos 1280 has two watchdog instances:
 *   - watchdog_cl0 @ 0x10060000 (cluster 0 — LITTLE/A55)
 *   - watchdog_cl1 @ 0x10070000 (cluster 1 — big/A78)
 *
 * Register layout (standard Samsung watchdog, 0x100 bytes):
 *   WTCON   (+0x00): control register
 *   WTDAT   (+0x04): data register (timeout value)
 *   WTCNT   (+0x08): count register
 *   WTCLRINT (+0x0C): interrupt clear
 *
 * The watchdog is clocked from the ACPM-managed clock tree. On mainline
 * (non-GKI), it runs on the fin_pll (26 MHz) oscillator directly.
 *
 * Copyright (C) 2026 ExynosNext Project
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>

#define WTCON		0x00
#define WTDAT		0x04
#define WTCNT		0x08
#define WTCLRINT	0x0C

#define WTCON_RST_EN		BIT(0)
#define WTCON_INT_EN		BIT(2)
#define WTCON_CLK_SEL		(0x3 << 3)
#define WTCON_CLK_DIV		(0xFF << 8)
#define WTCON_WDT_EN		BIT(5)
#define WTCON_PRESCALER_SHIFT	8
#define WTCON_PRESCALER_MASK	0xFF

#define WDT_DEFAULT_TIMEOUT	30
#define WDT_FREQ		26000000 /* 26 MHz fin_pll */

struct exynos1280_wdt {
	struct watchdog_device	wdd;
	void __iomem		*base;
	struct clk		*clk;
	unsigned long		rate;
};

static int exynos1280_wdt_start(struct watchdog_device *wdd)
{
	struct exynos1280_wdt *wdt = watchdog_get_drvdata(wdd);
	u32 wtcon, wtcnt;

	wtcon = readl(wdt->base + WTCON);
	wtcnt = readl(wdt->base + WTCNT);

	/* Set timeout value */
	writel(wdd->timeout * (wdt->rate / 256), wdt->base + WTDAT);
	writel(wdd->timeout * (wdt->rate / 256), wdt->base + WTCNT);

	/* Enable watchdog: prescaler=255, clock divider=128, reset enable */
	wtcon = (0xFF << WTCON_PRESCALER_SHIFT) | WTCON_CLK_SEL | WTCON_WDT_EN | WTCON_RST_EN;
	writel(wtcon, wdt->base + WTCON);

	return 0;
}

static int exynos1280_wdt_stop(struct watchdog_device *wdd)
{
	struct exynos1280_wdt *wdt = watchdog_get_drvdata(wdd);

	writel(0, wdt->base + WTCON);
	return 0;
}

static int exynos1280_wdt_ping(struct watchdog_device *wdd)
{
	struct exynos1280_wdt *wdt = watchdog_get_drvdata(wdd);

	writel(wdd->timeout * (wdt->rate / 256), wdt->base + WTCNT);
	return 0;
}

static int exynos1280_wdt_set_timeout(struct watchdog_device *wdd,
				      unsigned int timeout)
{
	wdd->timeout = timeout;
	return exynos1280_wdt_start(wdd);
}

static unsigned int exynos1280_wdt_get_timeleft(struct watchdog_device *wdd)
{
	struct exynos1280_wdt *wdt = watchdog_get_drvdata(wdd);
	u32 count = readl(wdt->base + WTCNT);

	return count / (wdt->rate / 256);
}

static const struct watchdog_ops exynos1280_wdt_ops = {
	.start		= exynos1280_wdt_start,
	.stop		= exynos1280_wdt_stop,
	.ping		= exynos1280_wdt_ping,
	.set_timeout	= exynos1280_wdt_set_timeout,
	.get_timeleft	= exynos1280_wdt_get_timeleft,
	.owner		= THIS_MODULE,
};

static const struct watchdog_info exynos1280_wdt_info = {
	.options	= WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING |
			  WDIOF_MAGICCLOSE | WDIOF_TIMELEFT,
	.identity	= "Exynos 1280 Watchdog",
};

static int exynos1280_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct exynos1280_wdt *wdt;
	struct resource *res;
	int ret;

	wdt = devm_kzalloc(dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	wdt->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(wdt->base))
		return dev_err_probe(dev, PTR_ERR(wdt->base),
				     "failed to map watchdog registers\n");

	/* Get clock (optional — fall back to fin_pll if not provided) */
	wdt->clk = devm_clk_get_optional(dev, "watchdog");
	if (wdt->clk && !IS_ERR(wdt->clk)) {
		ret = clk_prepare_enable(wdt->clk);
		if (ret)
			return ret;
		wdt->rate = clk_get_rate(wdt->clk);
	} else {
		wdt->rate = WDT_FREQ;
	}

	wdt->wdd.info = &exynos1280_wdt_info;
	wdt->wdd.ops = &exynos1280_wdt_ops;
	wdt->wdd.timeout = WDT_DEFAULT_TIMEOUT;
	wdt->wdd.min_timeout = 1;
	wdt->wdd.max_timeout = 65535 / (wdt->rate / 256);
	wdt->wdd.parent = dev;

	watchdog_init_timeout(&wdt->wdd, 0, dev);
	watchdog_set_drvdata(&wdt->wdd, wdt);
	watchdog_set_nowayout(&wdt->wdd, WATCHDOG_NOWAYOUT);

	ret = devm_watchdog_register_device(dev, &wdt->wdd);
	if (ret)
		return dev_err_probe(dev, ret, "failed to register watchdog\n");

	platform_set_drvdata(pdev, wdt);
	dev_info(dev, "Exynos 1280 watchdog @ 0x%lx (rate=%lu Hz)\n",
		 (unsigned long)res->start, wdt->rate);
	return 0;
}

static const struct of_device_id exynos1280_wdt_match[] = {
	{ .compatible = "samsung,s5e8825-v1-wdt" },
	{ .compatible = "samsung,s5e8825-v2-wdt" },
	{ .compatible = "samsung,exynos1280-wdt" },
	{ }
};
MODULE_DEVICE_TABLE(of, exynos1280_wdt_match);

static struct platform_driver exynos1280_wdt_driver = {
	.probe	= exynos1280_wdt_probe,
	.driver = {
		.name = "exynos1280-wdt",
		.of_match_table = exynos1280_wdt_match,
	},
};
module_platform_driver(exynos1280_wdt_driver);

MODULE_DESCRIPTION("Samsung Exynos 1280 watchdog driver");
MODULE_AUTHOR("ExynosNext Project");
MODULE_LICENSE("GPL v2");
