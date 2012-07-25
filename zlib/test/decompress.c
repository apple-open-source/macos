#include <stdio.h>
#include <stdlib.h>

#include "ClockServices.h"
#include "common.h"

#if LZSS

#include "lzsscompress.h"

#define	InputBufferLength BUFSIZE_COMPRESSED
#define OutputBufferLength BUFSIZE_UNCOMPRESSED

#else
#include <zlib.h>

enum {
	UncompressedBufferLength = 65536,
	CompressedBufferLength = (UncompressedBufferLength * 1001 + 1) / 1000 + 12,
	InputBufferLength = CompressedBufferLength,
	OutputBufferLength = UncompressedBufferLength + 1,
		// xxx Reconsider whether +1 goes here or in malloc.
};


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


static void TestFreadResult(size_t result, size_t expected,
	const char *File, const char *Function, int Line)
{
	if (result == expected)
		return;

	fprintf(stderr,
"Error, fread returned error in file %s, function %s, line %d.\n",
		File, Function, Line);

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


typedef struct
{
#if !LZSS
	z_stream *stream;
#endif
	unsigned char *InputBuffer;
	unsigned char *OutputBuffer;
#if LZSS
	u_int32_t	length;
	u_int32_t	*outlength;
#else
	size_t length;
#endif
} Parameters;


void DecompressBlock(const Parameters *parameters)
{
#if LZSS
	*(parameters->outlength) = decompress_lzss(parameters->OutputBuffer, 0, parameters->InputBuffer, parameters->length);

#else	// LZSS
	z_stream *stream = parameters->stream;

#if defined UseReset
	int zresult = inflateReset(stream);
	TestZlibResult(zresult, "inflateReset", __FILE__, __func__, __LINE__);
			// xxx Omit testing result when measuring speed.  Others too?
#endif	// defined UseReset

	stream->next_in   = parameters->InputBuffer;
	stream->avail_in  = parameters->length;
	stream->next_out  = parameters->OutputBuffer;
	stream->avail_out = OutputBufferLength;

#if defined UseReset
	zresult = inflate(stream, Z_FINISH);

	if (zresult != Z_STREAM_END)
		if (zresult == Z_OK)
		{
			fprintf(stderr,
"Error, unexpected Z_OK from inflate in file %s, function %s, line %d.\n",
				__FILE__, __func__, __LINE__);
			exit(EXIT_FAILURE);
		}
		else
			TestZlibResult(zresult, "inflate",
				__FILE__, __func__, __LINE__);
#else	// defined UseReset
/*debug*/fprintf(stderr, "%d.\n", __LINE__);
	int zresult = inflate(stream, Z_SYNC_FLUSH);
	TestZlibResult(zresult, "inflate", __FILE__, __func__, __LINE__);
#endif	// defined UseReset
#endif	// LZSS
}


void Driver(unsigned int iterations, void *parameters)
{
	Parameters *p = (Parameters *) parameters;
	while (iterations--)
		DecompressBlock(p);
}


int main(int argc, char *argv[])
{
	/*	ShowPerformance is true iff performance information should be written
		to stdout.

		We set it to true iff stdout is available for such information, and
		stdout is available when another stream is being used to output the
		uncompressed data.
	*/
	#define	ShowPerformance	1|(OutStream != stdout)

	FILE *fi = stdin;
	FILE *OutStream = stdout;

	if (argc == 3) {
		if (!(fi = fopen(argv[1],"rb")))
		{
			fprintf(stderr,
				"Error, unable to read %s at file %s, function %s, line %d.\n",
				argv[1], __FILE__, __func__, __LINE__);
			perror("fopen");
			exit(EXIT_FAILURE);
		}
		if (!(OutStream = fopen(argv[2], "wb")))
		{
			fprintf(stderr,
				"Error, unable to write to %s at file %s, function %s, line %d.\n",
				argv[2], __FILE__, __func__, __LINE__);
			perror("fopen");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		fprintf(stderr, "Usage: %s input output\n", argv[0]);
		exit(EXIT_FAILURE);
	}

#if LZSS
	unsigned char InputBuffer[InputBufferLength];
	unsigned char OutputBuffer[OutputBufferLength];
#else
	unsigned char *InputBuffer = malloc(InputBufferLength);
	unsigned char *OutputBuffer = malloc(OutputBufferLength);
	if (InputBuffer == NULL || OutputBuffer == NULL)
	{
		fprintf(stderr, "Error, unable to allocate memory.\n");
		return EXIT_FAILURE;
	}
#endif

	double TotalTime = 0;
	size_t TotalCompressedBytes = 0;
	size_t TotalUncompressedBytes = 0;
	size_t TotalNoncompressedBlocks = 0;
	size_t TotalNoncompressedBytes = 0;

	size_t fresult;

#if !LZSS

	int zresult;

	z_stream stream = { .zalloc = zalloc, .zfree = zfree, .opaque = NULL };

	zresult = inflateInit(&stream);
	TestZlibResult(zresult, "inflateInit", __FILE__, __func__, __LINE__);

#endif

	// xxx Change fread and fwrite to read and write, see how times are affected.
	// xxx Measure with I/O and without compression, consider difference?

	while (1)
	{
		u_int32_t length, decompressed_length;

		fresult = fread(&length, sizeof length, 1, fi);
			// xxx Could read sizeof length items of length 1, check for less than full length result to detect format error in input file.

		if (fresult == 0)
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

		fresult = fread(InputBuffer, 1, length, fi);
#if !LZSS
		TestFreadResult(fresult, length, __FILE__, __func__, __LINE__);
			// xxx Not necessarily a read error, could be a format error.

		// Check for tag used to mark uncompressed blocks.
		if (InputBuffer[0] == (unsigned char) '\xff')
		{
/*debug*/fprintf(stderr, "%d.\n", __LINE__);
			TotalCompressedBytes += length + 4;

			--length;
			fresult = fwrite(InputBuffer + 1, 1, length, OutStream);
			TestFwriteResult(fresult, length, __FILE__, __func__, __LINE__);

			++TotalNoncompressedBlocks;
			TotalNoncompressedBytes += length;

			TotalUncompressedBytes += length;
		}
		else
#endif
		{
			Parameters parameters =
			{
#if !LZSS
				.stream = &stream,
#endif
				.InputBuffer = InputBuffer,
				.OutputBuffer = OutputBuffer,
#if LZSS
				.length = length,
				.outlength = &decompressed_length,
#else
				.length = length,
#endif
			};

			if (ShowPerformance)
			{

				TotalCompressedBytes += length + 4;

				double t = MeasureNetTimeInCPUCycles(Driver,
					10, &parameters, 10);


				TotalTime += t;
#if LZSS
				TotalUncompressedBytes += decompressed_length;
#else
				TotalUncompressedBytes += stream.total_out;
#endif
			}
			else
				DecompressBlock(&parameters);

#if LZSS
			fresult = fwrite(OutputBuffer, 1, decompressed_length, stdout);
#else
			fresult = fwrite(OutputBuffer, 1, stream.total_out, OutStream);
			TestFwriteResult(fresult, stream.total_out,
				__FILE__, __func__, __LINE__);
#endif
		}
	}

#if !LZSS
	zresult = inflateEnd(&stream);
	TestZlibResult(zresult, "deflateEnd", __FILE__, __func__, __LINE__);
#endif

	// If we opened a stream for the uncompressed data, close it.
	if (OutStream != stdout)
		if (fclose(OutStream))
		{
			fprintf(stderr,
"Error, stream error in output file, file %s, function %s, line %d.\n",
				__FILE__, __func__, __LINE__);
			exit(EXIT_FAILURE);
		}

#if !LZSS
    free(InputBuffer);
	free(OutputBuffer);
#endif


	if (ShowPerformance)
	{
		fprintf(stderr,"Total CPU cycles = %g.\n", TotalTime);
		fprintf(stderr,"Compressed size / Uncompressed size = %zd / %zd = %g.\n",
			TotalCompressedBytes, TotalUncompressedBytes,
			(double) TotalCompressedBytes / TotalUncompressedBytes);
		fprintf(stderr,"CPU cycles per compressed byte = %g.\n",
			TotalTime / TotalCompressedBytes);
		fprintf(stderr,"CPU cycles per uncompressed byte = %g.\n",
			TotalTime / TotalUncompressedBytes);
		fprintf(stderr,"%zd blocks %s not compressed, totaling %zd bytes.\n",
			TotalNoncompressedBlocks,
			TotalNoncompressedBlocks == 1 ? "was" : "were",
			TotalNoncompressedBytes);
	}

	return 0;
}
