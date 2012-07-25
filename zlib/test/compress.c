#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#if LZSS
#include "lzsscompress.h"
#else
#include <zlib.h>

static void *zalloc(void *opaque, unsigned int items, unsigned int size)
	{ return malloc(items * (size_t) size); }

static void zfree(void *opaque, void *address)
	{ free(address); }


static const char *ErrorName(int result)
{
	switch (result)
	{
		case Z_OK:             return "Z_OK";
		case Z_STREAM_END:     return "Z_STREAM_END";
		case Z_NEED_DICT:      return "Z_NEED_DICT";
		case Z_ERRNO:          return "Z_ERRNO";
		case Z_STREAM_ERROR:   return "Z_STREAM_ERROR";
		case Z_DATA_ERROR:     return "Z_DATA_ERROR";
		case Z_MEM_ERROR:      return "Z_MEM_ERROR";
		case Z_BUF_ERROR:      return "Z_BUF_ERROR";
		case Z_VERSION_ERROR:  return "Z_VERSION_ERROR";
		default:               return "unknown erorr";
	}
}


static void TestZlibResult(int result, const char *CalledRoutine,
	const char *File, const char *Function, int Line)
{
	if (result == Z_OK)
		return;

	fprintf(stderr, "Error, %s returned %s in file %s, function %s, line %d.\n",
		CalledRoutine, ErrorName(result), File, Function, Line);

	exit(EXIT_FAILURE);
}


static void TestFwriteResult(size_t result, size_t expected,
	const char *File, const char *Function, int Line)
{
	if (result == expected)
		return;

	fprintf(stderr,
"Error, fwrite returned error in file %s, function %s, line %d.\n",
		File, Function, Line);

	exit(EXIT_FAILURE);
}
#endif	// LZSS



#if LZSS


int main(int argc, char **argv)
{
	FILE	*fi, *fo;
	u_int8_t	inbuf[BUFSIZE_UNCOMPRESSED], outbuf[BUFSIZE_COMPRESSED], *bufend;
	u_int32_t	insize, outsize;
	float	fin=0., fout=0.;
	
	if (argc!=3) {
		fprintf(stderr," usage : %s infile outfile\n", argv[0]);
		exit(1);
	}
	if (!(fi=fopen(argv[1],"rb"))) {
		fprintf(stderr," error : fail to open %s\n", argv[1]);
		exit(2);
	}
	if (!(fo=fopen(argv[2],"wb"))) {
		fprintf(stderr," error : fail to open %s\n", argv[2]);
		exit(3);
	}

	while (insize=fread(inbuf,1,BUFSIZE_UNCOMPRESSED,fi)) {

		bufend = compress_lzss(outbuf, BUFSIZE_COMPRESSED, inbuf, insize);

    	if (!bufend) {
			fprintf(stderr,"error : failed to lzss compress\n");
			exit(4);
		}

    	outsize = bufend - outbuf;
		fin += insize;
		fout += outsize;
		fwrite(&outsize,sizeof(int),1,fo);
		fwrite(outbuf,1,outsize,fo);
    }

	printf("compression ratio = %f\n", fout/fin);

	fclose(fi);
	fclose(fo);

	return 0;

}
#else		// LZSS

int main(int argc, char **argv)
{
	enum {
		UncompressedBufferLength = 65536,
		CompressedBufferLength =
			(UncompressedBufferLength * 1001 + 1) / 1000 + 12,
		InputBufferLength = UncompressedBufferLength,
		OutputBufferLength = CompressedBufferLength,
	};

	FILE	*fi, *fo;
	if (argc!=3) {
		fprintf(stderr," usage : %s infile outfile\n", argv[0]);
		exit(1);
	}
	if (!(fi=fopen(argv[1],"rb"))) {
		fprintf(stderr," error : fail to open %s\n", argv[1]);
		exit(2);
	}
	if (!(fo=fopen(argv[2],"wb"))) {
		fprintf(stderr," error : fail to open %s\n", argv[2]);
		exit(3);
	}

	unsigned char *InputBuffer = malloc(InputBufferLength);
	unsigned char *OutputBuffer = malloc(OutputBufferLength);
	if (InputBuffer == NULL || OutputBuffer == NULL)
	{
		fprintf(stderr, "Error, unable to allocate memory.\n");
		return EXIT_FAILURE;
	}

	int fresult;
	int zresult;

	z_stream stream = { .zalloc = zalloc, .zfree = zfree, .opaque = NULL };

#if 0
	zresult = deflateInit(&stream, Z_BEST_COMPRESSION);
#else
	zresult = deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, 15, 9, Z_DEFAULT_STRATEGY);
