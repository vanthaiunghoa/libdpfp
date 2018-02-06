/*
 * Functions for image manipulation using FVS code
 * 
 *    Copyright (C) 2002-2003 Shivang Patel
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "dpfp.h"
#include "dpfp_private.h"

/* Applies a smooth effect to fp */
int dpfp_fprint_soften_mean(struct dpfp_fprint *fp, int size)
{
	struct timeval tv;
	double t1, t2;
	int soften_size, soften_area;
	int x, y;
	int c, p, q;
	unsigned char *copy;
	unsigned char *buf = fp->data;

	gettimeofday(&tv, NULL);
	t1 = TV_TO_DOUBLE(tv);

	/* We need a copy of the original during computation */
	copy = malloc(fp->data_size);
	if (copy == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memcpy(copy, fp->data, fp->data_size);

	soften_size = size / 2;		/* size */
	soften_area = size * size;	/* area */

	for (y = soften_size; y < DPFP_IMG_HEIGHT - soften_size; y++)
		for (x = soften_size; x < DPFP_IMG_WIDTH - soften_size; x++) {
			c = 0;
			for (q = -soften_size; q <= soften_size; q++)
				for (p = -soften_size; p <= soften_size; p++)
					c += copy[(x + p) + (y + q) * DPFP_IMG_WIDTH];
			buf[x + y * DPFP_IMG_WIDTH] = c / soften_area;
		}

	free(copy);

	gettimeofday(&tv, NULL);
	t2 = TV_TO_DOUBLE(tv);
	dbgf(DBG_INFO, "took %.6lf seconds", t2 - t1);

	return 0;
}

struct dpfp_ffield *dpfp_ffield_alloc()
{
	struct dpfp_ffield *ffield = malloc(sizeof(*ffield));
	int pimg_size = DPFP_IMG_WIDTH * DPFP_IMG_HEIGHT * sizeof(double);
	ffield->pimg = malloc(pimg_size);
	memset(ffield->pimg, 0, pimg_size);
	return ffield;
}

void dpfp_ffield_free(struct dpfp_ffield *ffield)
{
	free(ffield->pimg);
	free(ffield);
}

/*
** In this step, we estimate the ridge orientation field.
** Given a normalized image G, the main steps of the algorithm are as
** follows:
**
** 1 - Divide G into blocks of w x w - (15 x 15)
**
** 2 - Compute the gradients dx(i,j) and dy(i,j) at each pixel (i,j),
**     depending on the computational requirement, the gradient operator
**     may vary from the single Sobel operator to the more complex Marr-
**     Hildreth operator.
**
** 3 - Estimate the local orientation of each block centered at pixel
**     (i,j), using the following operations:
**
**               i+w/2 j+w/2
**               ---   --- 
**               \     \
**     Nx(i,j) =  --    -- 2 dx(u,v) dy(u,v)
**               /     /
**               ---   ---
**            u=i-w/2 v=j-w/2
**
**               i+w/2 j+w/2
**               ---   --- 
**               \     \
**     Ny(i,j) =  --    -- dx²(u,v) - dy²(u,v)
**               /     /
**               ---   ---
**            u=i-w/2 v=j-w/2
**
**                  1    -1  / Nx(i,j) \
**     Theta(i,j) = - tan   |  -------  |
**                  2        \ Ny(i,j) /
**
**     where Theta(i,j) is the least square estimate of the local ridge
**     orientation at the block centered at pixel (i,j). Mathematically,
**     it represents the direction that is orthogonal to the dominant
**     direction of the Fourier spectrum of the w x w window.
**
** 4 - Due to the presence of noise, corrupted ridge and furrow structures,
**     minutiae, etc. in the input image,the estimated local ridge
**     orientation may not always be a correct estimate. Since local ridge
**     orientation varies slowly in a local neighbourhood, where no
**     singular point appears, a low pass filter can be used to modify the
**     incorrect local ridge orientation. In order to perform the low-pass
**     filtering, the orientation image needs to be converted into a
**     continuous vector field, which is defined as follows:
**       Phi_x(i,j) = cos( 2 x theta(i,j) )
**       Phi_y(i,j) = sin( 2 x theta(i,j) )
**     With the resulting vector field, the low-pass filtering can then
**     be performed with a convolution as follows:
**       Phi2_x(i,j) = (W @ Phi_x) (i,j)
**       Phi2_y(i,j) = (W @ Phi_y) (i,j)
**     where W is a 2D low-pass filter.
**
** 5 - Compute the local ridge orientation at (i,j) with
**
**              1    -1  / Phi2_y(i,j) \
**     O(i,j) = - tan   |  -----------  |
**              2        \ Phi2_x(i,j) /
**
** With this algorithm, a fairly smooth orientatin field estimate can be
** obtained.
**
*/
static int fprint_direction_low_pass(double *theta, double *ffbuf,
		int filter_size)
{
	struct timeval tv;
	double t1, t2;
	double *fbuf;
	double *phix, *phiy;
	double *phi2x, *phi2y;
	int fsize = filter_size * 2 + 1;
	int fbuf_size = fsize * fsize * sizeof(double);
	int result;
	size_t nbytes = DPFP_IMG_WIDTH * DPFP_IMG_HEIGHT * sizeof(double);
	double nx, ny;
	int val;
	int i, j;
	int x, y;

	gettimeofday(&tv, NULL);
	t1 = TV_TO_DOUBLE(tv);

	fbuf = malloc(fbuf_size);
	phix = malloc(nbytes);
	phiy = malloc(nbytes);
	phi2x = malloc(nbytes);
	phi2y = malloc(nbytes);

	if (!fbuf || !phi2x || !phi2y || !phix || !phiy) {
		errno = ENOMEM;
		result = -1;
		goto free;
	}

	/* reset all fields to 0 */
	memset(fbuf, 0, fbuf_size);
	memset(phix, 0, nbytes);
	memset(phiy, 0, nbytes);
	memset(phi2x, 0, nbytes);
	memset(phi2y, 0, nbytes);

	/* 4 - Compute a continuous field from theta */
	for (y = 0; y < DPFP_IMG_HEIGHT; y++)
		for (x = 0; x < DPFP_IMG_WIDTH; x++) {
			val = x + y * DPFP_IMG_WIDTH;
			phix[val] = cos(theta[val]);
			phiy[val] = sin(theta[val]);
		}

	/* build the low-pass filter */
	nx = 0.0;
	for (j = 0; j < fsize; j++)
		for (i = 0; i < fsize; i++) {
			fbuf[j * fsize + i] = 1.0;
			/*
			   fbuf[j*fsize+i] = (FvsFloat_t)(fsize - (abs(nFilterSize-i)+abs(nFilterSize-j)));
			   */
			nx += fbuf[j*fsize+i]; /* sum of coefficients */
		}
	if (nx > 1.0) {
		for (j = 0; j < fsize; j++)
			for (i = 0; i < fsize; i++)
				/* normalize the result */
				fbuf[j * fsize + i] /= nx;
	}
	/* low-pass on the result arrays getting phi2 */
	for (y = 0; y < DPFP_IMG_HEIGHT - fsize; y++)
		for (x = 0; x < DPFP_IMG_WIDTH - fsize; x++)
		{
			nx = 0.0;
			ny = 0.0;
			for (j = 0; j < fsize; j++)
				for (i = 0; i < fsize; i++) {
					val = (x + i) + (j + y) * DPFP_IMG_WIDTH;
					nx += fbuf[j * fsize + i] * phix[val];
					ny += fbuf[j * fsize + i] * phiy[val];
				}
			val = x + y * DPFP_IMG_WIDTH;
			phi2x[val] = nx;
			phi2y[val] = ny;
		}

	/* 5 - local ridge orientation -> theta */
	for (y = 0; y < DPFP_IMG_HEIGHT - fsize; y++)
		for (x = 0; x < DPFP_IMG_WIDTH - fsize; x++) {
			val = x + y * DPFP_IMG_WIDTH;
			ffbuf[val] = atan2(phi2y[val], phi2x[val]) * 0.5;
		}

free:
	if (phix)
		free(phix);

	if (phiy)
		free(phiy);

	if (phi2x)
		free(phi2x);

	if (phi2y)
		free(phi2y);

	if (fbuf)
		free(fbuf);

	gettimeofday(&tv, NULL);
	t2 = TV_TO_DOUBLE(tv);
	dbgf(DBG_INFO, "took %.6lf seconds", t2 - t1);

	return result;
}

#define P(x,y)      ((int32_t) imgbuf[(x) + (y) * DPFP_IMG_WIDTH])

int dpfp_fprint_get_direction(struct dpfp_fprint *fp, struct dpfp_ffield *ff,
	int block_size, int filter_size)
{
	struct timeval tv;
	double t1, t2;
	int i, j;
	int u, v;
	int x, y;
	int result = 0;
	double nx, ny;
	double *ffbuf = ff->pimg;
	double *theta = NULL;
	unsigned char *imgbuf = fp->data;
	int diff_size = block_size * 2 + 1;
	double dx[diff_size * diff_size], dy[diff_size * diff_size];

	gettimeofday(&tv, NULL);
	t1 = TV_TO_DOUBLE(tv);

	/* allocate memory for the orientation values */
	if (filter_size > 0) {
		int theta_size = DPFP_IMG_WIDTH * DPFP_IMG_HEIGHT * sizeof(double);
		theta = malloc(theta_size);
		if (theta == NULL) {
			errno = ENOMEM;
			return -1;
		}

		memset(theta, 0, theta_size);
	}

	/* 1 - divide the image in blocks */
	for (y = block_size + 1; y < DPFP_IMG_HEIGHT - block_size - 1; y++)
		for (x = block_size + 1; x < DPFP_IMG_WIDTH - block_size - 1; x++) {
			/* 2 - for the block centered at x,y compute the gradient */
			for (j = 0; j < diff_size; j++)
				for (i = 0; i < diff_size; i++) {
					dx[i * diff_size + j] = (double)
						(P(x + i - block_size,     y + j - block_size) -
						 P(x + i - block_size - 1, y + j - block_size));
					dy[i * diff_size + j] = (double)
						(P(x + i - block_size, y + j - block_size) -
						 P(x + i - block_size, y + j - block_size - 1));
				}

			/* 3 - compute orientation */
			nx = 0.0;
			ny = 0.0;
			for (v = 0; v < diff_size; v++)
				for (u = 0; u < diff_size; u++) {
					nx += 2 * dx[u * diff_size + v] * dy[u * diff_size + v];
					ny += dx[u * diff_size + v] * dx[u * diff_size + v]
						- dy[u * diff_size + v] * dy[u * diff_size + v];
				}
			/* compute angle (-pi/2 .. pi/2) */
			if (filter_size > 0)
				theta[x + y * DPFP_IMG_WIDTH] = atan2(nx, ny);
			else
				ffbuf[x + y * DPFP_IMG_WIDTH] = atan2(nx, ny) * 0.5;
		}

	gettimeofday(&tv, NULL);
	t2 = TV_TO_DOUBLE(tv);
	dbgf(DBG_INFO, "took %.6lf seconds", t2 - t1);

	if (filter_size > 0)
		result = fprint_direction_low_pass(theta, ffbuf, filter_size);

	if (theta)
		free(theta);

	return result;
}

/* width */
#define BLOCK_W     16
#define BLOCK_W2     8

/* length */
#define BLOCK_L     32
#define BLOCK_L2    16

#define EPSILON     0.0001
#define LPSIZE      3
#define LPFACTOR    (1.0 / ((LPSIZE * 2 + 1) * (LPSIZE * 2 + 1)))

int dpfp_fprint_get_frequency(struct dpfp_fprint *fp,
	struct dpfp_ffield *direction, struct dpfp_ffield *frequency)
{
	struct timeval tv;
	double t1, t2;
	int x, y;
	int u, v;
	int d, k;
	double *out;
	double *freq = frequency->pimg;
	double *orientation = direction->pimg;
	unsigned char *imgbuf = fp->data;
	size_t size = DPFP_IMG_WIDTH * DPFP_IMG_HEIGHT * sizeof(double);

	double dir = 0.0, cosdir = 0.0, sindir = 0.0;
	int peak_pos[BLOCK_L]; /* peak positions */
	int peak_cnt;          /* peak count     */
	double peak_freq;         /* peak frequence */
	double Xsig[BLOCK_L];     /* x signature    */
	double pmin, pmax;

	gettimeofday(&tv, NULL);
	t1 = TV_TO_DOUBLE(tv);

	/* allocate memory for the output */
	out = malloc(size);
	if (out == NULL) {
		errno = ENOMEM;
		return -1;
	}

	memset(out, 0, size);
	memset(freq, 0, size);

	/* 1 - Divide G into blocks of BLOCK_W x BLOCK_W - (16 x 16) */
	for (y = BLOCK_L2; y < DPFP_IMG_HEIGHT - BLOCK_L2; y++)
		for (x = BLOCK_L2; x < DPFP_IMG_WIDTH - BLOCK_L2; x++) {
			/* 2 - oriented window of size l x w (32 x 16) in the ridge dir */
			dir = orientation[(x + BLOCK_W2) + (y + BLOCK_W2) * DPFP_IMG_WIDTH];
			cosdir = -sin(dir);  /* ever > 0 */
			sindir = cos(dir);   /* -1 ... 1 */

			/* 3 - compute the x-signature X[0], X[1], ... X[l-1] */
			for (k = 0; k < BLOCK_L; k++) {
				Xsig[k] = 0.0;
				for (d = 0; d < BLOCK_W; d++) {
					u = (int) (x + (d - BLOCK_W2) * cosdir
							+ (k - BLOCK_L2) * sindir);
					v = (int) (y + (d - BLOCK_W2) * sindir
							- (k - BLOCK_L2) * cosdir);
					/* clipping */
					if (u < 0)
						u = 0;
					else if (u > DPFP_IMG_WIDTH - 1)
						u = DPFP_IMG_WIDTH - 1;

					if (v < 0)
						v = 0;
					else if (v > DPFP_IMG_HEIGHT - 1)
						v = DPFP_IMG_HEIGHT - 1;

					Xsig[k] += imgbuf[u + (v * DPFP_IMG_WIDTH)];
				}
				Xsig[k] /= BLOCK_W;
			}

			/* Let T(i,j) be the avg number of pixels between 2 peaks */
			/* find peaks in the x signature */
			peak_cnt = 0;
			/* test if the max - min or peak to peak value too small is,
			   then we ignore this point */
			pmax = pmin = Xsig[0];
			for (k = 1; k < BLOCK_L; k++) {
				if (pmin>Xsig[k]) pmin = Xsig[k];
				if (pmax<Xsig[k]) pmax = Xsig[k];
			}

			if ((pmax - pmin) > 64.0)
				for (k = 1; k < BLOCK_L-1; k++)
					if ((Xsig[k-1] < Xsig[k]) &&
							(Xsig[k] >= Xsig[k+1])) 
						peak_pos[peak_cnt++] = k;

			/* compute mean value */
			peak_freq = 0.0;
			if (peak_cnt >= 2) {
				for (k = 0; k < peak_cnt - 1; k++)
					peak_freq += peak_pos[k + 1] - peak_pos[k];
				peak_freq /= peak_cnt - 1;
			}

			/* 4 - must lie in a certain range [1/25-1/3] */
			/*     changed to range [1/30-1/2] */
			if (peak_freq > 30.0)
				out[x + y * DPFP_IMG_WIDTH] = 0.0;
			else if (peak_freq < 2.0)
				out[x + y * DPFP_IMG_WIDTH] = 0.0;
			else
				out[x + y * DPFP_IMG_WIDTH] = 1.0 / peak_freq;
		}

	/* 5 - interpolated ridge period for the unknown points */
	for (y = BLOCK_L2; y < DPFP_IMG_HEIGHT - BLOCK_L2; y++)
		for (x = BLOCK_L2; x < DPFP_IMG_WIDTH - BLOCK_L2; x++)
			if (out[x + y * DPFP_IMG_WIDTH] < EPSILON) {
				if (out[x + (y - 1) * DPFP_IMG_WIDTH] > EPSILON)
					out[x + (y * DPFP_IMG_WIDTH)] =
						out[x + (y - 1) * DPFP_IMG_WIDTH];
				else if (out[x - 1 + (y * DPFP_IMG_WIDTH)] > EPSILON)
					out[x + (y * DPFP_IMG_WIDTH)] =
						out[x - 1 + (y * DPFP_IMG_WIDTH)];
			}

	/* 6 - Inter-ridges distance change slowly in a local neighbourhood */
	for (y = BLOCK_L2; y < DPFP_IMG_HEIGHT - BLOCK_L2; y++)
		for (x = BLOCK_L2; x < DPFP_IMG_WIDTH - BLOCK_L2; x++) {
			k = x + y * DPFP_IMG_WIDTH;
			peak_freq = 0.0;
			for (v = -LPSIZE; v <= LPSIZE; v++)
				for (u = -LPSIZE; u <= LPSIZE; u++)
					peak_freq += out[(x + u) + (y + v) * DPFP_IMG_WIDTH];
			freq[k] = peak_freq * LPFACTOR;
		}

	free(out);

	gettimeofday(&tv, NULL);
	t2 = TV_TO_DOUBLE(tv);
	dbgf(DBG_INFO, "took %.6lf seconds", t2 - t1);

	return 0;
}

#undef P
#define P(x,y)      imgbuf[(x) + (y) * DPFP_IMG_WIDTH]

/* Use a structural operator to dilate the image 
 **    X
 **  X X X
 **    X
 */
static void image_dilate(struct dpfp_fprint *image)
{
	unsigned char *imgbuf = image->data;
	int x , y;

	for (y = 1; y < DPFP_IMG_HEIGHT - 1; y++)
		for (x = 1; x < DPFP_IMG_WIDTH - 1; x++)
			if (P(x, y) == 0xff) {
				P(x - 1, y) |= 0x80;
				P(x + 1, y) |= 0x80;
				P(x, y - 1) |= 0x80;
				P(x, y + 1) |= 0x80;
			}

	for (y = 0; y < DPFP_IMG_WIDTH * DPFP_IMG_HEIGHT; y++)
		if (imgbuf[y])
			imgbuf[y] = 0xff;
}

static void image_erode(struct dpfp_fprint *image)
{
	unsigned char *imgbuf = image->data;
	int x, y;

	for (y = 1; y < DPFP_IMG_HEIGHT - 1; y++)
		for (x = 1; x < DPFP_IMG_WIDTH - 1; x++)
			if (P(x, y) == 0) {
				P(x - 1, y) &= 0x80;
				P(x + 1, y) &= 0x80;
				P(x, y - 1) &= 0x80;
				P(x, y + 1) &= 0x80;
			}

	for (y = 0; y < DPFP_IMG_WIDTH * DPFP_IMG_HEIGHT; y++)
		if (imgbuf[y] != 0xff)
			imgbuf[y] = 0;
}

int dpfp_fprint_get_mask(struct dpfp_fprint *fp, struct dpfp_ffield *direction,
	struct dpfp_ffield *frequency, struct dpfp_fprint *mask)
{
	struct timeval tv;
	double t1, t2;
	int pos, posout;
	int x, y;
	unsigned char *out = mask->data;
	double *freq = frequency->pimg;
	double freqmin = 1.0 / 25, freqmax = 1.0 / 3;

	gettimeofday(&tv, NULL);
	t1 = TV_TO_DOUBLE(tv);

	memset(out, 0, DPFP_IMG_WIDTH * DPFP_IMG_HEIGHT);

	for (y = 0; y < DPFP_IMG_HEIGHT; y++)
		for (x = 0; x < DPFP_IMG_WIDTH; x++) {
			pos = x + y * DPFP_IMG_WIDTH;
			posout = x + y * DPFP_IMG_WIDTH;
			out[posout] = 0;
			if (freq[pos] >= freqmin && freq[pos] <= freqmax)
				/*              out[posout] = (uint8_t)(10.0/freq[pos]);*/
				out[posout] = 255;
		}

	/* fill in the holes */
	for (y = 0; y < 4; y++)
		image_dilate(mask);

	/* remove borders */
	for (y = 0; y < 12; y++)
		image_erode(mask);

	gettimeofday(&tv, NULL);
	t2 = TV_TO_DOUBLE(tv);
	dbgf(DBG_INFO, "took %.6lf seconds", t2 - t1);

	return 0;
}

/*
** jdh: image enhancement part. This enhancement algorithm is specialized
** on fingerprint images. It marks regions that are not to be used with a
** special color in the mask. Other pixels are improved to that ridges can
** be clearly separated using a threshold value.
** The algorithm produces a ridges direction field image and a mask that
** masks out the unusable areas or areas that are likely to contain no
** fingerprint as well as border of fingerprint areas.
**
** Ideas have been found in small articles:
** 1 - Fingerprint Enhancement: Lin Hong, Anil Jain, Sharathcha Pankanti,
**     and Ruud Bolle. [Hong96]
** 2 - Fingerprint Image Enhancement, Algorithm and Performance Evaluation:
**     Lin Hong, Yifei Wan and Anil Jain. [Hong98]
**
** The enhancement is performed using several steps as detailed in (2)
**  A - Normalization
**  B - Compute Orientation
**  C - Compute Frequency
**  D - Compute Region Mask
**  E - Filter
**
*/

#undef P
#define P(x,y)      ((int32_t)p[(x)+(y)*pitch])

/* {{{ Filter the parts and improve the ridges */

/* 
** We apply a Gabor filter over the image using the direction and the
** frequency computer in the steps before. The even symetric gabor function
** used is the following
**
**                    / 1|x'²   y'²|\
** h(x,y:phi,f) = exp|- -|--- + ---| |.cos(2.PI.f.x')
**                    \ 2|dx²   dy²|/
**
** x' =  x.cos(phi) + y.sin(phi)
** y' = -x.sin(phi) + y.cos(phi)
**
** Its value is based on empirical data, we choose to set it to 4.0 at first.
** The bigger the value is, mre resistant to noise becomes the algorithm,
** but more likely will he procude spurious ridges.
**
** Let:
**  G be the normalized image
**  O the orientation image
**  F the frequency image
**  R the removable mask image
**  E the resulting enhanced image
**  Wg the size of the gabor filter
**
**          / 255                                          if R(i,j) = 0
**         |
**         |  Wg/2    Wg/2 
**         |  ---     ---
** E(i,j)= |  \       \
**         |   --      --  h(u,v:O(i,j),F(i,j)).G(i-u,j-v) otherwise
**         |  /       /
**          \ ---     ---
**            u=-Wg/2 v=-Wg/2
**
*/
/* helper function computing the gabor filter factor r2 is r*r */
static double enhance_gabor(double x, double y, double phi, double f, double r2)
{
	double dy2 = 1.0 / r2, dx2 = 1.0 / r2;
	double x2, y2;

	phi += M_PI / 2;
	x2 = -x * sin(phi) + y * cos(phi);
	y2 =  x * cos(phi) + y * sin(phi);

	return exp(-0.5 * (x2 * x2 * dx2 + y2 * y2 * dy2)) * cos(2 * M_PI * x2 * f);
}

/* Enhance a fingerprint image */
int dpfp_fprint_enhance_gabor(struct dpfp_fprint *fp,
	struct dpfp_ffield *direction, struct dpfp_ffield *frequency,
	struct dpfp_fprint *mask, double radius)
{
	struct timeval tv;
	double t1, t2;
	int Wg2 = 8; /* from -5 to 5 are 11 */
	int i, j;
	int u,v;
	double *orientation = direction->pimg;
	double *frequence = frequency->pimg;
	unsigned char *enhanced = malloc(DPFP_IMG_WIDTH * DPFP_IMG_HEIGHT);
	unsigned char *imgbuf = fp->data;
	double sum, f, o;

	gettimeofday(&tv, NULL);
	t1 = TV_TO_DOUBLE(tv);

	if (enhanced == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memset(enhanced, 0, DPFP_IMG_WIDTH * DPFP_IMG_HEIGHT);

	/* take square */
	radius = radius * radius;

	for (j = Wg2; j < DPFP_IMG_HEIGHT - Wg2; j++)
		for (i = Wg2; i < DPFP_IMG_WIDTH - Wg2; i++)
			if (mask == NULL || mask->data[i + j * DPFP_IMG_WIDTH] != 0) {
				sum = 0.0;
				o = orientation[i + j * DPFP_IMG_WIDTH];
				f = frequence[i + j * DPFP_IMG_WIDTH];
				for (v = -Wg2; v <= Wg2; v++)
					for (u = -Wg2; u <= Wg2; u++)
						sum += enhance_gabor((double) u, (double) v, o, f,
								radius) *
								imgbuf[(i - u) + (j - v) * DPFP_IMG_WIDTH];

				/* printf("%6.1f ", sum);*/
				if (sum > 255.0)
					sum = 255.0;
				if (sum < 0.0)
					sum = 0.0;

				enhanced[i + j * DPFP_IMG_WIDTH] = (unsigned char) sum;
			}
	
	memcpy(imgbuf, enhanced, DPFP_IMG_WIDTH * DPFP_IMG_HEIGHT);
	free(enhanced);

	gettimeofday(&tv, NULL);
	t2 = TV_TO_DOUBLE(tv);
	dbgf(DBG_INFO, "took %.6lf seconds", t2 - t1);

	return 0;
}

/* Transform the gray image into a black & white binary image */
void dpfp_fprint_binarize(struct dpfp_fprint *fp, unsigned char limit)
{
	int n;
	unsigned char *imgbuf = fp->data;

	/* loop through each pixel */
	for (n = 0; n < DPFP_IMG_WIDTH * DPFP_IMG_HEIGHT; n++)
		/* now a do some math to decided if its white or black */
		imgbuf[n] = (imgbuf[n] < limit) ? 0xff : 0;
}

