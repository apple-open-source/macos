/*---------------------------------------------------------------------------

    mac_xor

    This program demonstrates a MacOS-specific problem with Xlib XOR 
    rendering in an OpenGL window. In this problem, Xlib graphics rendered 
    with function XOR are not displayed in a window with an OpenGL context 
    (glXMakeCurrent).

    This program creates windows and OpenGL contexts in approximately the
    same manner as NX, and the same problem with Xlib XOR rendering affects
    NX. NX has used this approach successfully for many years on diverse 
    Xlib/OpenGL platforms, but this problem occurs on MacOS only.

    This program can be compiled on MacOS with the following command:

gcc -o mac_xor -I/usr/X11/include mac_xor.c -Wl,-dylib_file,\
/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib:\
/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib \
-L/usr/X11/lib -lX11 -lGL

    Here is how to demonstrate the problem.

    a.  Compile the program.

    b.  Start the program by invoking "mac_xor" from a shell window.

    c.  A window titled "XOR Test Program" is displayed.

    d.  Move the mouse cursor over the black background of the window. A
        white vertical line should follow the mouse, but you will see
        nothing. (The line is displayed with XOR rendering.)

    e.  Terminate the program.

    f.  Edit this file to comment-out the if-block that calls glXMake-
        Current. Then recompile the program and repeat the test. Observe 
        that the white vertical line is now displayed.

---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <GL/glu.h>

#define FALSE    0
#define TRUE     1
/* Global X Variables */

Display   *display;
GC         gc;
Window     window;
int        firstDraw = 1; 

/* Forward Declarations */

void CreateWindow(void);

static Window CreateChildWindow(
    Display    *display,
    Screen     *screen,
    Window     parent_window);

static int find_best_visual(
    Display        *display, 
    int            screen_number,
    XVisualInfo    *return_visual_info);

void DrawWindow(
    int    x,
    int    y);

void ProcessEvents(void);


main(

int     argc,
char    *argv[])

{
    CreateWindow();

    while (True) 
    {
        ProcessEvents();
    }
}


void CreateWindow(void)

{
    unsigned long         attributeMask;
    XSetWindowAttributes  attributes;
    int                   depth;
    Window                parentWindow;
    int                   screen;
    XGCValues             values;
    Visual*               visual;
    XVisualInfo*          visualList;
    int                   numVisuals;
 
    if ((display = XOpenDisplay (NULL)) == NULL) 
    {
        printf("Cannot connect to server %s; quitting.\n",
           XDisplayName (NULL));

        exit(-1);
    }

    screen = DefaultScreen(display);

    parentWindow = XCreateSimpleWindow(display, 
                                       RootWindow(display, screen),
                                       0, 0, 800, 600, 4,
                                       BlackPixel(display, screen), 
                                       BlackPixel(display, screen));
    XSelectInput(display, 
                 parentWindow, 
                 ButtonPressMask | 
                 PointerMotionMask);

    XMapWindow(display, parentWindow);
    XStoreName(display, parentWindow, "XOR Test Program");

    window = CreateChildWindow(display, 
                               ScreenOfDisplay(display, screen), 
                               parentWindow);

    gc = XCreateGC(display, 
                   window, 
                   (unsigned long)NULL,
                   (XGCValues *)&values);

    XSetFunction(display, gc, GXxor);
    XSetForeground(display, gc, AllPlanes);

    XMapWindow(display, window);
    XFlush (display);
}
    

void DrawWindow(

int    x,
int    y)

{
    static int    old_x; 

    if (firstDraw == 1) 
    {
         firstDraw = 0;
    }
    else 
    {
         XDrawLine(display, window, gc, old_x, 1000, old_x, 0);
    }

    old_x = x;
    XDrawLine(display, window, gc, old_x, 1000, old_x, 0);
    XSync (display, 0);

    return;
}


void ProcessEvents (void)

{
    XButtonEvent*    buttonEvent;
    XMotionEvent*    motionEvent;
    XEvent           theEvent;
    
    if (XPending (display)) {
        XNextEvent (display, &theEvent);
     
        switch (theEvent.type) {
           case MotionNotify:
              motionEvent = (XMotionEvent *)&theEvent;
              DrawWindow(motionEvent->x, motionEvent->y);
              break;

           case ButtonPress:
              buttonEvent = (XButtonEvent *) &theEvent;
              switch (buttonEvent->button) {

                 case Button1:
                    XClearWindow(display, window);
                    firstDraw = 1; 
                    break;

                 case Button2:
                    XClearWindow(display, window);
                    firstDraw = 1; 
                    break;

                 case Button3:
                    exit (-1);
                    break;

                 default:
                    break;
              }
              break;
           default:
              break;
        }  
 
    }

    return;
}


