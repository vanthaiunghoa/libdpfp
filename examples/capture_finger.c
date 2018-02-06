/*
 * libdpfp example to capture a single fingerprint to a PGM image file
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

#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <libdpfp/dpfp.h>

int main(void)
{
	int result;
	struct dpfp_dev *dev;
	struct dpfp_fprint *fp = dpfp_fprint_alloc();

	dpfp_init();

	dev = dpfp_open();
	if (dev == NULL) {
		perror("dev");
		goto exit;
	}

	/* Wait for finger */
	printf("place your finger on the sensor\n");

	if (dpfp_simple_await_finger_on(dev) < 0) {
		perror("await_finger_on");
		goto exit;
	}

	/* Capture fingerprint */

	if (dpfp_set_mode(dev, DPFP_MODE_SEND_FINGER) < 0) {
		perror("set_mode");
		goto exit;
	}

	if (dpfp_capture_fprint(dev, fp) < 0) {
		perror("simple_get_fingerprint");
		goto exit;
	}

	if (dpfp_fprint_write_to_file(fp, "finger.pgm") < 0) {
		perror("write_fingerprint_to_file");
		goto exit;
	}

exit:
	dpfp_fprint_free(fp);
	dpfp_close(dev);
}

