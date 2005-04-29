#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
                                                                                
#include <stdio.h>
#include "gd.h"

/* A short program which converts a .gif file into a .gd file, for
	your convenience in creating images on the fly from a
	basis image that must be loaded quickly. The .gd format
	is not intended to be a general-purpose format. */

int main(int argc, char **argv)
{
#ifdef WITH_GIF
	gdImagePtr im;
	FILE *in, *out;
	if (argc != 3) {
		fprintf(stderr, "Usage: giftogd filename.gif filename.gd\n");
		exit(1);
	}
	in = fopen(argv[1], "rb");
	if (!in) {
		fprintf(stderr, "Input file does not exist!\n");
		exit(1);
	}
	im = gdImageCreateFromGif(in);
	fclose(in);
	if (!im) {
		fprintf(stderr, "Input is not in GIF format!\n");
		exit(1);
	}
	out = fopen(argv[2], "wb");
	if (!out) {
		fprintf(stderr, "Output file cannot be written to!\n");
		gdImageDestroy(im);
		exit(1);	
	}
	gdImageGd(im, out);
	fclose(out);
	gdImageDestroy(im);
#else
	fprintf (stderr, "GIF support is not available.\n");
#endif
}
