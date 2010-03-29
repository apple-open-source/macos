

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


static GLXContext ctx;
static XVisualInfo *visinfo;
static GC gc;



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

int main( int argc, char *argv[] )
{
   Display *dpy;
   Window win;
   Pixmap pm;
   GLXPixmap glxpm;
   int eventbase, errorbase;

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

   fprintf(stderr, "This test should produce a GLXBadPixmap error.\n");

   glXDestroyPixmap(dpy, /*intentional*/ win);

   return 0;
}
