#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <GL/glx.h>

int main(int argc, char *argv[]) {
    Display *dpy;
    int eventbase, errorbase;
    int major, minor;

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

    return EXIT_SUCCESS;
}
