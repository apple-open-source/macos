/*
 *  $Id: gimp-print.h,v 1.1.1.1 2003/01/27 19:05:31 jlovell Exp $
 *
 *   Print plug-in header file for the GIMP.
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com) and
 *      Robert Krawitz (rlk@alum.mit.edu)
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

#ifndef __GIMP_PRINT_H__
#define __GIMP_PRINT_H__

/*
 * Include necessary header files...
 */

#include <stddef.h>     /* For size_t */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Library versioning details
 */

/*
 * compile-time version
 */
#define GIMPPRINT_MAJOR_VERSION       (4)
#define GIMPPRINT_MINOR_VERSION       (2)
#define GIMPPRINT_MICRO_VERSION       (5)
#define GIMPPRINT_CURRENT_INTERFACE   (2)
#define GIMPPRINT_BINARY_AGE          (1)
#define GIMPPRINT_INTERFACE_AGE       (0)
#define GIMPPRINT_CHECK_VERSION(major,minor,micro) \
  (GIMPPRINT_MAJOR_VERSION >  (major) || \
  (GIMPPRINT_MAJOR_VERSION == (major) && GIMPPRINT_MINOR_VERSION > (minor)) || \
  (GIMPPRINT_MAJOR_VERSION == (major) && GIMPPRINT_MINOR_VERSION == (minor) && \
   GIMPPRINT_MICRO_VERSION >= (micro)))

extern const unsigned int gimpprint_major_version;
extern const unsigned int gimpprint_minor_version;
extern const unsigned int gimpprint_micro_version;
extern const unsigned int gimpprint_current_interface;
extern const unsigned int gimpprint_binary_age;
extern const unsigned int gimpprint_interface_age;
extern const char* stp_check_version (unsigned int required_major,
                               unsigned int required_minor,
                               unsigned int required_micro);


/*
 * Constants...
 */

#define OUTPUT_GRAY             0       /* Grayscale output */
#define OUTPUT_COLOR            1       /* Color output */
#define OUTPUT_MONOCHROME       2       /* Raw monochrome output */
#define OUTPUT_RAW_CMYK         3       /* Raw CMYK output */

#define ORIENT_AUTO             -1      /* Best orientation */
#define ORIENT_PORTRAIT         0       /* Portrait orientation */
#define ORIENT_LANDSCAPE        1       /* Landscape orientation */
#define ORIENT_UPSIDEDOWN       2       /* Reverse portrait orientation */
#define ORIENT_SEASCAPE         3       /* Reverse landscape orientation */

#define IMAGE_LINE_ART          0
#define IMAGE_SOLID_TONE        1
#define IMAGE_CONTINUOUS        2
#define NIMAGE_TYPES            3

#define COLOR_MODEL_RGB         0
#define COLOR_MODEL_CMY         1
#define NCOLOR_MODELS           2

/*
 * Printer driver control structure.  See "print.c" for the actual list...
 */

typedef enum stp_papersize_unit
{
  PAPERSIZE_ENGLISH,
  PAPERSIZE_METRIC
} stp_papersize_unit_t;
  
typedef enum
{
  STP_JOB_MODE_PAGE,
  STP_JOB_MODE_JOB
} stp_job_mode_t;

typedef enum stp_image_status
{
  STP_IMAGE_OK,
  STP_IMAGE_ABORT
} stp_image_status_t;

