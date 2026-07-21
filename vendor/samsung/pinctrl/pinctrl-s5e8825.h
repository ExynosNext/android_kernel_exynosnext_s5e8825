/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos 1280 (s5e8825) pinctrl bank definitions.
 *
 * Bank names and controller addresses are transcribed from the real
 * device tree shipped in Samsung's official firmware
 * (A536BXXSMGZE1, SM-A536B / Galaxy A53 5G).
 *
 * Pin counts per bank follow the Samsung Exynos convention and are
 * consistent with the USI (Universal Serial Interface) architecture
 * used on this SoC. Verify against the TRM if available.
 *
 * Register layout (per bank, stride = 0x20):
 *   CON    (+0x00): pin function select, 4 bits per pin
 *   DAT    (+0x04): data input/output, 1 bit per pin
 *   PUD    (+0x08): pull up/down, 2 bits per pin
 *   DRV    (+0x0C): drive strength, 2 bits per pin
 *   CONPDN (+0x10): suspend config, 4 bits per pin
 *   PUDPDN (+0x14): suspend pull, 2 bits per pin
 *
 * Copyright (C) 2026 ExynosNext Project
 */

#ifndef __PINCTRL_S5E8825_H
#define __PINCTRL_S5E8825_H

/* Register offsets within each bank */
#define EXYNOS_PIN_CON		0x00
#define EXYNOS_PIN_DAT		0x04
#define EXYNOS_PIN_PUD		0x08
#define EXYNOS_PIN_DRV		0x0C
#define EXYNOS_PIN_CONPDN	0x10
#define EXYNOS_PIN_PUDPDN	0x14
#define EXYNOS_BANK_SIZE	0x20

/* CON register: 4 bits per pin, function 0 = GPIO input, 1 = GPIO output,
 * 2-F = alternate function (AF0-AF13) */
#define EXYNOS_PIN_FUNC_SHIFT	4
#define EXYNOS_PIN_FUNC_MASK	0xF
#define EXYNOS_PIN_FUNC_INPUT	0x0
#define EXYNOS_PIN_FUNC_OUTPUT	0x1

/* PUD register: 2 bits per pin */
#define EXYNOS_PIN_PUD_SHIFT	2
#define EXYNOS_PIN_PUD_MASK	0x3
#define EXYNOS_PIN_PUD_NONE	0x0
#define EXYNOS_PIN_PUD_DOWN	0x1
#define EXYNOS_PIN_PUD_UP	0x3

/* DRV register: 2 bits per pin */
#define EXYNOS_PIN_DRV_SHIFT	2
#define EXYNOS_PIN_DRV_MASK	0x3
#define EXYNOS_PIN_DRV_1X	0x0
#define EXYNOS_PIN_DRV_2X	0x1
#define EXYNOS_PIN_DRV_3X	0x2
#define EXYNOS_PIN_DRV_4X	0x3

/* EINT (external interrupt) support */
#define EXYNOS_PIN_EINTCON	0x700
#define EXYNOS_PIN_EINTMASK	0x900
#define EXYNOS_PIN_EINTPEND	0xA00
#define EXYNOS_PIN_EINTFLTCON0	0xB00
#define EXYNOS_PIN_EINTFLTCON1	0xB04

/**
 * struct s5e8825_pin_bank - per-bank data
 * @name:    bank name (e.g., "gpa0")
 * @pin_base: global pin number offset for this bank
 * @nr_pins: number of GPIO pins in this bank
 * @eint_type: external interrupt type (0 = none, 1 = GPIO EINT)
 */
struct s5e8825_pin_bank {
	const char	*name;
	unsigned int	pin_base;
	unsigned int	nr_pins;
	unsigned int	eint_type;
};

/**
 * struct s5e8825_pin_ctrl - per-controller data
 * @base:    physical base address of the controller
 * @nr_banks: number of GPIO banks in this controller
 * @banks:   array of bank descriptors
 * @eint_wakeup_mask: offset of the EINT wakeup mask register in PMU
 */
struct s5e8825_pin_ctrl {
	unsigned long		base;
	unsigned int		nr_banks;
	const struct s5e8825_pin_bank *banks;
};

/*
 * Bank definitions — all 7 controllers and 48 banks from the real DT.
 *
 * Pin counts are based on the Samsung Exynos convention:
 *   - gpa0/gpa1/gpx0/gpv0: 8 pins (general-purpose alive/volatile)
 *   - gpq0/gpq1: 7 pins
 *   - gpc0-gpc7: 4 pins each (USI function blocks)
 *   - gpm0-gpm20: 4 pins each (GPIO M group)
 *   - gpf0/gpf1/gpf2/gpf3: 4 pins each
 *   - gpp0-gpp3: 4 pins each
 *   - gpg0-gpg2: 8 pins each
 *   - gpb0/gpb1: 5 pins each
 */

