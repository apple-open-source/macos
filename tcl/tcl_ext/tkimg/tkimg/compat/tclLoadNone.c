/*
 * tclLoadNone.c --
 *
 *	This procedure provides a version of the dlopen() for use
 *	in systems that don't support dynamic loading; It will
 *	work if the libraries libz.a, libpng.a, libjpeg.a,
 *	libtiff.a and libimg1.2.a are statically linked into
 *	a modified wish.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclLoadNone.c 1.5 96/02/15 11:43:01
 */

#include "tcl.h"
#include "compat/dlfcn.h"

extern int deflate();
extern int deflateInit_();
extern int deflateReset();
extern int deflateParams();
extern int deflateEnd();
extern int inflate();
extern int inflateInit_();
extern int inflateReset();
extern int inflateSync();
extern int inflateEnd();

extern int png_create_read_struct();
extern int png_create_info_struct();
extern int png_create_write_struct();
extern int png_destroy_read_struct();
extern int png_destroy_write_struct();
extern int png_error();
extern int png_get_channels();
extern int png_get_error_ptr();
extern int png_get_progressive_ptr();
extern int png_get_rowbytes();
extern int png_get_IHDR();
extern int png_get_valid();
extern int png_init_io();
extern int png_read_image();
extern int png_read_info();
extern int png_read_update_info();
extern int png_set_interlace_handling();
extern int png_set_read_fn();
extern int png_set_text();
extern int png_set_write_fn();
extern int png_set_IHDR();
extern int png_write_end();
extern int png_write_info();
extern int png_write_row();
extern int png_set_expand();
extern int png_set_filler();
extern int png_set_strip_16();
extern int png_get_sRGB();
extern int png_set_sRGB();
extern int png_get_gAMA();
extern int png_set_gAMA();
extern int png_set_gamma();
extern int png_set_sRGB_gAMA_and_cHRM();