/*
 * Abstract data type for interfacing with the image creation program
 * (in this case, the Gimp).
 *
 * The image layer need not implement transpose(), hflip(), vflip(),
 * crop(), rotate_ccw(), rotate_cw(), and rotate_180() if it does not
 * require that functionality or implements it internally.  This
 * functionality will likely be dropped in future releases.
 *
 * The image layer must implement all of the other members.  The
 * progress_init(), note_progress(), and progress_conclude() members
 * are used to enable the image layer to deliver notification of
 * progress to the user.  It is likely that these functions will be
 * dropped in the future, and if desired must be implemented in
 * get_row().
 *
 * get_appname() should return the name of the application.  This is
 * embedded in the output by some drivers.
 *
 * width() and height() return the dimensions of the image in pixels.
 *
 * bpp(), or bytes per pixel, is used in combination with the output type
 * and presence of a color map, if supplied, to determine the format
 * of the input:
 *
 * Output_type is OUTPUT_MONOCHROME, OUTPUT_COLOR, or OUTPUT_GRAY:
 *
 *    bpp           No color map                Color map present
 *     1            grayscale                   indexed color (256 colors)
 *     2            grayscale w/alpha           indexed color w/alpha
 *     3            RGB                         N/A
 *     4            N/A                         RGB w/alpha (RGBA)
 *
 * Output_type is OUTPUT_CMYK:
 *
 *    bpp           No color map                Color map present
 *     4            8 bits/plane CMYK           N/A
 *     8            16 bits/plane CMYK          N/A
 *
 * init() is used to perform any initialization required by the image
 * layer for the image.  It will be called once per image.  reset() is
 * called to reset the image to the beginning.  It may (in principle)
 * be called multiple times if a page is being printed more than once.
 * The reset() call may be removed in the future.
 *
 * get_row() transfers the data from the image to the gimp-print
 * library.  It is called from the driver layer.  It should copy WIDTH
 * (as returned by the width() member) pixels of data into the data
 * buffer.  It normally returns STP_IMAGE_OK; if something goes wrong,
 * or the application wishes to stop producing any further output
 * (e. g. because the user cancelled the print job), it should return
 * STP_IMAGE_ABORT.  This will cause the driver to flush any remaining
 * data to the output.  It will always request rows in monotonically
 * ascending order, but it may skip rows (if, for example, the
 * resolution of the input is higher than the resolution of the
 * output).
 */

typedef struct stp_image
{
  void (*init)(struct stp_image *image);
  void (*reset)(struct stp_image *image);
  void (*transpose)(struct stp_image *image);
  void (*hflip)(struct stp_image *image);
  void (*vflip)(struct stp_image *image);
  void (*crop)(struct stp_image *image,
               int left, int top, int right, int bottom);
  void (*rotate_ccw)(struct stp_image *image);
  void (*rotate_cw)(struct stp_image *image);
  void (*rotate_180)(struct stp_image *image);
  int  (*bpp)(struct stp_image *image);
  int  (*width)(struct stp_image *image);
  int  (*height)(struct stp_image *image);
  stp_image_status_t (*get_row)(struct stp_image *image, unsigned char *data,
                                int row);
  const char *(*get_appname)(struct stp_image *image);
  void (*progress_init)(struct stp_image *image);
  void (*note_progress)(struct stp_image *image, double current, double total);
  void (*progress_conclude)(struct stp_image *image);
  void *rep;
} stp_image_t;

