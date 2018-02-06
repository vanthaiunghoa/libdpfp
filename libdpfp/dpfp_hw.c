/*
 * Functions for device control
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
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "dpfp.h"
#include "dpfp_private.h"

int dpfp_set_mode(struct dpfp_dev *dev, unsigned char mode)
{
	/* FIXME: only allow known modes */

	dbgf(DBG_INFO, "%x", mode);
	return usb_control_msg(dev->handle, USB_OUT, USB_RQ, MODE_CONTROL, 0,
		(char *) &mode, 1, CTRL_TIMEOUT);
}

int dpfp_capture_fprint(struct dpfp_dev *dev, struct dpfp_fprint *fp)
{
	int trf1, trf2;

	/* FIXME 5000 */

	trf1 = usb_bulk_read(dev->handle, EP_DATA, fp->header,
		DATABLK1_RQSIZE, 5000);
	if (trf1 < 0) {
		dbg(DBG_ERR, "first read failed");
		return trf1;
	}

	trf2 = usb_bulk_read(dev->handle, EP_DATA, fp->header + trf1,
		DATABLK2_RQSIZE, 5000);
	if (trf2 < 0) {
		dbg(DBG_ERR, "second read failed");
		return trf2;
	}

	fp->header_size = 64;
	fp->data_size = trf1 + trf2 - 64;

	return 0;
}

/* Timeout is in seconds. 0 means infinite timeout. */
int dpfp_get_irq(struct dpfp_dev *dev, unsigned char *buf, int timeout)
{
	uint16_t type;
	int r;
	int infinite_timeout = 0;

	if (timeout == 0)
		infinite_timeout = 1;

	/* Darwin and Linux behave inconsistently with regard to infinite timeouts.
	 * Linux accepts a timeout value of 0 as infinite timeout, whereas darwin
	 * returns -ETIMEDOUT immediately when a 0 timeout is used. We use a
	 * looping hack until libusb is fixed.
	 * See http://thread.gmane.org/gmane.comp.lib.libusb.devel.general/1315 */

retry:
	r = usb_interrupt_read(dev->handle, EP_INTR, buf, DPFP_IRQ_LENGTH, 1000);
	if (r == -ETIMEDOUT &&
			((!infinite_timeout && timeout > 0) || infinite_timeout)) {
		dbg(DBG_INFO, "timeout, retry");
		timeout--;
		goto retry;
	}
	
	if (r < 0) {
		return r;
	} else if (r < DPFP_IRQ_LENGTH) {
		dbgf(DBG_ERR, "received %d byte IRQ!?", r);
		return -1;
	}

	type = be16_to_cpu(*((uint16_t *) buf));
	dbgf(DBG_INFO, "irq type %04x", type);

	return 0;
}

int dpfp_get_hwstat(struct dpfp_dev *dev, unsigned char *data)
{
	int r;

	/* The windows driver uses a request of 0x0c here. We use 0x04 to be
	 * consistent with every other command we know about. */
	r = usb_control_msg(dev->handle, USB_IN, USB_RQ, HWSTAT_CONTROL, 0,
		data, 1, CTRL_TIMEOUT);
	dbgf(DBG_INFO, "[%d] %x", r, *data);
	return r;
}

int dpfp_set_hwstat(struct dpfp_dev *dev, unsigned char val)
{
	dbgf(DBG_INFO, "set val %x", val);
	return usb_control_msg(dev->handle, USB_OUT, USB_RQ, HWSTAT_CONTROL, 0,
		&val, 1, CTRL_TIMEOUT);
}

#if 0
/*!
 * Control the brightness of the edge light.
 *
 * The edge light is a dim light found around the edge of the scanner, in oval
 * shape. It is purely for decoration.
 *
 * The <i>brightness</i> parameter controls the brightness. 0 means off, 1
 * means very dim, 255 means bright.
 *
 * In blocking mode, this function will block momentarily. When the function
 * returns, the edge light will have changed brightness to the requested value.
 *
 * In nonblocking mode, this function will return immediately. You will receive
 * a response shortly after, confirming the change has succeeded.
 */
int dpfp_set_edge_light(struct dpfp_dev *dev, int brightness)
{
	if (brightness < 0 || brightness > 255) {
		errno = EINVAL;
		return -1;
	}

	return dpfp_write_rq_short(dev, DPFP_RQ_SET_EDGE_LIGHT, brightness);
}
#endif

/* Encryption challenge? */
int dpfp_challenge(struct dpfp_dev *dev, unsigned char *param)
{
	dbgf(DBG_INFO, "%x %x %x %x %x",
		param[0], param[1], param[2], param[3], param[4]);
	return usb_control_msg(dev->handle, USB_OUT, USB_RQ, CHALLENGE_CONTROL, 0,
		param, DPFP_CHALLENGE_LENGTH, CTRL_TIMEOUT);
}

/* Encryption response? */
int dpfp_read_response(struct dpfp_dev *dev, unsigned char *buf)
{
	int r;

	r = usb_control_msg(dev->handle, USB_IN, USB_RQ, RESPONSE_CONTROL, 0,
		buf, DPFP_RESPONSE_LENGTH, CTRL_TIMEOUT);
	dbgf(DBG_INFO, "%x %x %x %x", buf[0], buf[1], buf[2], buf[3]);
	return r;
}

int dpfp_auth_read_challenge(struct dpfp_dev *dev, unsigned char *data)
{
	dbgf(DBG_INFO, "read auth challenge");
	return usb_control_msg(dev->handle, USB_IN, USB_RQ, AUTH_CHALLENGE, 0,
		data, DPFP_AUTH_CR_LENGTH, CTRL_TIMEOUT);
}

int dpfp_auth_write_response(struct dpfp_dev *dev, unsigned char *data)
{
	dbgf(DBG_INFO, "write auth response");
	return usb_control_msg(dev->handle, USB_OUT, USB_RQ, AUTH_RESPONSE, 0,
		data, DPFP_AUTH_CR_LENGTH, CTRL_TIMEOUT);
}

/* TODO 1C R+W */

