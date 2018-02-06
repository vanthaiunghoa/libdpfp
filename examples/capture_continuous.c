/*
 * libdpfp example to capture a series of fingerprints and display them
 * on-screen using Xlib. Some of this code is taken from libdc1394.
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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <libdpfp/dpfp.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/Xvlib.h>

#define FORMAT 0x32595559

int count = 0;
int capture_next = 0;
int enhanced_mode = 0;
int ccd_mode = 0;
int adaptor = -1;
struct dpfp_dev *dev;
struct dpfp_fprint *fp, *fp_base;

unsigned char *framebuffer;
Display *display = NULL;
Window window=(Window)NULL;
XvImage *xv_image = NULL;
XvAdaptorInfo *info;
GC gc;
int connection = -1;

int prepare_device()
{
	int result;

	result = dpfp_init();

	dev = dpfp_open();
	if (dev == NULL) {
		perror("open");
		return 1;
	}

	fp = dpfp_fprint_alloc();
	fp_base = dpfp_fprint_alloc();

	return 0;
}

/* based on macro by Bart Nabbe */
#define GREY2YUV(grey, y, u, v)\
  y = (9798*grey + 19235*grey + 3736*grey)  / 32768;\
  u = (-4784*grey - 9437*grey + 14221*grey)  / 32768 + 128;\
  v = (20218*grey - 16941*grey - 3277*grey) / 32768 + 128;\
  y = y < 0 ? 0 : y;\
  u = u < 0 ? 0 : u;\
  v = v < 0 ? 0 : v;\
  y = y > 255 ? 255 : y;\
  u = u > 255 ? 255 : u;\
  v = v > 255 ? 255 : v

void grey2yuy2 (unsigned char *grey, unsigned char *YUV, int num) {
	int i, j;
	int y0, y1, u0, u1, v0, v1;
	int gval;

	for (i = 0, j = 0; i < num; i += 2, j += 4)
	{
		gval = grey[i];
		GREY2YUV (gval, y0, u0 , v0);
		gval = grey[i + 1];
		GREY2YUV (gval, y1, u1 , v1);
		YUV[j + 0] = y0;
		YUV[j + 1] = (u0+u1)/2;
		YUV[j + 2] = y1;
		YUV[j + 3] = (v0+v1)/2;
	}
}


void display_frames()
{
	if (adaptor < 0)
		return;

	xv_image = XvCreateImage(display, info[adaptor].base_id, FORMAT,
			framebuffer, DPFP_IMG_WIDTH, DPFP_IMG_HEIGHT);
	XvPutImage(display, info[adaptor].base_id, window, gc, xv_image,
			0, 0, DPFP_IMG_WIDTH, DPFP_IMG_HEIGHT, 0, 0, DPFP_IMG_WIDTH,
			DPFP_IMG_HEIGHT);
	xv_image = NULL;
}


void QueryXv()
{
	int num_adaptors;
	int num_formats;
	XvImageFormatValues *formats = NULL;
	int i,j;
	char xv_name[5];

	XvQueryAdaptors(display, DefaultRootWindow(display), &num_adaptors,
			&info);

	for(i = 0; i < num_adaptors; i++) {
		formats = XvListImageFormats(display, info[i].base_id,
				&num_formats);
		for(j = 0; j < num_formats; j++) {
			xv_name[4] = 0;
			memcpy(xv_name, &formats[j].id, 4);
			if(formats[j].id == FORMAT) {
				printf("using Xv format 0x%x %s %s\n",
						formats[j].id, xv_name,
						(formats[j].format==XvPacked)
						? "packed" : "planar");
				if (adaptor < 0)
					adaptor = i;
			}
		}
	}
	XFree(formats);
	if (adaptor < 0)
		printf("No suitable Xv adaptor found\n");
}

void cleanup() {
	dpfp_close(dev);

	if ((void *) window != NULL)
		XUnmapWindow(display, window);
	if (display != NULL)
		XFlush(display);
	if (framebuffer != NULL)
		free(framebuffer);
}

int get_frame()
{
	if (dpfp_capture_fprint(dev, fp) < 0) {
		perror("simple_get_fingerprint");
		return 1;
	}

	if (enhanced_mode) {
		dpfp_fprint_subtract(fp, fp_base);
		dpfp_fprint_flip_v(fp);
		dpfp_fprint_flip_h(fp);
	}

	grey2yuy2 (fp->data, framebuffer, fp->data_size);
	if (capture_next) {
		char filename[20];
		sprintf(filename, "finger%d.pgm", ++count);
		dpfp_fprint_write_to_file(fp, filename);
		capture_next = 0;
	}

	return 0;
}

void change_mode()
{
	unsigned char mode;

	if (ccd_mode == 0) {
		mode = DPFP_MODE_SHUT_UP;
		ccd_mode = 1;
	} else {
		mode = DPFP_MODE_SEND_FINGER;
		ccd_mode = 0;
	}

	dpfp_set_mode(dev, mode);
	memset(framebuffer, 0, DPFP_IMG_WIDTH * DPFP_IMG_HEIGHT * 2);
}

void toggle_enhanced_mode()
{
	if (enhanced_mode == 0)
		enhanced_mode = 1;
	else
		enhanced_mode = 0;
}

int main(void)
{
	XEvent xev;
	XGCValues xgcv;
	long background=0x010203;

	if (prepare_device())
		return 1;

	/* make the window */
	display = XOpenDisplay(getenv("DISPLAY"));
	if(display == NULL) {
		fprintf(stderr,"Could not open display \"%s\"\n",
				getenv("DISPLAY"));
		cleanup();
		exit(-1);
	}

	QueryXv();
	framebuffer = malloc(DPFP_IMG_WIDTH * DPFP_IMG_HEIGHT * 2);

	if (adaptor < 0) {
		cleanup();
		exit(-1);
	}

	window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0,
			DPFP_IMG_WIDTH, DPFP_IMG_HEIGHT, 0,
			WhitePixel(display, DefaultScreen(display)),
			background);

	XSelectInput(display, window, StructureNotifyMask | KeyPressMask);
	XMapWindow(display, window);
	connection = ConnectionNumber(display);

	gc = XCreateGC(display, window, 0, &xgcv);

	if (dpfp_set_mode(dev, DPFP_MODE_SEND_FINGER) < 0) {
		perror("set_mode");
		return 1;
	}

	/* For enhanced mode */
	if (dpfp_capture_fprint(dev, fp_base) < 0) {
		perror("simple_get_fingerprint");
		cleanup();
		exit(-1);
	}

	printf("Press M for CCD mode, E for enhanced mode, Q to quit\n");
	
	while (1) { /* event loop */
		get_frame();
		display_frames();
		XFlush(display);

		while (XPending(display) > 0) {
			XNextEvent(display, &xev);
			if (xev.type != KeyPress)
				continue;

			switch (XKeycodeToKeysym(display, xev.xkey.keycode, 0)) {
			case XK_q:
			case XK_Q:
				cleanup();
				exit(0);
				break;
			case XK_m:
			case XK_M:
				change_mode();
				break;
			case XK_c:
			case XK_C:
				capture_next = 1;
				break;
			case XK_e:
			case XK_E:
				toggle_enhanced_mode();
				break;
			}
		} /* XPending */
	}

	cleanup();
	return 0;
}

