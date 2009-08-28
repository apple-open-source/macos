#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <GL/glx.h>

Window root, win, win2;
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
    XWindowAttributes wattr;

    while(1) {
	XNextEvent(dpy, &event);

	switch(event.type) {
	case ButtonPress:
	    printf("event.xbutton.window 0x%lx\n",
		   event.xbutton.window);
	    XDestroyWindow(dpy, event.xbutton.window);
	    break;

	case Expose:
	    glXMakeCurrent(dpy, event.xexpose.window, ctx);
	    XGetWindowAttributes(dpy, event.xexpose.window, &wattr);
	    resize(dpy, event.xexpose.window, wattr.width, wattr.height);
	    draw(dpy, event.xexpose.window);
	    break;
	    	
	case KeyPress: {	    
	    glXMakeCurrent(dpy, win2, ctx);
	    XGetWindowAttributes(dpy, win2, &wattr);
	    resize(dpy, win2, wattr.width, wattr.height);
	    draw(dpy, win2);
	    XFlush(dpy);
	}
	    break;
    
	case ConfigureNotify:
	    glXMakeCurrent(dpy, event.xconfigure.window, ctx);
	    resize(dpy, event.xconfigure.window, event.xconfigure.width,
		   event.xconfigure.height);
	    break;
	}	
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
    attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask
	| ButtonPressMask;

    win = XCreateWindow(dpy, root, /*x*/ 0, /*y*/ 0, 
			/*width*/ 400, /*height*/ 400,
			0, visinfo->depth, InputOutput,
			visinfo->visual, 
			CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
			&attr);
   
    win2 = XCreateWindow(dpy, win, 0, 0,
			 100, 100,
			 0, visinfo->depth, InputOutput,
			 visinfo->visual,
			 CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
			 &attr);

    printf("win 0x%lx win2 0x%lx\n", win, win2);

    ctx = glXCreateContext(dpy, visinfo, NULL, True);
    if(!ctx) {
	fprintf(stderr, "error: glXCreateContext failed!\n");
	return EXIT_FAILURE;
    }
    
    XMapWindow(dpy, win);
    XMapWindow(dpy, win2);

    glXMakeCurrent(dpy, win, ctx);

    event_loop(dpy);

    return EXIT_SUCCESS;
}
