/*
 * "$Id: gimp-print-internal.h,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
 *
 *   Print plug-in header file for the GIMP.
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com) and
 *	Robert Krawitz (rlk@alum.mit.edu)
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 2 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Revision History:
 *
 *   See ChangeLog
 */

/*
 * This file must include only standard C header files.  The core code must
 * compile on generic platforms that don't support glib, gimp, gtk, etc.
 */

#ifndef _GIMP_PRINT_INTERNAL_H_
#define _GIMP_PRINT_INTERNAL_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*
 * ECOLOR_K must be 0
 */
#define ECOLOR_K  0
#define ECOLOR_C  1
#define ECOLOR_M  2
#define ECOLOR_Y  3
#define ECOLOR_LC 4
#define ECOLOR_LM 5
#define ECOLOR_LY 6
#define NCOLORS (4)
#define MAX_WEAVE (8)

typedef struct
{
  double value;
  unsigned bit_pattern;
  int subchannel;
  unsigned dot_size;
} stp_simple_dither_range_t;

typedef struct
{
  double value;
  double lower;
  double upper;
  unsigned bit_pattern;
  int subchannel;
  unsigned dot_size;
} stp_dither_range_t;

typedef struct
{
   double value[2];
   unsigned bits[2];
   int subchannel[2];
} stp_full_dither_range_t;

typedef struct
{
  unsigned subchannel_count;
  unsigned char **c;
} stp_channel_t;

typedef struct
{
  unsigned channel_count;
  stp_channel_t *c;
} stp_dither_data_t;


typedef struct			/* Weave parameters for a specific row */
{
  int row;			/* Absolute row # */
  int pass;			/* Computed pass # */
  int jet;			/* Which physical nozzle we're using */
  int missingstartrows;		/* Phantom rows (nonexistent rows that */
				/* would be printed by nozzles lower than */
				/* the first nozzle we're using this pass; */
				/* with the current algorithm, always zero */
  int logicalpassstart;		/* Offset in rows (from start of image) */
				/* that the printer must be for this row */
				/* to print correctly with the specified jet */
  int physpassstart;		/* Offset in rows to the first row printed */
				/* in this pass.  Currently always equal to */
				/* logicalpassstart */
  int physpassend;		/* Offset in rows (from start of image) to */
				/* the last row that will be printed this */
				/* pass (assuming that we're printing a full */
				/* pass). */
} stp_weave_t;

typedef struct			/* Weave parameters for a specific pass */
{
  int pass;			/* Absolute pass number */
  int missingstartrows;		/* All other values the same as weave_t */
  int logicalpassstart;
  int physpassstart;
  int physpassend;
  int subpass;
} stp_pass_t;

typedef struct {		/* Offsets from the start of each line */
  int ncolors;
  unsigned long *v;		/* (really pass) */
} stp_lineoff_t;

typedef struct {		/* Is this line (really pass) active? */
  int ncolors;
  char *v;
} stp_lineactive_t;

typedef struct {		/* number of rows for a pass */
  int ncolors;
  int *v;
} stp_linecount_t;

typedef struct {		/* Base pointers for each pass */
  int ncolors;
  unsigned char **v;
} stp_linebufs_t;

