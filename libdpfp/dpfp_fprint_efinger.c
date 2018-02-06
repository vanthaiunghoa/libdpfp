/*
 * Functions for image manipulation, based on eFinger code
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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dpfp.h"
#include "dpfp_private.h"

/* Direction masks:
							 N     S     W     E    */
static const int masks[] = { 0200, 0002, 0040, 0010 };

/*	True if pixel neighbor map indicates the pixel is 8-simple and	*/
/*	not an end point and thus can be deleted.  The neighborhood	*/
/*	map is defined as an integer of bits abcdefghi with a non-zero	*/
/*	bit representing a non-zero pixel.  The bit assignment for the	*/
/*	neighborhood is:						*/
/*									*/
/*				a b c					*/
/*				d e f					*/
/*				g h i					*/

static const unsigned char delet[512] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

void dpfp_fprint_thin(struct dpfp_fprint *fp)
{
	struct timeval tv;
	double t1, t2;
	int	x, y; /* Pixel location */
	int	i; /* Pass index */
	int	pc = 0; /* Pass count */
	int	count = 1; /* Deleted pixel count */
	int	p, q; /* Neighborhood maps of adjacent cells */
	unsigned char qb[DPFP_IMG_WIDTH]; /* Neighborhood maps of prev scanline */
	unsigned char *imgbuf = fp->data;

	gettimeofday(&tv, NULL);
	t1 = TV_TO_DOUBLE(tv);

	qb[DPFP_IMG_WIDTH - 1] = 0;		/* Used for lower-right pixel	*/

	/* Scan image while deletions */
	while (count) {
		pc++;
		count = 0;

		for (i = 0 ; i < 4; i++) {
			int m = masks[i]; /* deletion direction mask */

			/* Build initial previous scan buffer. */
			p = imgbuf[0] != 0;
			for (x = 0; x < DPFP_IMG_WIDTH - 1; x++)
				qb[x] = p = ((p << 1) & 0006) | (imgbuf[x + 1] != 0);

			/* Scan image for pixel deletion candidates. */
			for (y = 0; y < DPFP_IMG_HEIGHT - 1; y++) {
				q = qb[0];
				p = ((q<<3)&0110) | (imgbuf[(y + 1) * DPFP_IMG_WIDTH] != 0);

				for (x = 0; x < DPFP_IMG_WIDTH - 1; x++) {
					q = qb[x];
					p = ((p << 1) & 0666) | ((q << 3) & 0110) |
						(imgbuf[((y + 1) * DPFP_IMG_WIDTH) + x + 1] != 0);
					qb[x] = p;
					if  (((p & m) == 0) && delet[p]) {
						count++;
						imgbuf[(y * DPFP_IMG_WIDTH) +x] = 0;
					}
				}

				/* Process right edge pixel. */
				p = (p << 1) & 0666;
				if	((p & m) == 0 && delet[p]) {
					count++;
					imgbuf[(y * DPFP_IMG_WIDTH) + DPFP_IMG_WIDTH - 1] = 0;
				}
			}

			/* Process bottom scan line. */
			for (x = 0 ; x < DPFP_IMG_WIDTH; x++) {
				q = qb[x];
				p = ((p << 1) & 0666) | ((q << 3) & 0110);
				if	((p & m) == 0 && delet[p]) {
					count++;
					imgbuf[((DPFP_IMG_HEIGHT - 1) * DPFP_IMG_WIDTH) + x] = 0;
				}
			}
		}
	}

	gettimeofday(&tv, NULL);
	t2 = TV_TO_DOUBLE(tv);
	dbgf(DBG_INFO, "took %.6lf seconds", t2 - t1);
}

struct dpfp_mset *dpfp_mset_alloc()
{
	struct dpfp_mset *mset = malloc(sizeof(*mset));
	if (mset != NULL)
		memset(mset, 0, sizeof(*mset));
	return mset;
}

int dpfp_fprint_detect_minutiae(struct dpfp_fprint *fp, struct dpfp_mset *mset)
{
	struct timeval tv;
	double t1, t2;
	unsigned char *buf = fp->data;
	int i, j;
	int k, l;
	int pos;

	/*
	   % mask for detecting the end points or bifurcation points in ridges
	   mask = [ 1 1  1
	   1 10 1
	   1 1  1]/10;
	   */
	const float mask[3][3] = {{0.1,0.1,0.1},{0.1,1,0.1},{0.1,0.1,0.1}};

	gettimeofday(&tv, NULL);
	t1 = TV_TO_DOUBLE(tv);

	for (i = 1; i < DPFP_IMG_HEIGHT - 1; i++) {
		for (j = 1; j < DPFP_IMG_WIDTH - 1; j++) {
			float value = 0;
			int mag;

			for (k = -1; k < 2; k++)
				for (l = -1; l < 2; l++) {
					unsigned char pval = buf[((i + k) * DPFP_IMG_WIDTH) + j + l];
					if (pval != 0)
						pval = 1;
					value += (float) ((int) pval * mask[k + 1][l + 1]);
				}

			/* if mag is 1.1 it means only one neighbour present */
			/* if mag is 1.3 it means exactly 2 neighbours present */
			mag = value * 10;
			if (mag == 11 || mag == 13) {
				pos = mset->count++;
				mset->minutiae[pos].x = j;
				mset->minutiae[pos].y = i;
				if (mset->count >= DPFP_MAX_MINUTIAE)
					break;
			}
		}
		if (mset->count >= DPFP_MAX_MINUTIAE)
			break;
	}

	gettimeofday(&tv, NULL);
	t2 = TV_TO_DOUBLE(tv);
	dbgf(DBG_INFO, "took %.6lf seconds, %d minutiae found",
		t2 - t1, mset->count);

	return 0;
}

