/*
 * "$Id: print-escp2.h,v 1.1.1.4 2004/12/22 23:49:39 jlovell Exp $"
 *
 *   Print plug-in EPSON ESC/P2 driver for the GIMP.
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
 */

#ifndef GIMP_PRINT_INTERNAL_ESCP2_H
#define GIMP_PRINT_INTERNAL_ESCP2_H

/*
 * Maximum number of channels in a printer.  If Epson comes out with an
 * 8-head printer, this needs to be increased.
 */
#define PHYSICAL_CHANNEL_LIMIT 8
#define MAX_DROP_SIZES 3

#define XCOLOR_R     (STP_NCOLORS + 0)
#define XCOLOR_B     (STP_NCOLORS + 1)
#define XCOLOR_GLOSS (STP_NCOLORS + 2)

/*
 * Printer capabilities.
 *
 * Various classes of printer capabilities are represented by bitmasks.
 */

typedef unsigned long model_cap_t;
typedef unsigned long model_featureset_t;


#define RES_LOW		 0
#define RES_360		 1
#define RES_720_360	 2
#define RES_720		 3
#define RES_1440_720	 4
#define RES_2880_720	 5
#define RES_2880_1440	 6
#define RES_2880_2880	 7
#define RES_N		 8

/*
 ****************************************************************
 *                                                              *
 * DROP SIZES                                                   *
 *                                                              *
 ****************************************************************
 */

typedef struct
{
  const char *listname;
  short numdropsizes;
  const double dropsizes[MAX_DROP_SIZES];
} escp2_dropsize_t;

typedef const escp2_dropsize_t *const escp2_drop_list_t[RES_N];

/*
 ****************************************************************
 *                                                              *
 * PAPERS                                                       *
 *                                                              *
 ****************************************************************
 */

typedef struct
{
  const char *name;
  float base_density;
  float subchannel_cutoff;
  float k_transition;
  float k_lower;
  float k_upper;
  float cyan;
  float magenta;
  float yellow;
  float black;
  float saturation;
  float gamma;
  const char *hue_adjustment;
  const char *lum_adjustment;
  const char *sat_adjustment;
} paper_adjustment_t;

typedef struct
{
  const char *listname;
  short paper_count;
  const paper_adjustment_t *papers;
} paper_adjustment_list_t;

typedef enum
{
  PAPER_PLAIN         = 0x01,
  PAPER_GOOD          = 0x02,
  PAPER_PHOTO         = 0x04,
  PAPER_PREMIUM_PHOTO = 0x08,
  PAPER_TRANSPARENCY  = 0x10
} paper_class_t;

typedef struct
{
  const char *name;
  const char *text;
  paper_class_t paper_class;
  short paper_feed_sequence;
  short platen_gap;
  short feed_adjustment;
  short vacuum_intensity;
  short paper_thickness;
  const char *preferred_ink_type;
  const char *preferred_ink_set;
} paper_t;

typedef struct
{
  const char *listname;
  short paper_count;
  const paper_t *papers;
} paperlist_t;


/*
 ****************************************************************
 *                                                              *
 * RESOLUTIONS                                                  *
 *                                                              *
 ****************************************************************
 */

typedef struct
{
  const char *name;
  const char *text;
  short hres;
  short vres;
  short printed_hres;
  short printed_vres;
  short softweave;
  short printer_weave;
  short vertical_passes;
} res_t;


/*
 ****************************************************************
 *                                                              *
 * INKS                                                         *
 *                                                              *
 ****************************************************************
 */

typedef struct
{
  short color;
  short subchannel;
  short head_offset;
  const char *channel_density;
  const char *subchannel_scale;
} physical_subchannel_t;

typedef struct
{
  const char *listname;
  const physical_subchannel_t *subchannels;
  short n_subchannels;
} ink_channel_t;

typedef enum
{
  INKSET_CMYK             = 0,
  INKSET_CcMmYK           = 1,
  INKSET_CcMmYyK          = 2,
  INKSET_CcMmYKk          = 3,
  INKSET_QUADTONE         = 4,
  INKSET_CMYKRB		  = 5,
  INKSET_EXTENDED	  = 6
} inkset_id_t;

typedef struct
{
  const char *name;
  const ink_channel_t *const *channels;
  short channel_count;
} channel_set_t;