/*
 * Definition of a printer.  A printer definition contains some data
 * about the printer and a set of member functions that operate on it.
 *
 * The data members are:
 *
 * long_name is a human-readable name.  It is intended to be used by
 *   a user interface to print the name of the printer.
 *
 * driver is the short name of the printer.  This is an alternate name
 *   that is used internally.  A user interface may use this for input
 *   purposes, or a client program may use this to generate a filename.
 *   The driver name should consist of lowercase alphanumerics and hyphens
 *   only.
 *
 * model is a model number used only by the underlying driver.  It is
 *   treated as an opaque, but static, identifier.  It should not be a
 *   pointer value, but the exact interpretation of the model number
 *   is up to the driver implementation (it may be an index into an
 *   array, for example).
 *
 * printvars is the default settings for this printer.
 *
 * The member functions are:
 *
 * char **(*parameters)(const stp_printer_t printer,
 *                      char *ppd_file,
 *                      char *name,
 *                      int *count)
 *
 *   returns a list of option values of the specified parameter NAME
 *   for the specified PRINTER.  If a PPD filename is specified, the driver
 *   may use that to help generate the valid parameter list.  The number
 *   of options returned is placed in COUNT.  Both the array and the
 *   options themselves are allocated on the heap; it is the caller's
 *   responsibility to free them upon completion of use.  The driver
 *   must therefore return a copy of data.
 *
 *   In all cases, the returned option names should be appropriate for a
 *   user interface to display.
 *
 *   The list of parameters is subject to change.  The currently supported
 *   parameters are:
 *
 *     PageSize returns a list of legal page size names for the printer
 *       in question.
 *
 *     Resolution returns a list of valid resolution settings.  The
 *       resolutions are to be interpreted as opaque names; the caller
 *       must not attempt to interpret them except with the
 *       describe_resolution function described below.  There may be
 *       multiple resolution names that resolve to the same printing
 *       resolution; they may correspond to different quality settings,
 *       for example.
 *
 *     InkType returns a list of legal ink types.  The printer driver may
 *       define these as it sees fit.  If a printer offers a choice of
 *       ink cartridges, the choices would be enumerated here.
 *
 *     MediaType returns a list of legal media types.  The printer driver
 *       may define these as it sees fit.  This is normally different kinds
 *       of paper that the printer can handle.
 *
 *     InputSlot returns a list of legal input sources for the printer.
 *       This is typically things like different input trays, manual feed,
 *       roll feed, and the like.
 *
 * void (*media_size)(const stp_printer_t printer,
 *                    const stp_vars_t v,
 *                    int *width,
 *                    int *height)
 *
 *   returns the physical WIDTH and HEIGHT of the page using the settings
 *   in V.  The driver will almost always look at the media_size variable
 *   in V; it may look at other data in V to determine the physical page
 *   size.  WIDTH and HEIGHT are expressed in units of 1/72".
 *
 * void (*imageable_area)(const stp_printer_t printer,
 *                        const stp_vars_t v,
 *                        int *left,
 *                        int *right,
 *                        int *bottom,
 *                        int *top)
 *
 *   returns the width of the LEFT, RIGHT, BOTTOM, and TOP border of the
 *   page for the given printer and variable settings.  The caller can
 *   use this, in combination with the media_size member, to determine
 *   the printable region of the page, and if needed, exactly where to
 *   place the image to achieve a given physical placement (e. g.
 *   centering) on the page.  All returned values are in units of
 *   1/72".
 *
 * void (*limit)(const stp_printer_t printer,
 *               const stp_vars_t v,
 *               int *width,
 *               int *height)
 *
 *   returns the maximum page size the printer can handle, in units of
 *   1/72".
 *
 * void (*print)(const stp_printer_t printer,
 *               stp_image_t *image,
 *               const stp_vars_t v)
 *
 *   prints a page.  The variable settings provided in V are used to control
 *   the printing; PRN is a file pointer that the raw printer output
 *   is to be written to, and IMAGE is an object that sources the input
 *   data to the driver (the contents of which are opaque to the low level
 *   driver and are interpreted by the high level program).
 *
 * const char *(*default_resolution)(const stp_printer_t printer)
 *
 *   returns the name of the default resolution for the printer.  The
 *   caller must not attempt to free the returned value.
 *
 * void (*describe_resolution)(const stp_printer_t printer,
 *                             const char *resolution,
 *                             int *x,
 *                             int *y)
 *
 *   returns the horizontal (X) and vertical (Y) resolution of the chosen
 *   RESOLUTION name.  The high level program may choose to use this to
 *   rasterize at an appropriate resolution.
 *
 */

typedef void *stp_printer_t;
typedef void *stp_vars_t;
typedef void *stp_papersize_t;
typedef struct
{
  const char	*name,	/* Option name */
		*text;	/* Human-readable (translated) text */
} stp_param_t;

typedef void (*stp_outfunc_t) (void *data, const char *buffer, size_t bytes);

