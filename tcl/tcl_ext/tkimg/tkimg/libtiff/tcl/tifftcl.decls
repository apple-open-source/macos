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
#hooks {}

#########################################################################
###  TIFF interface

# Source: tiffio.h ...

declare 0 generic {
    const char *TIFFGetVersion(void)
}
declare 1 generic {
    const TIFFCodec *TIFFFindCODEC(uint16 a)
}
declare 2 generic {
    TIFFCodec *TIFFRegisterCODEC(uint16 a, const char *b, TIFFInitMethod c)
}
declare 3 generic {
    void TIFFUnRegisterCODEC(TIFFCodec *a)
}
declare 4 generic {
    tdata_t _TIFFmalloc(tsize_t a)
}
declare 5 generic {
    tdata_t _TIFFrealloc(tdata_t a, tsize_t b)
}
declare 6 generic {
    void _TIFFmemset(tdata_t a, int b, tsize_t c)
}
declare 7 generic {
    void _TIFFmemcpy(tdata_t a, const tdata_t b, tsize_t c)
}
declare 8 generic {
    int _TIFFmemcmp(const tdata_t a, const tdata_t b, tsize_t c)
}
declare 9 generic {
    void _TIFFfree(tdata_t a)
}
declare 10 generic {
    void TIFFClose(TIFF *tiffptr)
}
declare 11 generic {
    int TIFFFlush(TIFF *tiffptr)
}
declare 12 generic {
    int TIFFFlushData(TIFF *tiffptr)
}
declare 13 generic {
    int TIFFGetField(TIFF *tiffptr, ttag_t a, ...)
}
declare 14 generic {
    int TIFFVGetField(TIFF *tiffptr, ttag_t a, va_list b)
}
declare 15 generic {
    int TIFFGetFieldDefaulted(TIFF *tiffptr, ttag_t a, ...)
}
declare 16 generic {
    int TIFFVGetFieldDefaulted(TIFF *tiffptr, ttag_t a, va_list b)
}
declare 17 generic {
    int TIFFReadDirectory(TIFF *tiffptr)
}
declare 18 generic {
    tsize_t TIFFScanlineSize(TIFF *tiffptr)
}
declare 19 generic {
    tsize_t TIFFRasterScanlineSize(TIFF *tiffptr)
}
declare 20 generic {
    tsize_t TIFFStripSize(TIFF *tiffptr)
}
declare 21 generic {
    tsize_t TIFFVStripSize(TIFF *tiffptr, uint32 a)
}
declare 22 generic {
    tsize_t TIFFTileRowSize(TIFF *tiffptr)
}
declare 23 generic {
    tsize_t TIFFTileSize(TIFF *tiffptr)
}
declare 24 generic {
    tsize_t TIFFVTileSize(TIFF *tiffptr, uint32 a)
}
declare 25 generic {
    uint32 TIFFDefaultStripSize(TIFF *tiffptr, uint32 a)
}
declare 26 generic {
    void TIFFDefaultTileSize(TIFF *tiffptr, uint32 *a, uint32 *b)
}
declare 27 generic {
    int TIFFFileno(TIFF *tiffptr)
}
declare 28 generic {
    int TIFFGetMode(TIFF *tiffptr)
}
declare 29 generic {
    int TIFFIsTiled(TIFF *tiffptr)
}
declare 30 generic {
    int TIFFIsByteSwapped(TIFF *tiffptr)
}
declare 31 generic {
    int TIFFIsUpSampled(TIFF *tiffptr)
}
declare 32 generic {
    int TIFFIsMSB2LSB(TIFF *tiffptr)
}
declare 33 generic {
    uint32 TIFFCurrentRow(TIFF *tiffptr)
}
declare 34 generic {
    tdir_t TIFFCurrentDirectory(TIFF *tiffptr)
}
declare 35 generic {
    tdir_t TIFFNumberOfDirectories(TIFF *tiffptr)
}
declare 36 generic {
    uint32 TIFFCurrentDirOffset(TIFF *tiffptr)
}
declare 37 generic {
    tstrip_t TIFFCurrentStrip(TIFF *tiffptr)
}
declare 38 generic {
    ttile_t TIFFCurrentTile(TIFF *tiffptr)
}
declare 39 generic {
    int TIFFReadBufferSetup(TIFF *tiffptr, tdata_t a, tsize_t b)
}
declare 40 generic {
    int TIFFWriteBufferSetup(TIFF *tiffptr, tdata_t a, tsize_t b)
}
declare 41 generic {
    int TIFFWriteCheck(TIFF *tiffptr, int a, const char *b)
}
declare 42 generic {
    int TIFFCreateDirectory(TIFF *tiffptr)
}
declare 43 generic {
    int TIFFLastDirectory(TIFF *tiffptr)
}
declare 44 generic {
    int TIFFSetDirectory(TIFF *tiffptr, tdir_t a)
}
declare 45 generic {
    int TIFFSetSubDirectory(TIFF *tiffptr, uint32 a)
}
declare 46 generic {
    int TIFFUnlinkDirectory(TIFF *tiffptr, tdir_t a)
}
declare 47 generic {
    int TIFFSetField(TIFF *tiffptr, ttag_t a, ...)
}
declare 48 generic {
    int TIFFVSetField(TIFF *tiffptr, ttag_t a, va_list b)
}
declare 49 generic {
    int TIFFWriteDirectory(TIFF *tiffptr)
}
declare 50 generic {
    int TIFFReassignTagToIgnore(enum TIFFIgnoreSense a, int b)
}
declare 51 generic {
    void TIFFPrintDirectory(TIFF *tiffptr, FILE *a, long b)
}
declare 52 generic {
    int TIFFReadScanline(TIFF *tiffptr, tdata_t a, uint32 b, tsample_t c)
}
declare 53 generic {
    int TIFFWriteScanline(TIFF *tiffptr, tdata_t a, uint32 b, tsample_t c)
}
declare 54 generic {
    int TIFFReadRGBAImage(TIFF *tiffptr, uint32 a, uint32 b, uint32 *c, int d)
}
declare 55 generic {
    int TIFFReadRGBAStrip(TIFF *tiffptr, tstrip_t a, uint32 *b)
}
declare 56 generic {
    int TIFFReadRGBATile(TIFF *tiffptr, uint32 a, uint32 b, uint32 *c)
}
declare 57 generic {
    int TIFFRGBAImageOK(TIFF *tiffptr, char *a)
}
declare 58 generic {
    int TIFFRGBAImageBegin(TIFFRGBAImage *a, TIFF *tiffptr, int b, char *c)
}
declare 59 generic {
    int TIFFRGBAImageGet(TIFFRGBAImage *d, uint32 *c, uint32 b, uint32 a)
}
declare 60 generic {
    void TIFFRGBAImageEnd(TIFFRGBAImage *a)
}
declare 61 generic {
    TIFF *TIFFOpen(const char *b, const char *a)
}
declare 62 generic {
    TIFF *TIFFFdOpen(int a, const char *b, const char *c)
}
declare 63 generic {
    TIFF *TIFFClientOpen(const char *a, const char *b,
	    thandle_t c,
	    TIFFReadWriteProc d, TIFFReadWriteProc e,
	    TIFFSeekProc f, TIFFCloseProc g,
	    TIFFSizeProc h,
	    TIFFMapFileProc i, TIFFUnmapFileProc j)
}
declare 64 generic {
    const char *TIFFFileName(TIFF *tiffptr)
}
declare 65 generic {
    void TIFFError(const char *a, const char *b, ...)
}
declare 66 generic {
    void TIFFWarning(const char *a, const char *b, ...)
}
declare 67 generic {
    TIFFErrorHandler TIFFSetErrorHandler(TIFFErrorHandler a)
}
declare 68 generic {
    TIFFErrorHandler TIFFSetWarningHandler(TIFFErrorHandler a)
}
declare 69 generic {
    TIFFExtendProc TIFFSetTagExtender(TIFFExtendProc a)
}
declare 70 generic {
    ttile_t TIFFComputeTile(TIFF *tiffptr, uint32 a, uint32 b, uint32 c, tsample_t d)
}
declare 71 generic {
    int TIFFCheckTile(TIFF *tiffptr, uint32 d, uint32 c, uint32 b, tsample_t a)
}
declare 72 generic {
    ttile_t TIFFNumberOfTiles(TIFF *tiffptr)
}
declare 73 generic {
    tsize_t TIFFReadTile(TIFF *tiffptr,
	    tdata_t a, uint32 b, uint32 c, uint32 d, tsample_t e)
}
declare 74 generic {
    tsize_t TIFFWriteTile(TIFF *tiffptr,
	    tdata_t e, uint32 d, uint32 c, uint32 b, tsample_t a)
}
declare 75 generic {
    tstrip_t TIFFComputeStrip(TIFF *tiffptr, uint32 a, tsample_t b)
}
declare 76 generic {
    tstrip_t TIFFNumberOfStrips(TIFF *tiffptr)
}
declare 77 generic {
    tsize_t TIFFReadEncodedStrip(TIFF *tiffptr, tstrip_t a, tdata_t b, tsize_t c)
}
declare 78 generic {
    tsize_t TIFFReadRawStrip(TIFF *tiffptr, tstrip_t a, tdata_t b, tsize_t c)
}
declare 79 generic {
    tsize_t TIFFReadEncodedTile(TIFF *tiffptr, ttile_t a, tdata_t b, tsize_t c)
}
declare 80 generic {
    tsize_t TIFFReadRawTile(TIFF *tiffptr, ttile_t c, tdata_t b, tsize_t a)
}
declare 81 generic {
    tsize_t TIFFWriteEncodedStrip(TIFF *tiffptr, tstrip_t a, tdata_t b, tsize_t c)
}
declare 82 generic {
    tsize_t TIFFWriteRawStrip(TIFF *tiffptr, tstrip_t a, tdata_t b, tsize_t c)
}
declare 83 generic {
    tsize_t TIFFWriteEncodedTile(TIFF *tiffptr, ttile_t a, tdata_t b, tsize_t c)
}
declare 84 generic {
    tsize_t TIFFWriteRawTile(TIFF *tiffptr, ttile_t c, tdata_t b, tsize_t a)
}
declare 85 generic {
    void TIFFSetWriteOffset(TIFF *tiffptr, toff_t a)
}
declare 86 generic {
    void TIFFSwabShort(uint16 *a)
}
declare 87 generic {
    void TIFFSwabLong(uint32 *a)
}
declare 88 generic {
    void TIFFSwabDouble(double *a)
}
declare 89 generic {
    void TIFFSwabArrayOfShort(uint16 *a, unsigned long b)
}
declare 90 generic {
    void TIFFSwabArrayOfLong(uint32 *b, unsigned long a)
}
declare 91 generic {
    void TIFFSwabArrayOfDouble(double *a, unsigned long b)
}
declare 92 generic {
    void TIFFReverseBits(unsigned char *a, unsigned long b)
}
declare 93 generic {
    const unsigned char *TIFFGetBitRevTable(int a)
}