extern int jpeg_CreateCompress();
extern int jpeg_abort_compress();
extern int jpeg_destroy_compress();
extern int jpeg_finish_compress();
extern int jpeg_suppress_tables();
extern int jpeg_write_marker();
extern int jpeg_write_tables();
extern int jpeg_start_compress();
extern int jpeg_write_raw_data();
extern int jpeg_write_scanlines();
extern int jpeg_gen_optimal_table();
extern int jpeg_make_c_derived_tbl();
extern int jpeg_abort();
extern int jpeg_alloc_huff_table();
extern int jpeg_alloc_quant_table();
extern int jpeg_destroy();
extern int jpeg_add_quant_table();
extern int jpeg_default_colorspace();
extern int jpeg_quality_scaling();
extern int jpeg_set_colorspace();
extern int jpeg_set_defaults();
extern int jpeg_set_linear_quality();
extern int jpeg_set_quality();
extern int jpeg_simple_progression();
extern int jpeg_copy_critical_parameters();
extern int jpeg_write_coefficients();
extern int jpeg_CreateDecompress();
extern int jpeg_abort_decompress();
extern int jpeg_consume_input();
extern int jpeg_destroy_decompress();
extern int jpeg_finish_decompress();
extern int jpeg_has_multiple_scans();
extern int jpeg_input_complete();
extern int jpeg_read_header();
extern int jpeg_set_marker_processor();
extern int jpeg_finish_output();
extern int jpeg_read_raw_data();
extern int jpeg_read_scanlines();
extern int jpeg_start_decompress();
extern int jpeg_start_output();
extern int jpeg_stdio_dest();
extern int jpeg_stdio_src();
extern int jpeg_fill_bit_buffer();
extern int jpeg_huff_decode();
extern int jpeg_make_d_derived_tbl();
extern int jpeg_resync_to_restart();
extern int jpeg_calc_output_dimensions();
extern int jpeg_new_colormap();
extern int jpeg_read_coefficients();
extern int jpeg_std_error();
extern int jpeg_fdct_float();
extern int jpeg_fdct_ifast();
extern int jpeg_fdct_islow();
extern int jpeg_idct_float();
extern int jpeg_idct_ifast();
extern int jpeg_idct_islow();
extern int jpeg_idct_1x1();
extern int jpeg_idct_2x2();
extern int jpeg_idct_4x4();
extern int jpeg_free_large();
extern int jpeg_free_small();
extern int jpeg_get_large();
extern int jpeg_get_small();
extern int jpeg_mem_available();
extern int jpeg_mem_init();
extern int jpeg_mem_term();
extern int jpeg_open_backing_store();
extern int TIFFGetFieldDefaulted();
extern int TIFFVGetFieldDefaulted();
extern int TIFFClose();
extern int TIFFFindCODEC();
extern int TIFFRegisterCODEC();
extern int TIFFSetCompressionScheme();
extern int TIFFUnRegisterCODEC();
extern int _TIFFNoPreCode();
extern int _TIFFNoRowDecode();
extern int _TIFFNoRowEncode();
extern int _TIFFNoSeek();
extern int _TIFFNoStripDecode();
extern int _TIFFNoStripEncode();
extern int _TIFFNoTileDecode();
extern int _TIFFNoTileEncode();
extern int TIFFCurrentDirOffset();
extern int TIFFDefaultDirectory();
extern int TIFFFreeDirectory();
extern int TIFFGetField();
extern int TIFFLastDirectory();
extern int TIFFSetDirectory();
extern int TIFFSetField();
extern int TIFFSetSubDirectory();
extern int TIFFSetTagExtender();
extern int TIFFUnlinkDirectory();
extern int TIFFVGetField();
extern int TIFFVSetField();
extern int _TIFFsetByteArray();
extern int _TIFFsetDoubleArray();
extern int _TIFFsetFloatArray();
extern int _TIFFsetLongArray();
extern int _TIFFsetShortArray();
extern int _TIFFsetString();
extern int _TIFFFieldWithTag();
extern int _TIFFFindFieldInfo();
extern int _TIFFMergeFieldInfo();
extern int _TIFFPrintFieldInfo();
extern int _TIFFSampleToTagType();
extern int _TIFFSetupFieldInfo();
extern int TIFFReadDirectory();
extern int TIFFWriteDirectory();
extern int TIFFError();
extern int TIFFSetErrorHandler();
extern int _TIFFFax3fillruns();
extern int TIFFRGBAImageBegin();
extern int TIFFRGBAImageEnd();
extern int TIFFRGBAImageGet();
extern int TIFFRGBAImageOK();
extern int TIFFReadRGBAImage();
extern int TIFFFlush();
extern int TIFFFlushData();
extern int TIFFClientOpen();
extern int TIFFCurrentDirectory();
extern int TIFFCurrentRow();
extern int TIFFCurrentStrip();
extern int TIFFCurrentTile();
extern int TIFFFileName();
extern int TIFFFileno();
extern int TIFFGetMode();
extern int TIFFIsByteSwapped();
extern int TIFFIsMSB2LSB();
extern int TIFFIsTiled();
extern int TIFFIsUpSampled();
extern int _TIFFgetMode();
extern int TIFFPredictorInit();
extern int TIFFPrintDirectory();
extern int _TIFFprintAscii();
extern int _TIFFprintAsciiTag();
extern int TIFFReadBufferSetup();
extern int TIFFReadEncodedStrip();
extern int TIFFReadEncodedTile();
extern int TIFFReadRawStrip();
extern int TIFFReadRawTile();
extern int TIFFReadScanline();
extern int TIFFReadTile();
extern int _TIFFNoPostDecode();
extern int _TIFFSwab16BitData();
extern int _TIFFSwab32BitData();
extern int _TIFFSwab64BitData();
extern int TIFFGetBitRevTable();
extern int TIFFReverseBits();
extern int TIFFSwabArrayOfDouble();
extern int TIFFSwabArrayOfLong();
extern int TIFFSwabArrayOfShort();
extern int TIFFSwabDouble();
extern int TIFFSwabLong();
extern int TIFFSwabShort();
extern int TIFFComputeStrip();
extern int TIFFDefaultStripSize();
extern int TIFFNumberOfStrips();
extern int TIFFRasterScanlineSize();
extern int TIFFScanlineSize();
extern int TIFFStripSize();
extern int TIFFVStripSize();
extern int _TIFFDefaultStripSize();
extern int TIFFCheckTile();
extern int TIFFComputeTile();
extern int TIFFDefaultTileSize();
extern int TIFFNumberOfTiles();
extern int TIFFTileRowSize();
extern int TIFFTileSize();
extern int TIFFVTileSize();
extern int _TIFFDefaultTileSize();
extern int TIFFFdOpen();
extern int TIFFOpen();
extern int _TIFFfree();
extern int _TIFFmalloc();
extern int _TIFFmemcmp();
extern int _TIFFmemcpy();
extern int _TIFFmemset();
extern int _TIFFrealloc();
extern int TIFFGetVersion();
extern int TIFFSetWarningHandler();
extern int TIFFWarning();
extern int TIFFFlushData1();
extern int TIFFSetWriteOffset();
extern int TIFFWriteBufferSetup();
extern int TIFFWriteEncodedStrip();
extern int TIFFWriteEncodedTile();
extern int TIFFWriteRawStrip();
extern int TIFFWriteRawTile();
extern int TIFFWriteScanline();
extern int TIFFWriteTile();


