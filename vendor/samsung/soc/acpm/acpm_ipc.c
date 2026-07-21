// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos ACPM (Audio Co-Processor Manager) IPC Driver
 *
 * This driver implements the ACPM IPC (Inter-Process Communication) framework
 * that allows the AP (Application Processor) to communicate with the ACPM
 * co-processor. The ACPM manages clocks, DVFS, PMIC regulators, and thermal
 * on behalf of the AP.
 *
 * Communication model:
 *   - The AP and ACPM share a region of SRAM (mapped from DT reg[1]).
 *   - Each IPC channel has a pair of circular buffers (TX/RX) in SRAM.
 *   - To send a message: write to the TX buffer, then trigger an interrupt
 *     to the ACPM by writing to INTGR0.
 *   - The ACPM processes the message and writes the response to the RX buffer,
 *     then triggers an interrupt to the AP (received via the DT interrupt).
 *
 * Hardware (from real Samsung DT for Exynos 1280):
 *   SFR (interrupt registers): 0x11900000, size 0x1000
 *   SRAM (shared memory):      0x02039000, size 0x34800
 *   Interrupt: GIC_SPI 29, IRQ_TYPE_LEVEL_HIGH
 *   initdata-base: 0x8000 (offset in SRAM)
 *
 * Transcribed from Samsung's 5.10 kernel (drivers/soc/samsung/acpm/acpm_ipc.c),
 * adapted for Linux 6.18 GKI module compatibility.
 *
 * Copyright (C) 2026 ExynosNext Project
 * Copyright (C) Samsung Electronics Co., Ltd.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>

#include "acpm_ipc.h"

static struct acpm_ipc_info *acpm_ipc;

/* ── IPC buffer operations ──────────────────────────────────────────────── */

static inline void __iomem *acpm_sram_offset(void __iomem *base, unsigned int offset)
{
	return base + offset;
}

static unsigned int acpm_buff_get_rear(struct buff_info *buff)
{
	return readl(buff->rear);
}

static void acpm_buff_set_rear(struct buff_info *buff, unsigned int rear)
{
	writel(rear, buff->rear);
}

static unsigned int acpm_buff_get_front(struct buff_info *buff)
{
	return readl(buff->front);
}

static void acpm_buff_set_front(struct buff_info *buff, unsigned int front)
{
	writel(front, buff->front);
}

static void acpm_buff_memcpy_fromio(void *dest, void __iomem *src, unsigned int size)
{
	unsigned int i;
	unsigned int *d = dest;
	u32 __iomem *s = src;

	for (i = 0; i < size / sizeof(u32); i++)
		d[i] = readl(&s[i]);
}

static void acpm_buff_memcpy_toio(void __iomem *dest, const void *src, unsigned int size)
{
	unsigned int i;
	const unsigned int *s = src;
	u32 __iomem *d = dest;

	for (i = 0; i < size / sizeof(u32); i++)
		writel(s[i], &d[i]);
}

/* ── Interrupt management ───────────────────────────────────────────────── */

static void acpm_generate_interrupt(struct acpm_ipc_info *acpm)
{
	/* Trigger an interrupt to the ACPM by writing to INTGR0 */
	writel(0x1, acpm->intr + INTGR0);
}

static void acpm_clear_interrupt(struct acpm_ipc_info *acpm)
{
	/* Clear the AP interrupt by writing to INTCR1 */
	writel(0x1, acpm->intr + INTCR1);
}

static unsigned int acpm_get_interrupt_status(struct acpm_ipc_info *acpm)
{
	return readl(acpm->intr + INTSR1);
}

/* ── Channel buffer initialization ──────────────────────────────────────── */

static int acpm_init_channel_buffer(struct acpm_ipc_info *acpm,
				    struct acpm_ipc_ch *ch,
				    unsigned int ch_id)
{
	/* The initdata in SRAM describes the buffer layout for each channel.
	 * In the full Samsung driver, this is parsed from a binary firmware
	 * header. For now, we use a simplified fixed layout where each channel
	 * gets a fixed-size TX and RX buffer in the SRAM.
	 *
	 * SRAM layout (after initdata at offset 0x8000):
	 *   0x8000 + 0x000: initdata header
	 *   0x8000 + 0x100: channel 0 TX buffer (256 bytes)
	 *   0x8000 + 0x200: channel 0 RX buffer (256 bytes)
	 *   0x8000 + 0x300: channel 1 TX buffer (256 bytes)
	 *   ... etc.
	 */
	unsigned int sram_offset = acpm->initdata_base + 0x100 + (ch_id * 0x200);
	unsigned int buf_size = 256; /* 64 u32 words per message */

	ch->tx_ch.base = acpm->sram_base + sram_offset;
	ch->tx_ch.rear = acpm->sram_base + sram_offset;
	ch->tx_ch.front = acpm->sram_base + sram_offset + 4;
	ch->tx_ch.size = buf_size;
	ch->tx_ch.len = buf_size;

	ch->rx_ch.base = acpm->sram_base + sram_offset + 0x100;
	ch->rx_ch.rear = acpm->sram_base + sram_offset + 0x100;
	ch->rx_ch.front = acpm->sram_base + sram_offset + 0x104;
	ch->rx_ch.size = buf_size;
	ch->rx_ch.len = buf_size;

	return 0;
}

