#include <stdio.h>
#include <stdlib.h>
#include <OpenGL/OpenGL.h>

static void print_extensions(const GLubyte *strings) {
	int n = 1;
	
	printf("extension ");

	for(; *strings; ++strings) {
		if(isspace(*strings))
			printf("\nextension");
	
		putchar(*strings);	
	}

	putchar('\n');
	fflush(stdout);
}

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

	//printf("%s\n", glGetString(GL_EXTENSIONS));

	print_extensions(glGetString(GL_EXTENSIONS));

	return EXIT_SUCCESS;
}
