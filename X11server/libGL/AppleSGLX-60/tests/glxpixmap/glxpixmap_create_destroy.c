

/*
 * A demonstration of using the GLXPixmap functions.  This program is in
 * the public domain.
 *
 * Brian Paul
 */


#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static GLXContext ctx;
static XVisualInfo *visinfo;
static GC gc;
static GC rectgc;

static Window make_rgb_window( Display *dpy,
				  unsigned int width, unsigned int height )
{
   const int sbAttrib[] = { GLX_RGBA,
                            GLX_RED_SIZE, 1,
                            GLX_GREEN_SIZE, 1,
                            GLX_BLUE_SIZE, 1,
                            None };
   const int dbAttrib[] = { GLX_RGBA,
                            GLX_RED_SIZE, 1,
                            GLX_GREEN_SIZE, 1,
                            GLX_BLUE_SIZE, 1,
                            GLX_DOUBLEBUFFER,
                            None };
   int scrnum;
   XSetWindowAttributes attr;
   unsigned long mask;
   Window root;
   Window win;

   scrnum = DefaultScreen( dpy );
   root = RootWindow( dpy, scrnum );

   visinfo = glXChooseVisual( dpy, scrnum, (int *) sbAttrib );
   if (!visinfo) {
      visinfo = glXChooseVisual( dpy, scrnum, (int *) dbAttrib );
      if (!visinfo) {
         printf("Error: couldn't get an RGB visual\n");
         exit(1);
      }
   }

   /* window attributes */
   attr.background_pixel = 0;
   attr.border_pixel = 0;
   /* TODO: share root colormap if possible */
   attr.colormap = XCreateColormap( dpy, root, visinfo->visual, AllocNone);
   attr.event_mask = StructureNotifyMask | ExposureMask;
   mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

   win = XCreateWindow( dpy, root, 0, 0, width, height,
		        0, visinfo->depth, InputOutput,
		        visinfo->visual, mask, &attr );

   /* make an X GC so we can do XCopyArea later */
   gc = XCreateGC( dpy, win, 0, NULL );

   /* need indirect context */
   ctx = glXCreateContext( dpy, visinfo, NULL, False );
   if (!ctx) {
      printf("Error: glXCreateContext failed\n");
      exit(-1);
   }

   printf("Direct rendering: %s\n", glXIsDirect(dpy, ctx) ? "Yes" : "No");

   return win;
}


static GLXPixmap make_pixmap( Display *dpy, Window win,
			       unsigned int width, unsigned int height,
                               Pixmap *pixmap)
{
   Pixmap pm;
   GLXPixmap glxpm;
   XWindowAttributes attr;

   pm = XCreatePixmap( dpy, win, width, height, visinfo->depth );
   if (!pm) {
      printf("Error: XCreatePixmap failed\n");
      exit(-1);
   }

   XGetWindowAttributes( dpy, win, &attr );

   printf("pm %lx\n", pm);
   /*
    * IMPORTANT:
    *   Use the glXCreateGLXPixmapMESA funtion when using Mesa because
    *   Mesa needs to know the colormap associated with a pixmap in order
    *   to render correctly.  This is because Mesa allows RGB rendering
    *   into any kind of visual, not just TrueColor or DirectColor.
    */
#ifdef GLX_MESA_pixmap_colormap
   if (strstr(glXQueryExtensionsString(dpy, 0), "GLX_MESA_pixmap_colormap")) {
      /* stand-alone Mesa, specify the colormap */
      glxpm = glXCreateGLXPixmapMESA( dpy, visinfo, pm, attr.colormap );
   }
   else {
      glxpm = glXCreateGLXPixmap( dpy, visinfo, pm );
   }
#else
   /* This will work with Mesa too if the visual is TrueColor or DirectColor */
   glxpm = glXCreateGLXPixmap( dpy, visinfo, pm );
#endif

   if (!glxpm) {
      printf("Error: GLXCreateGLXPixmap failed\n");
      exit(-1);
   }

   *pixmap = pm;

   return glxpm;
}



static void event_loop( Display *dpy, GLXPixmap pm )
{
   XEvent event;

   while (1) {
      XNextEvent( dpy, &event );

      switch (event.type) {
	 case Expose:
	    printf("Redraw\n");
	    /* copy the image from GLXPixmap to window */
	    XCopyArea( dpy, pm, event.xany.window,  /* src, dest */
		       gc, 0, 0, 300, 300,          /* gc, src pos, size */
		       0, 0 );                      /* dest pos */
	    break;
	 case ConfigureNotify:
	    /* nothing */
	    break;
      }
   }
}



int main( int argc, char *argv[] )
{
   Display *dpy;
   Window win;
   Pixmap pm;
   GLXPixmap glxpm;
   int eventbase, errorbase;
   double rcolor, gcolor, bcolor;
   XGCValues gcval;

   dpy = XOpenDisplay(NULL);

   if(NULL == dpy) {
       fprintf(stderr, "error: opening display\n");
       return EXIT_FAILURE;
   }

   if(!glXQueryExtension(dpy, &eventbase, &errorbase)) {
       fprintf(stderr, "GLX is not available!\n");
       return EXIT_FAILURE;
   }
   

   win = make_rgb_window( dpy, 300, 300 );
   
   XMapWindow(dpy, win);

   rcolor = 0.0;
   gcolor = 0.0;
   bcolor = 0.0;

   while(1) {
       glxpm = make_pixmap( dpy, win, 300, 300, &pm );

       printf("glxpm 0x%lx\n", glxpm);

       if(!glXMakeCurrent(dpy, glxpm, ctx)) {
	   fprintf(stderr, "glXMakeCurrent failed!\n");
	   return EXIT_FAILURE;
       }
       
       printf("GL_RENDERER: %s\n", (char *) glGetString(GL_RENDERER));

       /* Render an image into the pixmap */
       glShadeModel( GL_FLAT );
       glClearColor( 0.5, 0.5, 0.5, 1.0 );
       glClear( GL_COLOR_BUFFER_BIT );
       glViewport( 0, 0, 300, 300 );
       glOrtho( -1.0, 1.0, -1.0, 1.0, -1.0, 1.0 );
       glColor3f( rcolor, gcolor, bcolor);
       glRectf( -0.75, -0.75, 0.75, 0.75 );
       glFlush();
       
       glFinish();

       gcval.foreground = WhitePixel(dpy, DefaultScreen(dpy));
       
       rectgc = XCreateGC(dpy, pm, GCForeground, &gcval);

       XFillRectangle(dpy, pm, rectgc, 130, 130, 40, 40);
       
       XCopyArea( dpy, pm, win,
		  gc, 0, 0, 300, 300,          /* gc, src pos, size */
		  0, 0 );

       glXWaitX();

       XFreeGC(dpy, rectgc);
 
       glXMakeCurrent(dpy, None, ctx);

       glXDestroyGLXPixmap(dpy, glxpm);

       XFreePixmap(dpy, pm);

       rcolor += 0.01;
       gcolor += 0.02;
       bcolor += 0.03;

       if(rcolor >= 1.0)
	   rcolor = 0.0;

       if(gcolor >= 1.0)
	   gcolor = 0.0;

       if(bcolor >= 1.0)
	   bcolor = 0.0;
       
       usleep(5000);
   }

   /* NOT REACHED */
   event_loop( dpy, pm );

   return 0;
}