# Source: tif_predict.h ...
declare 100 generic {
    int TIFFPredictorInit(TIFF *tiffptr)
}

# Source: tif_dir.h ...
declare 110 generic {
    void _TIFFSetupFieldInfo(TIFF *tiffptr, const TIFFFieldInfo a[], size_t b)
}
declare 111 generic {
    void TIFFMergeFieldInfo(TIFF *tiffptr, const TIFFFieldInfo *a, int b)
}
declare 112 generic {
    void _TIFFPrintFieldInfo(TIFF *tiffptr, FILE *a)
}
declare 113 generic {
    const TIFFFieldInfo *TIFFFindFieldInfo(TIFF *tiffptr, ttag_t a, TIFFDataType b)
}
declare 114 generic {
    const TIFFFieldInfo *TIFFFieldWithTag(TIFF *tiffptr, ttag_t a)
}
declare 115 generic {
    TIFFDataType _TIFFSampleToTagType(TIFF *tiffptr)
}


# Source: tiffiop.h ...

declare 120 generic {
    int _TIFFgetMode(const char *a, const char *b)
}
declare 121 generic {
    int _TIFFNoRowEncode(TIFF *tiffptr, tidata_t a, tsize_t b, tsample_t c)
}
declare 122 generic {
    int _TIFFNoStripEncode(TIFF *tiffptr, tidata_t c, tsize_t b, tsample_t a)
}
declare 123 generic {
    int _TIFFNoTileEncode(TIFF *tiffptr, tidata_t a, tsize_t b, tsample_t c)
}
declare 124 generic {
    int _TIFFNoRowDecode(TIFF *tiffptr, tidata_t c, tsize_t b, tsample_t a)
}
declare 125 generic {
    int _TIFFNoStripDecode(TIFF *tiffptr, tidata_t a, tsize_t b, tsample_t c)
}
declare 126 generic {
    int _TIFFNoTileDecode(TIFF *tiffptr, tidata_t c, tsize_t b, tsample_t a)
}
declare 127 generic {
    void _TIFFNoPostDecode(TIFF *tiffptr, tidata_t a, tsize_t b)
}
declare 128 generic {
    int  _TIFFNoPreCode (TIFF *tiffptr, tsample_t a)
}
declare 129 generic {
    int _TIFFNoSeek(TIFF *tiffptr, uint32 a)
}
declare 130 generic {
    void _TIFFSwab16BitData(TIFF *tiffptr, tidata_t a, tsize_t b)
}
declare 131 generic {
    void _TIFFSwab32BitData(TIFF *tiffptr, tidata_t b, tsize_t a)
}
declare 132 generic {
    void _TIFFSwab64BitData(TIFF *tiffptr, tidata_t a, tsize_t b)
}
declare 133 generic {
    int TIFFFlushData1(TIFF *tiffptr)
}
declare 134 generic {
    void TIFFFreeDirectory(TIFF *tiffptr)
}
declare 135 generic {
    int TIFFDefaultDirectory(TIFF *tiffptr)
}
declare 136 generic {
    int TIFFSetCompressionScheme(TIFF *tiffptr, int a)
}
declare 137 generic {
    void _TIFFSetDefaultCompressionState(TIFF *tiffptr)
}
declare 138 generic {
    uint32 _TIFFDefaultStripSize(TIFF *tiffptr, uint32 a)
}
declare 139 generic {
    void _TIFFDefaultTileSize(TIFF *tiffptr, uint32 *a, uint32 *b)
}
declare 140 generic {
    void _TIFFsetByteArray(void **a, void *b, uint32 c)
}
declare 141 generic {
    void _TIFFsetString(char **a, char *b)
}
declare 142 generic {
    void _TIFFsetShortArray(uint16 **a, uint16 *b, uint32 c)
}
declare 143 generic {
    void _TIFFsetLongArray(uint32 **a, uint32 *b, uint32 c)
}
declare 144 generic {
    void _TIFFsetFloatArray(float **a, float *b, uint32 c)
}
declare 145 generic {
    void _TIFFsetDoubleArray(double **a, double *b, uint32 c)
}
declare 146 generic {
    void _TIFFprintAscii(FILE *a, const char *b)
}
declare 147 generic {
    void _TIFFprintAsciiTag(FILE *a, const char *b, const char *c)
}
declare 148 generic {
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