void dpfp_fprint_plot_mset(struct dpfp_mset *mset, struct dpfp_fprint *fp)
{
	int i;

	for (i = 0; i < mset->count; i++) {
		struct dpfp_minutia *min = &mset->minutiae[i];
		fp->data[(min->y * DPFP_IMG_WIDTH) + min->x] = 0xff;
	}
}

#define NOISE_THICKNESS 15

struct dpfp_mset *dpfp_mset_remove_noise(struct dpfp_mset *mset,
	struct dpfp_fprint *mask)
{
	struct dpfp_mset *new;
	int i;

	new = dpfp_mset_alloc();
	if (new == NULL)
		return NULL;

	for (i = 0; i < mset->count; i++) {
		int x = mset->minutiae[i].x;
		int y = mset->minutiae[i].y;
		int val;
	
		/* see if point lies outside the mask */
		if (mask->data[(y * DPFP_IMG_WIDTH) + x] == 0)
			continue;

		/* check right */
		val = x + NOISE_THICKNESS;
		if (val > DPFP_IMG_WIDTH - 1)
			val = DPFP_IMG_WIDTH - 1;
		if (mask->data[(y * DPFP_IMG_WIDTH) + val] == 0)
			continue;

		/* check left */
		val = x - NOISE_THICKNESS;
		if (val < 0)
			val = 0;
		if (mask->data[(y * DPFP_IMG_WIDTH) + val] == 0)
			continue;

		/* check above */
		val = y + NOISE_THICKNESS;
		if (val > DPFP_IMG_HEIGHT - 1)
			val = DPFP_IMG_HEIGHT - 1;
		if (mask->data[(val * DPFP_IMG_WIDTH) + x] == 0)
			continue;

		/* check below */
		val = y - NOISE_THICKNESS;
		if (val < 0)
			val = 0;
		if (mask->data[(val * DPFP_IMG_WIDTH) + x] == 0)
			continue;

		val = new->count++;
		new->minutiae[val].x = x;
		new->minutiae[val].y = y;
	}

	dbgf(DBG_INFO, "reduced minutiae count from %d to %d",
		mset->count, new->count);
	return new;
}

float dpfp_fprint_mset_match1(struct dpfp_mset *mset1, struct dpfp_mset *mset2)
{

	float value = 0, returnValue;
	int mean1x = 0, mean1y = 0;
	int mean2x = 0, mean2y = 0;
	int i, j;

	/* find means */
	for (i = 0; i < mset1->count; i++) {
		mean1x += mset1->minutiae[i].x;
		mean1y += mset1->minutiae[i].y;
	}

	mean1x /= mset1->count;
	mean1y /= mset1->count;

	for (i = 0; i < mset2->count; i++) {
		mean2x += mset2->minutiae[i].x;
		mean2y += mset2->minutiae[i].y;
	}

	mean2x /= mset2->count;
	mean2y /= mset2->count;

	/* deviate arr1 my meandiff */
	for(i = 0; i < mset1->count; i++) {
		mset1->minutiae[i].x -= mean1x - mean2x;
		mset1->minutiae[i].y -= mean1y - mean2y;
	}

	for (i = 0; i < mset1->count; i++) {
		int x = mset1->minutiae[i].x;
		int y = mset1->minutiae[i].y;
		float thisValue = 0;
		for (j = 0; j < mset2->count; j++) {
			int x2 = mset2->minutiae[i].x;
			int y2 = mset2->minutiae[i].y;
			float newValue = 1.0f / (float)
				(pow((x2 - x) * (x2 - x) + (y2 - y) * (y2 - y), 0.2) + 1);
			if (newValue > thisValue)
				thisValue = newValue;
		}
		value += thisValue;
	}
	returnValue = value / mset1->count;

	value = 0;
	for (i = 0; i < mset2->count; i++) {
		int x = mset2->minutiae[i].x;
		int y = mset2->minutiae[i].y;
		float thisValue = 0;
		for (j = 0; j < mset1->count; j++) {
			int x2 = mset1->minutiae[i].x;
			int y2 = mset1->minutiae[i].y;
			float newValue = 1.0f / (float)
				(pow((x2 - x) * (x2 - x) + (y2 - y) * (y2 - y), 0.3) + 1);
			if (newValue > thisValue) 
				thisValue = newValue;
		}
		value += thisValue;
	}
	returnValue += value / mset2->count;

	return returnValue * 50.0f;
}

