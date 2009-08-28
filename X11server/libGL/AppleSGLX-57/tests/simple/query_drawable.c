#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <GL/glx.h>

int main(int argc, char *argv[]) {
    Display *dpy;
    int eventbase, errorbase;
    int major, minor;
    Window root;
    unsigned int width, height;

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

    root = DefaultRootWindow(dpy);

    glXQueryDrawable(dpy, root, GLX_WIDTH, &width);

    printf("query 1: width %d\n", width);

    glXQueryDrawable(dpy, root, GLX_HEIGHT, &height);

    printf("query 2: height %d\n", height);

    return EXIT_SUCCESS;
}