typedef struct
{
  const char *name;
  const char *text;
  inkset_id_t inkset;
  const channel_set_t *channel_set;
} escp2_inkname_t;

typedef struct
{
  int n_shades;
  const double shades[PHYSICAL_CHANNEL_LIMIT];
} shade_t;

typedef shade_t shade_set_t[PHYSICAL_CHANNEL_LIMIT];

typedef struct
{
  const char *name;
  const char *text;
  const escp2_inkname_t *const *inknames;
  const paperlist_t *papers;
  const paper_adjustment_list_t *paper_adjustments;
  const shade_set_t *shades;
  short n_inks;
} inklist_t;

typedef struct
{
  const char *listname;
  const inklist_t *const *inklists;
  short n_inklists;
} inkgroup_t;
    

/*
 ****************************************************************
 *                                                              *
 * MISCELLANEOUS                                                *
 *                                                              *
 ****************************************************************
 */

/*
 * For each printer, we can select from a variety of dot sizes.
 * For single dot size printers, the available sizes are usually 0,
 * which is the "default", and some subset of 1-4.  For simple variable
 * dot size printers (with only one kind of variable dot size), the
 * variable dot size is specified as 0x10.  For newer printers, there
 * is a choice of variable dot sizes available, 0x10, 0x11, and 0x12 in
 * order of increasing size.
 *
 * Normally, we want to specify the smallest dot size that lets us achieve
 * a density of less than .8 or thereabouts (above that we start to get
 * some dither artifacts).  This needs to be tested for each printer and
 * resolution.
 *
 * An entry of -1 in a slot means that this resolution is not available.
 */

typedef short escp2_dot_size_t[RES_N];

/*
 * Choose the number of bits to use at each resolution.
 */

typedef short escp2_bits_t[RES_N];

/*
 * Choose the base resolution to use at each resolution.
 */

typedef short escp2_base_resolutions_t[RES_N];

/*
 * Specify the base density for each available resolution.
 * This obviously depends upon the dot size.
 */

typedef float escp2_densities_t[RES_N];

#define ROLL_FEED_CUT_ALL (1)
#define ROLL_FEED_CUT_LAST (2)
#define ROLL_FEED_DONT_EJECT (4)

typedef struct
{
  const char *name;
  const char *text;
  short is_cd;
  short is_roll_feed;
  unsigned roll_feed_cut_flags;
  const stp_raw_t init_sequence;
  const stp_raw_t deinit_sequence;
} input_slot_t;

typedef struct
{
  const input_slot_t *slots;
  size_t n_input_slots;
} input_slot_list_t;

typedef struct
{
  const char *name;
  const char *text;
  short min_hres;
  short min_vres;
  short max_hres;
  short max_vres;
  short desired_hres;
  short desired_vres;
} quality_t;

typedef struct
{
  const quality_t *qualities;
  size_t n_quals;
} quality_list_t;

typedef enum
{
  AUTO_MODE_QUALITY,
  AUTO_MODE_MANUAL
} auto_mode_t;

typedef struct
{
  const char *name;
  const char *text;
  short value;
} printer_weave_t;

typedef struct
{
  const char *name;
  size_t n_printer_weaves;
  const printer_weave_t *printer_weaves;
} printer_weave_list_t;

#define MODEL_COMMAND_MASK	0xful /* What general command set does */
#define MODEL_COMMAND_1998	0x0ul
#define MODEL_COMMAND_1999	0x1ul /* The 1999 series printers */
#define MODEL_COMMAND_2000	0x2ul /* The 2000 series printers */
#define MODEL_COMMAND_PRO	0x3ul /* Stylus Pro printers */

#define MODEL_XZEROMARGIN_MASK	0x10ul /* Does this printer support */
#define MODEL_XZEROMARGIN_NO	0x00ul /* zero margin mode? */
#define MODEL_XZEROMARGIN_YES	0x10ul /* (print to edge of the paper) */

#define MODEL_ROLLFEED_MASK	0x20ul /* Does this printer support */
#define MODEL_ROLLFEED_NO	0x00ul /* a roll feed? */
#define MODEL_ROLLFEED_YES	0x20ul

#define MODEL_VARIABLE_DOT_MASK	0x40ul /* Does this printer support var */
#define MODEL_VARIABLE_NO	0x00ul /* dot size printing? The newest */
#define MODEL_VARIABLE_YES	0x40ul /* printers support multiple modes */

