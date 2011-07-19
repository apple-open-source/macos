# jpegtcl.decls -- -*- tcl -*-
#
# This file contains the declarations for all supported public functions
# that are exported by the JPEGTCL library via the stubs table. This file
# is used to generate the jpegtclDecls.h/jpegtclStubsLib.c/jpegtclStubsInit.c
# files.
#

# Declare each of the functions in the public BLT interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.

library jpegtcl

# Define the JPEGTCL interface:

interface jpegtcl
scspec JPEGTCLAPI

#########################################################################
###  JPEG interface

declare 0 {
    struct jpeg_error_mgr *jpeg_std_error(struct jpeg_error_mgr *err)
}
declare 1 {
    void jpeg_CreateCompress(j_compress_ptr cinfo, int version, size_t structsize)
}
declare 2 {
    void jpeg_CreateDecompress(j_decompress_ptr cinfo, int version, size_t structsize)
}
declare 3 {
    void jpeg_destroy_compress(j_compress_ptr cinfo)
}
declare 4 {
    void jpeg_destroy_decompress(j_decompress_ptr cinfo)
}
declare 5 {
    void jpeg_stdio_dest(j_compress_ptr cinfo, FILE *outfile)
}
declare 6 {
    void jpeg_stdio_src(j_decompress_ptr cinfo, FILE *infile)
}
declare 7 {
    void jpeg_set_defaults(j_compress_ptr cinfo)
}
declare 8 {
    void jpeg_set_colorspace(j_compress_ptr cinfo, J_COLOR_SPACE colorspace)
}
declare 9 {
    void jpeg_default_colorspace(j_compress_ptr cinfo)
}
declare 10 {
    void jpeg_set_quality(j_compress_ptr cinfo, int quality, boolean force_baseline)
}
declare 11 {
    void jpeg_set_linear_quality(j_compress_ptr cinfo, int scale_factor,
	boolean force_baseline)
}
declare 12 {
    void jpeg_add_quant_table(j_compress_ptr cinfo, int which_tbl,
	const unsigned int *basic_table, int scale_factor, boolean force_baseline)
}
declare 13 {
    int jpeg_quality_scaling(int quality)
}
declare 14 {
    void jpeg_simple_progression(j_compress_ptr cinfo)
}
declare 15 {
    void jpeg_suppress_tables(j_compress_ptr cinfo, boolean suppress)
}
declare 16 {
    JQUANT_TBL *jpeg_alloc_quant_table(j_common_ptr cinfo)
}
declare 17 {
    JHUFF_TBL *jpeg_alloc_huff_table(j_common_ptr cinfo)
}
declare 18 {
    void jpeg_start_compress(j_compress_ptr cinfo, boolean write_all_tables)
}
declare 19 {
    JDIMENSION jpeg_write_scanlines(j_compress_ptr cinfo, JSAMPARRAY scanlines,
	JDIMENSION num_lines)
}
declare 20 {
    void jpeg_finish_compress(j_compress_ptr cinfo)
}
declare 21 {
    JDIMENSION jpeg_write_raw_data(j_compress_ptr cinfo, JSAMPIMAGE data,
	JDIMENSION num_lines)
}
declare 22 {
    void jpeg_write_marker(j_compress_ptr cinfo, int marker,
	const JOCTET *dataptr, unsigned int datalen)
}
declare 23 {
    void jpeg_write_m_header(j_compress_ptr cinfo, int marker, unsigned int datalen)
}
declare 24 {
    void jpeg_write_m_byte(j_compress_ptr cinfo, int val)
}
declare 25 {
    void jpeg_write_tables(j_compress_ptr cinfo)
}
declare 26 {
    int jpeg_read_header(j_decompress_ptr cinfo, boolean require_image)
}
declare 27 {
    boolean jpeg_start_decompress(j_decompress_ptr cinfo)
}
declare 28 {
    JDIMENSION jpeg_read_scanlines(j_decompress_ptr cinfo, JSAMPARRAY scanlines,
	JDIMENSION max_lines)
}
declare 29 {
    boolean jpeg_finish_decompress(j_decompress_ptr cinfo)
}
declare 30 {
    JDIMENSION jpeg_read_raw_data(j_decompress_ptr cinfo, JSAMPIMAGE data,
	JDIMENSION max_lines)
}
declare 31 {
    boolean jpeg_has_multiple_scans(j_decompress_ptr cinfo)
}
declare 32 {
    boolean jpeg_start_output(j_decompress_ptr cinfo, int scan_number)
}
declare 33 {
    boolean jpeg_finish_output(j_decompress_ptr cinfo)
}
declare 34 {
    boolean jpeg_input_complete(j_decompress_ptr cinfo)
}
declare 35 {
    void jpeg_new_colormap(j_decompress_ptr cinfo)
}
declare 36 {
    int jpeg_consume_input(j_decompress_ptr cinfo)
}
declare 37 {
    void jpeg_calc_output_dimensions(j_decompress_ptr cinfo)
}
declare 38 {
    void jpeg_save_markers(j_decompress_ptr cinfo, int marker_code,
	unsigned int length_limit)
}
declare 39 {
    void jpeg_set_marker_processor(j_decompress_ptr cinfo, int marker_code,
	jpeg_marker_parser_method routine)
}
declare 40 {
    jvirt_barray_ptr *jpeg_read_coefficients(j_decompress_ptr cinfo)
}
declare 41 {
    void jpeg_write_coefficients(j_compress_ptr cinfo,
	jvirt_barray_ptr *coef_arrays)
}
declare 42 {
    void jpeg_copy_critical_parameters(j_decompress_ptr srcinfo,
	j_compress_ptr dstinfo)
}
declare 43 {
    void jpeg_abort_compress(j_compress_ptr cinfo)
}
declare 44 {
    void jpeg_abort_decompress(j_decompress_ptr cinfo)
}
declare 45 {
    void jpeg_abort(j_common_ptr cinfo)
}
declare 46 {
    void jpeg_destroy(j_common_ptr cinfo)
}
declare 47 {
    boolean jpeg_resync_to_restart(j_decompress_ptr cinfo, int desired)
}

#########################################################################
