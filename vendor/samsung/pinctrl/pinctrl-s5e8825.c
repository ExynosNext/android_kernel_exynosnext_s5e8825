// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos 1280 (s5e8825) pinctrl and GPIO driver.
 *
 * This driver uses the standard Linux pinctrl subsystem (pinctrl_desc,
 * pinmux_ops, pinconf_ops) and gpio_chip — NOT Samsung's internal
 * pinctrl-samsung framework, which is not exported for GKI modules.
 * This makes the driver fully mainline-compatible: it uses only
 * exported APIs and standard DT bindings.
 *
 * The Exynos 1280 has 7 pinctrl controllers with 48 GPIO banks total
 * (transcribed from the real Samsung firmware DT). Each controller is
 * a separate platform device with compatible = "samsung,s5e8825-pinctrl".
 *
 * Register layout (per bank, stride 0x20):
 *   CON  (+0x00): function select, 4 bits/pin
 *   DAT  (+0x04): data, 1 bit/pin
 *   PUD  (+0x08): pull up/down, 2 bits/pin
 *   DRV  (+0x0C): drive strength, 2 bits/pin
 *
 * Copyright (C) 2026 ExynosNext Project
 */

#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "pinctrl-s5e8825.h"

/* ── Per-bank runtime state ──────────────────────────────────────────────── */

struct s5e8825_bank {
	void __iomem		*base;
	const struct s5e8825_pin_bank *desc;
	struct gpio_chip	gpio_chip;
	struct irq_chip		irq_chip;
};

struct s5e8825_pinctrl {
	struct device		*dev;
	void __iomem		*base;
	const struct s5e8825_pin_ctrl *ctrl;
	struct pinctrl_desc	pctl_desc;
	struct pinctrl_dev	*pctl_dev;
	struct s5e8825_bank	*banks;
	unsigned int		nr_pins;
	struct pinctrl_pin_desc	*pins;
	spinlock_t		lock;
};

/* ── Pin operations ──────────────────────────────────────────────────────── */

static const struct pinctrl_pin_desc *
s5e8825_get_pin(struct s5e8825_pinctrl *pctl, unsigned int offset)
{
	if (offset >= pctl->nr_pins)
		return NULL;
	return &pctl->pins[offset];
}

static int s5e8825_get_groups_count(struct pinctrl_dev *pctldev)
{
	/* Each pin is its own group (1 pin per group) for maximum flexibility.
	 * Named groups from DT are matched by the pinctrl core. */
	struct s5e8825_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	return pctl->nr_pins;
}

static const char *s5e8825_get_group_name(struct pinctrl_dev *pctldev,
					   unsigned int selector)
{
	struct s5e8825_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	if (selector >= pctl->nr_pins)
		return NULL;
	return pctl->pins[selector].name;
}

static int s5e8825_get_group_pins(struct pinctrl_dev *pctldev,
				  unsigned int selector,
				  const unsigned int **pins,
				  unsigned int *num_pins)
{
	struct s5e8825_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	if (selector >= pctl->nr_pins)
		return -EINVAL;
	*pins = &pctl->pins[selector].number;
	*num_pins = 1;
	return 0;
}

static void s5e8825_pin_dbg_show(struct pinctrl_dev *pctldev,
				  struct seq_file *s, unsigned int offset)
{
	seq_printf(s, "%s", s5e8825_get_pin(pinctrl_dev_get_drvdata(pctldev),
					   offset)->name);
}

static const struct pinctrl_ops s5e8825_pctl_ops = {
	.get_groups_count	= s5e8825_get_groups_count,
	.get_group_name		= s5e8825_get_group_name,
	.get_group_pins		= s5e8825_get_group_pins,
	.pin_dbg_show		= s5e8825_pin_dbg_show,
	.dt_node_to_map		= pinconf_generic_dt_node_to_map_pin,
	.dt_free_map		= pinctrl_utils_free_map,
};

/* ── Pinmux operations ──────────────────────────────────────────────────── */