typedef struct stp_softweave
{
  stp_linebufs_t *linebases;	/* Base address of each row buffer */
  stp_lineoff_t *lineoffsets;	/* Offsets within each row buffer */
  stp_lineactive_t *lineactive;	/* Does this line have anything printed? */
  stp_linecount_t *linecounts;	/* How many rows we've printed this pass */
  stp_pass_t *passes;		/* Circular list of pass numbers */
  int last_pass_offset;		/* Starting row (offset from the start of */
				/* the page) of the most recently printed */
				/* pass (so we can determine how far to */
				/* advance the paper) */
  int last_pass;		/* Number of the most recently printed pass */

  int jets;			/* Number of jets per color */
  int virtual_jets;		/* Number of jets per color, taking into */
				/* account the head offset */
  int separation;		/* Offset from one jet to the next in rows */
  void *weaveparm;		/* Weave calculation parameter block */

  int horizontal_weave;		/* Number of horizontal passes required */
				/* This is > 1 for some of the ultra-high */
				/* resolution modes */
  int vertical_subpasses;	/* Number of passes per line (for better */
				/* quality) */
  int vmod;			/* Number of banks of passes */
  int oversample;		/* Excess precision per row */
  int repeat_count;		/* How many times a pass is repeated */
  int ncolors;			/* How many colors */
  int linewidth;		/* Line width in input pixels */
  int vertical_height;		/* Image height in output pixels */
  int firstline;		/* Actual first line (referenced to paper) */

  int bitwidth;			/* Bits per pixel */
  int lineno;
  int vertical_oversample;	/* Vertical oversampling */
  int current_vertical_subpass;
  int horizontal_width;		/* Horizontal width, in bits */
  int *head_offset;		/* offset of printheads */
  unsigned char *s[MAX_WEAVE];
  unsigned char *fold_buf;
  unsigned char *comp_buf;
  stp_weave_t wcache;
  int rcache;
  int vcache;
  stp_vars_t v;
  void (*flushfunc)(struct stp_softweave *sw, int passno, int model,
		    int width, int hoffset, int ydpi, int xdpi,
		    int physical_xdpi, int vertical_subpass);
  void (*fill_start)(struct stp_softweave *sw, int row, int subpass,
		     int width, int missingstartrows, int color);
  int (*pack)(const unsigned char *in, int bytes,
	      unsigned char *out, unsigned char **optr);
  int (*compute_linewidth)(const struct stp_softweave *sw, int n);
} stp_softweave_t;

typedef struct stp_dither_matrix_short
{
  int x;
  int y;
  int bytes;
  int prescaled;
  const unsigned short *data;
} stp_dither_matrix_short_t;

typedef struct stp_dither_matrix_normal
{
  int x;
  int y;
  int bytes;
  int prescaled;
  const unsigned *data;
} stp_dither_matrix_normal_t;

typedef struct stp_dither_matrix
{
  int x;
  int y;
  int bytes;
  int prescaled;
  const void *data;
} stp_dither_matrix_t;

/*
 * Prototypes...
 */

extern void	stp_set_driver_data (stp_vars_t vv, void * val);
extern void * 	stp_get_driver_data (const stp_vars_t vv);

extern void	stp_set_verified(stp_vars_t vv, int value);
extern int	stp_get_verified(stp_vars_t vv);

extern void     stp_copy_options(stp_vars_t vd, const stp_vars_t vs);

extern void	stp_default_media_size(const stp_printer_t printer,
				       const stp_vars_t v, int *width,
				       int *height);

extern void *	stp_init_dither(int in_width, int out_width,
				int horizontal_aspect,
				int vertical_aspect, stp_vars_t vars);
extern void	stp_dither_set_iterated_matrix(void *vd, size_t edge,
					       size_t iterations,
					       const unsigned *data,
					       int prescaled,
					       int x_shear, int y_shear);
extern void	stp_dither_set_matrix(void *vd, const stp_dither_matrix_t *mat,
				      int transpose, int x_shear, int y_shear);
extern void	stp_dither_set_transition(void *vd, double);
extern void	stp_dither_set_density(void *vd, double);
extern void	stp_dither_set_black_density(void *vd, double);
extern void 	stp_dither_set_black_lower(void *vd, double);
extern void 	stp_dither_set_black_upper(void *vd, double);
extern void	stp_dither_set_black_level(void *vd, int color, double);
extern void 	stp_dither_set_randomizer(void *vd, int color, double);
extern void 	stp_dither_set_ink_darkness(void *vd, int color, double);
extern void 	stp_dither_set_light_ink(void *vd, int color, double, double);
extern void	stp_dither_set_ranges(void *vd, int color, int nlevels,
				      const stp_simple_dither_range_t *ranges,
				      double density);
extern void	stp_dither_set_ranges_full(void *vd, int color, int nlevels,
					   const stp_full_dither_range_t *ranges,
					   double density);
extern void	stp_dither_set_ranges_simple(void *vd, int color, int nlevels,
					     const double *levels,
					     double density);
extern void	stp_dither_set_ranges_complete(void *vd, int color, int nlevels,
					       const stp_dither_range_t *ranges);
extern void	stp_dither_set_ink_spread(void *vd, int spread);
extern void	stp_dither_set_x_oversample(void *vd, int os);
extern void	stp_dither_set_y_oversample(void *vd, int os);
extern void	stp_dither_set_adaptive_limit(void *vd, double limit);
extern int	stp_dither_get_first_position(void *vd, int color, int dark);
extern int	stp_dither_get_last_position(void *vd, int color, int dark);


