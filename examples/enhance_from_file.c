/*
 * Quick hack to load a pgm file and feed it into libdpfp's image enhancement
 * algorithms.
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

int main(int argc, char *argv[])
{
	int result;
	struct dpfp_dev *dev;
	struct dpfp_fprint *fp = dpfp_fprint_alloc();
	struct dpfp_fprint *mask = dpfp_fprint_alloc();
	struct dpfp_ffield *direction = dpfp_ffield_alloc();
	struct dpfp_ffield *frequency = dpfp_ffield_alloc();
	struct dpfp_mset *mset = dpfp_mset_alloc();
	struct timeval tv;
	double t1, t2;
	FILE *fd;

	if (argc < 2) {
		printf("Usage: %s <PGM image file>\n", argv[0]);
		return 1;
	}

	fd = fopen(argv[1], "r");
	if (fd == NULL) {
		perror("open");
		return 1;
	}

	fseek(fd, 15, SEEK_SET);
	fp->data_size = fread(fp->data, 1, DPFP_IMG_HEIGHT * DPFP_IMG_WIDTH, fd);
	fclose(fd);

	gettimeofday(&tv, NULL);
	t1 = TV_TO_DOUBLE(tv);

	/* More advanced enhancements */
	dpfp_fprint_soften_mean(fp, 3);
	dpfp_fprint_get_direction(fp, direction, 7, 8);
	dpfp_fprint_get_frequency(fp, direction, frequency);
	dpfp_fprint_get_mask(fp, direction, frequency, mask);
	dpfp_fprint_enhance_gabor(fp, direction, frequency, mask, 4.0);
	dpfp_fprint_binarize(fp, 0x80);

	if (dpfp_fprint_write_to_file(fp, "finger_enhanced.pgm") < 0)
		perror("write_fingerprint_to_file");

	dpfp_fprint_thin(fp);

	if (dpfp_fprint_write_to_file(fp, "finger_thinned.pgm") < 0)
		perror("write_fingerprint_to_file");

	dpfp_fprint_detect_minutiae(fp, mset);

	gettimeofday(&tv, NULL);
	t2 = TV_TO_DOUBLE(tv);
	printf("enhancements took %.6lf seconds in total\n", t2 - t1);

	dpfp_fprint_clear(fp);
	dpfp_fprint_plot_mset(mset, fp);

	if (dpfp_fprint_write_to_file(fp, "finger_minutiae.pgm") < 0)
		perror("write_fingerprint_to_file");

	return 0;
}

