/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Samsung Exynos 1280 (s5e8825) Clock Controller — driver-private definitions
 * ExynosNext Kernel — Linux 6.18 GKI
 *
 * The public clock IDs live in the device-tree binding header and are the
 * single source of truth shared with DT nodes. This header only adds
 * definitions that are internal to the driver.
 */

#ifndef _CLK_S5E8825_H
#define _CLK_S5E8825_H

#include <dt-bindings/clock/samsung,exynos1280-clk.h>

/*
 * Size of the clk_hw provider array. Must be strictly greater than the largest
 * CLK_* id defined in the binding header (currently CLK_PCLK_WDT = 251).
 */
#define CLK_MAX_CLKS	256

#endif /* _CLK_S5E8825_H */
