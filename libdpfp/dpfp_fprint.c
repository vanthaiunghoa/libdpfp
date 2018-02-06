/*
 * Functions for simple image manipulation
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dpfp.h"
#include "dpfp_private.h"

struct dpfp_fprint *dpfp_fprint_alloc()
{
	struct dpfp_fprint *fp = malloc(sizeof(*fp));
	if (fp == NULL)
		return NULL;

	memset(fp, 0, sizeof(*fp));

	fp->header = malloc(DATABLK1_RQSIZE + DATABLK2_RQSIZE);
	if (fp->header == NULL) {
		free(fp);
		return NULL;
	}

	fp->data = fp->header + 64;
	return fp;
}

void dpfp_fprint_free(struct dpfp_fprint *fp)
{
	free(fp->header);
	free(fp);
}

/* Writes a fingerprint as a pgm image */
int dpfp_fprint_write_to_file(struct dpfp_fprint *fp, char *filename)
{
	FILE *fd;
	int result;
	int num_rows;
	unsigned char pgm_header[4];
	
	if (fp->data_size <= 0) {
		errno = EIO;
		return -1;
	}

	num_rows = fp->data_size / DPFP_IMG_WIDTH;
	if (num_rows > 999) {
		errno = EFBIG;
		return -1;
	}

	fd = fopen(filename, "w");
	if (fd == NULL)
		return -1;

	result = fputs("P5 ", fd);
	if (result < 0)
		goto err;

	sprintf(pgm_header, "%d", DPFP_IMG_WIDTH);
	result = fputs(pgm_header, fd);
	if (result < 0)
		goto err;

	result = fputc(' ', fd);
	if (result == EOF)
		goto err;

	sprintf(pgm_header, "%d", num_rows);
	result = fputs(pgm_header, fd);
	if (result < 0)
		goto err;

	result = fputs(" 255 ", fd);
	if (result < 0)
		goto err;

	result = fwrite(fp->data, 1, fp->data_size, fd);
	if (result < 0)
		goto err;

	fclose(fd);

	dbgf(DBG_INFO, "wrote fprint to %s", filename);
	return 0;

err:
	fclose(fd);
	return -1;
}

/* In-place vertical flip on fp */
void dpfp_fprint_flip_v(struct dpfp_fprint *fp)
{
	unsigned char rowbuf[DPFP_IMG_WIDTH];
	int num_rows = fp->data_size / DPFP_IMG_WIDTH;
	int i;

	for (i = 0; i < num_rows / 2; i++) {
		int offset = i * DPFP_IMG_WIDTH;
		int swap_offset = fp->data_size - (DPFP_IMG_WIDTH * (i + 1));

		/* copy top row into buffer */
		memcpy(rowbuf, fp->data + offset, DPFP_IMG_WIDTH);

		/* copy lower row over top row */
		memcpy(fp->data + offset, fp->data + swap_offset,
			DPFP_IMG_WIDTH);

		/* copy buffer over lower row */
		memcpy(fp->data + swap_offset, rowbuf, DPFP_IMG_WIDTH);
	}
}

/* In-place horizontal flip on fp */
void dpfp_fprint_flip_h(struct dpfp_fprint *fp)
{
	unsigned char rowbuf[DPFP_IMG_WIDTH];
	int num_rows = fp->data_size / DPFP_IMG_WIDTH;
	int i, j;

	for (i = 0; i < num_rows; i++) {
		int offset = i * DPFP_IMG_WIDTH;

		memcpy(rowbuf, fp->data + offset, DPFP_IMG_WIDTH);
		for (j = 0; j < DPFP_IMG_WIDTH; j++) {
			int h_offset = DPFP_IMG_WIDTH - j - 1;
			fp->data[offset + j] = rowbuf[h_offset];
		}
	}
}

/* a = a - b */
void dpfp_fprint_subtract(struct dpfp_fprint *a, struct dpfp_fprint *b)
{
	int i;
	if (a->data_size != b->data_size) {
		dbgf(DBG_ERR, "a size %d does not match b size %d",
			a->data_size, b->data_size);
		return;
	}

	for (i = 0; i < a->data_size; i++) {
		int p = a->data[i] - b->data[i];
		if (p < 0)
			p = -p;
		a->data[i] = p;
	}
}

