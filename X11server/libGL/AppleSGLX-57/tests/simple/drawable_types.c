#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <GL/glx.h>

int main(int argc, char *argv[]) {
    Display *dpy;
    int eventbase, errorbase;
    int major, minor;
    GLXFBConfig *fbconfigs;
    int i, numfbconfigs;
    
    dpy = XOpenDisplay(NULL);

    if(NULL == dpy) {
	fprintf(stderr, "error: unable to open display!\n");
	return EXIT_FAILURE;
    }

    if(!glXQueryExtension(dpy, &eventbase, &errorbase)) {
	fprintf(stderr, "GLX is not available!\n");
	return EXIT_FAILURE;
    }

    printf("GLX eventbase %d errorbase %d\n", eventbase, errorbase);

    if(!glXQueryVersion(dpy, &major, &minor)) {
	fprintf(stderr, "GLX version query error!\n");
	return EXIT_FAILURE;
    }

    printf("GLX version: %d.%d\n", major, minor);

    fbconfigs = glXGetFBConfigs(dpy, DefaultScreen(dpy), &numfbconfigs);
  
    if(NULL == fbconfigs) {
	fprintf(stderr, "%s: failed to get GLXFBConfigs!\n", __func__);
	return EXIT_FAILURE;
    }

    for(i = 0; i < numfbconfigs; ++i) {
	int r, value;
	
	r = glXGetFBConfigAttrib(dpy, fbconfigs[i], GLX_DRAWABLE_TYPE, &value);
	
	printf("fbconfigs[%d] GLX_DRAWABLE_TYPE is %x supporting: %s %s %s\n",
	       i, value,
	       (value & GLX_WINDOW_BIT) ? "GLX_WINDOW_BIT" : "",
	       (value & GLX_PIXMAP_BIT) ? "GLX_PIXMAP_BIT" : "",
	       (value & GLX_PBUFFER_BIT) ? "GLX_PBUFFER_BIT" : "");
    }

    return EXIT_SUCCESS;
}
