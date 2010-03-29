#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <GL/glx.h>

Window mainwin;
GLXContext ctx;

void draw(Display *dpy, Window w) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();   
    glColor3f(0.5f, 0.5f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f( 0.0f, 1.0f, 0.0f);
    glVertex3f(-1.0f,-1.0f, 0.0f);
    glVertex3f( 1.0f,-1.0f, 0.0f);
    glEnd();    
    glXSwapBuffers(dpy, w);
}

void resize(Display *dpy, Window w, int width, int height) {
    glViewport(0, 0, width, height);
    draw(dpy, w);
}

void event_loop(Display *dpy) {
    XEvent event;
    
    while(1) {
	if(XPending(dpy) > 0) {
	    XNextEvent(dpy, &event);
	    
	    switch(event.type) {
	    case Expose:
		draw(dpy, event.xexpose.window);
		break;
		
	    case ConfigureNotify:
		resize(dpy, event.xconfigure.window, event.xconfigure.width,
		       event.xconfigure.height);
		break;
	    }	
	}

	draw(dpy, mainwin);
	/* This should destroy the surface. */
	glXMakeCurrent(dpy, None, ctx);
	/* This should recreate the surface. */
	glXMakeCurrent(dpy, mainwin, ctx);
    }
}

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

    mainwin = win = XCreateWindow(dpy, root, /*x*/ 0, /*y*/ 0, 
			/*width*/ 400, /*height*/ 400,
			0, visinfo->depth, InputOutput,
			visinfo->visual, 
			CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
			&attr);
   
    ctx = glXCreateContext(dpy, visinfo, NULL, True );
    if(!ctx) {
	fprintf(stderr, "error: glXCreateContext failed!\n");
	return EXIT_FAILURE;
    }
    
    XMapWindow(dpy, win);

    glXMakeCurrent(dpy, win, ctx);

    event_loop(dpy);

    return EXIT_SUCCESS;
}
