/*
 * Main includes for Digital Persona UareU fingerprint reader library
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

/*! \file dpfp.h
 * This is the file you should include in your application.
 * It provides the necessary functions for device control.
 */

#ifndef __DPFP_H__
#define __DPFP_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <usb.h>

struct dpfp_dev;

struct dpfp_fprint {
	size_t header_size;
	size_t data_size;
	unsigned char *header;
	unsigned char *data;
};

struct dpfp_ffield {
	double *pimg;
};

#define DPFP_MAX_MINUTIAE	384

struct dpfp_minutia {
	int x;
	int y;
};

/* minutiae set */
struct dpfp_mset {
	/* minutia pairs */
	struct dpfp_minutia minutiae[DPFP_MAX_MINUTIAE];

	/* number of minutia pairs in above array */
	int count;
};

enum dpfp_modes {
	DPFP_MODE_INIT = 0x00,
	DPFP_MODE_AWAIT_FINGER_ON = 0x10,
	DPFP_MODE_AWAIT_FINGER_OFF = 0x12,
	DPFP_MODE_SEND_FINGER = 0x20,
	DPFP_MODE_SHUT_UP = 0x30,
	DPFP_MODE_READY = 0x80,
};

#define DPFP_IRQ_LENGTH		64
#define DPFP_CHALLENGE_LENGTH	5
#define DPFP_RESPONSE_LENGTH	4
#define DPFP_AUTH_CR_LENGTH	16

/* We use the first 2 bytes of each interrupt to determine its type */
#define DPFP_IRQDATA_SCANPWR_ON	0x56aa
#define DPFP_IRQDATA_FINGER_ON	0x0101
#define DPFP_IRQDATA_FINGER_OFF	0x0200

/* Image dimensions */
#define DPFP_IMG_HEIGHT	289
#define DPFP_IMG_WIDTH	384

int dpfp_init();

struct dpfp_dev *dpfp_open();
struct dpfp_dev *dpfp_open_idx(int idx);
int dpfp_close(struct dpfp_dev *dev);

struct dpfp_fprint *dpfp_fprint_alloc();
void dpfp_fprint_free(struct dpfp_fprint *fp);
int dpfp_fprint_write_to_file(struct dpfp_fprint *fp, char *filename);
void dpfp_fprint_flip_v(struct dpfp_fprint *fp);
void dpfp_fprint_flip_h(struct dpfp_fprint *fp);
void dpfp_fprint_subtract(struct dpfp_fprint *a, struct dpfp_fprint *b);

struct dpfp_ffield *dpfp_ffield_alloc();
void dpfp_ffield_free(struct dpfp_ffield *ffield);

struct dpfp_mset *dpfp_mset_alloc();

int dpfp_fprint_soften_mean(struct dpfp_fprint *fp, int size);
int dpfp_fprint_get_direction(struct dpfp_fprint *fp, struct dpfp_ffield *ff,
	int block_size, int filter_size);
int dpfp_fprint_get_frequency(struct dpfp_fprint *fp,
	struct dpfp_ffield *direction, struct dpfp_ffield *frequency);
int dpfp_fprint_get_mask(struct dpfp_fprint *fp, struct dpfp_ffield *direction,
	struct dpfp_ffield *frequency, struct dpfp_fprint *mask);
int dpfp_fprint_enhance_gabor(struct dpfp_fprint *fp,
	struct dpfp_ffield *direction, struct dpfp_ffield *frequency,
	struct dpfp_fprint *mask, double radius);

int dpfp_fprint_detect_minutiae(struct dpfp_fprint *fp, struct dpfp_mset *mset);
void dpfp_fprint_plot_mset(struct dpfp_mset *mset, struct dpfp_fprint *fp);
float dpfp_fprint_mset_match1(struct dpfp_mset *mset1, struct dpfp_mset *mset2);
struct dpfp_mset *dpfp_mset_remove_noise(struct dpfp_mset *mset,
	struct dpfp_fprint *mask);

int dpfp_get_irq(struct dpfp_dev *dev, unsigned char *buf, int timeout);
int dpfp_set_mode(struct dpfp_dev *dev, unsigned char mode);
int dpfp_capture_fprint(struct dpfp_dev *dev, struct dpfp_fprint *fp);
int dpfp_set_edge_light(struct dpfp_dev *dev, int brightness);

int dpfp_get_hwstat(struct dpfp_dev *dev, unsigned char *data);
int dpfp_set_hwstat_pwr(struct dpfp_dev *dev, int on);

int dpfp_auth_read_challenge(struct dpfp_dev *dev, unsigned char *data);
int dpfp_auth_write_response(struct dpfp_dev *dev, unsigned char *data);

int dpfp_simple_get_irq_with_type(struct dpfp_dev *dev, uint16_t irqtype,
	unsigned char *irqbuf, int timeout);

int dpfp_simple_await_finger_on(struct dpfp_dev *dev);
int dpfp_simple_await_finger_on_irqbuf(struct dpfp_dev *dev,
		unsigned char *irqbuf);
int dpfp_simple_await_finger_off(struct dpfp_dev *dev);
int dpfp_simple_await_finger_off_irqbuf(struct dpfp_dev *dev,
		unsigned char *irqbuf);
struct dpfp_rsp *dpfp_simple_challenge(struct dpfp_dev *dev, unsigned char p1,
		unsigned char p2, unsigned char p3, unsigned char p4,
		unsigned char p5);

static inline void dpfp_fprint_clear(struct dpfp_fprint *fp)
{
	memset(fp->data, 0, DPFP_IMG_HEIGHT * DPFP_IMG_WIDTH);
}

static inline void *dpfp_mset_free(struct dpfp_mset *mset)
{
	free(mset);
}

#endif

