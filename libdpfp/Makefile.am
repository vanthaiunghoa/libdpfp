MAINTAINERCLEANFILES = Makefile.in
lib_LTLIBRARIES = libdpfp.la

libdpfp_la_CFLAGS = $(LIBUSB_CFLAGS) $(CRYPTO_CFLAGS)

libdpfp_la_LIBADD = $(LIBUSB_LIBS) $(CRYPTO_LIBS) -lm

libdpfp_la_LDFLAGS = -version-info @lt_major@:@lt_revision@:@lt_age@

libdpfp_la_SOURCES =		\
	dpfp.c			\
	dpfp_simple.c		\
	dpfp_hw.c		\
	dpfp_fprint.c		\
	dpfp_fprint_fvs.c	\
	dpfp_fprint_efinger.c	\
	dpfp.h			\
	dpfp_private.h

# headers to be installed
pkginclude_HEADERS = dpfp.h

