/* Test case for a CGL leak that seems to involve glRectf and only occurs
 * with CGLSetOffScreen (from what I've seen so far).
 * Author: George Staplin. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/OpenGL.h>

int main(int argc, char *argv[]) {
    CGLPixelFormatAttribute attr[60];
    CGLPixelFormatObj pfobj;
    CGLContextObj ctxobj;
    int numattr = 0;
    GLint vsref = 0;
    CGLError error;
    int width, height, pitch;
    void *ptr;
    double rcolor, gcolor, bcolor;
    
    attr[numattr++] = kCGLPFAOffScreen;
    attr[numattr++] = kCGLPFAColorSize;
    attr[numattr++] = 32;

#if 0
    attr[numattr++] = kCGLPFAColorSize;
    attr[numattr++] = 24;
#endif
    attr[numattr++] = kCGLPFAAlphaSize;
    attr[numattr++] = 8;
    attr[numattr++] = kCGLPFAAccumSize;
    attr[numattr++] = 32;
    attr[numattr++] = 0;

    width = 300;
    height = 300;
    pitch = width * 4;
    
    ptr = malloc(pitch * height);
    if(NULL == ptr)
	abort();

    rcolor = 0.0;
    gcolor = 0.0;
    bcolor = 0.0;
    
#if 0
    while(1) {
	error = CGLChoosePixelFormat(attr, &pfobj, &vsref);
	
	if(kCGLNoError != error)
	    abort();
	 
	error = CGLCreateContext(pfobj, NULL, &ctxobj);
	
	if(kCGLNoError != error)
	    abort();
	
	error = CGLSetCurrentContext(ctxobj);
	
	if(kCGLNoError != error)
	    abort();
	
	CGLSetCurrentContext(NULL);	
	CGLDestroyPixelFormat(pfobj);
	CGLDestroyContext(ctxobj);
    }

#else
    /* This is the leak path with glRectf. */
    while(1) {
	 error = CGLChoosePixelFormat(attr, &pfobj, &vsref);
	 
	 if(kCGLNoError != error)
	     abort();
	 
	 error = CGLCreateContext(pfobj, NULL, &ctxobj);
	 
	 if(kCGLNoError != error)
	     abort();

   	error = CGLSetCurrentContext(ctxobj);
	
	if(kCGLNoError != error)
	    abort();

	error = CGLSetOffScreen(ctxobj, width, height, pitch, ptr);
	
	if(kCGLNoError != error)
	    abort();

	glShadeModel( GL_FLAT );
	glClearColor( 0.5, 0.5, 0.5, 1.0 );
	glClear( GL_COLOR_BUFFER_BIT );
	glViewport( 0, 0, 300, 300 );
	glOrtho( -1.0, 1.0, -1.0, 1.0, -1.0, 1.0 );
	glColor3f( rcolor, gcolor, bcolor);
	/* The glRectf makes the whole process grow RPRVT in Leopard. */
	/* Try to comment this out and watch how the bug goes away: */
	glRectf( -0.75, -0.75, 0.75, 0.75 );

	rcolor += 0.01;
	gcolor += 0.02;
	bcolor += 0.03;
	
	if(rcolor >= 1.0)
           rcolor = 0.0;
	
	if(gcolor >= 1.0)
	    gcolor = 0.0;

	if(bcolor >= 1.0)
	    bcolor = 0.0;

	CGLClearDrawable(ctxobj);

	CGLSetCurrentContext(NULL);
	CGLDestroyPixelFormat(pfobj);
	CGLDestroyContext(ctxobj);
	
	printf("r %g b %g b %g\n", rcolor, gcolor, bcolor);
    }
#endif

    return EXIT_FAILURE;    
}