#define MODEL_GRAYMODE_MASK	0x80ul /* Does this printer support special */
#define MODEL_GRAYMODE_NO	0x00ul /* fast black printing? */
#define MODEL_GRAYMODE_YES	0x80ul

#define MODEL_VACUUM_MASK	0x100ul
#define MODEL_VACUUM_NO		0x000ul
#define MODEL_VACUUM_YES	0x100ul

#define MODEL_FAST_360_MASK	0x200ul
#define MODEL_FAST_360_NO	0x000ul
#define MODEL_FAST_360_YES	0x200ul

#define MODEL_SEND_ZERO_ADVANCE_MASK	0x400ul
#define MODEL_SEND_ZERO_ADVANCE_NO	0x000ul
#define MODEL_SEND_ZERO_ADVANCE_YES	0x400ul

#define MODEL_SUPPORTS_INK_CHANGE_MASK	0x800ul
#define MODEL_SUPPORTS_INK_CHANGE_NO	0x000ul
#define MODEL_SUPPORTS_INK_CHANGE_YES	0x800ul

#define MODEL_PACKET_MODE_MASK	0x1000ul
#define MODEL_PACKET_MODE_NO	0x0000ul
#define MODEL_PACKET_MODE_YES	0x1000ul

#define MODEL_PRINT_TO_CD_MASK	0x2000ul
#define MODEL_PRINT_TO_CD_NO	0x0000ul
#define MODEL_PRINT_TO_CD_YES	0x2000ul

typedef enum
{
  MODEL_COMMAND,
  MODEL_XZEROMARGIN,
  MODEL_ROLLFEED,
  MODEL_VARIABLE_DOT,
  MODEL_GRAYMODE,
  MODEL_VACUUM,
  MODEL_FAST_360,
  MODEL_SEND_ZERO_ADVANCE,
  MODEL_SUPPORTS_INK_CHANGE,
  MODEL_PACKET_MODE,
  MODEL_PRINT_TO_CD,
  MODEL_LIMIT
} escp2_model_option_t;

