INSTALL_DIR = /usr/X11
X11_DIR = $(INSTALL_DIR)

CC=gcc
GL_CFLAGS=-Wall -ggdb3 -Os -DPTHREADS -D_REENTRANT -DPUBLIC="" $(RC_CFLAGS) $(CFLAGS)
GL_LDFLAGS=-L$(INSTALL_DIR)/lib -L$(X11_DIR)/lib $(LDFLAGS) -Wl,-single_module

TCLSH=tclsh8.5

MKDIR=mkdir
INSTALL=install
LN=ln
RM=rm

INCLUDE=-I. -Iinclude -Iinclude/internal -DGLX_ALIAS_UNSUPPORTED -I$(INSTALL_DIR)/include -I$(X11_DIR)/include
COMPILE=$(CC) $(INCLUDE) $(GL_CFLAGS) -c

#The directory with the final binaries.
BUILD_DIR=builds

#The directory with binaries that can tested without an install.
TEST_BUILD_DIR=testbuilds

PROGRAMS=$(BUILD_DIR)/glxinfo $(BUILD_DIR)/glxgears

all: programs tests
programs: $(PROGRAMS)

include tests/tests.mk

OBJECTS=glxext.o glxcmds.o glx_pbuffer.o glx_query.o glxcurrent.o glxextensions.o \
    appledri.o apple_glx_context.o apple_glx.o pixel.o \
    compsize.o apple_visual.o apple_cgl.o glxreply.o glcontextmodes.o \
    apple_xgl_api.o apple_glx_drawable.o xfont.o apple_glx_pbuffer.o \
    apple_glx_pixmap.o apple_xgl_api_read.o glx_empty.o glx_error.o \
    apple_xgl_api_viewport.o apple_glx_surface.o apple_xgl_api_stereo.o

#This is used for building the tests.
#The tests don't require installation.
$(TEST_BUILD_DIR)/libGL.dylib: $(OBJECTS)
	-if ! test -d $(TEST_BUILD_DIR); then $(MKDIR) $(TEST_BUILD_DIR); fi
	$(CC) -O0 -ggdb3 -o $@ -dynamiclib -lXplugin -framework ApplicationServices -framework CoreFoundation -L$(X11_DIR)/lib -lX11 -lXext -Wl,-exported_symbols_list,exports.list -Wl,-single_module $(OBJECTS)

$(BUILD_DIR)/libGL.1.2.dylib: $(OBJECTS)
	-if ! test -d $(BUILD_DIR); then $(MKDIR) $(BUILD_DIR); fi
	$(CC) $(GL_CFLAGS) -o $@ -dynamiclib -install_name $(INSTALL_DIR)/lib/libGL.1.dylib -compatibility_version 1.2 -current_version 1.2 -lXplugin -framework ApplicationServices -framework CoreFoundation $(GL_LDFLAGS) -lXext -lX11 -Wl,-exported_symbols_list,exports.list $(OBJECTS)

.c.o:
	$(COMPILE) $<

