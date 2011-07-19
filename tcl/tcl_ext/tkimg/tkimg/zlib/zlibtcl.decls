# zlibtcl.decls -- -*- tcl -*-
#
# This file contains the declarations for all supported public functions
# that are exported by the ZLIBTCL library via the stubs table. This file
# is used to generate the zlibtclDecls.h/zlibtclStubsLib.c/zlibtclStubsInit.c
# files.
#

# Declare each of the functions in the public BLT interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.

library zlibtcl

# Define the ZLIBTCL interface:

interface zlibtcl
scspec ZEXTERN

#########################################################################
###  Misc. interfaces

declare 0 {
    const char *zlibVersion(void)
}

declare 1 {
    const char *zError(int err)
}

declare 2 {
    uLong crc32(uLong crc, const Bytef *buf, uInt len)
}

declare 3 {
    uLong adler32(uLong adler, const Bytef *buf, uInt len)
}

#########################################################################
###  Deflate = Compression

declare 10 {
    int deflateInit_(z_streamp stream, int level, const char *version, int stream_size)
}
declare 11 {
    int deflateInit2_(z_streamp stream, int level,
	int method, int windowBits, int memLevel, int strategy,
	const char *version, int stream_size)
}
declare 12 {
    int deflate(z_streamp stream, int flush)
}
declare 13 {
    int deflateEnd(z_streamp stream)
}
declare 14 {
    int deflateSetDictionary(z_streamp stream, const Bytef *dict, uInt dictLength)
}
declare 15 {
    int deflateCopy(z_streamp dst, z_streamp src)
}
declare 16 {
    int deflateReset(z_streamp stream)
}
declare 17 {
    int deflateParams(z_streamp stream, int level, int strategy)
}

#########################################################################

declare 18 {
    int compress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
}
declare 19 {
    int compress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level)
}

#########################################################################
###  Inflate = Decompression

declare 20 {
    int inflateInit_(z_streamp stream, const char *version, int stream_size)
}
declare 21 {
    int inflateInit2_(z_streamp stream, int windowBits, const char *version,
			int stream_size)
}
declare 22 {
    int inflate(z_streamp stream, int flush)
}
declare 23 {
    int inflateEnd(z_streamp stream)
}
declare 24 {
    int inflateSetDictionary(z_streamp stream, const Bytef *dict, uInt dictLength)
}
declare 25 {
    int inflateSync(z_streamp stream)
}
declare 26 {
    int inflateReset(z_streamp stream)
}

#########################################################################

declare 27 {
    int uncompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
}

#########################################################################
## gz'ip layer

declare 30 {
    gzFile gzopen(const char *path, const char *mode)
}
declare 31 {
    gzFile gzdopen(int fd, const char *mode)
}
declare 32 {
    int gzsetparams(gzFile file, int level, int strategy)
}
declare 33 {
    int gzread(gzFile file, voidp buf, unsigned len)
}
declare 34 {
    int gzwrite(gzFile file, voidpc buf, unsigned len)
}
declare 35 {
    int gzprintf(gzFile file, const char *format, ...)
}
declare 36 {
    int gzputs(gzFile file, const char *s)
}
declare 37 {
    char *gzgets(gzFile file, char *buf, int len)
}
declare 38 {
    int gzputc(gzFile file, int c)
}
declare 39 {
    int gzgetc(gzFile file)
}
declare 40 {
    int gzflush(gzFile file, int flush)
}
declare 41 {
    z_off_t gzseek(gzFile file, z_off_t offset, int whence)
}
declare 42 {
    int gzrewind(gzFile file)
}
declare 43 {
    z_off_t gztell(gzFile file)
}
declare 44 {
    int gzeof(gzFile file)
}
declare 45 {
    int gzclose(gzFile file)
}
declare 46 {
    const char *gzerror(gzFile file, int *errnum)
}

#########################################################################