typedef struct escp2_printer
{
  model_cap_t	flags;		/* Bitmask of flags, see above */
/*****************************************************************************/
  /* Basic head configuration */
  short		nozzles;	/* Number of nozzles per color */
  short		min_nozzles;	/* Minimum number of nozzles per color */
  short		nozzle_separation; /* Separation between rows, in 1/360" */
  short		black_nozzles;	/* Number of black nozzles (may be extra) */
  short		min_black_nozzles;	/* # of black nozzles (may be extra) */
  short		black_nozzle_separation; /* Separation between rows */
  short		fast_nozzles;	/* Number of fast nozzles */
  short		min_fast_nozzles;	/* # of fast nozzles (may be extra) */
  short		fast_nozzle_separation; /* Separation between rows */
  short		physical_channels; /* Number of ink channels */
/*****************************************************************************/
  /* Print head resolution */
  short		base_separation; /* Basic unit of row separation */
  short		resolution_scale;   /* Scaling factor for ESC(D command */
  short		max_black_resolution; /* Above this resolution, we */
				      /* must use color parameters */
				      /* rather than (faster) black */
				      /* only parameters*/
  short		max_hres;
  short		max_vres;
  short		min_hres;
  short		min_vres;
  /* Miscellaneous printer-specific data */
  short		extra_feed;	/* Extra distance the paper can be spaced */
				/* beyond the bottom margin, in 1/360". */
				/* (maximum useful value is */
				/* nozzles * nozzle_separation) */
  short		separation_rows; /* Some printers require funky spacing */
				/* arguments in softweave mode. */
  short		pseudo_separation_rows;/* Some printers require funky */
				/* spacing arguments in printer_weave mode */

  short         zero_margin_offset;   /* Offset to use to achieve */
				      /* zero-margin printing */
  short		initial_vertical_offset;
  short		black_initial_vertical_offset;
  short		extra_720dpi_separation;
/*****************************************************************************/
  /* Paper size limits */
  int		max_paper_width; /* Maximum paper width, in points */
  int		max_paper_height; /* Maximum paper height, in points */
  int		min_paper_width; /* Maximum paper width, in points */
  int		min_paper_height; /* Maximum paper height, in points */
/*****************************************************************************/
  /* Borders */
				/* SHEET FED: */
				/* Softweave: */
  short		left_margin;	/* Left margin, points */
  short		right_margin;	/* Right margin, points */
  short		top_margin;	/* Absolute top margin, points */
  short		bottom_margin;	/* Absolute bottom margin, points */
				/* Printer weave: */
  short		m_left_margin;	/* Left margin, points */
  short		m_right_margin;	/* Right margin, points */
  short		m_top_margin;	/* Absolute top margin, points */
  short		m_bottom_margin;	/* Absolute bottom margin, points */
				/* ROLL FEED: */
				/* Softweave: */
  short		roll_left_margin;	/* Left margin, points */
  short		roll_right_margin;	/* Right margin, points */
  short		roll_top_margin;	/* Absolute top margin, points */
  short		roll_bottom_margin;	/* Absolute bottom margin, points */
				/* Printer weave: */
  short		m_roll_left_margin;	/* Left margin, points */
  short		m_roll_right_margin;	/* Right margin, points */
  short		m_roll_top_margin;	/* Absolute top margin, points */
  short		m_roll_bottom_margin;	/* Absolute bottom margin, points */
				/* Print directly to CD */
  short		cd_x_offset;	/* Center of CD (horizontal offset) */
  short		cd_y_offset;	/* Center of CD (vertical offset) */
  short		cd_page_width;	/* Width of "page" when printing to CD */
  short		cd_page_height;	/* Height of "page" when printing to CD */
/*****************************************************************************/
  /* Parameters for escputil */
  short		alignment_passes;
  short		alignment_choices;
  short		alternate_alignment_passes;
  short		alternate_alignment_choices;
/*****************************************************************************/
  const short *dot_sizes;	/* Vector of dot sizes for resolutions */
  const float *densities;	/* List of densities for each printer */
  const escp2_drop_list_t *drops; /* Drop sizes */
/*****************************************************************************/
  const res_t *const *reslist;
  const inkgroup_t *inkgroup;
/*****************************************************************************/
  const short *bits;
  const short *base_resolutions;
  const input_slot_list_t *input_slots;
/*****************************************************************************/
  const quality_list_t *quality_list;
  const stp_raw_t *preinit_sequence;
  const stp_raw_t *postinit_remote_sequence;
/*****************************************************************************/
  const printer_weave_list_t *const printer_weaves;
} stpi_escp2_printer_t;

extern const stpi_escp2_printer_t stpi_escp2_model_capabilities[];
extern const int stpi_escp2_model_limit;

extern const escp2_drop_list_t stpi_escp2_simple_drops;
extern const escp2_drop_list_t stpi_escp2_spro10000_drops;
extern const escp2_drop_list_t stpi_escp2_variable_1_5pl_drops;
extern const escp2_drop_list_t stpi_escp2_variable_2pl_drops;
extern const escp2_drop_list_t stpi_escp2_variable_3pl_drops;
extern const escp2_drop_list_t stpi_escp2_variable_3pl_pigment_drops;
extern const escp2_drop_list_t stpi_escp2_variable_3pl_pmg_drops;
extern const escp2_drop_list_t stpi_escp2_variable_1440_4pl_drops;
extern const escp2_drop_list_t stpi_escp2_variable_ultrachrome_drops;
extern const escp2_drop_list_t stpi_escp2_variable_2880_4pl_drops;
extern const escp2_drop_list_t stpi_escp2_variable_6pl_drops;
extern const escp2_drop_list_t stpi_escp2_variable_2000p_drops;
extern const escp2_drop_list_t stpi_escp2_variable_x80_6pl_drops;

extern const paperlist_t stpi_escp2_standard_paper_list;
extern const paperlist_t stpi_escp2_durabrite_paper_list;
extern const paperlist_t stpi_escp2_ultrachrome_paper_list;

extern const paper_adjustment_list_t stpi_escp2_standard_paper_adjustment_list;
extern const paper_adjustment_list_t stpi_escp2_durabrite_paper_adjustment_list;
extern const paper_adjustment_list_t stpi_escp2_photo_paper_adjustment_list;
extern const paper_adjustment_list_t stpi_escp2_photo2_paper_adjustment_list;
extern const paper_adjustment_list_t stpi_escp2_photo3_paper_adjustment_list;
extern const paper_adjustment_list_t stpi_escp2_sp960_paper_adjustment_list;
extern const paper_adjustment_list_t stpi_escp2_ultrachrome_photo_paper_adjustment_list;
extern const paper_adjustment_list_t stpi_escp2_ultrachrome_matte_paper_adjustment_list;