typedef struct
{
  stp_param_t *(*parameters)(const stp_printer_t printer,
                             const char *ppd_file,
                             const char *name, int *count);
  void  (*media_size)(const stp_printer_t printer, const stp_vars_t v,
                      int *width, int *height);
  void  (*imageable_area)(const stp_printer_t printer,
                          const stp_vars_t v,
                          int *left, int *right, int *bottom, int *top);
  void  (*limit)(const stp_printer_t printer, const stp_vars_t v,
                 int *max_width, int *max_height,
                 int *min_width, int *min_height);
  void  (*print)(const stp_printer_t printer,
                 stp_image_t *image, const stp_vars_t v);
  const char *(*default_parameters)(const stp_printer_t printer,
                                    const char *ppd_file,
                                    const char *name);
  void  (*describe_resolution)(const stp_printer_t printer,
                               const char *resolution, int *x, int *y);
  int   (*verify)(const stp_printer_t p, const stp_vars_t v);
  int   (*start_job)(const stp_printer_t printer,
		     stp_image_t *image, const stp_vars_t v);
  int   (*end_job)(const stp_printer_t printer,
		   stp_image_t *image, const stp_vars_t v);
} stp_printfuncs_t;

/*
 * stp_init() must be called prior to any other use of the library.
 */
extern int stp_init (void);

extern const char * stp_printer_get_long_name (const stp_printer_t p);
extern const char * stp_printer_get_driver (const stp_printer_t p);
extern int stp_printer_get_model (const stp_printer_t p);
extern const stp_printfuncs_t *stp_printer_get_printfuncs(const stp_printer_t);
extern const stp_vars_t stp_printer_get_printvars (const stp_printer_t p);
extern void stp_set_printer_defaults(stp_vars_t, const stp_printer_t,
                                     const char *ppd_file);

extern stp_vars_t stp_allocate_vars (void);
extern void stp_copy_vars (stp_vars_t vd, const stp_vars_t vs);
extern stp_vars_t stp_allocate_copy (const stp_vars_t vs);

extern void stp_free_vars (stp_vars_t vv);

extern void stp_set_output_to (stp_vars_t vv, const char *val);
extern void stp_set_driver (stp_vars_t vv, const char *val);
extern void stp_set_ppd_file (stp_vars_t vv, const char *val);
extern void stp_set_resolution (stp_vars_t vv, const char *val);
extern void stp_set_media_size (stp_vars_t vv, const char *val);
extern void stp_set_media_type (stp_vars_t vv, const char *val);
extern void stp_set_media_source (stp_vars_t vv, const char *val);
extern void stp_set_ink_type (stp_vars_t vv, const char *val);
extern void stp_set_dither_algorithm (stp_vars_t vv, const char *val);

/*
 * The _n variants are used for strings that are not null-delimited.
 */
extern void stp_set_output_to_n (stp_vars_t vv, const char *val,
                                 int bytes);
extern void stp_set_driver_n (stp_vars_t vv, const char *val,
                              int bytes);
extern void stp_set_ppd_file_n (stp_vars_t vv, const char *val,
                                int bytes);
extern void stp_set_resolution_n (stp_vars_t vv, const char *val,
                                  int bytes);
extern void stp_set_media_size_n (stp_vars_t vv, const char *val,
                                  int bytes);
extern void stp_set_media_type_n (stp_vars_t vv, const char *val,
                                  int bytes);
extern void stp_set_media_source_n (stp_vars_t vv, const char *val,
                                    int bytes);
extern void stp_set_ink_type_n (stp_vars_t vv, const char *val,
                                int bytes);
extern void stp_set_dither_algorithm_n (stp_vars_t vv, const char *val,
                                        int bytes);

extern void stp_set_output_type (stp_vars_t vv, int val);
extern void stp_set_orientation (stp_vars_t vv, int val);
extern void stp_set_left (stp_vars_t vv, int val);
extern void stp_set_top (stp_vars_t vv, int val);
extern void stp_set_image_type (stp_vars_t vv, int val);
extern void stp_set_unit (stp_vars_t vv, int val);
extern void stp_set_page_width (stp_vars_t vv, int val);
extern void stp_set_page_height (stp_vars_t vv, int val);

