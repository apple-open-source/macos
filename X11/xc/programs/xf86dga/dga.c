/* $XFree86: xc/programs/xf86dga/dga.c,v 3.20 2002/05/31 18:46:11 dawes Exp $ */

#include <X11/Xos.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Scrollbar.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xmu/StdSel.h>
#include <X11/Xmd.h>
#include <X11/extensions/xf86dga.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>



#define MINMAJOR 0
#define MINMINOR 0

/* copied from xf86Io.c */
static int
GetTimeInMillis(void)
{
    struct timeval  tp;

    gettimeofday(&tp, 0);
    return(tp.tv_sec * 1000) + (tp.tv_usec / 1000);
}

int
main(int argc, char *argv[])
{
    int MajorVersion, MinorVersion;
    int EventBase, ErrorBase;
    Display *dis;
    int i, bpp;
    char *addr;
    int width, bank, banks, ram;
    XEvent event;
    Colormap cmap = 0;
    Visual *vis;
    int flags;

    if (geteuid()) {
	fprintf(stderr, "Must be suid root\n");
	exit(1);
    }

    if ( (dis = XOpenDisplay(NULL)) == NULL )
    {
        (void) fprintf( stderr, " cannot connect to X server %s\n",
                       XDisplayName(NULL));
        exit( -1 );
    }


    if (!XF86DGAQueryVersion(dis, &MajorVersion, &MinorVersion))
    { 
        fprintf(stderr, "Unable to query video extension version\n");
        exit(2);
    }

    if (!XF86DGAQueryExtension(dis, &EventBase, &ErrorBase)) {
        fprintf(stderr, "Unable to query video extension information\n");
        exit(2);
    }

    /* Fail if the extension version in the server is too old */
    if (MajorVersion < MINMAJOR ||
        (MajorVersion == MINMAJOR && MinorVersion < MINMINOR)) {
        fprintf(stderr,
                "Xserver is running an old XFree86-DGA version"
                " (%d.%d)\n", MajorVersion, MinorVersion);
        fprintf(stderr, "Minimum required version is %d.%d\n",
                MINMAJOR, MINMINOR);
        exit(2);
    }

    XF86DGAQueryDirectVideo(dis, DefaultScreen(dis), &flags);
    if (!(flags & XF86DGADirectPresent)) {
      fprintf(stderr, "Xserver driver doesn't support DirectVideo on screen %d\n", DefaultScreen(dis) );
      exit(2);
    }
	
   XGrabKeyboard(dis, DefaultRootWindow(dis), True, GrabModeAsync,
		 GrabModeAsync,  CurrentTime);

   /* and all the mouse moves */
   XGrabPointer(dis, DefaultRootWindow(dis), True,
		PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
		GrabModeAsync, GrabModeAsync, None,  None, CurrentTime);
   /* we want _our_ cmap */
   vis = DefaultVisual(dis, DefaultScreen(dis));
   
   bpp = DisplayPlanes (dis, DefaultScreen(dis));
   

   if (bpp <= 8)
   {
   cmap = XCreateColormap(dis, DefaultRootWindow(dis), vis, AllocAll);

   for (i = 0; i < 256; i++) {
       XColor xcol;

       xcol.pixel = i;
       xcol.red = (256 - i) * 255;
       xcol.green = i * 255;
       xcol.blue = ((128 - i)%256) * 255;
       xcol.flags = DoBlue | DoGreen | DoRed;
       XStoreColor(dis, cmap, &xcol);
   }
   }

   /*
    * Lets go live
    */

   XF86DGAGetVideo(dis, DefaultScreen(dis), &addr, &width, &bank, &ram);
   fprintf(stderr, "%x addr:%p, width %d, bank size %d, depth %d planes\n", True,
	   addr, width, bank, bpp);

   XF86DGADirectVideo(dis, DefaultScreen(dis),
			   XF86DGADirectGraphics|
			   XF86DGADirectMouse|
			   XF86DGADirectKeyb);

   if (bpp <= 8)
   {
   /* must be called _after_ entering DGA DirectGraphics mode */
   XF86DGAInstallColormap(dis, DefaultScreen(dis), cmap);
   }


#ifndef __UNIXOS2__
   /* Give up root privs */
   setuid(getuid());
#endif

   XF86DGASetViewPort(dis, DefaultScreen(dis), 0, 0);

   banks = (ram * 1024)/bank;
#ifdef DEBUG
   fprintf(stderr, "%x ram:%x, addr %x, banks %d\n", True,
	   ram, addr, banks);
#endif
   while (1) {
      XMotionEvent *mevent = (XMotionEvent *) &event;
      XButtonEvent *bevent = (XButtonEvent *) &event;
      int n_chars = 0;
      char buf[21];
      KeySym ks = 0;
      
      i = 0;
      XNextEvent(dis, &event);
      switch (event.type) {
       case KeyPress:
	 n_chars = XLookupString(&event.xkey, buf, 20, &ks, NULL);
         buf[n_chars] = '\0';
	 fprintf(stderr,"KeyPress [%d]: %s\n", event.xkey.keycode, buf);

	 if (buf[0] == 'b') {
	   /*
	    * Benchmark mode: run write/read speed test for 1 second each
	    */
	    int start_clock,finish_clock,diff_clock;
	    int cycles;
	    int size = 64;
	    void *membuf;

	    if (bank < 65536)
		size = bank / 1024;

	    /* get write timings */
	    XF86DGASetVidPage(dis, DefaultScreen(dis), i);

	    start_clock = GetTimeInMillis(); finish_clock = start_clock;

	    cycles=0;
	    while ((finish_clock - start_clock) < 1000)
	    {
	      cycles++;
	      memset(addr, (char) (cycles % 255), size * 1024);
	      finish_clock = GetTimeInMillis();
	    }

	    diff_clock = finish_clock - start_clock;

	    fprintf(stderr, "Framebuffer write speed: %6dK/s\n",
		    (size * 1000 * cycles) / diff_clock);

	    /* get read timings */
	    if ((membuf=malloc(size*1024)))
	    {
		XF86DGASetVidPage(dis, DefaultScreen(dis), i);

		start_clock = GetTimeInMillis(); finish_clock = start_clock; 

	        cycles=0;
	        while ((finish_clock - start_clock) < 1000)
	        {
	            cycles++;
		    memcpy(membuf, addr, size * 1024);
	            finish_clock = GetTimeInMillis();
	        }

		diff_clock = finish_clock - start_clock;

		fprintf(stderr, "Framebuffer read speed: %6dK/s\n",
		    (size * 1000 * cycles) / diff_clock);
	    }
	    else
	      fprintf(stderr, "could not allocate scratch buffer -- read timing measurement skipped\n");
	 }

         for (i = 0; i < banks; i++) {
		XF86DGASetVidPage(dis, DefaultScreen(dis), i);
		memset(addr, buf[0], bank);
#ifdef DEBUG
   fprintf(stderr, "XF86DGASetVidPage(dis, DefaultScreen(dis), %d);\n",i);
   fprintf(stderr, "memset(addr:%x, buf[0]:%d, bank:%d);\n",addr,buf[0],bank);
#endif
	 }
	 break;
       case KeyRelease:
	 n_chars = XLookupString(&event.xkey, buf, 20, &ks, NULL);
         buf[n_chars] = '\0';
	 fprintf(stderr,"KeyRelease[%d]: %s\n", event.xkey.keycode, buf);
	 break;
       case MotionNotify:
 	 fprintf(stderr,"Relative Motion: %d %d\n", mevent->x_root, mevent->y_root);
         break;
       case ButtonPress:
	 fprintf(stderr,"Button %d pressed\n", bevent->button);
	 break;
       case ButtonRelease:
	 fprintf(stderr,"Button %d released\n", bevent->button);
	 break;
       case Expose:
	 /* maybe we should RaiseWindow? */
	 break;
       default:
	 fprintf(stderr,"%X\n", event.type);
	 break;
      }
      if (n_chars && (buf[0] == 'q' || buf[0] == 'Q')) {
	 fprintf(stderr,"EXITTING\n");
         break;
      }
   }
   /*
    * back to the X server
    */
   XF86DGADirectVideo(dis, DefaultScreen(dis), 0);
   fprintf(stderr, "back now in X\n");

   /* and give back control */
   XUngrabPointer(dis, CurrentTime);
   XUngrabKeyboard(dis, CurrentTime);
   fprintf(stderr, "Thats all folks\n");
   exit(0);
}