apple_glx_drawable.o: apple_glx_drawable.h apple_glx_drawable.c include/GL/gl.h
apple_xgl_api.o: apple_xgl_api.h apple_xgl_api.c apple_xgl_api_stereo.c include/GL/gl.h
apple_xgl_api_read.o: apple_xgl_api_read.h apple_xgl_api_read.c apple_xgl_api.h include/GL/gl.h
apple_xgl_api_viewport.o: apple_xgl_api_viewport.h apple_xgl_api_viewport.c apple_xgl_api.h include/GL/gl.h
apple_xgl_api_stereo.o: apple_xgl_api_stereo.h apple_xgl_api_stereo.c apple_xgl_api.h include/GL/gl.h
glcontextmodes.o: glcontextmodes.c glcontextmodes.h include/GL/gl.h
glxext.o: glxext.c include/GL/gl.h
glxreply.o: glxreply.c include/GL/gl.h
glxcmds.o: glxcmds.c apple_glx_context.h include/GL/gl.h
glx_pbuffer.o: glx_pbuffer.c include/GL/gl.h
glx_error.o: glx_error.c include/GL/gl.h
glx_query.o: glx_query.c include/GL/gl.h
glxcurrent.o: glxcurrent.c include/GL/gl.h
glxextensions.o: glxextensions.h glxextensions.c include/GL/gl.h
glxhash.o: glxhash.h glxhash.c include/GL/gl.h
appledri.o: appledri.h appledristr.h appledri.c include/GL/gl.h
apple_glx_context.o: apple_glx_context.c apple_glx_context.h apple_glx_context.h include/GL/gl.h
apple_glx.o: apple_glx.h apple_glx.c apple_xgl_api.h include/GL/gl.h
apple_visual.o: apple_visual.h apple_visual.c include/GL/gl.h
apple_cgl.o: apple_cgl.h apple_cgl.c include/GL/gl.h
apple_glx_pbuffer.o: apple_glx_drawable.h apple_glx_pbuffer.c include/GL/gl.h
apple_glx_pixmap.o: apple_glx_drawable.h apple_glx_pixmap.c appledri.h include/GL/gl.h
apple_glx_surface.o: apple_glx_drawable.h apple_glx_surface.c appledri.h include/GL/gl.h
xfont.o: xfont.c glxclient.h include/GL/gl.h
compsize.o: compsize.c include/GL/gl.h
renderpix.o: renderpix.c include/GL/gl.h
singlepix.o: singlepix.c include/GL/gl.h
pixel.o: pixel.c include/GL/gl.h
glx_empty.o: glx_empty.c include/GL/gl.h

apple_xgl_api.c: apple_xgl_api.h
apple_xgl_api.h: gen_api_header.tcl  gen_api_library.tcl  gen_code.tcl  gen_defs.tcl  gen_exports.tcl  gen_funcs.tcl  gen_types.tcl
	$(TCLSH) gen_code.tcl

include/GL/gl.h: include/GL/gl.h.template gen_gl_h.sh
	./gen_gl_h.sh include/GL/gl.h.template $@

$(BUILD_DIR)/glxinfo: tests/glxinfo/glxinfo.c $(BUILD_DIR)/libGL.1.2.dylib
	$(CC) tests/glxinfo/glxinfo.c $(INCLUDE) -L$(X11_DIR)/lib -lX11 $(BUILD_DIR)/libGL.1.2.dylib -o $@

$(BUILD_DIR)/glxgears: tests/glxgears/glxgears.c $(BUILD_DIR)/libGL.1.2.dylib
	$(CC) tests/glxgears/glxgears.c $(INCLUDE) -L$(X11_DIR)/lib -lX11 $(BUILD_DIR)/libGL.1.2.dylib -o $@

install_headers:
	$(INSTALL) -d $(DESTDIR)$(INSTALL_DIR)/include/GL
	$(INSTALL) -m 644 include/GL/gl.h include/GL/glext.h include/GL/glx.h include/GL/glxext.h $(DESTDIR)$(INSTALL_DIR)/include/GL

install_programs: programs
	$(INSTALL) -d $(DESTDIR)$(INSTALL_DIR)/bin
	$(INSTALL) -m 755 $(PROGRAMS) $(DESTDIR)$(INSTALL_DIR)/bin

install_libraries: $(BUILD_DIR)/libGL.1.2.dylib
	$(INSTALL) -d $(DESTDIR)$(INSTALL_DIR)/lib
	$(INSTALL) -m 755 $(BUILD_DIR)/libGL.1.2.dylib $(DESTDIR)$(INSTALL_DIR)/lib
	$(RM) -f $(DESTDIR)$(INSTALL_DIR)/lib/libGL.dylib
	$(LN) -s libGL.1.2.dylib $(DESTDIR)$(INSTALL_DIR)/lib/libGL.dylib
	$(RM) -f $(DESTDIR)$(INSTALL_DIR)/lib/libGL.1.dylib
	$(LN) -s libGL.1.2.dylib $(DESTDIR)$(INSTALL_DIR)/lib/libGL.1.dylib

install: install_headers install_libraries

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(TEST_BUILD_DIR)
	rm -f *.o *.a
	rm -f *.c~ *.h~
	rm -f apple_xgl_api.h apple_xgl_api.c
	rm -f *.dylib
	rm -f include/GL/gl.h