/*
 * Input color model refers to how the data is being sent to the
 * driver library; the default is RGB.  Output color model refers to
 * the characteristics of the device; the default is CMYK.  The output
 * color model is set by the printer driver and cannot be overridden.
 * It is provided to permit applications to generate previews using
 * the color machinery in Gimp-Print.  If this is done, normally
 * the output color model will be RGB.
 */
extern void stp_set_input_color_model (stp_vars_t vv, int val);
extern void stp_set_output_color_model (stp_vars_t vv, int val);

extern void stp_set_brightness (stp_vars_t vv, float val);
extern void stp_set_scaling (stp_vars_t vv, float val);
extern void stp_set_gamma (stp_vars_t vv, float val);
extern void stp_set_contrast (stp_vars_t vv, float val);
extern void stp_set_cyan (stp_vars_t vv, float val);
extern void stp_set_magenta (stp_vars_t vv, float val);
extern void stp_set_yellow (stp_vars_t vv, float val);
extern void stp_set_saturation (stp_vars_t vv, float val);
extern void stp_set_density (stp_vars_t vv, float val);

/*
 * Application gamma is used to initially correct the input data
 * for the application's characteristics.  This cannot be done by the
 * application (except in 16-bit CMYK mode) without losing data.
 */
extern void stp_set_app_gamma (stp_vars_t vv, float val);

/*
 * Please see the source in print-color.c for an explanation of this.
 */
extern void stp_set_lut (stp_vars_t vv, void * val);

/*
 * For use with indexed color: the LUT should be an array of 256
 * RGB values.
 */
extern void stp_set_cmap (stp_vars_t vv, unsigned char * val);

/*
 * These functions are used to print output and diagnostic information
 * respectively.  These must be supplied by the caller.
 */
extern void stp_set_outfunc (const stp_vars_t vv, stp_outfunc_t val);
extern void stp_set_errfunc (const stp_vars_t vv, stp_outfunc_t val);
extern void stp_set_outdata (stp_vars_t vv, void * val);
extern void stp_set_errdata (stp_vars_t vv, void * val);

extern const char * stp_get_output_to (const stp_vars_t vv);
extern const char * stp_get_driver (const stp_vars_t vv);
extern const char * stp_get_ppd_file (const stp_vars_t vv);
extern const char * stp_get_resolution (const stp_vars_t vv);
extern const char * stp_get_media_size (const stp_vars_t vv);
extern const char * stp_get_media_type (const stp_vars_t vv);
extern const char * stp_get_media_source (const stp_vars_t vv);
extern const char * stp_get_ink_type (const stp_vars_t vv);
extern const char * stp_get_dither_algorithm (const stp_vars_t vv);
extern int stp_get_output_type (const stp_vars_t vv);
extern int stp_get_orientation (const stp_vars_t vv);
extern int stp_get_left (const stp_vars_t vv);
extern int stp_get_top (const stp_vars_t vv);
extern int stp_get_image_type (const stp_vars_t vv);
extern int stp_get_unit (const stp_vars_t vv);
extern int stp_get_page_width (const stp_vars_t vv);
extern int stp_get_page_height (const stp_vars_t vv);
extern int stp_get_input_color_model (const stp_vars_t vv);
extern int stp_get_output_color_model (const stp_vars_t vv);
extern float stp_get_brightness (const stp_vars_t vv);
extern float stp_get_scaling (const stp_vars_t vv);
extern float stp_get_gamma (const stp_vars_t vv);
extern float stp_get_contrast (const stp_vars_t vv);
extern float stp_get_cyan (const stp_vars_t vv);
extern float stp_get_magenta (const stp_vars_t vv);
extern float stp_get_yellow (const stp_vars_t vv);
extern float stp_get_saturation (const stp_vars_t vv);
extern float stp_get_density (const stp_vars_t vv);
extern float stp_get_app_gamma (const stp_vars_t vv);
extern void * stp_get_lut (const stp_vars_t vv);
extern stp_outfunc_t stp_get_outfunc (const stp_vars_t vv);
extern stp_outfunc_t stp_get_errfunc (const stp_vars_t vv);
extern void * stp_get_outdata (const stp_vars_t vv);
extern void * stp_get_errdata (const stp_vars_t vv);
extern unsigned char * stp_get_cmap (const stp_vars_t vv);