extern const res_t *const stpi_escp2_superfine_reslist[];
extern const res_t *const stpi_escp2_no_printer_weave_reslist[];
extern const res_t *const stpi_escp2_pro_reslist[];
extern const res_t *const stpi_escp2_sp5000_reslist[];
extern const res_t *const stpi_escp2_720dpi_reslist[];
extern const res_t *const stpi_escp2_720dpi_soft_reslist[];
extern const res_t *const stpi_escp2_g3_720dpi_reslist[];
extern const res_t *const stpi_escp2_1440dpi_reslist[];
extern const res_t *const stpi_escp2_2880dpi_reslist[];
extern const res_t *const stpi_escp2_2880_1440dpi_reslist[];
extern const res_t *const stpi_escp2_g3_reslist[];
extern const res_t *const stpi_escp2_sc500_reslist[];
extern const res_t *const stpi_escp2_sc640_reslist[];

extern const inkgroup_t stpi_escp2_cmy_inkgroup;
extern const inkgroup_t stpi_escp2_standard_inkgroup;
extern const inkgroup_t stpi_escp2_c80_inkgroup;
extern const inkgroup_t stpi_escp2_c64_inkgroup;
extern const inkgroup_t stpi_escp2_x80_inkgroup;
extern const inkgroup_t stpi_escp2_photo_gen1_inkgroup;
extern const inkgroup_t stpi_escp2_photo_gen2_inkgroup;
extern const inkgroup_t stpi_escp2_photo_gen3_inkgroup;
extern const inkgroup_t stpi_escp2_photo_pigment_inkgroup;
extern const inkgroup_t stpi_escp2_photo7_japan_inkgroup;
extern const inkgroup_t stpi_escp2_ultrachrome_inkgroup;
extern const inkgroup_t stpi_escp2_f360_photo_inkgroup;
extern const inkgroup_t stpi_escp2_f360_photo7_japan_inkgroup;
extern const inkgroup_t stpi_escp2_f360_ultrachrome_inkgroup;
extern const inkgroup_t stpi_escp2_cmykrb_inkgroup;

extern const escp2_inkname_t stpi_escp2_default_black_inkset;

extern const printer_weave_list_t stpi_escp2_standard_printer_weave_list;
extern const printer_weave_list_t stpi_escp2_sp2200_printer_weave_list;
extern const printer_weave_list_t stpi_escp2_pro7000_printer_weave_list;
extern const printer_weave_list_t stpi_escp2_pro7500_printer_weave_list;
extern const printer_weave_list_t stpi_escp2_pro7600_printer_weave_list;

