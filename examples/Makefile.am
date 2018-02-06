INCLUDES = -I$(top_srcdir)

noinst_PROGRAMS = capture_finger capture_finger_enhanced enhance_from_file match_finger

if XVOK
noinst_PROGRAMS +=  capture_continuous

capture_continuous_CFLAGS = $(X_CFLAGS) $(XV_CFLAGS)
capture_continuous_SOURCES = capture_continuous.c
capture_continuous_LDADD = ../libdpfp/libdpfp.la -ldpfp $(X_LIBS) $(X_PRE_LIBS) $(XV_LIBS) -lX11 $(X_EXTRA_LIBS);
endif

if HAS_GTK
noinst_PROGRAMS += capture_continuous_gtk
capture_continuous_gtk_CFLAGS = $(GTK_CFLAGS)
capture_continuous_gtk_LDADD = ../libdpfp/libdpfp.la -ldpfp $(GTK_LIBS)
capture_continuous_gtk_SOURCES = capture_continuous_gtk.c
endif

capture_finger_SOURCES = capture_finger.c
capture_finger_LDADD = ../libdpfp/libdpfp.la -ldpfp

capture_finger_enhanced_SOURCES = capture_finger_enhanced.c
capture_finger_enhanced_LDADD = ../libdpfp/libdpfp.la -ldpfp

enhance_from_file_SOURCES = enhance_from_file.c
enhance_from_file_LDADD = ../libdpfp/libdpfp.la -ldpfp

match_finger_SOURCES = match_finger.c
match_finger_LDADD = ../libdpfp/libdpfp.la -ldpfp

