# tifftcl.decls -- -*- tcl -*-
#
# This file contains the declarations for all supported public functions
# that are exported by the TIFFTCL library via the stubs table. This file
# is used to generate the tifftclDecls.h/tifftclStubsLib.c/tifftclStubsInit.c
# files.
#

# Declare each of the functions in the public BLT interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.

library tifftcl

# Define the TIFFTCL interface:

interface tifftcl
scspec TIFFTCLAPI

#########################################################################
###  TIFF interface

# Source: tiffio.h ...

declare 0 {
    const char *TIFFGetVersion(void)
}
declare 1 {
    const TIFFCodec *TIFFFindCODEC(uint16 a)
}
declare 2 {
    TIFFCodec *TIFFRegisterCODEC(uint16 a, const char *b, TIFFInitMethod c)
}
declare 3 {
    void TIFFUnRegisterCODEC(TIFFCodec *a)
}
declare 4 {
    tdata_t _TIFFmalloc(tsize_t a)
}
declare 5 {
    tdata_t _TIFFrealloc(tdata_t a, tsize_t b)
}
declare 6 {
    void _TIFFmemset(tdata_t a, int b, tsize_t c)
}
declare 7 {
    void _TIFFmemcpy(tdata_t a, const tdata_t b, tsize_t c)
}
declare 8 {
    int _TIFFmemcmp(const tdata_t a, const tdata_t b, tsize_t c)
}
declare 9 {
    void _TIFFfree(tdata_t a)
}
declare 10 {
    void TIFFClose(TIFF *tiffptr)
}
declare 11 {
    int TIFFFlush(TIFF *tiffptr)
}
declare 12 {
    int TIFFFlushData(TIFF *tiffptr)
}
declare 13 {
    int TIFFGetField(TIFF *tiffptr, ttag_t a, ...)
}
declare 14 {
    int TIFFVGetField(TIFF *tiffptr, ttag_t a, va_list b)
}
declare 15 {
    int TIFFGetFieldDefaulted(TIFF *tiffptr, ttag_t a, ...)
}
declare 16 {
    int TIFFVGetFieldDefaulted(TIFF *tiffptr, ttag_t a, va_list b)
}
declare 17 {
    int TIFFReadDirectory(TIFF *tiffptr)
}
declare 18 {
    tsize_t TIFFScanlineSize(TIFF *tiffptr)
}
declare 19 {
    tsize_t TIFFRasterScanlineSize(TIFF *tiffptr)
}
declare 20 {
    tsize_t TIFFStripSize(TIFF *tiffptr)
}
declare 21 {
    tsize_t TIFFVStripSize(TIFF *tiffptr, uint32 a)
}
declare 22 {
    tsize_t TIFFTileRowSize(TIFF *tiffptr)
}
declare 23 {
    tsize_t TIFFTileSize(TIFF *tiffptr)
}
declare 24 {
    tsize_t TIFFVTileSize(TIFF *tiffptr, uint32 a)
}
declare 25 {
    uint32 TIFFDefaultStripSize(TIFF *tiffptr, uint32 a)
}
declare 26 {
    void TIFFDefaultTileSize(TIFF *tiffptr, uint32 *a, uint32 *b)
}
declare 27 {
    int TIFFFileno(TIFF *tiffptr)
}
declare 28 {
    int TIFFGetMode(TIFF *tiffptr)
}
declare 29 {
    int TIFFIsTiled(TIFF *tiffptr)
}
declare 30 {
    int TIFFIsByteSwapped(TIFF *tiffptr)
}
declare 31 {
    int TIFFIsUpSampled(TIFF *tiffptr)
}
declare 32 {
    int TIFFIsMSB2LSB(TIFF *tiffptr)
}
declare 33 {
    uint32 TIFFCurrentRow(TIFF *tiffptr)
}
declare 34 {
    tdir_t TIFFCurrentDirectory(TIFF *tiffptr)
}
declare 35 {
    tdir_t TIFFNumberOfDirectories(TIFF *tiffptr)
}
declare 36 {
    uint32 TIFFCurrentDirOffset(TIFF *tiffptr)
}
declare 37 {
    tstrip_t TIFFCurrentStrip(TIFF *tiffptr)
}
declare 38 {
    ttile_t TIFFCurrentTile(TIFF *tiffptr)
}
declare 39 {
    int TIFFReadBufferSetup(TIFF *tiffptr, tdata_t a, tsize_t b)
}
declare 40 {
    int TIFFWriteBufferSetup(TIFF *tiffptr, tdata_t a, tsize_t b)
}
declare 41 {
    int TIFFWriteCheck(TIFF *tiffptr, int a, const char *b)
}
declare 42 {
    int TIFFCreateDirectory(TIFF *tiffptr)
}
declare 43 {
    int TIFFLastDirectory(TIFF *tiffptr)
}
declare 44 {
    int TIFFSetDirectory(TIFF *tiffptr, tdir_t a)
}
declare 45 {
    int TIFFSetSubDirectory(TIFF *tiffptr, uint32 a)
}
declare 46 {
    int TIFFUnlinkDirectory(TIFF *tiffptr, tdir_t a)
}
declare 47 {
    int TIFFSetField(TIFF *tiffptr, ttag_t a, ...)
}
declare 48 {
    int TIFFVSetField(TIFF *tiffptr, ttag_t a, va_list b)
}
declare 49 {
    int TIFFWriteDirectory(TIFF *tiffptr)
}
declare 50 {
    int TIFFReassignTagToIgnore(enum TIFFIgnoreSense a, int b)
}
declare 51 {
    void TIFFPrintDirectory(TIFF *tiffptr, FILE *a, long b)
}
declare 52 {
    int TIFFReadScanline(TIFF *tiffptr, tdata_t a, uint32 b, tsample_t c)
}
declare 53 {
    int TIFFWriteScanline(TIFF *tiffptr, tdata_t a, uint32 b, tsample_t c)
}
declare 54 {
    int TIFFReadRGBAImage(TIFF *tiffptr, uint32 a, uint32 b, uint32 *c, int d)
}
declare 55 {
    int TIFFReadRGBAStrip(TIFF *tiffptr, tstrip_t a, uint32 *b)
}
declare 56 {
    int TIFFReadRGBATile(TIFF *tiffptr, uint32 a, uint32 b, uint32 *c)
}
declare 57 {
    int TIFFRGBAImageOK(TIFF *tiffptr, char *a)
}
declare 58 {
    int TIFFRGBAImageBegin(TIFFRGBAImage *a, TIFF *tiffptr, int b, char *c)
}
declare 59 {
    int TIFFRGBAImageGet(TIFFRGBAImage *d, uint32 *c, uint32 b, uint32 a)
}
declare 60 {
    void TIFFRGBAImageEnd(TIFFRGBAImage *a)
}
declare 61 {
    TIFF *TIFFOpen(const char *b, const char *a)
}
declare 62 {
    TIFF *TIFFFdOpen(int a, const char *b, const char *c)
}
declare 63 {
    TIFF *TIFFClientOpen(const char *a, const char *b,
	    thandle_t c,
	    TIFFReadWriteProc d, TIFFReadWriteProc e,
	    TIFFSeekProc f, TIFFCloseProc g,
	    TIFFSizeProc h,
	    TIFFMapFileProc i, TIFFUnmapFileProc j)
}
declare 64 {
    const char *TIFFFileName(TIFF *tiffptr)
}
declare 65 {
    void TIFFError(const char *a, const char *b, ...)
}
declare 66 {
    void TIFFWarning(const char *a, const char *b, ...)
}
declare 67 {
    TIFFErrorHandler TIFFSetErrorHandler(TIFFErrorHandler a)
}
declare 68 {
    TIFFErrorHandler TIFFSetWarningHandler(TIFFErrorHandler a)
}
declare 69 {
    TIFFExtendProc TIFFSetTagExtender(TIFFExtendProc a)
}
declare 70 {
    ttile_t TIFFComputeTile(TIFF *tiffptr, uint32 a, uint32 b, uint32 c, tsample_t d)
}
declare 71 {
    int TIFFCheckTile(TIFF *tiffptr, uint32 d, uint32 c, uint32 b, tsample_t a)
}
declare 72 {
    ttile_t TIFFNumberOfTiles(TIFF *tiffptr)
}
declare 73 {
    tsize_t TIFFReadTile(TIFF *tiffptr,
	    tdata_t a, uint32 b, uint32 c, uint32 d, tsample_t e)
}
declare 74 {
    tsize_t TIFFWriteTile(TIFF *tiffptr,
	    tdata_t e, uint32 d, uint32 c, uint32 b, tsample_t a)
}
declare 75 {
    tstrip_t TIFFComputeStrip(TIFF *tiffptr, uint32 a, tsample_t b)
}
declare 76 {
    tstrip_t TIFFNumberOfStrips(TIFF *tiffptr)
}
declare 77 {
    tsize_t TIFFReadEncodedStrip(TIFF *tiffptr, tstrip_t a, tdata_t b, tsize_t c)
}
declare 78 {
    tsize_t TIFFReadRawStrip(TIFF *tiffptr, tstrip_t a, tdata_t b, tsize_t c)
}
declare 79 {
    tsize_t TIFFReadEncodedTile(TIFF *tiffptr, ttile_t a, tdata_t b, tsize_t c)
}
declare 80 {
    tsize_t TIFFReadRawTile(TIFF *tiffptr, ttile_t c, tdata_t b, tsize_t a)
}
declare 81 {
    tsize_t TIFFWriteEncodedStrip(TIFF *tiffptr, tstrip_t a, tdata_t b, tsize_t c)
}
declare 82 {
    tsize_t TIFFWriteRawStrip(TIFF *tiffptr, tstrip_t a, tdata_t b, tsize_t c)
}
declare 83 {
    tsize_t TIFFWriteEncodedTile(TIFF *tiffptr, ttile_t a, tdata_t b, tsize_t c)
}
declare 84 {
    tsize_t TIFFWriteRawTile(TIFF *tiffptr, ttile_t c, tdata_t b, tsize_t a)
}
declare 85 {
    void TIFFSetWriteOffset(TIFF *tiffptr, toff_t a)
}
declare 86 {
    void TIFFSwabShort(uint16 *a)
}
declare 87 {
    void TIFFSwabLong(uint32 *a)
}
declare 88 {
    void TIFFSwabDouble(double *a)
}
declare 89 {
    void TIFFSwabArrayOfShort(uint16 *a, unsigned long b)
}
declare 90 {
    void TIFFSwabArrayOfLong(uint32 *b, unsigned long a)
}
declare 91 {
    void TIFFSwabArrayOfDouble(double *a, unsigned long b)
}
declare 92 {
    void TIFFReverseBits(unsigned char *a, unsigned long b)
}
declare 93 {
    const unsigned char *TIFFGetBitRevTable(int a)
}

