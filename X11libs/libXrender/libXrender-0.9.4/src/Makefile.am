AM_CFLAGS = $(RENDER_CFLAGS) $(X_CFLAGS) -I$(top_srcdir) -I$(top_srcdir)/include/X11/extensions $(MALLOC_ZERO_CFLAGS)

lib_LTLIBRARIES = libXrender.la

libXrender_la_SOURCES = AddTrap.c \
                        Color.c \
                        Composite.c \
                        Cursor.c \
                        FillRect.c \
                        FillRects.c \
                        Filter.c \
                        Glyph.c \
                        Picture.c \
                        Poly.c \
                        Trap.c \
                        Tri.c \
                        Xrender.c \
                        Xrenderint.h

libXrender_la_LIBADD = @X_LIBS@ -lX11

# -version-number requires libtool >= 1.5
libXrender_la_LDFLAGS = -version-number 1:3:0 -no-undefined 

libXrenderincludedir = $(includedir)/X11/extensions
libXrenderinclude_HEADERS = $(top_srcdir)/include/X11/extensions/Xrender.h 
