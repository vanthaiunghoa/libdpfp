/*
 * Convenience functions
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

#include <errno.h>
#include <stdint.h>

#include <openssl/aes.h>

#include "dpfp.h"
#include "dpfp_private.h"

int dpfp_simple_get_irq_with_type(struct dpfp_dev *dev, uint16_t irqtype,
	unsigned char *irqbuf, int timeout)
{
	uint16_t hdr;
	int result;
	int discarded = -1;

	/* Sometimes we get an interrupt from a previous 'session' indicating
	 * finger-on-sensor, we ignore this and wait for the real interrupt */
	do {
		discarded++;

		result = dpfp_get_irq(dev, irqbuf, timeout);
		if (result < 0) {
			dbg(DBG_ERR, "get_irq fail");
			return result;
		}
		hdr = be16_to_cpu(*((uint16_t *) irqbuf));
	} while (hdr != irqtype);

	if (discarded > 0)
		dbgf(DBG_INFO, "discarded %d interrupts", discarded);

	return 0;
}

static int set_mode_and_get_irq_with_type(struct dpfp_dev *dev,
	unsigned char mode, uint16_t irqtype, unsigned char *irqbuf)
{
	int result;

	result = dpfp_set_mode(dev, mode);
	if (result < 0) {
		dbg(DBG_ERR, "set_mode fail");
		return result;
	}

	return dpfp_simple_get_irq_with_type(dev, irqtype, irqbuf, 0);
}

int dpfp_simple_await_finger_on_irqbuf(struct dpfp_dev *dev,
		unsigned char *irqbuf)
{
	return set_mode_and_get_irq_with_type(dev, DPFP_MODE_AWAIT_FINGER_ON,
		DPFP_IRQDATA_FINGER_ON, irqbuf);
}

int dpfp_simple_await_finger_on(struct dpfp_dev *dev)
{
	unsigned char irqbuf[DPFP_IRQ_LENGTH];
	return dpfp_simple_await_finger_on_irqbuf(dev, irqbuf);
}

int dpfp_simple_await_finger_off_irqbuf(struct dpfp_dev *dev,
		unsigned char *irqbuf)
{
	return set_mode_and_get_irq_with_type(dev, DPFP_MODE_AWAIT_FINGER_OFF,
		DPFP_IRQDATA_FINGER_OFF, irqbuf);
}

int dpfp_simple_await_finger_off(struct dpfp_dev *dev)
{
	unsigned char irqbuf[DPFP_IRQ_LENGTH];
	return dpfp_simple_await_finger_off_irqbuf(dev, irqbuf);
}

int dpfp_simple_auth_cr(struct dpfp_dev *dev)
{
	int r;
	unsigned char challenge[DPFP_AUTH_CR_LENGTH];
	unsigned char response[DPFP_AUTH_CR_LENGTH];

	r = dpfp_auth_read_challenge(dev, challenge);
	if (r < 0)
		return r;
	if (r < 0x10)
		return -EIO;

	AES_encrypt(challenge, response, &aeskey);

	r = dpfp_auth_write_response(dev, response);
	if (r > 0 && r < 0x10)
		return -EIO;
	else
		return r;
}

#if 0
struct dpfp_rsp *dpfp_simple_challenge(struct dpfp_dev *dev, unsigned char p1,
		unsigned char p2, unsigned char p3, unsigned char p4,
		unsigned char p5)
{
	struct dpfp_rsp *rsp;
	int result;

	/* Challenge */
	result = dpfp_cmd33(dev, p1, p2, p3, p4, p5);
	if (result < 0)
		return NULL;

	/* Read response */
	result = dpfp_cmd34(dev);
	if (result < 0)
		return NULL;

	rsp = wait_for_rsp(dev, DPFP_RSP_CMD34);
	if (rsp == NULL)
		return NULL;

	result = dpfp_read_rsp_data(dev, rsp);
	if (result < 0)
		return NULL;

	return rsp;
}
#endif