extern void	stp_free_dither(void *);

extern stp_dither_data_t *stp_create_dither_data(void);
extern void	stp_add_channel(stp_dither_data_t *d, unsigned char *data,
				unsigned channel, unsigned subchannel);
extern void	stp_free_dither_data(stp_dither_data_t *d);

extern void	stp_dither(const unsigned short *, int, void *,
			   stp_dither_data_t *, int duplicate_line,
			   int zero_mask);

extern void	stp_fold(const unsigned char *line, int single_height,
			 unsigned char *outbuf);

extern void	stp_split_2(int height, int bits, const unsigned char *in,
			    unsigned char *outhi, unsigned char *outlo);

extern void	stp_split_4(int height, int bits, const unsigned char *in,
			    unsigned char *out0, unsigned char *out1,
			    unsigned char *out2, unsigned char *out3);

extern void	stp_unpack_2(int height, int bits, const unsigned char *in,
			     unsigned char *outlo, unsigned char *outhi);

extern void	stp_unpack_4(int height, int bits, const unsigned char *in,
			     unsigned char *out0, unsigned char *out1,
			     unsigned char *out2, unsigned char *out3);

extern void	stp_unpack_8(int height, int bits, const unsigned char *in,
			     unsigned char *out0, unsigned char *out1,
			     unsigned char *out2, unsigned char *out3,
			     unsigned char *out4, unsigned char *out5,
			     unsigned char *out6, unsigned char *out7);

extern int	stp_pack_tiff(const unsigned char *line, int height,
			      unsigned char *comp_buf,
			      unsigned char **comp_ptr);

extern int	stp_pack_uncompressed(const unsigned char *line, int height,
				      unsigned char *comp_buf,
				      unsigned char **comp_ptr);

extern void *stp_initialize_weave(int jets, int separation, int oversample,
				  int horizontal, int vertical,
				  int ncolors, int width, int linewidth,
				  int lineheight,
				  int first_line, int phys_lines, int strategy,
                                  int *head_offset,  /* Get from model - used for 480/580 printers */
				  stp_vars_t v,
				  void (*flushfunc)(stp_softweave_t *sw,
						    int passno, int model,
						    int width, int hoffset,
						    int ydpi, int xdpi,
						    int physical_xdpi,
						    int vertical_subpass),
				  void (*fill_start)(stp_softweave_t *sw,
						     int row,
						     int subpass, int width,
						     int missingstartrows,
						     int vertical_subpass),
				  int (*pack)(const unsigned char *in,
					      int bytes, unsigned char *out,
					      unsigned char **optr),
				  int (*compute_linewidth)(const stp_softweave_t *sw,
							   int n));

extern void stp_fill_tiff(stp_softweave_t *sw, int row, int subpass,
			  int width, int missingstartrows, int color);
extern void stp_fill_uncompressed(stp_softweave_t *sw, int row, int subpass,
				  int width, int missingstartrows, int color);

extern int stp_compute_tiff_linewidth(const stp_softweave_t *sw, int n);
extern int stp_compute_uncompressed_linewidth(const stp_softweave_t *sw, int n);

extern int stp_start_job(const stp_printer_t printer,
			 stp_image_t *image, const stp_vars_t v);
extern int stp_end_job(const stp_printer_t printer,
		       stp_image_t *image, const stp_vars_t v);

extern void stp_flush_all(void *, int model, int width, int hoffset,
			  int ydpi, int xdpi, int physical_xdpi);

extern void
stp_write_weave(void *        vsw,
		int           length,	/* I - Length of bitmap data */
		int           ydpi,	/* I - Vertical resolution */
		int           model,	/* I - Printer model */
		int           width,	/* I - Printed width */
		int           offset,	/* I - Offset from left side of page */
		int		xdpi,
		int		physical_xdpi,
		unsigned char *const cols[]);

extern stp_lineoff_t *
stp_get_lineoffsets_by_pass(const stp_softweave_t *sw, int pass);

extern stp_lineactive_t *
stp_get_lineactive_by_pass(const stp_softweave_t *sw, int pass);

extern stp_linecount_t *
stp_get_linecount_by_pass(const stp_softweave_t *sw, int pass);

extern const stp_linebufs_t *
stp_get_linebases_by_pass(const stp_softweave_t *sw, int pass);

