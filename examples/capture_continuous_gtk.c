/*
 * libdpfp example to capture a series of fingerprints and display them
 * on-screen using GdkDrawable.
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

#include <libdpfp/dpfp.h>
#include <gtk/gtk.h>
#include <string.h>

struct dpfp_dev *dev;
struct dpfp_fprint *fp;

int prepare_device()
{
	dpfp_init();
	
	dev = dpfp_open();
	if (dev == NULL) {
		perror("open");
		return 1;
	}

	fp = dpfp_fprint_alloc();

	return 0;
}

int get_frame()
{
	if (dpfp_capture_fprint(dev, fp) < 0) {
		perror("simple_get_fingerprint");
		return 1;
	}

	return 0;
}

gboolean draw_frame (gpointer data)
{
	GtkWidget *widget = (GtkWidget *) data;
	get_frame();
	gdk_draw_gray_image (widget->window, widget->style->fg_gc[GTK_STATE_NORMAL],
			0, 0, DPFP_IMG_WIDTH, DPFP_IMG_HEIGHT,
			GDK_RGB_DITHER_NONE, fp->data, DPFP_IMG_WIDTH);

	return TRUE;
}

static void on_destroy(GtkWidget *widget, gpointer data)
{
	dpfp_close(dev);
	gtk_main_quit();
}

int main (int argc, char *argv[])
{
	GtkWidget *window, *darea;
	gint x, y;
	guchar *pos;

	if (prepare_device())
		return 1;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	darea = gtk_drawing_area_new ();
	gtk_widget_set_size_request (darea, DPFP_IMG_WIDTH, DPFP_IMG_HEIGHT);
	gtk_container_add (GTK_CONTAINER (window), darea);
	gtk_widget_show_all (window);

	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(on_destroy), NULL);

	if (dpfp_set_mode(dev, DPFP_MODE_SEND_FINGER) < 0) {
		perror("set_mode");
		return 1;
	}

	g_idle_add (draw_frame, darea);
	gtk_main ();
	return 0;
}


