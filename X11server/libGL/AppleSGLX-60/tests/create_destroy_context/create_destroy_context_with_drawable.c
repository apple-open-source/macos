#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <unistd.h>

int main() {
    Display *dpy;
    int attrib[] = { GLX_RGBA,
		     GLX_RED_SIZE, 8,
		     GLX_GREEN_SIZE, 8,
		     GLX_BLUE_SIZE, 8,
		     GLX_DEPTH_SIZE, 24,
		     GLX_DOUBLEBUFFER,
		     None };
    int eventbase, errorbase;
    int screen;
    Window root, win;
    XVisualInfo *visinfo;
    XSetWindowAttributes attr;
    GLXContext ctx;
    int iter;
    unsigned int p;

    dpy = XOpenDisplay(NULL);
    
    if(NULL == dpy) {
        fprintf(stderr, "error: unable to open display!\n");
        return EXIT_FAILURE;
    }
    
    if(!glXQueryExtension(dpy, &eventbase, &errorbase)) {
        fprintf(stderr, "GLX is not available!\n");
        return EXIT_FAILURE;
    }
    
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    visinfo = glXChooseVisual(dpy, screen, attrib);

    if(!visinfo) {
	fprintf(stderr, "error: couldn't get an RGBA, double-buffered visual!\n");
	return EXIT_FAILURE;
    }

    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
    attr.event_mask = StructureNotifyMask | ExposureMask;

    win = XCreateWindow(dpy, root, /*x*/ 0, /*y*/ 0, 
			/*width*/ 400, /*height*/ 400,
			0, visinfo->depth, InputOutput,
			visinfo->visual, 
			CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
			&attr);

    p = (unsigned int)getpid();
    printf("pid %u\n", p);

    while(1) {
	ctx = glXCreateContext(dpy, visinfo, NULL, True );
	if(!ctx) {
	    fprintf(stderr, "error: glXCreateContext failed!\n");
	    return EXIT_FAILURE;
	}
	
	glXMakeCurrent(dpy, win, ctx);

	/* We can't destroy a context if it's still current according to the
	 * documentation.
	 */
	glXMakeCurrent(dpy, None, NULL);

    	glXDestroyContext(dpy, ctx);

	sleep(1);
    }

    return EXIT_SUCCESS;
}
