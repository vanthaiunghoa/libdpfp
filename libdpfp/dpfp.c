/*
 * Core library functions
 * 
 *    Copyright (C) 2006 Daniel Drake <dsd@gentoo.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <openssl/aes.h>

#include "dpfp.h"
#include "dpfp_private.h"

AES_KEY aeskey;

const struct dpfp_dev_type dev_type_tbl[] = {
	[DEV_TYPE_URU4000] = {
		.firmware_start = 0x400,
		.fw_enc_offset = 0x3f7 },
	[DEV_TYPE_URU4000B] = {
		.firmware_start = 0x100,
		.fw_enc_offset = 0x42b },
	[DEV_TYPE_URU4000Bg2] = {
		.firmware_start = 0x100,
		.fw_enc_offset = 0x52e },
};

static const struct dpfp_dev_entry device_tbl[] = {
	{
		0x045e, 0x00bb,
		DEV_TYPE_URU4000B, DEV_HAS_EDGE_LIGHT,
		"Microsoft Keyboard with Fingerprint reader"
	},
	{
		0x045e, 0x00bc,
		DEV_TYPE_URU4000B, DEV_HAS_EDGE_LIGHT,
		"Microsoft Wireless IntelliMouse with Fingerprint reader"
	},
	{
		0x045e, 0x00bd,
		DEV_TYPE_URU4000B, DEV_HAS_EDGE_LIGHT,
		"Microsoft Fingerprint reader (standalone)"
	},
	{
		0x045e, 0x00ca,
		DEV_TYPE_URU4000Bg2, DEV_HAS_EDGE_LIGHT,
		"Microsoft Fingerprint reader v2 (standalone)"
	},
	{
		0x05ba, 0x0007,
		DEV_TYPE_URU4000, 0,
		"Digital Persona U.are.U 4000"
	},
	{
		0x05ba, 0x000a,
		DEV_TYPE_URU4000B, 0,
		"Digital Persona U.are.U 4000B"
	},
};

static const struct dpfp_dev_entry *get_dev_entry(struct usb_device *udev)
{
	int i;

	for (i = 0; i < (sizeof(device_tbl) / sizeof(*device_tbl)); i++) {
		const struct dpfp_dev_entry *deventry = &device_tbl[i];
		if (deventry->vid == udev->descriptor.idVendor
				&& deventry->pid == udev->descriptor.idProduct)
			return deventry;
	}

	return NULL;
}

#if 0
/* FIXME err check */
static int dpfp_upload_firmware(struct dpfp_dev *dev)
{
	int offset, size, len;
	unsigned char buf[256];
	FILE *fp;

	dbg(DBG_INFO, "uploading firmware");

	fp = fopen("firmware", "r");
	if (fp == NULL) {
		dbg(DBG_ERR, "could not find firmware file");
		return -1;
	}

	offset = dev_type_tbl[dev->dev_entry->type].firmware_start;
	len = fseek(fp, 0, SEEK_END);
	rewind(fp);

	while (len > 0) {
		size = 256;
		if (len < size)
			size = len;

		fread(buf, 256, 1, fp);

		usb_control_msg(dev->handle, 0x40, 0x04, offset, 0,
			buf, size, CTRL_TIMEOUT);

		len -= size;
		offset += size;
	}

	fclose(fp);

	dbg(DBG_INFO, "firmware upload done");

	return 0;
}
#endif

/* This is a prototype function to try and disable encryption without even
 * having to upload any firmware. However I can't enable encryption on my
 * devices at the moment, maybe this will be useful in the future...
 */
int fix_firmware(struct dpfp_dev *dev)
{
	const struct dpfp_dev_type *devtype =
		&dev_type_tbl[dev->dev_entry->type];
	uint32_t enc_addr = devtype->firmware_start + devtype->fw_enc_offset;
	unsigned char val, new;
	int r;

	r = usb_control_msg(dev->handle, 0xc0, 0x0c, enc_addr, 0, &val, 1,
		CTRL_TIMEOUT);
	if (r < 0)
		return r;
	
	dbgf(DBG_INFO, "encryption byte at %x reads %02x",
		devtype->fw_enc_offset, val);

	new = val & 0xef;
	//new = 0x17;
	if (new == val)
		return 0;

	r = usb_control_msg(dev->handle, 0x40, 0x04, enc_addr, 0, &new, 1,
		CTRL_TIMEOUT);
	if (r < 0)
		return r;

	dbgf(DBG_INFO, "fixed encryption byte to %02x", new);
	return 1;
}