/*
 * hue_map is an array of 49 doubles representing the mapping of hue
 * from (0..6) to (0..6) in increments of .125.  The hue_map is in CMY space,
 * so hue=0 is cyan.  Interpolation between values is linear.
 *
 * lum_map and sat_map are correction factors for luminosity and saturation
 * respectively.  Both of these are inverse exponential.  The degree of
 * luminosity correction is scaled by the saturation of the particular
 * pixel.  This is likely to change drastically in the future.
 */
typedef void (*stp_convert_t) (const stp_vars_t vars, const unsigned char *in,
                               unsigned short *out, int *zero_mask,
                               int width, int bpp, const unsigned char *cmap,
                               const double *hue_map, const double *lum_map,
                               const double *sat_map);

extern void stp_merge_printvars (stp_vars_t user, const stp_vars_t print);

extern size_t stp_dither_algorithm_count (void);
extern const char * stp_dither_algorithm_name (int id);
extern const char * stp_dither_algorithm_text (int id);
extern const char * stp_default_dither_algorithm (void);

extern int stp_known_papersizes (void);
extern const stp_papersize_t stp_get_papersize_by_name (const char *name);
extern const stp_papersize_t stp_get_papersize_by_size (int l, int w);
extern const stp_papersize_t stp_get_papersize_by_index (int index);
extern const char * stp_papersize_get_name (const stp_papersize_t pt);
extern const char * stp_papersize_get_text (const stp_papersize_t pt);
extern unsigned stp_papersize_get_width (const stp_papersize_t pt);
extern unsigned stp_papersize_get_height (const stp_papersize_t pt);
extern unsigned stp_papersize_get_top (const stp_papersize_t pt);
extern unsigned stp_papersize_get_left (const stp_papersize_t pt);
extern unsigned stp_papersize_get_bottom (const stp_papersize_t pt);
extern unsigned stp_papersize_get_right (const stp_papersize_t pt);
extern stp_papersize_unit_t stp_papersize_get_unit (const stp_papersize_t pt);

extern void stp_set_job_mode(stp_vars_t, stp_job_mode_t);
extern stp_job_mode_t stp_get_job_mode(const stp_vars_t);
extern void stp_set_page_number(stp_vars_t, int);
extern int stp_get_page_number(const stp_vars_t);

extern int stp_known_printers (void);
extern const stp_printer_t stp_get_printer_by_index (int idx);
extern const stp_printer_t stp_get_printer_by_long_name (const char *long_name);
extern const stp_printer_t stp_get_printer_by_driver (const char *driver);
extern int stp_get_printer_index_by_driver (const char *driver);

/*
 * This is likely to change in the future.
 */
extern stp_convert_t stp_choose_colorfunc (int output_type, int image_bpp,
                                           const unsigned char *cmap,
                                           int *out_bpp,
                                           const stp_vars_t v);
extern void stp_allocate_lut (stp_vars_t v, size_t steps);
extern void stp_free_lut (stp_vars_t v);
extern void stp_compute_lut (stp_vars_t v, size_t steps);

/*
 * This is likely to change in the future.  In particular, responsibility
 * for orientation will likely be transferred to the application from
 * the library.
 */
extern void stp_compute_page_parameters (int page_right, int page_left,
                                         int page_top, int page_bottom,
                                         double scaling, int image_width,
                                         int image_height, stp_image_t *image,
                                         int *orientation,
                                         int *page_width, int *page_height,
                                         int *out_width, int *out_height,
                                         int *left, int *top);

extern const stp_vars_t stp_default_settings (void);
extern const stp_vars_t stp_maximum_settings (void);
extern const stp_vars_t stp_minimum_settings (void);

#ifdef __cplusplus
  }
#endif

#endif /* __GIMP_PRINT_H__ */
/*
 * End of $Id: gimp-print.h,v 1.1.1.1 2003/01/27 19:05:31 jlovell Exp $
 */