typedef struct
{
  /* Basic print head parameters */
  int nozzles;			/* Number of nozzles */
  int min_nozzles;		/* Fewest nozzles we're allowed to use */
  int nozzle_separation;	/* Nozzle separation, in dots */
  int *head_offset;		/* Head offset (for C80-type printers) */
  int max_head_offset;		/* Largest head offset */
  int page_management_units;	/* Page management units (dpi) */
  int vertical_units;		/* Vertical units (dpi) */
  int horizontal_units;		/* Horizontal units (dpi) */
  int micro_units;		/* Micro-units for horizontal positioning */
  int unit_scale;		/* Scale factor for units */
  int send_zero_pass_advance;	/* Send explicit command for zero advance */

  /* Ink parameters */
  int bitwidth;			/* Number of bits per ink drop */
  int drop_size;		/* ID of the drop size we're using */
  int ink_resid;		/* Array index for the drop set we're using */
  const escp2_inkname_t *inkname; /* Description of the ink set */

  /* Ink channels */
  int logical_channels;		/* Number of logical ink channels (e.g.CMYK) */
  int physical_channels;	/* Number of physical channels (e.g. CcMmYK) */
  int channels_in_use;		/* Number of channels we're using
				   FIXME merge with physical_channels! */
  unsigned char **cols;		/* Output dithered data */
  const physical_subchannel_t **channels; /* Description of each channel */

  /* Miscellaneous printer control */
  int use_black_parameters;	/* Can we use (faster) black head parameters */
  int use_fast_360;		/* Can we use fast 360 DPI 4 color mode */
  int advanced_command_set;	/* Uses one of the advanced command sets */
  int use_extended_commands;	/* Do we use the extended commands? */
  const input_slot_t *input_slot; /* Input slot description */
  const paper_t *paper_type;	/* Paper type */
  const paper_adjustment_t *paper_adjustment;	/* Paper adjustments */
  const inkgroup_t *ink_group;	/* Which set of inks */
  const stp_raw_t *init_sequence; /* Initialization sequence */
  const stp_raw_t *deinit_sequence; /* De-initialization sequence */
  model_featureset_t command_set; /* Which command set this printer supports */
  int variable_dots;		/* Print supports variable dot sizes */
  int has_vacuum;		/* Printer supports vacuum command */
  int has_graymode;		/* Printer supports fast grayscale mode */
  int base_separation;		/* Basic unit of separation */
  int resolution_scale;		/* Scale factor for ESC(D command */
  int printing_resolution;	/* Printing resolution for this resolution */
  int separation_rows;		/* Row separation scaling */
  int pseudo_separation_rows;	/* Special row separation for some printers */
  int extra_720dpi_separation;	/* Special separation needed at 720 DPI */

  /* weave parameters */
  int horizontal_passes;	/* Number of horizontal passes required
				   to print a complete row */
  int physical_xdpi;		/* Horizontal distance between dots in pass */
  const res_t *res;		/* Description of the printing resolution */
  const printer_weave_t *printer_weave; /* Printer weave parameters */
  int use_printer_weave;	/* Use the printer weaving mechanism */

  /* page parameters */		/* Indexed from top left */
  int page_left;		/* Left edge of page (points) */
  int page_right;		/* Right edge of page (points) */
  int page_top;			/* Top edge of page (points) */
  int page_bottom;		/* Bottom edge of page (points) */
  int page_width;		/* Page width (points) */
  int page_height;		/* Page height (points) */
  int page_true_height;		/* Physical page height (points) */
  int cd_x_offset;		/* CD X offset (micro units) */
  int cd_y_offset;		/* CD Y offset (micro units) */
  int cd_outer_radius;		/* CD radius (micro units) */
  int cd_inner_radius;		/* CD radius (micro units) */

  /* Image parameters */	/* Indexed from top left */
  int image_height;		/* Height of printed region (points) */
  int image_width;		/* Width of printed region (points) */
  int image_top;		/* First printed row (points) */
  int image_left;		/* Left edge of image (points) */
  int image_scaled_width;	/* Width of physical printed region (dots) */
  int image_printed_width;	/* Width of printed region (dots) */
  int image_scaled_height;	/* Height of physical printed region (dots) */
  int image_printed_height;	/* Height of printed region (dots) */
  int image_left_position;	/* Left dot position of image */

  /* Transitory state */
  int printed_something;	/* Have we actually printed anything? */
  int initial_vertical_offset;	/* Vertical offset for C80-type printers */
  int printing_initial_vertical_offset;	/* Vertical offset, for print cmd */
  int last_color;		/* Last color we printed */
  int last_pass_offset;		/* Starting row of last pass we printed */
  int last_pass;		/* Last pass printed */

} escp2_privdata_t;

extern void stpi_escp2_init_printer(stp_vars_t *v);
extern void stpi_escp2_deinit_printer(stp_vars_t *v);
extern void stpi_escp2_flush_pass(stp_vars_t *v, int passno,
				  int vertical_subpass);
extern void stpi_escp2_terminate_page(stp_vars_t *v);

#ifdef TEST_UNCOMPRESSED
#define COMPRESSION (0)
#define FILLFUNC stp_fill_uncompressed
#define COMPUTEFUNC stp_compute_uncompressed_linewidth
#define PACKFUNC stp_pack_uncompressed
#else
#define COMPRESSION (1)
#define FILLFUNC stp_fill_tiff
#define COMPUTEFUNC stp_compute_tiff_linewidth
#define PACKFUNC stp_pack_tiff
#endif

#endif /* GIMP_PRINT_INTERNAL_ESCP2_H */
/*
 * End of "$Id: print-escp2.h,v 1.1.1.4 2004/12/22 23:49:39 jlovell Exp $".
 */