static struct dpfp_dev *dpfp_open_usb(struct usb_device *udev,
		const struct dpfp_dev_entry *deventry)
{
	int i;
	int r;
	unsigned char buf[DPFP_IRQ_LENGTH];
	unsigned char status;
	struct usb_dev_handle *handle;
	struct usb_config_descriptor *config;
	struct usb_interface *iface = NULL;
	struct usb_interface_descriptor *iface_desc;
	struct usb_endpoint_descriptor *ep;
	struct dpfp_dev *dev = NULL;

	handle = usb_open(udev);
	if (handle == NULL) {
		dbg(DBG_ERR, "usb_open returned NULL");
		return NULL;
	}

	/* Find fingerprint interface */
	config = udev->config;
	for (i = 0; i < config->bNumInterfaces; i++) {
		struct usb_interface *cur_iface = &config->interface[i];

		if (cur_iface->num_altsetting < 1)
			continue;

		iface_desc = &cur_iface->altsetting[0];
		if (iface_desc->bInterfaceClass == 255
				&& iface_desc->bInterfaceSubClass == 255 
				&& iface_desc->bInterfaceProtocol == 255) {
			iface = cur_iface;
			break;
		}
	}

	if (iface == NULL) {
		dbg(DBG_ERR, "could not find interface");
		goto err;
	}

	/* Find/check endpoints */

	if (iface_desc->bNumEndpoints != 2) {
		dbgf(DBG_ERR, "found %d endpoints!?", iface_desc->bNumEndpoints);
		goto err;
	}

	ep = &iface_desc->endpoint[0];

	if (ep->bEndpointAddress != EP_INTR
			|| (ep->bmAttributes & USB_ENDPOINT_TYPE_MASK) !=
				USB_ENDPOINT_TYPE_INTERRUPT) {
		dbg(DBG_ERR, "unrecognised interrupt endpoint");
		goto err;
	}

	ep = &iface_desc->endpoint[1];

	if (ep->bEndpointAddress != EP_DATA
			|| (ep->bmAttributes & USB_ENDPOINT_TYPE_MASK) !=
				USB_ENDPOINT_TYPE_BULK) {
		dbg(DBG_ERR, "unrecognised bulk endpoint");
		goto err;
	}

	/* Device looks like a supported reader */

	r = usb_claim_interface(handle, iface_desc->bInterfaceNumber);
	if (r < 0) {
		dbg(DBG_ERR, "interface claim failed");
		goto err;
	}

	dev = malloc(sizeof(*dev));
	if (dev == NULL)
		goto err_release;

	memset(dev, 0, sizeof(*dev));
	dev->handle = handle;
	dev->dev_entry = deventry;

	r = dpfp_get_hwstat(dev, &status);
	if (r < 0)
		goto err_release;

	/* After closing a dpfp app and setting hwstat to 0x80, my ms keyboard
	 * gets in a confused state and returns hwstat 0x85. On next app run,
	 * we don't get the 56aa interrupt. This is the best way I've found to
	 * fix it: mess around with hwstat until it starts returning more
	 * recognisable values. This doesn't happen on my other devices:
	 * uru4000, uru4000b, ms fp rdr v2 
	 * The windows driver copes with this OK, but then again it uploads
	 * firmware right after reading the 0x85 hwstat, allowing some time
	 * to pass before it attempts to tweak hwstat again... */
	if ((status & 0x84) == 0x84) {
		printf("rebooting device power...\n");
		r = dpfp_set_hwstat(dev, status & 0xf);
		if (r < 0)
			goto err_release;

		for (i = 0; i < 100; i++) {
			r = dpfp_get_hwstat(dev, &status);
			if (r < 0)
				goto err_release;
			if (status & 0x1)
				break;
			usleep(10000);
		}
		if ((status & 0x1) == 0) {
			dbg(DBG_ERR, "could not reboot device power");
			goto err_release;
		}
	}
	
	if ((status & 0x80) == 0) {
		status |= 0x80;
		r = dpfp_set_hwstat(dev, status);
		if (r < 0)
			goto err_release;
	}

	r = fix_firmware(dev);
	if (r < 0)
		goto err_release;

	/* Power up device and wait for interrupt notification */
	/* The combination of both modifying firmware *and* doing C-R auth on
	 * my ms fp v2 device causes us not to get to get the 56aa interrupt and
	 * for the hwstat write not to take effect. We loop a few times,
	 * authenticating each time, until the device wakes up. */
	for (i = 0; i < 100; i++) { /* max 1 sec */
		r = dpfp_set_hwstat(dev, status & 0xf);
		if (r < 0)
			goto err_release;

		r = dpfp_get_hwstat(dev, buf);
		if (r < 0)
			goto err_release;

		if ((buf[0] & 0x80) == 0)
			break;

		usleep(10000);

		if (dev->dev_entry->type == DEV_TYPE_URU4000Bg2) {
			r = dpfp_simple_auth_cr(dev);
			if (r < 0)
				goto err_release;
		}
	}

	if (buf[0] & 0x80) {
		dbg(DBG_ERR, "could not power up device");
		goto err_release;
	}

	r = dpfp_simple_get_irq_with_type(dev, DPFP_IRQDATA_SCANPWR_ON, buf, 5);
	if (r < 0)
		goto err_release;

	return dev;

err_release:
	usb_release_interface(handle, iface_desc->bInterfaceNumber);
err:
	if (dev)
		free(dev);

	usb_close(handle);
	return NULL;
}

struct dpfp_dev *dpfp_open_idx(int idx)
{
	struct usb_bus *bus;
	int count = -1;

	usb_find_busses();
	usb_find_devices();

	for (bus = usb_busses; bus; bus = bus->next) {
		struct usb_device *udev;

		for (udev = bus->devices; udev; udev = udev->next) {
			const struct dpfp_dev_entry *deventry;

			deventry = get_dev_entry(udev);
			if (deventry == NULL)
				continue;

			if (++count == idx)
				return dpfp_open_usb(udev, deventry);
		}
	}

	errno = ENODEV;
	return NULL;
}

struct dpfp_dev *dpfp_open()
{
	return dpfp_open_idx(0);
}

/*! Close the device previously opened with dpfp_open() */
int dpfp_close(struct dpfp_dev *dev)
{
	int r;

	dpfp_set_mode(dev, DPFP_MODE_INIT);
	dpfp_set_hwstat(dev, 0x80);

	r = usb_close(dev->handle);
	free(dev);
	return r;
}

static const unsigned char crkey[] = {
	0x79, 0xac, 0x91, 0x79, 0x5c, 0xa1, 0x47, 0x8e,
	0x98, 0xe0, 0x0f, 0x3c, 0x59, 0x8f, 0x5f, 0x4b,
};

int dpfp_init()
{
	usb_init();
	AES_set_encrypt_key(crkey, 128, &aeskey);
	return 0;
}

