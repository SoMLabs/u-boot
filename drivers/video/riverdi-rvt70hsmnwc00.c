// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 SoMLabs - All Rights Reserved
 *
 */
#include <common.h>
#include <backlight.h>
#include <dm.h>
#include <mipi_dsi.h>
#include <panel.h>
#include <asm/gpio.h>
#include <dm/device_compat.h>
#include <linux/delay.h>


struct rvt70hsmnwc00_panel_priv {
	struct udevice *backlight;
	struct gpio_desc reset;
};

static const struct display_timing default_timing = {
	.pixelclock.typ		= 52000000,
	.hactive.typ		= 1024,
	.hfront_porch.typ	= 160,
	.hback_porch.typ	= 160,
	.hsync_len.typ		= 1,
	.vactive.typ		= 600,
	.vfront_porch.typ	= 12,
	.vback_porch.typ	= 23,
	.vsync_len.typ		= 1,
};

static void rvt70hsmnwc00_dcs_write_cmd(struct udevice *dev, u8 cmd)
{
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct mipi_dsi_device *device = plat->device;
	int err;

        err = mipi_dsi_dcs_write_buffer(device, &cmd, 1);
        if (err < 0)
                dev_err(dev, "write failed: %d\n", err);
}

static void rvt70hsmnwc00_dcs_write_cmd_with_param(struct udevice *dev, u8 cmd, u8 value)
{
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct mipi_dsi_device *device = plat->device;
        int err;
        u8 buf[2] = {0};

        buf[0] = cmd;
        buf[1] = value;
        err = mipi_dsi_dcs_write_buffer(device, buf, 2);
        if (err < 0)
                dev_err(dev, "write failed: %d\n", err);
}


static void rvt70hsmnwc00_init_sequence(struct udevice *dev)
{
	rvt70hsmnwc00_dcs_write_cmd(dev, 0x01);
	mdelay(120);

	rvt70hsmnwc00_dcs_write_cmd_with_param(dev, 0xB2, 0x50);
	rvt70hsmnwc00_dcs_write_cmd_with_param(dev, 0x80, 0x4B);
	rvt70hsmnwc00_dcs_write_cmd_with_param(dev, 0x81, 0xFF);
	rvt70hsmnwc00_dcs_write_cmd_with_param(dev, 0x82, 0x1A);
	rvt70hsmnwc00_dcs_write_cmd_with_param(dev, 0x83, 0x88);
	rvt70hsmnwc00_dcs_write_cmd_with_param(dev, 0x84, 0x8F);
	rvt70hsmnwc00_dcs_write_cmd_with_param(dev, 0x85, 0x35);
	rvt70hsmnwc00_dcs_write_cmd_with_param(dev, 0x86, 0xB0);
}

static int rvt70hsmnwc00_panel_enable_backlight(struct udevice *dev)
{
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct mipi_dsi_device *device = plat->device;
	struct rvt70hsmnwc00_panel_priv *priv = dev_get_priv(dev);
	int ret;

	ret = mipi_dsi_attach(device);
	if (ret < 0)
		return ret;

	rvt70hsmnwc00_init_sequence(dev);

	ret = mipi_dsi_dcs_exit_sleep_mode(device);
	if (ret)
		return ret;

	mdelay(125);

	ret = mipi_dsi_dcs_set_display_on(device);
	if (ret)
		return ret;

	mdelay(20);

	ret = backlight_enable(priv->backlight);
	if (ret)
		return ret;

	return 0;
}

static int rvt70hsmnwc00_panel_get_display_timing(struct udevice *dev,
					    struct display_timing *timings)
{
	memcpy(timings, &default_timing, sizeof(*timings));

	return 0;
}

static int rvt70hsmnwc00_panel_ofdata_to_platdata(struct udevice *dev)
{
	struct rvt70hsmnwc00_panel_priv *priv = dev_get_priv(dev);
	int ret;

	ret = gpio_request_by_name(dev, "reset-gpios", 0, &priv->reset,
				   GPIOD_IS_OUT);
	if (ret) {
		dev_err(dev, "Warning: cannot get reset GPIO\n");
		if (ret != -ENOENT)
			return ret;
	}

	ret = uclass_get_device_by_phandle(UCLASS_PANEL_BACKLIGHT, dev,
					   "backlight", &priv->backlight);
	if (ret) {
		dev_err(dev, "Cannot get backlight: ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static int rvt70hsmnwc00_panel_probe(struct udevice *dev)
{
	struct rvt70hsmnwc00_panel_priv *priv = dev_get_priv(dev);
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	int ret;

	/* reset panel */
	dm_gpio_set_value(&priv->reset, true);
	mdelay(1);
	dm_gpio_set_value(&priv->reset, false);
	mdelay(10);

	/* fill characteristics of DSI data link */
	plat->lanes = 2;
	plat->format = MIPI_DSI_FMT_RGB888;
	plat->mode_flags = MIPI_DSI_MODE_VIDEO |
                           MIPI_DSI_MODE_VIDEO_BURST |
                           MIPI_DSI_MODE_LPM;

	return 0;
}

static const struct panel_ops rvt70hsmnwc00_panel_ops = {
	.enable_backlight = rvt70hsmnwc00_panel_enable_backlight,
	.get_display_timing = rvt70hsmnwc00_panel_get_display_timing,
};

static const struct udevice_id rvt70hsmnwc00_panel_ids[] = {
	{ .compatible = "riverdi,rvt70hsmnwc00" },
	{ }
};

U_BOOT_DRIVER(rvt70hsmnwc00_panel) = {
	.name	    = "rvt70hsmnwc00_panel",
	.id	    = UCLASS_PANEL,
	.of_match   = rvt70hsmnwc00_panel_ids,
	.ops	    = &rvt70hsmnwc00_panel_ops,
	.of_to_plat = rvt70hsmnwc00_panel_ofdata_to_platdata,
	.probe	    = rvt70hsmnwc00_panel_probe,
	.plat_auto  = sizeof(struct mipi_dsi_panel_plat),
	.priv_auto  = sizeof(struct rvt70hsmnwc00_panel_priv),
};
