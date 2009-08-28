#include <stdio.h>
#include <stdlib.h>
#include <OpenGL/OpenGL.h>

int main() {
	CGLPixelFormatAttribute attribs[] = {
                kCGLPFAAccelerated,
                kCGLPFADoubleBuffer,
                kCGLPFAColorSize, 24,
		0
	};	
	
	CGLPixelFormatObj pixobj;
	GLint npix;
	CGLError err;
	CGLContextObj context;
	GLint maxview[2];

	err = CGLChoosePixelFormat(attribs, &pixobj, &npix);

	if(err != kCGLNoError) {
		fprintf(stderr, "choose pixel format error!\n");
		return EXIT_FAILURE;
	}

	err = CGLCreateContext(pixobj, NULL, &context);
	
	if(err != kCGLNoError) {
		fprintf(stderr, "create context error!\n");
		return EXIT_FAILURE;
	}

	CGLSetCurrentContext(context);

	maxview[0] = 0;
	maxview[1] = 0;
	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, maxview);

	printf("max pbuffer width %d heigth %d\n", maxview[0], maxview[1]);

	return EXIT_SUCCESS;
}