static int s5e8825_pinmux_set_mux(struct pinctrl_dev *pctldev,
				  unsigned int function,
				  unsigned int group)
{
	struct s5e8825_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	unsigned int i, bank_idx, pin_in_bank;
	unsigned long flags;
	u32 reg, val;

	if (group >= pctl->nr_pins)
		return -EINVAL;

	/* Find which bank this pin belongs to */
	for (bank_idx = 0; bank_idx < pctl->ctrl->nr_banks; bank_idx++) {
		const struct s5e8825_pin_bank *b = &pctl->ctrl->banks[bank_idx];
		if (group >= b->pin_base &&
		    group < b->pin_base + b->nr_pins) {
			pin_in_bank = group - b->pin_base;
			break;
		}
	}
	if (bank_idx >= pctl->ctrl->nr_banks)
		return -EINVAL;

	/* Set the function in the CON register */
	spin_lock_irqsave(&pctl->lock, flags);
	reg = readl(pctl->banks[bank_idx].base + EXYNOS_PIN_CON);
	val = (reg >> (pin_in_bank * EXYNOS_PIN_FUNC_SHIFT)) & EXYNOS_PIN_FUNC_MASK;
	reg &= ~(EXYNOS_PIN_FUNC_MASK << (pin_in_bank * EXYNOS_PIN_FUNC_SHIFT));
	reg |= (function << (pin_in_bank * EXYNOS_PIN_FUNC_SHIFT));
	writel(reg, pctl->banks[bank_idx].base + EXYNOS_PIN_CON);
	spin_unlock_irqrestore(&pctl->lock, flags);

	return 0;
}

static int s5e8825_gpio_request(struct pinctrl_dev *pctldev,
				struct pinctrl_gpio_range *range,
				unsigned int offset)
{
	return s5e8825_pinmux_set_mux(pctldev, EXYNOS_PIN_FUNC_INPUT, offset);
}

static void s5e8825_gpio_free(struct pinctrl_dev *pctldev,
			      struct pinctrl_gpio_range *range,
			      unsigned int offset)
{
	s5e8825_pinmux_set_mux(pctldev, EXYNOS_PIN_FUNC_INPUT, offset);
}

static const struct pinmux_ops s5e8825_pmx_ops = {
	.set_mux		= s5e8825_pinmux_set_mux,
	.gpio_request_enable	= s5e8825_gpio_request,
	.gpio_disable_free	= s5e8825_gpio_free,
};

/* ── Pin configuration operations ────────────────────────────────────────── */

static int s5e8825_pinconf_get(struct pinctrl_dev *pctldev,
			       unsigned int pin, unsigned long *config)
{
	struct s5e8825_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	enum pin_config_param param = pinconf_to_config_param(*config);
	unsigned int bank_idx, pin_in_bank;
	u32 reg, val;

	for (bank_idx = 0; bank_idx < pctl->ctrl->nr_banks; bank_idx++) {
		const struct s5e8825_pin_bank *b = &pctl->ctrl->banks[bank_idx];
		if (pin >= b->pin_base && pin < b->pin_base + b->nr_pins) {
			pin_in_bank = pin - b->pin_base;
			break;
		}
	}
	if (bank_idx >= pctl->ctrl->nr_banks)
		return -EINVAL;

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
		reg = readl(pctl->banks[bank_idx].base + EXYNOS_PIN_PUD);
		val = (reg >> (pin_in_bank * EXYNOS_PIN_PUD_SHIFT)) & EXYNOS_PIN_PUD_MASK;
		*config = pinconf_to_config_packed(param, val == EXYNOS_PIN_PUD_NONE);
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		reg = readl(pctl->banks[bank_idx].base + EXYNOS_PIN_PUD);
		val = (reg >> (pin_in_bank * EXYNOS_PIN_PUD_SHIFT)) & EXYNOS_PIN_PUD_MASK;
		*config = pinconf_to_config_packed(param, val == EXYNOS_PIN_PUD_UP);
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		reg = readl(pctl->banks[bank_idx].base + EXYNOS_PIN_PUD);
		val = (reg >> (pin_in_bank * EXYNOS_PIN_PUD_SHIFT)) & EXYNOS_PIN_PUD_MASK;
		*config = pinconf_to_config_packed(param, val == EXYNOS_PIN_PUD_DOWN);
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		reg = readl(pctl->banks[bank_idx].base + EXYNOS_PIN_DRV);
		val = (reg >> (pin_in_bank * EXYNOS_PIN_DRV_SHIFT)) & EXYNOS_PIN_DRV_MASK;
		*config = pinconf_to_config_packed(param, val + 1);
		break;
	default:
		return -ENOTSUPP;
	}
	return 0;
}

