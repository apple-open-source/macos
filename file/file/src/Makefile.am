MAGIC	 = $(pkgdatadir)/magic

# buildit hack!
top_builddir = $(OBJROOT)

bin_PROGRAMS = file

AM_CPPFLAGS = -DMAGIC='"$(MAGIC)"'
AM_CFLAGS = -DBUILTIN_FAT

file_SOURCES = magic.c apprentice.c softmagic.c ascmagic.c \
	compress.c is_tar.c readelf.c print.c fsmagic.c \
	funcs.c file.h names.h patchlevel.h readelf.h tar.h \
	apptype.c file.c magic.h readfat.c

