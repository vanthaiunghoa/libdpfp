/*
 * libdpfp example to capture and enhance 2 fingerprints, and compare them
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
#include <sys/time.h>

#include <libdpfp/dpfp.h>

#define TV_TO_DOUBLE(tv) (tv.tv_sec + (tv.tv_usec / 1000000.0))

struct dpfp_dev *dev;

int capture_fprint(struct dpfp_fprint *fp, struct dpfp_fprint *base_img)
{
	/* Capture base image. We assume the finger is away from the sensor at
	 * this point... */
	if (dpfp_set_mode(dev, DPFP_MODE_SEND_FINGER) < 0) {
		perror("set_mode");
		return 1;
	}

	if (dpfp_capture_fprint(dev, base_img) < 0) {
		perror("simple_get_fingerprint");
		return 1;
	}

	/* Wait for finger */
	printf("place your finger on the sensor\n");

	if (dpfp_simple_await_finger_on(dev) < 0) {
		perror("await_finger_on");
		return 1;
	}

	/* Capture fingerprint */
	if (dpfp_set_mode(dev, DPFP_MODE_SEND_FINGER) < 0) {
		perror("set_mode");
		return 1;
	}

	if (dpfp_capture_fprint(dev, fp) < 0) {
		perror("simple_get_fingerprint");
		return 1;
	}

	printf("remove finger from sensor\n");

	if (dpfp_simple_await_finger_off(dev) < 0) {
		perror("await_finger_off");
		return 1;
	}

}

struct dpfp_mset *process_fprint(struct dpfp_fprint *fp,
	struct dpfp_fprint *base_img)
{
	struct timeval tv;
	double t1, t2;
	struct dpfp_fprint *mask = dpfp_fprint_alloc();
	struct dpfp_ffield *direction = dpfp_ffield_alloc();
	struct dpfp_ffield *frequency = dpfp_ffield_alloc();
	struct dpfp_mset *mset = dpfp_mset_alloc();
	struct dpfp_mset *new;

	gettimeofday(&tv, NULL);
	t1 = TV_TO_DOUBLE(tv);

	/* Basic enhancements: subtract base image, flip to correct orientation */
	dpfp_fprint_subtract(fp, base_img);
	dpfp_fprint_flip_v(fp);
	dpfp_fprint_flip_h(fp);

	/* More advanced enhancements */
	dpfp_fprint_soften_mean(fp, 3);
	dpfp_fprint_get_direction(fp, direction, 7, 8);
	dpfp_fprint_get_frequency(fp, direction, frequency);
	dpfp_fprint_get_mask(fp, direction, frequency, mask);
	dpfp_fprint_enhance_gabor(fp, direction, frequency, mask, 4.0);
	dpfp_fprint_binarize(fp, 0x80);

	/* Minutiae detection */
	dpfp_fprint_thin(fp);
	dpfp_fprint_detect_minutiae(fp, mset);
	new = dpfp_mset_remove_noise(mset, mask);

	gettimeofday(&tv, NULL);
	t2 = TV_TO_DOUBLE(tv);
	printf("enhancements + processing took %.6lf seconds in total\n", t2 - t1);

	dpfp_fprint_free(mask);
	dpfp_mset_free(mset);
	dpfp_ffield_free(direction);
	dpfp_ffield_free(frequency);
	return new;
}

int main(void)
{
	struct dpfp_fprint *base1 = dpfp_fprint_alloc();
	struct dpfp_fprint *base2 = dpfp_fprint_alloc();
	struct dpfp_fprint *fp1 = dpfp_fprint_alloc();
	struct dpfp_fprint *fp2 = dpfp_fprint_alloc();
	struct dpfp_mset *mset1;
	struct dpfp_mset *mset2;
	float result = 0;

	dpfp_init();

	dev = dpfp_open();
	if (dev == NULL) {
		perror("dev");
		goto exit;
	}

	capture_fprint(fp1, base1);
	sleep(1);
	capture_fprint(fp2, base2);
	dpfp_close(dev);

	printf("capturing completed\n");

/*
	dpfp_fprint_write_to_file(fp1, "fp1.pgm");
	dpfp_fprint_write_to_file(fp2, "fp2.pgm");
	dpfp_fprint_write_to_file(base1, "base1.pgm");
	dpfp_fprint_write_to_file(base2, "base2.pgm");
*/

	printf("processing fingerprint 1...\n");
	mset1 = process_fprint(fp1, base1);

	printf("processing fingerprint 2...\n");
	mset2 = process_fprint(fp2, base2);

	result = dpfp_fprint_mset_match1(mset1, mset2);
	printf("match1 result %f\n", result);

exit:
	return 0;
}

