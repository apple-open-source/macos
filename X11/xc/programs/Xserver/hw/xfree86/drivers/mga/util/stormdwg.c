/* $XConsortium: dwg.c /main/2 1996/10/28 06:57:55 kaleb $ */



/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/util/stormdwg.c,v 1.1 1997/04/12 14:11:29 hohndel Exp $ */

#include <stdio.h>
/* #include <stdlib.h> */

char *opcodes[] = {
	"line_open",		/* 0000 */
	"autoline_open",	/* 0001 */
	"line_close",		/* 0010 */
	"autoline_close",	/* 0011 */

	"trapezoid",		/* 0100 */
	"trapezoid texture map", /* 0101 */
	"reserved trap",	/* 0110 */
	"reserved trap",	/* 0111 */

	"bitblt",		/* 1000 */
	"iload",		/* 1001 */
	"reserved",		/* 1010 */
	"reserved",		/* 1011 */

	"fbitblt",		/* 1100 */
	"iload scale",		/* 1101 */
	"reserved",		/* 1110 */
	"iload filter"		/* 1111 */
};

char *atype[] = {
	"rpl - Write (Replace)",		/* 000 */
	"rstr - read-modify-write (raster)",	/* 001 */
	"reserved",	/* 010 */
	"z1 - depth mode with gouraud",	/* 011 */
	"blk - block write mode",	/* 100 */
	"reserved",	/* 101 */
	"reserved",	/* 110 */
	"I - Gouraud (with depth compare)"	/* 111 */
};

char *zmode[] = {
	"NOZCMP - always",		/* 000 */
	"reserved",	/* 001 */
	"ZE - depth =",	/* 010 */
	"zne - depth !=",	/* 011 */
	"zlt",	/* 100 */
	"zlte",	/* 101 */
	"zgt",	/* 110 */
	"zgte"	/* 111 */
};

char *bop[] = {
	"0",		/* 0000 */
	"~(D|S)",	/* 0001 */
	"D & ~S",	/* 0010 */
	"~S",		/* 0011 */

	"(~D) & S",	/* 0100 */
	"~D",		/* 0101 */
	"D ^ S",	/* 0110 */
	"~ (D & S)",	/* 0111 */

	"D & S",	/* 1000 */
	"~(D^S)",	/* 1001 */
	"D",		/* 1010 */
	"D | ~S",	/* 1011 */

	"S",		/* 1100 */
	"(~D) | S",	/* 1101 */
	"D | S",	/* 1110 */
	"1"		/* 1111 */
};

char *bitmod[] = {
	"BMONOLEF - Source is mono, or if iload, source is little endian",	/* 0000 */
	"BPLAN - source is mono from one plane",				/* 0001 */
	"BFCOL - source is colour, and is formatted from host",			/* 0010 */
	"BU32BGR - source is colour. For ILOAD, it's in 32bpp, BGR format",	/* 0011 */

	"BMONOWEF - source is mono, or if iload, source is windows format",	/* 0100 */
	"error! no such mode",	/* 0101 */
	"error! no such mode",	/* 0110 */
	"BU32RGB - source is colour, or if iload, source is 32 bpp RGB",	/* 0111 */

	"error! no such mode",	/* 1000 */
	"error! no such mode",	/* 1001 */
	"error! no such mode",	/* 1010 */
	"BU24BGR - source is colour, of if iload, source is 24 bpp BGR",	/* 1011 */

	"error! no such mode",	/* 1100 */
	"error! no such mode",	/* 1101 */
	"BUYUV - source is color, or for ILOAD, it's in 4:2:2 YUV format",	/* 1110 */
	"BU24RGB - source is color, or for ILOAD, it's in 24 bpp RGB"		/* 1111 */
};

main(argc, argv)
int argc;
char *argv[];
{
	unsigned long val, tmp;

	if ( argc != 2 )
	{
		fprintf(stderr, "usage: %s hexval\n", argv[0]);
		return 1;
	}	

	val = strtoul(argv[1], NULL, 16);

	printf("the val is : %d\n", val);

	/* opcode */

	tmp = val & 0x0f;

	printf("opcode: %s\n", opcodes[tmp]);

	tmp = ( val >> 4 ) & 0x7;

	printf("atype: %s\n", atype[tmp]);


	if ( val & 128 ) 
		printf("xy bitblt\n");
	else
		printf("linear bitblt\n");

	tmp = ( val >> 8 ) & 7;

	printf("zmode: %s\n", zmode[tmp]);


	if ( val & ( 1 << 11 ) )
		printf("solid line or constant trapezoid\n");
	else
		printf("src0 - 3 may need to be loaded");

	if ( val & ( 1 << 12 ))
		printf("ar0-ar6 = 0\n");
	else
		printf("you may need to set ar0-6.\n");


	if ( val & ( 1 << 13 ))
		printf("sgn = 0\n");
	else
		printf("sgn is not affected\n");


	if ( val & ( 1 << 14 ))
		printf("shift = 0\n");
	else
		printf("shift is not affected\n");


	tmp = (val>>16) & 0x0f; 

	if ( ((val >> 4) & 7) == 4 && tmp != 0x0c )
		printf("Error! Block (BLK) atype and non-source binary op chosen. Replace (S) bop will be used.\n");

	printf("bop = %s, where ~ = bitwise complement, ^ = xor\n", bop[tmp]);

	if ( ((val >> 20) & 0x0f) == 0 )
		printf("opaque object\n");
	else
		printf("partially opaque object\n");

	tmp = (val >> 25) & 0x0f;
	printf("bitmod: %s\n", bitmod[tmp]);

	if ((val >> 29) & 0x01)
		printf("patterning enabled\n");
	else
		printf("patterning disabled\n");

	if ((val >> 30) & 0x01)
		printf("background colour is transparent\n");
	else
		printf("background colour is opaque\n");

	return 0;
}