/* Controller 0: pinctrl@11850000 — Alive domain (always-on) */
static const struct s5e8825_pin_bank s5e8825_banks_alive[] = {
	{ "gpa0", 0,  8, 1 }, { "gpa1", 8,  8, 1 },
	{ "gpq0", 16, 7, 1 }, { "gpq1", 23, 7, 1 },
	{ "gpc0", 30, 4, 0 }, { "gpc1", 34, 4, 0 },
	{ "gpc2", 38, 4, 0 }, { "gpc3", 42, 4, 0 },
	{ "gpc4", 46, 4, 0 }, { "gpc5", 50, 4, 0 },
	{ "gpc6", 54, 4, 0 }, { "gpc7", 58, 4, 0 },
	{ "gpx0", 62, 8, 1 },
};

/* Controller 1: pinctrl@11430000 — CMU_PERI domain */
static const struct s5e8825_pin_bank s5e8825_banks_peri[] = {
	{ "gpm0", 0, 4, 0 },  { "gpm1", 4, 4, 0 },
	{ "gpm2", 8, 4, 0 },  { "gpm3", 12, 4, 0 },
	{ "gpm4", 16, 4, 0 }, { "gpm5", 20, 4, 0 },
	{ "gpm6", 24, 4, 0 }, { "gpm7", 28, 4, 0 },
	{ "gpm8", 32, 4, 0 }, { "gpm9", 36, 4, 0 },
	{ "gpm10", 40, 4, 0 }, { "gpm11", 44, 4, 0 },
	{ "gpm12", 48, 4, 0 }, { "gpm13", 52, 4, 0 },
	{ "gpm14", 56, 4, 0 }, { "gpm15", 60, 4, 0 },
	{ "gpm16", 64, 4, 0 }, { "gpm17", 68, 4, 0 },
	{ "gpm18", 72, 4, 0 }, { "gpm19", 76, 4, 0 },
	{ "gpm20", 80, 4, 0 },
};

/* Controller 2: pinctrl@13450000 — FSYS domain */
static const struct s5e8825_pin_bank s5e8825_banks_fsys[] = {
	{ "gpf0", 0, 4, 0 },
	{ "gpf1", 4, 4, 0 },
};

/* Controller 3: pinctrl@13440000 — FSYS extra */
static const struct s5e8825_pin_bank s5e8825_banks_fsys2[] = {
	{ "gpf3", 0, 4, 0 },
};

/* Controller 4: pinctrl@10040000 — Core domain */
static const struct s5e8825_pin_bank s5e8825_banks_core[] = {
	{ "gpp0", 0, 4, 0 },  { "gpp1", 4, 4, 0 },
	{ "gpp2", 8, 4, 0 },  { "gpp3", 12, 4, 0 },
	{ "gpg0", 16, 8, 0 }, { "gpg1", 24, 8, 0 },
	{ "gpg2", 32, 8, 0 }, { "gpb0", 40, 5, 0 },
	{ "gpb1", 45, 5, 0 },
};

/* Controller 5: pinctrl@100f0000 — Core extra */
static const struct s5e8825_pin_bank s5e8825_banks_core2[] = {
	{ "gpf2", 0, 4, 0 },
};

/* Controller 6: pinctrl@11780000 — Audio domain */
static const struct s5e8825_pin_bank s5e8825_banks_aud[] = {
	{ "gpv0", 0, 8, 0 },
};

/* Controller 7: pinctrl@111d0000 — Additional domain */
static const struct s5e8825_pin_bank s5e8825_banks_misc[] = {
	/* The real DT shows this controller exists but has no bank
	 * sub-nodes with gpio-controller — it's used for pin config
	 * only. Leave empty for now. */
};

/* Controller table — indexed by DT reg[0] */
static const struct s5e8825_pin_ctrl s5e8825_pin_ctrls[] = {
	{ 0x11850000, ARRAY_SIZE(s5e8825_banks_alive),  s5e8825_banks_alive },
	{ 0x11430000, ARRAY_SIZE(s5e8825_banks_peri),   s5e8825_banks_peri },
	{ 0x13450000, ARRAY_SIZE(s5e8825_banks_fsys),   s5e8825_banks_fsys },
	{ 0x13440000, ARRAY_SIZE(s5e8825_banks_fsys2),  s5e8825_banks_fsys2 },
	{ 0x10040000, ARRAY_SIZE(s5e8825_banks_core),   s5e8825_banks_core },
	{ 0x100f0000, ARRAY_SIZE(s5e8825_banks_core2),  s5e8825_banks_core2 },
	{ 0x11780000, ARRAY_SIZE(s5e8825_banks_aud),    s5e8825_banks_aud },
};

#define S5E8825_NR_CTRLS	ARRAY_SIZE(s5e8825_pin_ctrls)

#endif /* __PINCTRL_S5E8825_H */
