/*	If UseReset is defined, the implementation resets the compression stream
	before each block, to make random access possible.  Otherwise, the
	implementation uses Z_FULL_FLUSH when deflating to make random access
	possible.
*/
#define	UseReset

	/*	If reset is not used, then performance cannot be measured (because
		the decompression routine cannot be called repeatedly), and there
		is a segment fault during decompression.
	*/


// define LZSS=1 to switch to LZSS algorithm

#define	LZSS	0
