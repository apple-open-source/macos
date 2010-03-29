#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <GL/glx.h>

int main(int argc, char *argv[]) {
    Display *dpy;
    int eventbase, errorbase;
    int major, minor;
    GLXFBConfig *configs;
    int numconfigs, i;

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

    configs = glXGetFBConfigs(dpy, DefaultScreen(dpy), &numconfigs);
    if(NULL == configs) {
	fprintf(stderr, "error: retrieving GLXFBConfigs!\n");
	return EXIT_FAILURE;
    }

    for(i = 0; i < numconfigs; ++i) {
	int value;
	if(Success != glXGetFBConfigAttrib(dpy, configs[i], GLX_STEREO, &value)) {
	    fprintf(stderr, "unable to query GLX_STEREO!\n");
	    return EXIT_FAILURE;
	}
	
	printf("GLXFBConfig[%d] has stereo %d\n", i, value);
    }
    

    return EXIT_SUCCESS;
}