# Source: tif_predict.h ...
declare 100 {
    int TIFFPredictorInit(TIFF *tiffptr)
}

# Source: tif_dir.h ...
declare 110 {
    void _TIFFSetupFieldInfo(TIFF *tiffptr, const TIFFFieldInfo a[], size_t b)
}
declare 111 {
    int _TIFFMergeFieldInfo(TIFF *tiffptr, const TIFFFieldInfo *a, int b)
}
declare 112 {
    void _TIFFPrintFieldInfo(TIFF *tiffptr, FILE *a)
}
declare 113 {
    const TIFFFieldInfo *TIFFFindFieldInfo(TIFF *tiffptr, ttag_t a, TIFFDataType b)
}
declare 114 {
    const TIFFFieldInfo *TIFFFieldWithTag(TIFF *tiffptr, ttag_t a)
}
declare 115 {
    TIFFDataType _TIFFSampleToTagType(TIFF *tiffptr)
}


# Source: tiffiop.h ...

declare 120 {
    int _TIFFgetMode(const char *a, const char *b)
}
declare 121 {
    int _TIFFNoRowEncode(TIFF *tiffptr, tidata_t a, tsize_t b, tsample_t c)
}
declare 122 {
    int _TIFFNoStripEncode(TIFF *tiffptr, tidata_t c, tsize_t b, tsample_t a)
}
declare 123 {
    int _TIFFNoTileEncode(TIFF *tiffptr, tidata_t a, tsize_t b, tsample_t c)
}
declare 124 {
    int _TIFFNoRowDecode(TIFF *tiffptr, tidata_t c, tsize_t b, tsample_t a)
}
declare 125 {
    int _TIFFNoStripDecode(TIFF *tiffptr, tidata_t a, tsize_t b, tsample_t c)
}
declare 126 {
    int _TIFFNoTileDecode(TIFF *tiffptr, tidata_t c, tsize_t b, tsample_t a)
}
declare 127 {
    void _TIFFNoPostDecode(TIFF *tiffptr, tidata_t a, tsize_t b)
}
declare 128 {
    int _TIFFNoPreCode(TIFF *tiffptr, tsample_t a)
}
declare 129 {
    int _TIFFNoSeek(TIFF *tiffptr, uint32 a)
}
declare 130 {
    void _TIFFSwab16BitData(TIFF *tiffptr, tidata_t a, tsize_t b)
}
declare 131 {
    void _TIFFSwab32BitData(TIFF *tiffptr, tidata_t b, tsize_t a)
}
declare 132 {
    void _TIFFSwab64BitData(TIFF *tiffptr, tidata_t a, tsize_t b)
}
declare 133 {
    int TIFFFlushData1(TIFF *tiffptr)
}
declare 134 {
    void TIFFFreeDirectory(TIFF *tiffptr)
}
declare 135 {
    int TIFFDefaultDirectory(TIFF *tiffptr)
}
declare 136 {
    int TIFFSetCompressionScheme(TIFF *tiffptr, int a)
}
declare 137 {
    void _TIFFSetDefaultCompressionState(TIFF *tiffptr)
}
declare 138 {
    uint32 _TIFFDefaultStripSize(TIFF *tiffptr, uint32 a)
}
declare 139 {
    void _TIFFDefaultTileSize(TIFF *tiffptr, uint32 *a, uint32 *b)
}
declare 140 {
    void _TIFFsetByteArray(void **a, void *b, uint32 c)
}
declare 141 {
    void _TIFFsetString(char **a, char *b)
}
declare 142 {
    void _TIFFsetShortArray(uint16 **a, uint16 *b, uint32 c)
}
declare 143 {
    void _TIFFsetLongArray(uint32 **a, uint32 *b, uint32 c)
}
declare 144 {
    void _TIFFsetFloatArray(float **a, float *b, uint32 c)
}
declare 145 {
    void _TIFFsetDoubleArray(double **a, double *b, uint32 c)
}
declare 146 {
    void _TIFFprintAscii(FILE *a, const char *b)
}
declare 147 {
    void _TIFFprintAsciiTag(FILE *a, const char *b, const char *c)
}
declare 148 {
    int TIFFInitDumpMode(TIFF *tiffptr, int a)
}
declare 149 generic {!PACKBITS_SUPPORT} {
    int TIFFInitPackBits(TIFF *tiffptr, int a)
}
declare 150 generic {!CCITT_SUPPORT} {
    int TIFFInitCCITTRLE(TIFF *tiffptr, int a)
}
declare 151 generic {!CCITT_SUPPORT} {
    int TIFFInitCCITTRLEW(TIFF *tiffptr, int a)
}
declare 152 generic {!CCITT_SUPPORT} {
    int TIFFInitCCITTFax3(TIFF *tiffptr, int a)
}
declare 153 generic {!CCITT_SUPPORT} {
    int TIFFInitCCITTFax4(TIFF *tiffptr, int a)
}
declare 154 generic {!THUNDER_SUPPORT} {
    int TIFFInitThunderScan(TIFF *tiffptr, int a)
}
declare 155 generic {!NEXT_SUPPORT} {
    int TIFFInitNeXT(TIFF *tiffptr, int a)
}
declare 156 generic {!LZW_SUPPORT} {
    int TIFFInitLZW(TIFF *tiffptr, int a)
}
declare 157 generic {!OJPEG_SUPPORT} {
    int TIFFInitOJPEG(TIFF *tiffptr, int a)
}
declare 158 generic {!JPEG_SUPPORT} {
    int TIFFInitJPEG(TIFF *tiffptr, int a)
}
declare 159 generic {!JBIG_SUPPORT} {
    int TIFFInitJBIG(TIFF *tiffptr, int a)
}
declare 160 generic {!ZIP_SUPPORT} {
    int TIFFInitZIP(TIFF *tiffptr, int a)
}
declare 161 generic {!PIXARLOG_SUPPORT} {
    int TIFFInitPixarLog(TIFF *tiffptr, int a)
}
declare 162 generic {!LOGLUV_SUPPORT} {
    int TIFFInitSGILog(TIFF *tiffptr, int a)
}

#########################################################################