#endif
	TestZlibResult(zresult, "deflateInit", __FILE__, __func__, __LINE__);

	while (1)
	{
#if defined UseReset
		zresult = deflateReset(&stream);
		TestZlibResult(zresult, "deflateReset", __FILE__, __func__, __LINE__);
#endif	// defined UseReset

		int length = fread(InputBuffer, 1, InputBufferLength, fi);

		if (length == 0)
		{
			if (ferror(fi))
			{
				fprintf(stderr,
"Error, stream error in file %s, function %s, line %d.\n",
					__FILE__, __func__, __LINE__);
				exit(EXIT_FAILURE);
			}
			break;
		}

		//#define LengthAllottedToCompressionOutput	length
		#define LengthAllottedToCompressionOutput	OutputBufferLength
			/*	Set avail_out to OutputBufferLength to use deflate for all
				blocks instead of using original data for those that do not
				compress well.
			*/
		stream.next_in   = InputBuffer;
		stream.avail_in  = length;
		stream.next_out  = OutputBuffer;
		stream.avail_out = LengthAllottedToCompressionOutput;

#if defined UseReset
		zresult = deflate(&stream, Z_FINISH);

		if (zresult == Z_OK)
		{
			int ChunkLength = length+1;

			fresult = fwrite(&ChunkLength, sizeof ChunkLength, 1, fo);
			TestFwriteResult(fresult, 1, __FILE__, __func__, __LINE__);

			fresult = fwrite("\xff", 1, 1, fo);
			TestFwriteResult(fresult, 1, __FILE__, __func__, __LINE__);

			fresult = fwrite(InputBuffer, 1, length, fo);
			TestFwriteResult(fresult, length, __FILE__, __func__, __LINE__);
		}
		else
		{
			if (zresult != Z_STREAM_END)
	#if 0	// Would prefer to design so this never happens, and we would have:
				if (zresult == Z_OK)
				{
					fprintf(stderr,
	"Error, unexpected Z_OK from deflate in file %s, function %s, line %d.\n",
						__FILE__, __func__, __LINE__);
					exit(EXIT_FAILURE);
				}
				else
	#endif
					TestZlibResult(zresult, "deflate",
						__FILE__, __func__, __LINE__);

			int ChunkLength =
				LengthAllottedToCompressionOutput - stream.avail_out;

			fresult = fwrite(&ChunkLength, sizeof ChunkLength, 1, fo);
			TestFwriteResult(fresult, 1, __FILE__, __func__, __LINE__);

			fresult = fwrite(OutputBuffer, 1, ChunkLength, fo);
			TestFwriteResult(fresult, ChunkLength,
				__FILE__, __func__, __LINE__);
		}
#else	// defined UseReset
		#undef LengthAllottedToCompressionOutput
		#define LengthAllottedToCompressionOutput	OutputBufferLength
		stream.avail_out = LengthAllottedToCompressionOutput;
		zresult = deflate(&stream, Z_FULL_FLUSH);

		if (stream.avail_out == 0)
		{
			int ChunkLength = length+1;

			fresult = fwrite(&ChunkLength, sizeof ChunkLength, 1, fo);
			TestFwriteResult(fresult, 1, __FILE__, __func__, __LINE__);

			fresult = fwrite("\xff", 1, 1, fo);
			TestFwriteResult(fresult, 1, __FILE__, __func__, __LINE__);

			fresult = fwrite(InputBuffer, 1, length, fo);
			TestFwriteResult(fresult, length, __FILE__, __func__, __LINE__);
/*debug*/fprintf(stderr, "Uck.\n");
		}
		else
		{
			TestZlibResult(zresult, "deflate", __FILE__, __func__, __LINE__);

			int ChunkLength =
				LengthAllottedToCompressionOutput - stream.avail_out;
/*debug*/fprintf(stderr, "ChunkLength = %d.\n", ChunkLength);

			fresult = fwrite(&ChunkLength, sizeof ChunkLength, 1, fo);
			TestFwriteResult(fresult, 1, __FILE__, __func__, __LINE__);

			fresult = fwrite(OutputBuffer, 1, ChunkLength, fo);
			TestFwriteResult(fresult, ChunkLength,
				__FILE__, __func__, __LINE__);
		}
#endif	// defined UseReset

		#undef LengthAllottedToCompressionOutput
	}

	{
		do
		{
			stream.next_out  = OutputBuffer;
			stream.avail_out = OutputBufferLength;

			zresult = deflate(&stream, Z_FINISH);
			int ChunkLength =
				OutputBufferLength - stream.avail_out;
			fprintf(stderr, "Final ChunkLength = %d.\n", ChunkLength);
			if (ChunkLength == 0)
			{
				if (zresult != Z_STREAM_END)
				{
					fprintf(stderr, "Error, ChunkLength is zero but zresult is %d, not Z_STREAM_END.\n", zresult);
					exit(EXIT_FAILURE);
				}
			}
			else
			{
				if (zresult != Z_OK && zresult != Z_STREAM_END)
				{
					fprintf(stderr, "Error, ChunkLength is %d but zresult is %d, not Z_OK.\n", ChunkLength, zresult);
					exit(EXIT_FAILURE);
				}

				fresult = fwrite(&ChunkLength, sizeof ChunkLength, 1, fo);
				TestFwriteResult(fresult, 1, __FILE__, __func__, __LINE__);

				fresult = fwrite(OutputBuffer, 1, ChunkLength, fo);
				TestFwriteResult(fresult, ChunkLength,
					__FILE__, __func__, __LINE__);
			}
		} while (zresult != Z_STREAM_END);
	}
	zresult = deflateEnd(&stream);
	TestZlibResult(zresult, "deflateEnd", __FILE__, __func__, __LINE__);

	free(InputBuffer);
    free(OutputBuffer);

	return 0;
}
#endif		// LZSS