static struct {
  char * name;
  int (*value)();
}dictionary [] = {
  {"deflate", deflate},
  {"deflateInit_", deflateInit_},
  {"deflateReset", deflateReset},
  {"deflateParams", deflateParams},
  {"deflateEnd", deflateEnd},
  {"inflate", inflate},
  {"inflateInit_", inflateInit_},
  {"inflateReset", inflateReset},
  {"inflateSync", inflateSync},
  {"inflateEnd", inflateEnd},

  {"png_create_read_struct", png_create_read_struct},
  {"png_create_info_struct", png_create_info_struct},
  {"png_create_write_struct", png_create_write_struct},
  {"png_destroy_read_struct", png_destroy_read_struct},
  {"png_destroy_write_struct", png_destroy_write_struct},
  {"png_error", png_error},
  {"png_get_channels", png_get_channels},
  {"png_get_error_ptr", png_get_error_ptr},
  {"png_get_progressive_ptr", png_get_progressive_ptr},
  {"png_get_rowbytes", png_get_rowbytes},
  {"png_get_IHDR", png_get_IHDR},
  {"png_get_valid", png_get_valid},
  {"png_init_io", png_init_io},
  {"png_read_image", png_read_image},
  {"png_read_info", png_read_info},
  {"png_read_update_info", png_read_update_info},
  {"png_set_interlace_handling", png_set_interlace_handling},
  {"png_set_read_fn", png_set_read_fn},
  {"png_set_text", png_set_text},
  {"png_set_write_fn", png_set_write_fn},
  {"png_set_IHDR", png_set_IHDR},
  {"png_write_end", png_write_end},
  {"png_write_info", png_write_info},
  {"png_write_row", png_write_row},
  {"png_set_expand", png_set_expand},
  {"png_set_filler", png_set_filler},
  {"png_set_strip_16", png_set_strip_16},
  {"png_get_sRGB", png_get_sRGB},
  {"png_set_sRGB", png_set_sRGB},
  {"png_get_gAMA", png_get_gAMA},
  {"png_set_gAMA", png_set_gAMA},
  {"png_set_gamma", png_set_gamma},
  {"png_set_sRGB_gAMA_and_cHRM", png_set_sRGB_gAMA_and_cHRM},

  {"jpeg_CreateCompress", jpeg_CreateCompress},
  {"jpeg_abort_compress", jpeg_abort_compress},
  {"jpeg_destroy_compress", jpeg_destroy_compress},
  {"jpeg_finish_compress", jpeg_finish_compress},
  {"jpeg_suppress_tables", jpeg_suppress_tables},
  {"jpeg_write_marker", jpeg_write_marker},
  {"jpeg_write_tables", jpeg_write_tables},
  {"jpeg_start_compress", jpeg_start_compress},
  {"jpeg_write_raw_data", jpeg_write_raw_data},
  {"jpeg_write_scanlines", jpeg_write_scanlines},
  {"jpeg_gen_optimal_table", jpeg_gen_optimal_table},
  {"jpeg_make_c_derived_tbl", jpeg_make_c_derived_tbl},
  {"jpeg_abort", jpeg_abort},
  {"jpeg_alloc_huff_table", jpeg_alloc_huff_table},
  {"jpeg_alloc_quant_table", jpeg_alloc_quant_table},
  {"jpeg_destroy", jpeg_destroy},
  {"jpeg_add_quant_table", jpeg_add_quant_table},
  {"jpeg_default_colorspace", jpeg_default_colorspace},
  {"jpeg_quality_scaling", jpeg_quality_scaling},
  {"jpeg_set_colorspace", jpeg_set_colorspace},
  {"jpeg_set_defaults", jpeg_set_defaults},
  {"jpeg_set_linear_quality", jpeg_set_linear_quality},
  {"jpeg_set_quality", jpeg_set_quality},
  {"jpeg_simple_progression", jpeg_simple_progression},
  {"jpeg_copy_critical_parameters", jpeg_copy_critical_parameters},
  {"jpeg_write_coefficients", jpeg_write_coefficients},
  {"jpeg_CreateDecompress", jpeg_CreateDecompress},
  {"jpeg_abort_decompress", jpeg_abort_decompress},
  {"jpeg_consume_input", jpeg_consume_input},
  {"jpeg_destroy_decompress", jpeg_destroy_decompress},
  {"jpeg_finish_decompress", jpeg_finish_decompress},
  {"jpeg_has_multiple_scans", jpeg_has_multiple_scans},
  {"jpeg_input_complete", jpeg_input_complete},
  {"jpeg_read_header", jpeg_read_header},
  {"jpeg_set_marker_processor", jpeg_set_marker_processor},
  {"jpeg_finish_output", jpeg_finish_output},
  {"jpeg_read_raw_data", jpeg_read_raw_data},
  {"jpeg_read_scanlines", jpeg_read_scanlines},
  {"jpeg_start_decompress", jpeg_start_decompress},
  {"jpeg_start_output", jpeg_start_output},
  {"jpeg_stdio_dest", jpeg_stdio_dest},
  {"jpeg_stdio_src", jpeg_stdio_src},
  {"jpeg_fill_bit_buffer", jpeg_fill_bit_buffer},
  {"jpeg_huff_decode", jpeg_huff_decode},
  {"jpeg_make_d_derived_tbl", jpeg_make_d_derived_tbl},
  {"jpeg_resync_to_restart", jpeg_resync_to_restart},
  {"jpeg_calc_output_dimensions", jpeg_calc_output_dimensions},
  {"jpeg_new_colormap", jpeg_new_colormap},
  {"jpeg_read_coefficients", jpeg_read_coefficients},
  {"jpeg_std_error", jpeg_std_error},
  {"jpeg_fdct_float", jpeg_fdct_float},
  {"jpeg_fdct_ifast", jpeg_fdct_ifast},
  {"jpeg_fdct_islow", jpeg_fdct_islow},
  {"jpeg_idct_float", jpeg_idct_float},
  {"jpeg_idct_ifast", jpeg_idct_ifast},
  {"jpeg_idct_islow", jpeg_idct_islow},
  {"jpeg_idct_1x1", jpeg_idct_1x1},
  {"jpeg_idct_2x2", jpeg_idct_2x2},
  {"jpeg_idct_4x4", jpeg_idct_4x4},
  {"jpeg_free_large", jpeg_free_large},
  {"jpeg_free_small", jpeg_free_small},
  {"jpeg_get_large", jpeg_get_large},
  {"jpeg_get_small", jpeg_get_small},
  {"jpeg_mem_available", jpeg_mem_available},
  {"jpeg_mem_init", jpeg_mem_init},
  {"jpeg_mem_term", jpeg_mem_term},
  {"jpeg_open_backing_store", jpeg_open_backing_store},
  {"TIFFGetFieldDefaulted", TIFFGetFieldDefaulted},
  {"TIFFVGetFieldDefaulted", TIFFVGetFieldDefaulted},
  {"TIFFClose", TIFFClose},
  {"TIFFFindCODEC", TIFFFindCODEC},
  {"TIFFRegisterCODEC", TIFFRegisterCODEC},
  {"TIFFSetCompressionScheme", TIFFSetCompressionScheme},
  {"TIFFUnRegisterCODEC", TIFFUnRegisterCODEC},
  {"_TIFFNoPreCode", _TIFFNoPreCode},
  {"_TIFFNoRowDecode", _TIFFNoRowDecode},
  {"_TIFFNoRowEncode", _TIFFNoRowEncode},
  {"_TIFFNoSeek", _TIFFNoSeek},
  {"_TIFFNoStripDecode", _TIFFNoStripDecode},
  {"_TIFFNoStripEncode", _TIFFNoStripEncode},
  {"_TIFFNoTileDecode", _TIFFNoTileDecode},
  {"_TIFFNoTileEncode", _TIFFNoTileEncode},
  {"TIFFCurrentDirOffset", TIFFCurrentDirOffset},
  {"TIFFDefaultDirectory", TIFFDefaultDirectory},
  {"TIFFFreeDirectory", TIFFFreeDirectory},
  {"TIFFGetField", TIFFGetField},
  {"TIFFLastDirectory", TIFFLastDirectory},
  {"TIFFSetDirectory", TIFFSetDirectory},
  {"TIFFSetField", TIFFSetField},
  {"TIFFSetSubDirectory", TIFFSetSubDirectory},
  {"TIFFSetTagExtender", TIFFSetTagExtender},
  {"TIFFUnlinkDirectory", TIFFUnlinkDirectory},
  {"TIFFVGetField", TIFFVGetField},
  {"TIFFVSetField", TIFFVSetField},
  {"_TIFFsetByteArray", _TIFFsetByteArray},
  {"_TIFFsetDoubleArray", _TIFFsetDoubleArray},
  {"_TIFFsetFloatArray", _TIFFsetFloatArray},
  {"_TIFFsetLongArray", _TIFFsetLongArray},
  {"_TIFFsetShortArray", _TIFFsetShortArray},
  {"_TIFFsetString", _TIFFsetString},
  {"_TIFFFieldWithTag", _TIFFFieldWithTag},
  {"_TIFFFindFieldInfo", _TIFFFindFieldInfo},
  {"_TIFFMergeFieldInfo", _TIFFMergeFieldInfo},
  {"_TIFFPrintFieldInfo", _TIFFPrintFieldInfo},
  {"_TIFFSampleToTagType", _TIFFSampleToTagType},
  {"_TIFFSetupFieldInfo", _TIFFSetupFieldInfo},
  {"TIFFReadDirectory", TIFFReadDirectory},
  {"TIFFWriteDirectory", TIFFWriteDirectory},
  {"TIFFError", TIFFError},
  {"TIFFSetErrorHandler", TIFFSetErrorHandler},
  {"_TIFFFax3fillruns", _TIFFFax3fillruns},
  {"TIFFRGBAImageBegin", TIFFRGBAImageBegin},
  {"TIFFRGBAImageEnd", TIFFRGBAImageEnd},
  {"TIFFRGBAImageGet", TIFFRGBAImageGet},
  {"TIFFRGBAImageOK", TIFFRGBAImageOK},
  {"TIFFReadRGBAImage", TIFFReadRGBAImage},
  {"TIFFFlush", TIFFFlush},
  {"TIFFFlushData", TIFFFlushData},
  {"TIFFClientOpen", TIFFClientOpen},
  {"TIFFCurrentDirectory", TIFFCurrentDirectory},
  {"TIFFCurrentRow", TIFFCurrentRow},
  {"TIFFCurrentStrip", TIFFCurrentStrip},
  {"TIFFCurrentTile", TIFFCurrentTile},
  {"TIFFFileName", TIFFFileName},
  {"TIFFFileno", TIFFFileno},
  {"TIFFGetMode", TIFFGetMode},
  {"TIFFIsByteSwapped", TIFFIsByteSwapped},
  {"TIFFIsMSB2LSB", TIFFIsMSB2LSB},
  {"TIFFIsTiled", TIFFIsTiled},
  {"TIFFIsUpSampled", TIFFIsUpSampled},
  {"_TIFFgetMode", _TIFFgetMode},
  {"TIFFPredictorInit", TIFFPredictorInit},
  {"TIFFPrintDirectory", TIFFPrintDirectory},
  {"_TIFFprintAscii", _TIFFprintAscii},
  {"_TIFFprintAsciiTag", _TIFFprintAsciiTag},
  {"TIFFReadBufferSetup", TIFFReadBufferSetup},
  {"TIFFReadEncodedStrip", TIFFReadEncodedStrip},
  {"TIFFReadEncodedTile", TIFFReadEncodedTile},
  {"TIFFReadRawStrip", TIFFReadRawStrip},
  {"TIFFReadRawTile", TIFFReadRawTile},
  {"TIFFReadScanline", TIFFReadScanline},
  {"TIFFReadTile", TIFFReadTile},
  {"_TIFFNoPostDecode", _TIFFNoPostDecode},
  {"_TIFFSwab16BitData", _TIFFSwab16BitData},
  {"_TIFFSwab32BitData", _TIFFSwab32BitData},
  {"_TIFFSwab64BitData", _TIFFSwab64BitData},
  {"TIFFGetBitRevTable", TIFFGetBitRevTable},
  {"TIFFReverseBits", TIFFReverseBits},
  {"TIFFSwabArrayOfDouble", TIFFSwabArrayOfDouble},
  {"TIFFSwabArrayOfLong", TIFFSwabArrayOfLong},
  {"TIFFSwabArrayOfShort", TIFFSwabArrayOfShort},
  {"TIFFSwabDouble", TIFFSwabDouble},
  {"TIFFSwabLong", TIFFSwabLong},
  {"TIFFSwabShort", TIFFSwabShort},
  {"TIFFComputeStrip", TIFFComputeStrip},
  {"TIFFDefaultStripSize", TIFFDefaultStripSize},
  {"TIFFNumberOfStrips", TIFFNumberOfStrips},
  {"TIFFRasterScanlineSize", TIFFRasterScanlineSize},
  {"TIFFScanlineSize", TIFFScanlineSize},
  {"TIFFStripSize", TIFFStripSize},
  {"TIFFVStripSize", TIFFVStripSize},
  {"_TIFFDefaultStripSize", _TIFFDefaultStripSize},
  {"TIFFCheckTile", TIFFCheckTile},
  {"TIFFComputeTile", TIFFComputeTile},
  {"TIFFDefaultTileSize", TIFFDefaultTileSize},
  {"TIFFNumberOfTiles", TIFFNumberOfTiles},
  {"TIFFTileRowSize", TIFFTileRowSize},
  {"TIFFTileSize", TIFFTileSize},
  {"TIFFVTileSize", TIFFVTileSize},
  {"_TIFFDefaultTileSize", _TIFFDefaultTileSize},
  {"TIFFFdOpen", TIFFFdOpen},
  {"TIFFOpen", TIFFOpen},
  {"_TIFFfree", _TIFFfree},
  {"_TIFFmalloc", _TIFFmalloc},
  {"_TIFFmemcmp", _TIFFmemcmp},
  {"_TIFFmemcpy", _TIFFmemcpy},
  {"_TIFFmemset", _TIFFmemset},
  {"_TIFFrealloc", _TIFFrealloc},
  {"TIFFGetVersion", TIFFGetVersion},
  {"TIFFSetWarningHandler", TIFFSetWarningHandler},
  {"TIFFWarning", TIFFWarning},
  {"TIFFFlushData1", TIFFFlushData1},
  {"TIFFSetWriteOffset", TIFFSetWriteOffset},
  {"TIFFWriteBufferSetup", TIFFWriteBufferSetup},
  {"TIFFWriteEncodedStrip", TIFFWriteEncodedStrip},
  {"TIFFWriteEncodedTile", TIFFWriteEncodedTile},
  {"TIFFWriteRawStrip", TIFFWriteRawStrip},
  {"TIFFWriteRawTile", TIFFWriteRawTile},
  {"TIFFWriteScanline", TIFFWriteScanline},
  {"TIFFWriteTile", TIFFWriteTile},
  {0, 0}
};

/*
 *----------------------------------------------------------------------
 *
 * dlopen  --
 * dlsym   --
 * dlerror --
 * dlclose --
 *
 *	Dummy functions, in case our system doesn't support
 *	dynamic loading.
 *
 * Results:
 *	NULL for dlopen() and dlsym(). Error for other functions.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */
VOID *dlopen(path, mode)
    const char *path;
    int mode;
{
    return (VOID *) (dictionary[0].value != NULL);
}

VOID *dlsym(handle, symbol)
    VOID *handle;
    const char *symbol;
{
    int i;
    for (i = 0; dictionary[i].name != 0; ++i) {
      if (!strcmp(symbol, dictionary[i].name)) {
	return (VOID *) dictionary [i].value;
      }
    }
    return (VOID *) NULL;
}

char *dlerror()
{
    return "dynamic loading is not currently available on this system";
}

int dlclose(handle)
    VOID *handle;
{
    return 0;
}