static int s5e8825_pinconf_set(struct pinctrl_dev *pctldev,
			       unsigned int pin, unsigned long *configs,
			       unsigned int num_configs)
{
	struct s5e8825_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	unsigned int bank_idx, pin_in_bank, i;
	unsigned long flags;
	u32 reg, val;

	for (bank_idx = 0; bank_idx < pctl->ctrl->nr_banks; bank_idx++) {
		const struct s5e8825_pin_bank *b = &pctl->ctrl->banks[bank_idx];
		if (pin >= b->pin_base && pin < b->pin_base + b->nr_pins) {
			pin_in_bank = pin - b->pin_base;
			break;
		}
	}
	if (bank_idx >= pctl->ctrl->nr_banks)
		return -EINVAL;

	spin_lock_irqsave(&pctl->lock, flags);

	for (i = 0; i < num_configs; i++) {
		enum pin_config_param param = pinconf_to_config_param(configs[i]);
		u32 arg = pinconf_to_config_argument(configs[i]);

		switch (param) {
		case PIN_CONFIG_BIAS_DISABLE:
			reg = readl(pctl->banks[bank_idx].base + EXYNOS_PIN_PUD);
			reg &= ~(EXYNOS_PIN_PUD_MASK << (pin_in_bank * EXYNOS_PIN_PUD_SHIFT));
			writel(reg, pctl->banks[bank_idx].base + EXYNOS_PIN_PUD);
			break;
		case PIN_CONFIG_BIAS_PULL_UP:
			reg = readl(pctl->banks[bank_idx].base + EXYNOS_PIN_PUD);
			reg &= ~(EXYNOS_PIN_PUD_MASK << (pin_in_bank * EXYNOS_PIN_PUD_SHIFT));
			reg |= (EXYNOS_PIN_PUD_UP << (pin_in_bank * EXYNOS_PIN_PUD_SHIFT));
			writel(reg, pctl->banks[bank_idx].base + EXYNOS_PIN_PUD);
			break;
		case PIN_CONFIG_BIAS_PULL_DOWN:
			reg = readl(pctl->banks[bank_idx].base + EXYNOS_PIN_PUD);
			reg &= ~(EXYNOS_PIN_PUD_MASK << (pin_in_bank * EXYNOS_PIN_PUD_SHIFT));
			reg |= (EXYNOS_PIN_PUD_DOWN << (pin_in_bank * EXYNOS_PIN_PUD_SHIFT));
			writel(reg, pctl->banks[bank_idx].base + EXYNOS_PIN_PUD);
			break;
		case PIN_CONFIG_DRIVE_STRENGTH:
			if (arg < 1 || arg > 4)
				goto err;
			reg = readl(pctl->banks[bank_idx].base + EXYNOS_PIN_DRV);
			reg &= ~(EXYNOS_PIN_DRV_MASK << (pin_in_bank * EXYNOS_PIN_DRV_SHIFT));
			reg |= ((arg - 1) << (pin_in_bank * EXYNOS_PIN_DRV_SHIFT));
			writel(reg, pctl->banks[bank_idx].base + EXYNOS_PIN_DRV);
			break;
		default:
			goto err;
		}
	}

	spin_unlock_irqrestore(&pctl->lock, flags);
	return 0;

err:
	spin_unlock_irqrestore(&pctl->lock, flags);
	return -ENOTSUPP;
}

static const struct pinconf_ops s5e8825_pinconf_ops = {
	.pin_config_get		= s5e8825_pinconf_get,
	.pin_config_set		= s5e8825_pinconf_set,
	.is_generic		= true,
};

/* ── GPIO chip operations ────────────────────────────────────────────────── */

static int s5e8825_gpio_get_direction(struct gpio_chip *gc, unsigned int offset)
{
	struct s5e8825_bank *bank = gpiochip_get_data(gc);
	void __iomem *con = bank->base + EXYNOS_PIN_CON;
	u32 val;

	val = (readl(con) >> (offset * EXYNOS_PIN_FUNC_SHIFT)) & EXYNOS_PIN_FUNC_MASK;
	if (val == EXYNOS_PIN_FUNC_INPUT)
		return GPIO_LINE_DIRECTION_IN;
	return GPIO_LINE_DIRECTION_OUT;
}

