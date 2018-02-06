SUBDIRS = libdpfp examples

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libdpfp.pc

EXTRA_DIST = libdpfp.pc.in

dist-up: distcheck
	ncftpput ftp.berlios.de incoming $(DIST_ARCHIVES)
	firefox https://developer.berlios.de/project/admin/editpackages.php?group_id=5701