static Window CreateChildWindow(

Display    *display,
Screen     *screen,
Window     parent_window)

{
    unsigned long           attribute_mask;
    Window                  child_window;
    GLXContext              context;
    Colormap                colormap;
    Window                  root_window;
    XWindowAttributes       screen_attributes;
    XVisualInfo             visual_info;
    XSetWindowAttributes    window_attributes;
    Window                  windows[2];

    if (find_best_visual(display,
                         XScreenNumberOfScreen(screen),
                         &visual_info) == FALSE)
    {
        printf("Unable to find suitable visual.\n");
        exit(-1);
    }

    /*  Create the colormap.  */

    colormap = XCreateColormap(display, 
                               parent_window,
                               visual_info.visual, 
                               AllocNone);

    /*  Set attributes for the graphics window and then create the 
        window.  */

    window_attributes.background_pixel = BlackPixelOfScreen(screen);
    window_attributes.border_pixel     = BlackPixelOfScreen(screen);
    window_attributes.colormap         = colormap;
    window_attributes.event_mask       = ExposureMask;
    window_attributes.backing_store    = NotUseful;
    window_attributes.win_gravity      = NorthWestGravity;

    attribute_mask = CWBackPixel    |
                     CWBorderPixel  |
                     CWColormap     |
                     CWEventMask    |
                     CWBackingStore |
                     CWWinGravity;

    root_window = RootWindowOfScreen(screen), 

    XGetWindowAttributes(display, root_window, &screen_attributes);

    child_window = XCreateWindow(display, 
                                 root_window, 
                                 0,
                                 0,
                                 screen_attributes.width,
                                 screen_attributes.height,
                                 0, 
                                 visual_info.depth,  
                                 (unsigned int)CopyFromParent, 
                                 visual_info.visual,
                                 attribute_mask,
                                 &window_attributes);

    windows[0] = child_window;
    windows[1] = parent_window; 
    XSetWMColormapWindows(display, windows[1], windows, 2);

    XReparentWindow(display, child_window, parent_window, 0,0);
    XMapWindow(display, child_window);

    /*  Create an Open rendering context and make it current.  */

    context = glXCreateContext(display, &visual_info, NULL, GL_TRUE); 
    if (context == NULL)
    {
        printf("Unable to create OpenGL rendering context.\n");
        exit(-1);
    }

    if (glXMakeCurrent(display, child_window, context) == FALSE) 
    {
        printf("Unable to make OpenGL rendering context current.\n");
        exit(-1);
    }

    return child_window;
}


static int find_best_visual(

Display        *display, 
int            screen_number,
XVisualInfo    *return_visual_info)

{
#define LAST_REQUIRED_ATTRIBUTE    9

    int            last_attribute;
    int            success;
    XVisualInfo    *visual_info;

    static int visual_attributes[32] = 
    {
        GLX_RGBA,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_DEPTH_SIZE, 1,
    };

    success = FALSE;

    last_attribute = LAST_REQUIRED_ATTRIBUTE;
    
    visual_attributes[last_attribute++] = GLX_DOUBLEBUFFER;
    visual_attributes[last_attribute]   = None;

    /*  Request double buffering.  */

    visual_info = glXChooseVisual(display, 
                                  screen_number, 
                                  visual_attributes);

    if (visual_info == NULL)
         last_attribute--;
    
    /*  Request a stencil buffer.  */

    visual_attributes[last_attribute++] = GLX_STENCIL_SIZE;
    visual_attributes[last_attribute++] = 1;
    visual_attributes[last_attribute]   = None;
    visual_info = glXChooseVisual(display, 
                                  screen_number, 
                                  visual_attributes);

    if (visual_info == NULL)
    {
        last_attribute -= 2;
        visual_attributes[last_attribute] = None;
        visual_info = glXChooseVisual(display, 
                                      screen_number, 
                                      visual_attributes);
    }

    if (visual_info != NULL)
    {
        *return_visual_info = *visual_info;
        XFree(visual_info);
        success = TRUE;
    }

    return(success);
}