extern stp_pass_t *
stp_get_pass_by_pass(const stp_softweave_t *sw, int pass);

extern void
stp_weave_parameters_by_row(const stp_softweave_t *sw, int row,
			    int vertical_subpass, stp_weave_t *w);

extern void stp_destroy_weave(void *);

extern void stp_destroy_weave_params(void *vw);

extern int
stp_verify_printer_params(const stp_printer_t, const stp_vars_t);

extern void stp_zprintf(const stp_vars_t v, const char *format, ...);

extern void stp_zfwrite(const char *buf, size_t bytes, size_t nitems,
			const stp_vars_t v);

extern void stp_putc(int ch, const stp_vars_t v);
extern void stp_erputc(int ch);

extern void stp_puts(const char *s, const stp_vars_t v);

extern void stp_eprintf(const stp_vars_t v, const char *format, ...);
extern void stp_erprintf(const char *format, ...);

#define STP_DBG_LUT 		0x1
#define STP_DBG_COLORFUNC	0x2
#define STP_DBG_INK		0x4
#define STP_DBG_PS		0x8
#define STP_DBG_PCL		0x10
#define STP_DBG_ESCP2		0x20
#define STP_DBG_CANON		0x40
#define STP_DBG_LEXMARK		0x80
#define STP_DBG_WEAVE_PARAMS	0x100
#define STP_DBG_ROWS		0x200
#define STP_DBG_MARK_FILE       0x400
extern void stp_dprintf(unsigned long level, const stp_vars_t v,
			const char *format, ...);
extern void stp_deprintf(unsigned long level, const char *format, ...);
extern unsigned long stp_debug_level;

extern void *stp_malloc (size_t);
extern void *stp_zalloc (size_t);
extern void *stp_realloc (void *ptr, size_t);
extern void stp_free(void *ptr);

/* Uncomment the next line to get performance statistics:
 * look for QUANT(#) in the code. At the end of escp2-print
 * run, it will print out how long and how many time did
 * certain pieces of code take. Of course, don't forget about
 * overhead of call to gettimeofday - it's not zero.
 * If you need more detailed performance stats, just put
 * QUANT() calls in the interesting spots in the code */
/*#define QUANTIFY*/
#ifdef QUANTIFY
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#else
#define QUANT(n)
#endif

#ifdef QUANTIFY
/* Used for performance analysis - to be called before and after
 * the interval to be quantified */
#define NUM_QUANTIFY_BUCKETS 1024
extern unsigned quantify_counts[NUM_QUANTIFY_BUCKETS];
extern struct timeval quantify_buckets[NUM_QUANTIFY_BUCKETS];
extern int quantify_high_index;
extern int quantify_first_time;
extern struct timeval quantify_cur_time;
extern struct timeval quantify_prev_time;

#define QUANT(number) \
{\
    gettimeofday(&quantify_cur_time, NULL);\
    assert(number < NUM_QUANTIFY_BUCKETS);\
    quantify_counts[number]++;\
\
    if (quantify_first_time) {\
        quantify_first_time = 0;\
    } else {\
        if (number > quantify_high_index) quantify_high_index = number;\
        if (quantify_prev_time.tv_usec > quantify_cur_time.tv_usec) {\
           quantify_buckets[number].tv_usec += ((quantify_cur_time.tv_usec + 1000000) - quantify_prev_time.tv_usec);\
           quantify_buckets[number].tv_sec += (quantify_cur_time.tv_sec - quantify_prev_time.tv_sec - 1);\
        } else {\
           quantify_buckets[number].tv_sec += quantify_cur_time.tv_sec - quantify_prev_time.tv_sec;\
           quantify_buckets[number].tv_usec += quantify_cur_time.tv_usec - quantify_prev_time.tv_usec;\
        }\
        if (quantify_buckets[number].tv_usec >= 1000000)\
        {\
           quantify_buckets[number].tv_usec -= 1000000;\
           quantify_buckets[number].tv_sec++;\
        }\
    }\
\
    gettimeofday(&quantify_prev_time, NULL);\
}

extern void  print_timers(void );
#endif

#ifdef __cplusplus
  }
#endif

#endif /* _GIMP_PRINT_INTERNAL_H_ */
/*
 * End of "$Id: gimp-print-internal.h,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $".
 */