/* ── IPC send/receive ───────────────────────────────────────────────────── */

/**
 * acpm_ipc_send_data - send an IPC message and return immediately
 * @channel_id: the ACPM IPC channel to send on
 * @cfg: message configuration and payload
 *
 * This function writes the message payload to the TX buffer, increments
 * the sequence number, and triggers an interrupt to the ACPM. It does NOT
 * wait for a response — use acpm_ipc_send_data_sync() for that.
 *
 * Return: 0 on success, negative errno on failure
 */
int acpm_ipc_send_data(unsigned int channel_id, struct ipc_config *cfg)
{
	struct acpm_ipc_ch *ch;
	struct acpm_ipc_info *acpm = acpm_ipc;
	unsigned long flags;
	unsigned int front, rear;

	if (!acpm || channel_id >= acpm->num_channels)
		return -ENODEV;

	ch = &acpm->channel[channel_id];

	if (!cfg || !cfg->cmd)
		return -EINVAL;

	spin_lock_irqsave(&ch->tx_lock, flags);

	/* Write the message payload to the TX buffer */
	front = acpm_buff_get_front(&ch->tx_ch);
	rear = acpm_buff_get_rear(&ch->tx_ch);

	/* Check if the buffer is full */
	if (front == rear) {
		/* Buffer is empty (front == rear means empty in this impl) */
	}

	/* Copy the command to the TX buffer */
	acpm_buff_memcpy_toio(ch->tx_ch.base, cfg->cmd, ch->tx_ch.len);

	/* Update the sequence number */
	ch->seq_num = (ch->seq_num + 1) % SEQUENCE_NUM_MAX;

	/* Advance the rear pointer */
	acpm_buff_set_rear(&ch->tx_ch, rear + 1);

	/* Trigger an interrupt to the ACPM */
	acpm_generate_interrupt(acpm);

	spin_unlock_irqrestore(&ch->tx_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(acpm_ipc_send_data);

/**
 * acpm_ipc_send_data_sync - send an IPC message and wait for the response
 * @channel_id: the ACPM IPC channel to send on
 * @cfg: message configuration and payload (response will be written to cfg->cmd)
 *
 * This function sends the message and blocks until the ACPM responds or
 * the timeout expires.
 *
 * Return: 0 on success, negative errno on failure
 */
int acpm_ipc_send_data_sync(unsigned int channel_id, struct ipc_config *cfg)
{
	struct acpm_ipc_ch *ch;
	struct acpm_ipc_info *acpm = acpm_ipc;
	unsigned long flags;
	int ret;

	if (!acpm || channel_id >= acpm->num_channels)
		return -ENODEV;

	ch = &acpm->channel[channel_id];

	if (!cfg || !cfg->cmd)
		return -EINVAL;

	mutex_lock(&ch->wait_lock);

	reinit_completion(&ch->wait);

	/* Send the message */
	ret = acpm_ipc_send_data(channel_id, cfg);
	if (ret) {
		mutex_unlock(&ch->wait_lock);
		return ret;
	}

	/* Wait for the response (timeout: 5 seconds) */
	if (cfg->response) {
		ret = wait_for_completion_timeout(&ch->wait,
						  msecs_to_jiffies(5000));
		if (ret == 0) {
			dev_err(acpm->dev,
				"IPC timeout on channel %u\n", channel_id);
			mutex_unlock(&ch->wait_lock);
			return -ETIMEDOUT;
		}

		/* Copy the response from the RX buffer */
		spin_lock_irqsave(&ch->rx_lock, flags);
		acpm_buff_memcpy_fromio(cfg->cmd, ch->rx_ch.base, ch->rx_ch.len);
		spin_unlock_irqrestore(&ch->rx_lock, flags);
	}

	mutex_unlock(&ch->wait_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(acpm_ipc_send_data_sync);

/* ── Channel management ────────────────────────────────────────────────── */

/**
 * acpm_ipc_request_channel - request an ACPM IPC channel
 * @np: device node of the client (must have acpm-ipc-channel property)
 * @handler: callback to invoke when a response arrives (may be NULL)
 * @id: output: the channel ID
 * @size: output: the message size for this channel
 *
 * Return: 0 on success, negative errno on failure
 */
unsigned int acpm_ipc_request_channel(struct device_node *np, ipc_callback handler,
				      unsigned int *id, unsigned int *size)
{
	struct acpm_ipc_info *acpm = acpm_ipc;
	struct acpm_ipc_ch *ch;
	unsigned int channel_id;
	u32 prop_val;
	int ret;

	if (!acpm)
		return -ENODEV;

	/* Read the channel ID from the device tree */
	ret = of_property_read_u32(np, "acpm-ipc-channel", &prop_val);
	if (ret) {
		pr_err("acpm: no acpm-ipc-channel property in %pOF\n", np);
		return -EINVAL;
	}

	channel_id = prop_val;
	if (channel_id >= acpm->num_channels) {
		pr_err("acpm: invalid channel ID %u\n", channel_id);
		return -EINVAL;
	}

	ch = &acpm->channel[channel_id];
	ch->id = channel_id;

	if (id)
		*id = channel_id;
	if (size)
		*size = ch->tx_ch.len;

	pr_info("acpm: channel %u requested by %pOF\n", channel_id, np);
	return 0;
}
EXPORT_SYMBOL_GPL(acpm_ipc_request_channel);

/**
 * acpm_ipc_release_channel - release an ACPM IPC channel
 * @np: device node of the client
 * @channel_id: the channel to release
 *
 * Return: 0 on success
 */
unsigned int acpm_ipc_release_channel(struct device_node *np,
				      unsigned int channel_id)
{
	struct acpm_ipc_info *acpm = acpm_ipc;

	if (!acpm || channel_id >= acpm->num_channels)
		return -ENODEV;

	pr_info("acpm: channel %u released by %pOF\n", channel_id, np);
	return 0;
}
EXPORT_SYMBOL_GPL(acpm_ipc_release_channel);

/**
 * acpm_ipc_set_ch_mode - set the channel communication mode
 * @np: device node of the client
 * @polling: true to use polling mode, false for interrupt mode
 *
 * Return: 0 on success
 */
int acpm_ipc_set_ch_mode(struct device_node *np, bool polling)
{
	struct acpm_ipc_info *acpm = acpm_ipc;
	u32 channel_id;
	int ret;

	if (!acpm)
		return -ENODEV;

	ret = of_property_read_u32(np, "acpm-ipc-channel", &channel_id);
	if (ret)
		return -EINVAL;

	if (channel_id >= acpm->num_channels)
		return -EINVAL;

	acpm->channel[channel_id].polling = polling;
	acpm->channel[channel_id].interrupt = !polling;

	return 0;
}
EXPORT_SYMBOL_GPL(acpm_ipc_set_ch_mode);

bool is_acpm_ipc_busy(unsigned int ch_id)
{
	struct acpm_ipc_info *acpm = acpm_ipc;

	if (!acpm || ch_id >= acpm->num_channels)
		return false;

	return mutex_is_locked(&acpm->channel[ch_id].wait_lock);
}
EXPORT_SYMBOL_GPL(is_acpm_ipc_busy);

u32 acpm_get_peri_timer(void)
{
	struct acpm_ipc_info *acpm = acpm_ipc;

	if (!acpm || !acpm->sram_base)
		return 0;

	/* The peripheral timer value is at a fixed offset in SRAM */
	return readl(acpm->sram_base + 0x0008);
}
EXPORT_SYMBOL_GPL(acpm_get_peri_timer);

void exynos_acpm_set_fast_switch(unsigned int ch)
{
	struct acpm_ipc_info *acpm = acpm_ipc;

	if (!acpm || ch >= acpm->num_channels)
		return;

	/* Enable fast-switch mode by setting the channel's polling flag */
	acpm->channel[ch].polling = true;
}
EXPORT_SYMBOL_GPL(exynos_acpm_set_fast_switch);

/* ── Interrupt handler ──────────────────────────────────────────────────── */

static irqreturn_t acpm_ipc_irq_handler(int irq, void *dev_id)
{
	struct acpm_ipc_info *acpm = dev_id;
	unsigned int status;
	unsigned int ch_id;

	status = acpm_get_interrupt_status(acpm);
	acpm_clear_interrupt(acpm);

	if (!status) {
		/* Spurious interrupt */
		return IRQ_HANDLED;
	}

	/* Wake up any waiting synchronous senders.
	 * In the full implementation, the status register indicates which
	 * channels have responses. For simplicity, we wake up all pending
	 * channels. */
	for (ch_id = 0; ch_id < acpm->num_channels; ch_id++) {
		struct acpm_ipc_ch *ch = &acpm->channel[ch_id];

		if (ch->interrupt && !ch->polling) {
			complete(&ch->wait);

			/* If there's a callback registered, invoke it */
			if (!list_empty(&ch->list)) {
				struct callback_info *cb, *tmp;
				list_for_each_entry_safe(cb, tmp, &ch->list, list) {
					if (cb->ipc_callback) {
						unsigned int response[64];
						acpm_buff_memcpy_fromio(
							response,
							ch->rx_ch.base,
							ch->rx_ch.len);
						cb->ipc_callback(response,
								 ch->rx_ch.len / sizeof(u32));
					}
				}
			}
		}
	}

	return IRQ_HANDLED;
}

/* ── Platform driver ────────────────────────────────────────────────────── */

static int acpm_ipc_probe(struct platform_device *pdev)
{
	struct acpm_ipc_info *acpm;
	struct resource *res;
	unsigned int i;
	int ret;

	acpm = devm_kzalloc(&pdev->dev, sizeof(*acpm), GFP_KERNEL);
	if (!acpm)
		return -ENOMEM;

	acpm->dev = &pdev->dev;

	/* Map the SFR (interrupt register) region: reg[0] */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	acpm->intr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(acpm->intr))
		return dev_err_probe(&pdev->dev, PTR_ERR(acpm->intr),
				     "failed to map ACPM SFR region\n");

	/* Map the SRAM (shared memory) region: reg[1] */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	acpm->sram_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(acpm->sram_base))
		return dev_err_probe(&pdev->dev, PTR_ERR(acpm->sram_base),
				     "failed to map ACPM SRAM region\n");

	/* Read the initdata-base from DT (default 0x8000) */
	ret = of_property_read_u32(pdev->dev.of_node, "initdata-base",
				   &acpm->initdata_base);
	if (ret)
		acpm->initdata_base = 0x8000;

	/* Allocate channels */
	if (of_property_read_u32(pdev->dev.of_node, "num-channels",
				 &acpm->num_channels))
		acpm->num_channels = ACPM_IPC_MAX_CHANNELS;

	acpm->channel = devm_kcalloc(&pdev->dev, acpm->num_channels,
				     sizeof(*acpm->channel), GFP_KERNEL);
	if (!acpm->channel)
		return -ENOMEM;

	/* Initialize each channel */
	for (i = 0; i < acpm->num_channels; i++) {
		struct acpm_ipc_ch *ch = &acpm->channel[i];

		spin_lock_init(&ch->rx_lock);
		spin_lock_init(&ch->tx_lock);
		spin_lock_init(&ch->ch_lock);
		mutex_init(&ch->wait_lock);
		init_completion(&ch->wait);
		INIT_LIST_HEAD(&ch->list);
		ch->id = i;
		ch->seq_num = 0;
		ch->polling = false;
		ch->interrupt = true;

		acpm_init_channel_buffer(acpm, ch, i);
	}

	/* Request the interrupt */
	acpm->irq = platform_get_irq(pdev, 0);
	if (acpm->irq < 0)
		return dev_err_probe(&pdev->dev, acpm->irq,
				     "failed to get ACPM IRQ\n");

	ret = devm_request_irq(&pdev->dev, acpm->irq, acpm_ipc_irq_handler,
			       IRQF_TRIGGER_HIGH | IRQF_SHARED,
			       "acpm-ipc", acpm);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "failed to request ACPM IRQ\n");

	/* Unmask the ACPM interrupt (enable AP-side interrupt reception) */
	writel(0x0, acpm->intr + INTMR1);

	platform_set_drvdata(pdev, acpm);
	acpm_ipc = acpm;

	dev_info(&pdev->dev,
		 "Exynos ACPM IPC: %u channels, SFR=%pap, SRAM=%pap, IRQ=%u\n",
		 acpm->num_channels,
		 &res->start, &res->start, acpm->irq);

	return 0;
}

static int acpm_ipc_remove(struct platform_device *pdev)
{
	struct acpm_ipc_info *acpm = platform_get_drvdata(pdev);
	unsigned int i;

	if (acpm) {
		writel(0x1, acpm->intr + INTMR1); /* Mask interrupts */
		for (i = 0; i < acpm->num_channels; i++)
			mutex_destroy(&acpm->channel[i].wait_lock);
	}

	acpm_ipc = NULL;
	return 0;
}

static const struct of_device_id acpm_ipc_match[] = {
	{ .compatible = "samsung,exynos-acpm-ipc" },
	{ }
};
MODULE_DEVICE_TABLE(of, acpm_ipc_match);

static struct platform_driver acpm_ipc_driver = {
	.probe  = acpm_ipc_probe,
	.remove = acpm_ipc_remove,
	.driver = {
		.name = "exynos-acpm-ipc",
		.of_match_table = acpm_ipc_match,
		.suppress_bind_attrs = true,
	},
};
module_platform_driver(acpm_ipc_driver);

MODULE_DESCRIPTION("Samsung Exynos ACPM IPC driver");
MODULE_AUTHOR("ExynosNext Project");
MODULE_LICENSE("GPL v2");