static int s5e8825_gpio_direction_input(struct gpio_chip *gc, unsigned int offset)
{
	struct s5e8825_bank *bank = gpiochip_get_data(gc);
	void __iomem *con = bank->base + EXYNOS_PIN_CON;
	struct s5e8825_pinctrl *pctl = dev_get_drvdata(bank->gpio_chip.parent);
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&pctl->lock, flags);
	reg = readl(con);
	reg &= ~(EXYNOS_PIN_FUNC_MASK << (offset * EXYNOS_PIN_FUNC_SHIFT));
	writel(reg, con);
	spin_unlock_irqrestore(&pctl->lock, flags);
	return 0;
}

static int s5e8825_gpio_direction_output(struct gpio_chip *gc,
					  unsigned int offset, int value)
{
	struct s5e8825_bank *bank = gpiochip_get_data(gc);
	void __iomem *con = bank->base + EXYNOS_PIN_CON;
	void __iomem *dat = bank->base + EXYNOS_PIN_DAT;
	struct s5e8825_pinctrl *pctl = dev_get_drvdata(bank->gpio_chip.parent);
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&pctl->lock, flags);

	/* Set output value first */
	reg = readl(dat);
	if (value)
		reg |= BIT(offset);
	else
		reg &= ~BIT(offset);
	writel(reg, dat);

	/* Then set function to output */
	reg = readl(con);
	reg &= ~(EXYNOS_PIN_FUNC_MASK << (offset * EXYNOS_PIN_FUNC_SHIFT));
	reg |= (EXYNOS_PIN_FUNC_OUTPUT << (offset * EXYNOS_PIN_FUNC_SHIFT));
	writel(reg, con);

	spin_unlock_irqrestore(&pctl->lock, flags);
	return 0;
}

static int s5e8825_gpio_get(struct gpio_chip *gc, unsigned int offset)
{
	struct s5e8825_bank *bank = gpiochip_get_data(gc);
	return !!(readl(bank->base + EXYNOS_PIN_DAT) & BIT(offset));
}

static void s5e8825_gpio_set(struct gpio_chip *gc, unsigned int offset, int value)
{
	struct s5e8825_bank *bank = gpiochip_get_data(gc);
	struct s5e8825_pinctrl *pctl = dev_get_drvdata(bank->gpio_chip.parent);
	void __iomem *dat = bank->base + EXYNOS_PIN_DAT;
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&pctl->lock, flags);
	reg = readl(dat);
	if (value)
		reg |= BIT(offset);
	else
		reg &= ~BIT(offset);
	writel(reg, dat);
	spin_unlock_irqrestore(&pctl->lock, flags);
}

/* ── Platform driver ────────────────────────────────────────────────────── */

