AC_INIT([libdpfp], [0.2.2])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([libdpfp/dpfp.h])
AM_CONFIG_HEADER([config.h])

AC_PREREQ([2.59])
AC_PROG_CC
AC_PROG_LIBTOOL

AC_TYPE_SIZE_T
AC_C_CONST
AC_FUNC_MALLOC

AC_CHECK_HEADERS([fcntl.h])

AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([strtol])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([memset])

# Library versioning
lt_major="0"
lt_revision="0"
lt_age="0"
AC_SUBST(lt_major)
AC_SUBST(lt_revision)
AC_SUBST(lt_age)

# check for libusb
PKG_CHECK_MODULES(LIBUSB, "libusb")
AC_SUBST(LIBUSB_CFLAGS)
AC_SUBST(LIBUSB_LIBS)

# check for openssl's libcrypto
PKG_CHECK_MODULES(CRYPTO, "libcrypto")
AC_SUBST(CRYPTO_CFLAGS)
AC_SUBST(CRYPTO_LIBS)

# check for Xv extensions (necessary for examples/capture_continuous)
# imported from Coriander
AC_DEFUN([AC_CHECK_XV],[
	AC_SUBST(XV_CFLAGS)
	AC_SUBST(XV_LIBS)
	AC_MSG_CHECKING(for Xv extensions)
	AC_TRY_COMPILE([
	#include <X11/Xlib.h>
 	#include <X11/extensions/Xvlib.h>],[
	int main(void) { (void) XvGetPortAttribute(0, 0, 0, 0); return 0; }
	],xv=yes,xv=no);
	AC_MSG_RESULT($xv)
 	if test x$xv = xyes; then
           XV_LIBS="-lXv -lXext"
           XV_CFLAGS=""
	   AC_DEFINE(HAVE_XV,1,[defined if XV video overlay is available])
 	fi
        AM_CONDITIONAL(XVOK, test x$xv = xyes)
])
AC_CHECK_XV

# check for GTK+ (necessary for examples/capture_continuous_gtk)
PKG_CHECK_MODULES(GTK, "gtk+-2.0", gtk=yes, gtk=no)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)
AM_CONDITIONAL(HAS_GTK, test x$gtk = xyes)

AC_CONFIG_FILES([Makefile] [libdpfp.pc] [libdpfp/Makefile] [examples/Makefile])
AC_OUTPUT

