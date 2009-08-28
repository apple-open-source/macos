#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <GL/glx.h>

GLuint tlist, checkerlist;

void draw(Display *dpy, Window w) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();   
    glEnable(GL_MULTISAMPLE);

    glCallList(tlist);
    glXSwapBuffers(dpy, w);
}

void resize(Display *dpy, Window w, int width, int height) {
    glViewport(0, 0, width, height);
    draw(dpy, w);
}

void create_lists(void) {
    GLint sbufs, samples;
    int i;

    tlist = glGenLists(1);
    checkerlist = glGenLists(1);

    if(0 == tlist || 0 == checkerlist) {
	fprintf(stderr, "unable to generate lists!\n");
	abort();
    }

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glGetIntegerv(GL_SAMPLE_BUFFERS, &sbufs);
    glGetIntegerv(GL_SAMPLES, &samples);

    printf("sample buffers %d samples %d\n", sbufs, samples);

    glNewList(tlist, GL_COMPILE);
    
    for(i = 0; i < 20; ++i) {
	glPushMatrix();
	glRotatef(360.0f * (float) i / 20, 0.0f, 0.0f, 1.0f);
	glColor3f(1.0f, 0.5f, 0.5f);
	glLineWidth(0.1);
	glBegin(GL_LINES);
	glVertex2f(0.1, 0.5);
	glVertex2f(0.8, 0.4);
	glEnd();
	
	glColor3f(0.5f, 0.5f, 0.5f);
	glBegin(GL_TRIANGLES);
	glVertex2f(0.5, 0.0f);
	glVertex2f(0.9, 0.9f);
	glVertex2f(0.1, 0.9f);
	glEnd();

	glPopMatrix();
    }

    glEndList();
}

void event_loop(Display *dpy) {
    XEvent event;
    
    while(1) {
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
}

int main() {
    Display *dpy;
    int fbattrib[] = {
	GLX_DOUBLEBUFFER, True,
	GLX_RED_SIZE, 8,
	GLX_GREEN_SIZE, 8,
	GLX_BLUE_SIZE, 8,
	GLX_ALPHA_SIZE, 8,
	GLX_SAMPLE_BUFFERS, 1,
	GLX_SAMPLES, 1,
	None
    };

    int eventbase, errorbase;
    int screen;
    Window root, win;
    XVisualInfo *visinfo;
    XSetWindowAttributes attr;
    GLXContext ctx;
    GLXFBConfig *fbconfig;
    int i, numfbconfig, maxsample = 0;
    int bestfbi = 0, value;

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

    fbconfig = glXChooseFBConfig(dpy, screen, fbattrib, &numfbconfig);

    if(NULL == fbconfig) {
	fprintf(stderr, "error: choosing GLXFBConfig!\n");
	return EXIT_FAILURE;
    }
    
    for(i = 0; i < numfbconfig; ++i) {
	glXGetFBConfigAttrib(dpy, fbconfig[i], GLX_SAMPLES, &value);

	if(value > maxsample) {
	    bestfbi = i;
	    maxsample = value;
	}
    }

    glXGetFBConfigAttrib(dpy, fbconfig[bestfbi], GLX_SAMPLES, &value);
    printf("peak GLX_SAMPLES %d\n", value);

    visinfo = glXGetVisualFromFBConfig(dpy, fbconfig[bestfbi]);

    if (!visinfo) {
	fprintf(stderr, "error: couldn't get an RGBA, double-buffered visual!\n");
	return EXIT_FAILURE;
   }

    printf("visinfo->visualid %lx\n", visinfo->visualid);

    glXGetFBConfigAttrib(dpy, fbconfig[bestfbi], GLX_FBCONFIG_ID, &value);

    printf("fbconfig id 0x%x\n", value);
						    

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
   
    ctx = glXCreateNewContext(dpy, fbconfig[bestfbi], GLX_WINDOW_BIT,
			      NULL, True);

    if (!ctx) {
	fprintf(stderr, "error: glXCreateNewContext failed!\n");
	return EXIT_FAILURE;
    }
    
    XMapWindow(dpy, win);

    if(!glXMakeCurrent(dpy, win, ctx)) {
	fprintf(stderr, "error: making context current!\n");
	return EXIT_FAILURE;
    }

    printf("GL_RENDERER %s\n", (char *) glGetString(GL_RENDERER));

    glEnable(GL_MULTISAMPLE);

    create_lists();

    {
	GLint alpha, red, green;
	GLboolean dbuf;
	
	glGetIntegerv(GL_RED_BITS, &red);
	printf("glGetIntegerv(GL_RED_BITS) %d\n", red);

	glGetIntegerv(GL_GREEN_BITS, &green);
	printf("glGetIntegerv(GL_GREEN_BITS) %d\n", green);
	
	glGetIntegerv(GL_ALPHA_BITS, &alpha);
	printf("glGetIntegerv(GL_ALPHA_BITS) %d\n", alpha); 

	glGetBooleanv(GL_DOUBLEBUFFER, &dbuf);
	printf("glGetBooleanv(GL_DOUBLEBUFFER) %d\n", dbuf);

	GLint aux;
	glGetIntegerv(GL_AUX_BUFFERS, &aux);
	printf("aux buffers %d\n", aux);

	GLint ared;
	glGetIntegerv(GL_ACCUM_RED_BITS, &ared);
	printf("accum red bits %d\n", ared);
    }

    event_loop(dpy);

    return EXIT_SUCCESS;
}