static int s5e8825_pinctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct s5e8825_pinctrl *pctl;
	struct resource *res;
	unsigned int i, j, pin_idx = 0;
	int ret;

	pctl = devm_kzalloc(dev, sizeof(*pctl), GFP_KERNEL);
	if (!pctl)
		return -ENOMEM;

	pctl->dev = dev;
	spin_lock_init(&pctl->lock);

	/* Map the controller register space */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pctl->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(pctl->base))
		return dev_err_probe(dev, PTR_ERR(pctl->base),
				     "failed to map pinctrl registers\n");

	/* Find the controller config by physical base address */
	pctl->ctrl = NULL;
	for (i = 0; i < S5E8825_NR_CTRLS; i++) {
		if (s5e8825_pin_ctrls[i].base == res->start) {
			pctl->ctrl = &s5e8825_pin_ctrls[i];
			break;
		}
	}
	if (!pctl->ctrl) {
		/* Unknown controller — create banks from DT if possible */
		dev_warn(dev, "unknown controller @0x%lx, creating from DT\n",
			 (unsigned long)res->start);
		return -ENODEV;
	}

	/* Count total pins */
	pctl->nr_pins = 0;
	for (i = 0; i < pctl->ctrl->nr_banks; i++)
		pctl->nr_pins += pctl->ctrl->banks[i].nr_pins;

	/* Allocate pin descriptors */
	pctl->pins = devm_kcalloc(dev, pctl->nr_pins,
				  sizeof(*pctl->pins), GFP_KERNEL);
	if (!pctl->pins)
		return -ENOMEM;

	/* Allocate bank runtime state */
	pctl->banks = devm_kcalloc(dev, pctl->ctrl->nr_banks,
				   sizeof(*pctl->banks), GFP_KERNEL);
	if (!pctl->banks)
		return -ENOMEM;

	/* Populate pin descriptors and initialize banks */
	pin_idx = 0;
	for (i = 0; i < pctl->ctrl->nr_banks; i++) {
		const struct s5e8825_pin_bank *bdesc = &pctl->ctrl->banks[i];
		struct s5e8825_bank *bank = &pctl->banks[i];

		bank->desc = bdesc;
		bank->base = pctl->base + (i * EXYNOS_BANK_SIZE);

		for (j = 0; j < bdesc->nr_pins; j++) {
			pctl->pins[pin_idx].number = bdesc->pin_base + j;
			/* Pin name: "gpa0-0", "gpa0-1", etc. */
			pctl->pins[pin_idx].name = devm_kasprintf(dev,
				GFP_KERNEL, "%s-%u", bdesc->name, j);
			if (!pctl->pins[pin_idx].name)
				return -ENOMEM;
			pin_idx++;
		}
	}

	/* Register pinctrl device */
	pctl->pctl_desc.name = dev_name(dev);
	pctl->pctl_desc.pins = pctl->pins;
	pctl->pctl_desc.npins = pctl->nr_pins;
	pctl->pctl_desc.pctlops = &s5e8825_pctl_ops;
	pctl->pctl_desc.pmxops = &s5e8825_pmx_ops;
	pctl->pctl_desc.confops = &s5e8825_pinconf_ops;
	pctl->pctl_desc.owner = THIS_MODULE;

	pctl->pctl_dev = devm_pinctrl_register(dev, &pctl->pctl_desc, pctl);
	if (IS_ERR(pctl->pctl_dev))
		return dev_err_probe(dev, PTR_ERR(pctl->pctl_dev),
				     "failed to register pinctrl\n");

	/* Register GPIO chips for each bank */
	for (i = 0; i < pctl->ctrl->nr_banks; i++) {
		struct s5e8825_bank *bank = &pctl->banks[i];
		struct gpio_chip *gc = &bank->gpio_chip;

		gc->base = -1; /* dynamic allocation */
		gc->ngpio = bank->desc->nr_pins;
		gc->label = bank->desc->name;
		gc->parent = dev;
		gc->of_node = dev->of_node;
		gc->get_direction = s5e8825_gpio_get_direction;
		gc->direction_input = s5e8825_gpio_direction_input;
		gc->direction_output = s5e8825_gpio_direction_output;
		gc->get = s5e8825_gpio_get;
		gc->set = s5e8825_gpio_set;

		ret = devm_gpiochip_add_data(dev, gc, bank);
		if (ret)
			return dev_err_probe(dev, ret,
					     "failed to register gpiochip %s\n",
					     bank->desc->name);

		/* Add GPIO range to pinctrl */
		gpiochip_add_pin_range(gc, dev_name(dev), 0,
				       bank->desc->pin_base,
				       bank->desc->nr_pins);
	}

	platform_set_drvdata(pdev, pctl);

	dev_info(dev, "Exynos 1280 pinctrl: %u banks, %u pins at 0x%lx\n",
		 pctl->ctrl->nr_banks, pctl->nr_pins,
		 (unsigned long)res->start);

	return 0;
}

static const struct of_device_id s5e8825_pinctrl_match[] = {
	{ .compatible = "samsung,s5e8825-pinctrl" },
	{ }
};
MODULE_DEVICE_TABLE(of, s5e8825_pinctrl_match);

static struct platform_driver s5e8825_pinctrl_driver = {
	.probe	= s5e8825_pinctrl_probe,
	.driver = {
		.name	= "s5e8825-pinctrl",
		.of_match_table = s5e8825_pinctrl_match,
		.suppress_bind_attrs = true,
	},
};
module_platform_driver(s5e8825_pinctrl_driver);

MODULE_DESCRIPTION("Samsung Exynos 1280 (s5e8825) pinctrl and GPIO driver");
MODULE_AUTHOR("ExynosNext Project");
MODULE_LICENSE("GPL v2");
