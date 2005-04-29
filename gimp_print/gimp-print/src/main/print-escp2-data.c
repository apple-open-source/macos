/*
 * "$Id: print-escp2-data.c,v 1.1.1.4 2004/12/22 23:49:38 jlovell Exp $"
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gimp-print/gimp-print.h>
#include "gimp-print-internal.h"
#include <gimp-print/gimp-print-intl-internal.h>
#include "print-escp2.h"

/*
 * Dot sizes are for:
 *
 *  0: 120/180
 *  1: 360
 *  2: 720x360
 *  3: 720
 *  4: 1440x720
 *  5: 2880x720 or 1440x1440
 *  6: 2880x1440
 *  7: 2880x2880
 */

/*   0     1     2     3     4     5     6     7 */

static const escp2_dot_size_t g1_dotsizes =
{   -2,   -2,   -2,   -2,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t g2_dotsizes =
{   -2,   -2,   -2,   -2,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t g3_dotsizes =
{    3,    3,    2,    1,    1,   -1,   -1,   -1 };

static const escp2_dot_size_t c6pl_dotsizes =
{ 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10 };

static const escp2_dot_size_t c4pl_dotsizes =
{ 0x12, 0x12, 0x12, 0x11, 0x10, 0x10, 0x10, 0x10 };

static const escp2_dot_size_t c4pl_pigment_dotsizes =
{ 0x12, 0x12, 0x12, 0x11, 0x11, 0x10, 0x10, 0x10 };

static const escp2_dot_size_t c3pl_dotsizes =
{ 0x11, 0x11, 0x11, 0x10, 0x10, 0x10, 0x10, 0x10 };

static const escp2_dot_size_t c3pl_pigment_dotsizes =
{ 0x10, 0x10, 0x10, 0x11, 0x12, 0x12, 0x12, 0x12 };

static const escp2_dot_size_t p3pl_dotsizes =
{ 0x10, 0x10, 0x10, 0x11, 0x12, 0x12, 0x12, 0x12 };

static const escp2_dot_size_t p1_5pl_dotsizes =
{ 0x10, 0x10, 0x11, 0x11, 0x12, 0x13, 0x13, 0x13 };

static const escp2_dot_size_t c2pl_dotsizes =
{ 0x12, 0x12, 0x12, 0x11, 0x13,   -1, 0x10, 0x10 };

static const escp2_dot_size_t c1_8pl_dotsizes =
{ 0x10, 0x10, 0x10, 0x10, 0x11, 0x12, 0x12, 0x13 };

static const escp2_dot_size_t sc440_dotsizes =
{    3,    3,    2,    1,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t sc480_dotsizes =
{ 0x13, 0x13, 0x13, 0x10, 0x10, 0x10, 0x10, 0x10 };

static const escp2_dot_size_t sc600_dotsizes =
{    4,    4,    3,    2,    1,   -1,   -1,   -1 };

static const escp2_dot_size_t sc640_dotsizes =
{    3,    3,    2,    1,    1,   -1,   -1,   -1 };

static const escp2_dot_size_t sc660_dotsizes =
{    3,    3,    0,    0,    0,   -1,   -1,   -1 };

static const escp2_dot_size_t sc670_dotsizes =
{ 0x12, 0x12, 0x12, 0x11, 0x11,   -1,   -1,   -1 };

static const escp2_dot_size_t sp700_dotsizes =
{    3,    3,    2,    1,    4,   -1,   -1,   -1 };

static const escp2_dot_size_t sp720_dotsizes =
{ 0x12, 0x12, 0x11, 0x11, 0x11,   -1,   -1,   -1 };

static const escp2_dot_size_t sp2000_dotsizes =
{ 0x11, 0x11, 0x11, 0x10, 0x10,   -1,   -1,   -1 };

static const escp2_dot_size_t spro_dye_dotsizes =
{    3,    3,    3,    1,    1,   -1,   -1,   -1 };

static const escp2_dot_size_t spro_pigment_dotsizes =
{    3,    3,    2,    1,    1,   -1,   -1,   -1 };

static const escp2_dot_size_t spro10000_dotsizes =
{    4, 0x11, 0x11, 0x10, 0x10,   -1,   -1,   -1 };

static const escp2_dot_size_t spro5000_dotsizes =
{    3,    3,    2,    1,    4,   -1,   -1,   -1 };

static const escp2_dot_size_t spro_c4pl_pigment_dotsizes =
{ 0x11, 0x11, 0x11, 0x10, 0x10,   -1,    5,    5 };

/*
 * Bits are for:
 *
 *  0: 120/180
 *  1: 360
 *  2: 720x360
 *  3: 720
 *  4: 1440x720
 *  5: 2880x720 or 1440x1440
 *  6: 2880x1440
 *  7: 2880x2880
 */

/*   0     1     2     3     4     5     6     7 */

static const escp2_bits_t variable_bits =
{    2,    2,    2,    2,    2,    2,    2,    2 };

static const escp2_bits_t stp950_bits =
{    2,    2,    2,    2,    2,    2,    1,    1 };

static const escp2_bits_t ultrachrome_bits =
{    2,    2,    2,    2,    2,    1,    1,    1 };

static const escp2_bits_t standard_bits =
{    1,    1,    1,    1,    1,    1,    1,    1 };

static const escp2_bits_t c1_8_bits =
{    2,    2,    2,    2,    2,    1,    1,    1 };

/*
 * Base resolutions are for:
 *
 *  0: 120/180
 *  1: 360
 *  2: 720x360
 *  3: 720
 *  4: 1440x720
 *  5: 2880x720 or 1440x1440
 *  6: 2880x1440
 *  7: 2880x2880
 */

/*   0     1     2     3     4     5     6     7 */

static const escp2_base_resolutions_t standard_base_res =
{  720,  720,  720,  720,  720,  720,  720,  720 };

static const escp2_base_resolutions_t g3_base_res =
{  720,  720,  720,  720,  360,  360,  360,  360 };

static const escp2_base_resolutions_t variable_base_res =
{  360,  360,  360,  360,  360,  360,  360,  360 };

static const escp2_base_resolutions_t stp950_base_res =
{  360,  360,  360,  360,  360,  720,  720,  720 };

static const escp2_base_resolutions_t ultrachrome_base_res =
{  360,  360,  360,  360,  360,  720,  720,  720 };

static const escp2_base_resolutions_t c1_8_base_res =
{  360,  360,  720,  720,  720, 1440, 1440, 1440 };

static const escp2_base_resolutions_t c1_5_base_res =
{  360,  360,  720,  720,  720,  720,  720,  720 };

static const escp2_base_resolutions_t stc900_base_res =
{  360,  360,  360,  360,  180,  180,  360,  360 };

static const escp2_base_resolutions_t pro_base_res =
{ 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880 };

/*
 * Densities are for:
 *
 *  0: 120/180
 *  1: 360
 *  2: 720x360
 *  3: 720
 *  4: 1440x720
 *  5: 2880x720 or 1440x1440
 *  6: 2880x1440
 *  7: 2880x2880
 */

/*  0    1     2       3    4      5      6      7 */

static const escp2_densities_t g1_densities =
{ 2.6, 1.3,  1.3,  0.568, 0.0,   0.0,   0.0,   0.0   };

static const escp2_densities_t g3_densities =
{ 2.6, 1.3,  0.65, 0.775, 0.388, 0.0,   0.0,   0.0,  };

static const escp2_densities_t c6pl_densities =
{ 4.0, 2.0,  1.0,  0.568, 0.568, 0.568, 0.0,   0.0   };

static const escp2_densities_t c4pl_2880_densities =
{ 2.6, 1.3,  0.65, 0.650, 0.650, 0.650, 0.32,  0.0   };

static const escp2_densities_t c4pl_densities =
{ 2.6, 1.3,  0.65, 0.568, 0.523, 0.792, 0.396, 0.0   };

static const escp2_densities_t c4pl_pigment_densities =
{ 2.3, 1.15, 0.58, 0.766, 0.388, 0.958, 0.479, 0.0   };

static const escp2_densities_t c3pl_pigment_densities =
{ 2.4, 1.2,  0.60, 0.600, 0.512, 0.512, 0.512, 0.0   };

static const escp2_densities_t c3pl_densities =
{ 2.6, 1.3,  0.65, 0.730, 0.7,   0.91,  0.455, 0.0   };

static const escp2_densities_t p3pl_densities =
{ 2.0, 1.5,  0.75, 0.627, 0.523, 0.523, 0.523, 0.262 };

static const escp2_densities_t p1_5pl_densities =
{ 2.0, 1.0,  0.5,  0.627, 0.483, 0.523, 0.262, 0.131 };

static const escp2_densities_t c2pl_densities =
{ 2.0, 1.0,  0.5,  0.650, 0.650, 0.0,   0.650, 0.325 };

static const escp2_densities_t c1_8pl_densities =
{ 2.3, 1.15, 0.57, 0.650, 0.650, 0.0,   0.650, 0.360 };

static const escp2_densities_t sc1500_densities =
{ 2.6, 1.3,  1.3,  0.631, 0.0,   0.0,   0.0,   0.0   };

static const escp2_densities_t sc440_densities =
{ 4.0, 2.0,  1.0,  0.900, 0.45,  0.0,   0.0,   0.0   };

static const escp2_densities_t sc480_densities =
{ 2.8, 1.4,  0.7,  0.710, 0.710, 0.546, 0.0,   0.0   };

static const escp2_densities_t sc660_densities =
{ 4.0, 2.0,  1.0,  0.646, 0.323, 0.0,   0.0,   0.0   };

static const escp2_densities_t sc980_densities =
{ 2.6, 1.3,  0.65, 0.511, 0.49,  0.637, 0.455, 0.0   };

static const escp2_densities_t sp700_densities =
{ 2.6, 1.3,  1.3,  0.775, 0.55,  0.0,   0.0,   0.0   };

static const escp2_densities_t sp2000_densities =
{ 2.6, 1.3,  0.65, 0.852, 0.438, 0.219, 0.0,   0.0   };

static const escp2_densities_t spro_dye_densities =
{ 2.6, 1.3,  1.3,  0.775, 0.388, 0.275, 0.0,   0.0   };

static const escp2_densities_t spro_pigment_densities =
{ 3.0, 1.5,  0.78, 0.775, 0.388, 0.194, 0.0,   0.0   };

static const escp2_densities_t spro10000_densities =
{ 2.6, 1.3,  0.65, 0.431, 0.216, 0.392, 0.0,   0.0   };


static const input_slot_t standard_roll_feed_input_slots[] =
{
  {
    "Standard",
    N_("Standard"),
    0,
    0,
    0,
    { 16, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "Roll",
    N_("Roll Feed"),
    0,
    1,
    ROLL_FEED_DONT_EJECT,
    { 16, "IR\002\000\000\001EX\006\000\000\000\000\000\005\001" },
    { 6, "IR\002\000\000\002" }
  }
};

static const input_slot_list_t standard_roll_feed_input_slot_list =
{
  standard_roll_feed_input_slots,
  sizeof(standard_roll_feed_input_slots) / sizeof(const input_slot_t)
};

static const input_slot_t cutter_roll_feed_input_slots[] =
{
  {
    "Standard",
    N_("Standard"),
    0,
    0,
    0,
    { 16, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "RollCutPage",
    N_("Roll Feed (cut each page)"),
    0,
    1,
    ROLL_FEED_CUT_ALL,
    { 16, "IR\002\000\000\001EX\006\000\000\000\000\000\005\001" },
    { 6, "IR\002\000\000\002" }
  },
  {
    "RollCutNone",
    N_("Roll Feed (do not cut)"),
    0,
    1,
    ROLL_FEED_DONT_EJECT,
    { 16, "IR\002\000\000\001EX\006\000\000\000\000\000\005\001" },
    { 6, "IR\002\000\000\002" }
  }
};

static const input_slot_list_t cutter_roll_feed_input_slot_list =
{
  cutter_roll_feed_input_slots,
  sizeof(cutter_roll_feed_input_slots) / sizeof(const input_slot_t)
};

static const input_slot_t cd_cutter_roll_feed_input_slots[] =
{
  {
    "Standard",
    N_("Standard"),
    0,
    0,
    0,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000PP\003\000\000\001\377" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "Manual",
    N_("Manual Feed"),
    0,
    0,
    0,
    { 36, "PM\002\000\000\000IR\002\000\000\001EX\006\000\000\000\000\000\005\000FP\003\000\000\000\000PP\003\000\000\002\001" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "CD",
    N_("Print to CD"),
    1,
    0,
    0,
    { 36, "PM\002\000\000\000IR\002\000\000\001EX\006\000\000\000\000\000\005\000FP\003\000\000\000\000PP\003\000\000\002\001" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "RollCutPage",
    N_("Roll Feed (cut each page)"),
    0,
    1,
    ROLL_FEED_CUT_ALL,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\001PP\003\000\000\001\377" },
    { 6, "IR\002\000\000\002" }
  },
  {
    "RollCutNone",
    N_("Roll Feed (do not cut)"),
    0,
    1,
    ROLL_FEED_DONT_EJECT,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\001PP\003\000\000\001\377" },
    { 6, "IR\002\000\000\002" }
  }
};

static const input_slot_list_t cd_cutter_roll_feed_input_slot_list =
{
  cd_cutter_roll_feed_input_slots,
  sizeof(cd_cutter_roll_feed_input_slots) / sizeof(const input_slot_t)
};

static const input_slot_t cd_roll_feed_input_slots[] =
{
  {
    "Standard",
    N_("Standard"),
    0,
    0,
    0,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000PP\003\000\000\001\377" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "Manual",
    N_("Manual Feed"),
    0,
    0,
    0,
    { 36, "PM\002\000\000\000IR\002\000\000\001EX\006\000\000\000\000\000\005\000FP\003\000\000\000\000PP\003\000\000\002\001" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "CD",
    N_("Print to CD"),
    1,
    0,
    0,
    { 36, "PM\002\000\000\000IR\002\000\000\001EX\006\000\000\000\000\000\005\000FP\003\000\000\000\000PP\003\000\000\002\001" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "Roll",
    N_("Roll Feed"),
    0,
    1,
    ROLL_FEED_DONT_EJECT,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\001PP\003\000\000\001\377" },
    { 6, "IR\002\000\000\002" }
  }
};

static const input_slot_list_t cd_roll_feed_input_slot_list =
{
  cd_roll_feed_input_slots,
  sizeof(cd_roll_feed_input_slots) / sizeof(const input_slot_t)
};

static const input_slot_t pro_roll_feed_input_slots[] =
{
  {
    "Standard",
    N_("Standard"),
    0,
    0,
    0,
    { 7, "PP\003\000\000\002\000" },
    { 0, "" }
  },
  {
    "Roll",
    N_("Roll Feed"),
    0,
    1,
    0,
    { 7, "PP\003\000\000\003\000" },
    { 0, "" }
  }
};

static const input_slot_list_t pro_roll_feed_input_slot_list =
{
  pro_roll_feed_input_slots,
  sizeof(pro_roll_feed_input_slots) / sizeof(const input_slot_t)
};

static const input_slot_t spro5000_input_slots[] =
{
  {
    "CutSheet1",
    N_("Cut Sheet Bin 1"),
    0,
    0,
    0,
    { 7, "PP\003\000\000\001\001" },
    { 0, "" }
  },
  {
    "CutSheet2",
    N_("Cut Sheet Bin 2"),
    0,
    0,
    0,
    { 7, "PP\003\000\000\002\001" },
    { 0, "" }
  },
  {
    "CutSheetAuto",
    N_("Cut Sheet Autoselect"),
    0,
    0,
    0,
    { 7, "PP\003\000\000\001\377" },
    { 0, "" }
  },
  {
    "ManualSelect",
    N_("Manual Selection"),
    0,
    0,
    0,
    { 7, "PP\003\000\000\002\001" },
    { 0, "" }
  }
};

static const input_slot_list_t spro5000_input_slot_list =
{
  spro5000_input_slots,
  sizeof(spro5000_input_slots) / sizeof(const input_slot_t)
};

static const input_slot_list_t default_input_slot_list =
{
  NULL,
  0,
};

static const stp_raw_t new_init_sequence =
{
  29, "\0\0\0\033\001@EJL 1284.4\n@EJL     \n\033@"
};

static const stp_raw_t je_deinit_sequence =
{
  5, "JE\001\000\000"
};

#define INCH(x)		(72 * x)

static const quality_t standard_qualities[] =
{
  { "FastEconomy", N_("Fast Economy"), 360, 120, 0, 0, 360, 90 },
  { "Economy",     N_("Economy"),      360, 240, 0, 0, 360, 180 },
  { "Draft",       N_("Draft"),        360, 360, 0, 0, 360, 360 },
  { "Standard",    N_("Standard"),     0, 0, 0, 0, 720, 360 },
  { "High",        N_("High"),         0, 0, 0, 0, 720, 720 },
  { "Photo",       N_("Photo"),        0, 0, 1440, 720, 1440, 720 },
  { "HighPhoto",   N_("Super Photo"),  0, 0, 2880, 2880, 2880, 1440 },
  { "UltraPhoto",  N_("Ultra Photo"),  0, 0, 2880, 2880, 2880, 2880 },
  { "Best",        N_("Best"),         720, 360, 0, 0, -1, -1 },
};

static const quality_list_t standard_quality_list =
{
  standard_qualities,
  sizeof(standard_qualities) / sizeof(quality_t)
};
  

const stpi_escp2_printer_t stpi_escp2_model_capabilities[] =
{
  /* FIRST GENERATION PRINTERS */
  /* 0: Stylus Color */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    15, 1, 4, 15, 1, 4, 15, 1, 4, 4,
    360, 14400, -1, 720, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    1, 7, 0, 0,
    g1_dotsizes, g1_densities, &stpi_escp2_simple_drops,
    stpi_escp2_720dpi_reslist, &stpi_escp2_standard_inkgroup,
    standard_bits, standard_base_res, &default_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },
  /* 1: Stylus Color 400/500 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 4,
    360, 14400, -1, 720, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    1, 7, 0, 0,
    g2_dotsizes, g1_densities, &stpi_escp2_simple_drops,
    stpi_escp2_sc500_reslist, &stpi_escp2_standard_inkgroup,
    standard_bits, standard_base_res, &default_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },
  /* 2: Stylus Color 1500 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 4,
    360, 14400, -1, 720, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(17), INCH(44), INCH(2), INCH(2),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    1, 7, 0, 0,
    g1_dotsizes, sc1500_densities, &stpi_escp2_simple_drops,
    stpi_escp2_sc500_reslist, &stpi_escp2_cmy_inkgroup,
    standard_bits, standard_base_res, &standard_roll_feed_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },
  /* 3: Stylus Color 600 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 4,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 8,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    8, 9, 0, 30, 8, 9, 0, 30, 8, 9, 0, 0, 8, 9, 0, 0, -1, -1, 0, 0,
    1, 7, 0, 0,
    sc600_dotsizes, g3_densities, &stpi_escp2_simple_drops,
    stpi_escp2_g3_reslist, &stpi_escp2_standard_inkgroup,
    standard_bits, g3_base_res, &default_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },
  /* 4: Stylus Color 800 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    64, 1, 2, 64, 1, 2, 64, 1, 2, 4,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 4, 0, 0, 0, 0,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    8, 9, 9, 40, 8, 9, 9, 40, 8, 9, 0, 0, 8, 9, 0, 0, -1, -1, 0, 0,
    1, 7, 0, 0,
    g3_dotsizes, g3_densities, &stpi_escp2_simple_drops,
    stpi_escp2_g3_reslist, &stpi_escp2_standard_inkgroup,
    standard_bits, g3_base_res, &default_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },
  /* 5: Stylus Color 850 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    64, 1, 2, 64, 1, 2, 64, 1, 2, 4,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 4, 0, 0, 0, 0,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    1, 7, 0, 0,
    g3_dotsizes, g3_densities, &stpi_escp2_simple_drops,
    stpi_escp2_g3_reslist, &stpi_escp2_standard_inkgroup,
    standard_bits, g3_base_res, &default_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },
  /* 6: Stylus Color 1520 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    64, 1, 2, 64, 1, 2, 64, 1, 2, 4,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 4, 0, 0, 0, 0,
    INCH(17), INCH(44), INCH(2), INCH(2),
    8, 9, 9, 40, 8, 9, 9, 40, 8, 9, 0, 0, 8, 9, 0, 0, -1, -1, 0, 0,
    1, 7, 0, 0,
    g3_dotsizes, g3_densities, &stpi_escp2_simple_drops,
    stpi_escp2_g3_reslist, &stpi_escp2_standard_inkgroup,
    standard_bits, g3_base_res, &standard_roll_feed_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },

  /* SECOND GENERATION PRINTERS */
  /* 7: Stylus Photo 700 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 8,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 30, 9, 9, 0, 30, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    1, 7, 0, 0,
    sp700_dotsizes, sp700_densities, &stpi_escp2_simple_drops,
    stpi_escp2_g3_reslist, &stpi_escp2_photo_gen1_inkgroup,
    standard_bits, g3_base_res, &default_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },
  /* 8: Stylus Photo EX */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_NO |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 8,
    INCH(118 / 10), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 30, 9, 9, 0, 30, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    1, 7, 0, 0,
    sp700_dotsizes, sp700_densities, &stpi_escp2_simple_drops,
    stpi_escp2_g3_reslist, &stpi_escp2_photo_gen1_inkgroup,
    standard_bits, g3_base_res, &default_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },
  /* 9: Stylus Photo */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 6,
    360, 14400, -1, 720, 720, 90, 90, 0, 1, 0, 0, 0, 0, 8,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 30, 9, 9, 0, 30, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    1, 7, 0, 0,
    sp700_dotsizes, sp700_densities, &stpi_escp2_simple_drops,
    stpi_escp2_g3_720dpi_reslist, &stpi_escp2_photo_gen1_inkgroup,
    standard_bits, g3_base_res, &default_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },

  /* THIRD GENERATION PRINTERS */
  /* 10: Stylus Color 440/460 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    21, 1, 4, 21, 1, 4, 21, 1, 4, 4,
    360, 14400, -1, 720, 720, 90, 90, 0, 1, 0, 0, 0, 0, 8,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    1, 15, 0, 0,
    sc440_dotsizes, sc440_densities, &stpi_escp2_simple_drops,
    stpi_escp2_g3_720dpi_reslist, &stpi_escp2_standard_inkgroup,
    standard_bits, standard_base_res, &default_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },
  /* 11: Stylus Color 640 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 4,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 8,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    1, 15, 0, 0,
    sc640_dotsizes, sc440_densities, &stpi_escp2_simple_drops,
    stpi_escp2_sc640_reslist, &stpi_escp2_standard_inkgroup,
    standard_bits, standard_base_res, &default_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },
  /* 12: Stylus Color 740/Stylus Scan 2000/Stylus Scan 2500 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 4,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c6pl_dotsizes, c6pl_densities, &stpi_escp2_variable_6pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_standard_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },
  /* 13: Stylus Color 900 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    96, 1, 2, 192, 1, 1, 192, 1, 1, 4,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c3pl_dotsizes, c3pl_densities, &stpi_escp2_variable_3pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_standard_inkgroup,
    variable_bits, stc900_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 14: Stylus Photo 750 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c6pl_dotsizes, c6pl_densities, &stpi_escp2_variable_6pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_photo_gen1_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 15: Stylus Photo 1200 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(13), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c6pl_dotsizes, c6pl_densities, &stpi_escp2_variable_6pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_photo_gen1_inkgroup,
    variable_bits, variable_base_res, &standard_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 16: Stylus Color 860 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 4,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_densities, &stpi_escp2_variable_1440_4pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_standard_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 17: Stylus Color 1160 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 4,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(13), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_densities, &stpi_escp2_variable_1440_4pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_standard_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 18: Stylus Color 660 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 4,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 8,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 9, 9, 9, 9, 9, 26, 9, 9, 9, 0, 9, 9, 9, 0, -1, -1, 0, 0,
    1, 15, 0, 0,
    sc660_dotsizes, sc660_densities, &stpi_escp2_simple_drops,
    stpi_escp2_sc640_reslist, &stpi_escp2_standard_inkgroup,
    standard_bits, standard_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 19: Stylus Color 760 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 4,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_densities, &stpi_escp2_variable_1440_4pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_standard_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 20: Stylus Photo 720 (Australia) */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    sp720_dotsizes, c6pl_densities, &stpi_escp2_variable_6pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_photo_gen1_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 21: Stylus Color 480 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_YES | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    15, 15, 3, 48, 48, 3, 48, 48, 3, 4,
    360, 14400, 360, 720, 720, 90, 90, 0, 1, 0, 0, -99, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    sc480_dotsizes, sc480_densities, &stpi_escp2_variable_x80_6pl_drops,
    stpi_escp2_720dpi_soft_reslist, &stpi_escp2_x80_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 22: Stylus Photo 870/875 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 97, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_densities, &stpi_escp2_variable_1440_4pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_photo_gen2_inkgroup,
    variable_bits, variable_base_res, &standard_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 23: Stylus Photo 1270 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 97, 0, 0, 0,
    INCH(13), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_densities, &stpi_escp2_variable_1440_4pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_photo_gen2_inkgroup,
    variable_bits, variable_base_res, &standard_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 24: Stylus Color 3000 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    64, 1, 2, 64, 1, 2, 64, 1, 2, 4,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 4, 0, 0, 0, 0,
    INCH(17), INCH(44), INCH(2), INCH(2),
    8, 9, 9, 40, 8, 9, 9, 40, 8, 9, 0, 0, 8, 9, 0, 0, -1, -1, 0, 0,
    1, 7, 0, 0,
    g3_dotsizes, g3_densities, &stpi_escp2_simple_drops,
    stpi_escp2_g3_reslist, &stpi_escp2_standard_inkgroup,
    standard_bits, g3_base_res, &standard_roll_feed_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },
  /* 25: Stylus Color 670 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    32, 1, 4, 64, 1, 2, 64, 1, 2, 4,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    sc670_dotsizes, c6pl_densities, &stpi_escp2_variable_6pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_standard_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 26: Stylus Photo 2000P */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(13), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    2, 15, 0, 0,
    sp2000_dotsizes, sp2000_densities, &stpi_escp2_variable_2000p_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_photo_pigment_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 27: Stylus Pro 5000 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    64, 1, 2, 64, 1, 2, 64, 1, 2, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 4,
    INCH(13), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 30, 9, 9, 0, 30, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    1, 7, 0, 0,
    spro5000_dotsizes, sp700_densities, &stpi_escp2_simple_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_photo_gen1_inkgroup,
    standard_bits, g3_base_res, &spro5000_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },
  /* 28: Stylus Pro 7000 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(24), INCH(1200), INCH(7), INCH(7),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 9, 9, 9, 9, 9, 9, -1, -1, 0, 0,
    1, 7, 0, 0,
    spro_dye_dotsizes, spro_dye_densities, &stpi_escp2_simple_drops,
    stpi_escp2_pro_reslist, &stpi_escp2_photo_gen1_inkgroup,
    standard_bits, pro_base_res, &pro_roll_feed_input_slot_list,
    &standard_quality_list, NULL, NULL,
    &stpi_escp2_pro7000_printer_weave_list
  },
  /* 29: Stylus Pro 7500 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_YES |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(24), INCH(1200), INCH(7), INCH(7),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 9, 9, 9, 9, 9, 9, -1, -1, 0, 0,
    1, 7, 0, 0,
    spro_pigment_dotsizes, spro_pigment_densities, &stpi_escp2_simple_drops,
    stpi_escp2_pro_reslist, &stpi_escp2_photo_pigment_inkgroup,
    standard_bits, pro_base_res, &pro_roll_feed_input_slot_list,
    &standard_quality_list, NULL, NULL,
    &stpi_escp2_pro7500_printer_weave_list
  },
  /* 30: Stylus Pro 9000 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(44), INCH(1200), INCH(7), INCH(7),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 9, 9, 9, 9, 9, 9, -1, -1, 0, 0,
    1, 7, 0, 0,
    spro_dye_dotsizes, spro_dye_densities, &stpi_escp2_simple_drops,
    stpi_escp2_pro_reslist, &stpi_escp2_photo_gen1_inkgroup,
    standard_bits, pro_base_res, &pro_roll_feed_input_slot_list,
    &standard_quality_list, NULL, NULL,
    &stpi_escp2_pro7000_printer_weave_list
  },
  /* 31: Stylus Pro 9500 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_YES |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(44), INCH(1200), INCH(7), INCH(7),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 9, 9, 9, 9, 9, 9, -1, -1, 0, 0,
    1, 7, 0, 0,
    spro_pigment_dotsizes, spro_pigment_densities, &stpi_escp2_simple_drops,
    stpi_escp2_pro_reslist, &stpi_escp2_photo_pigment_inkgroup,
    standard_bits, pro_base_res, &pro_roll_feed_input_slot_list,
    &standard_quality_list, NULL, NULL,
    &stpi_escp2_pro7500_printer_weave_list
  },
  /* 32: Stylus Color 777/680 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 4,
    360, 14400, -1, 2880, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, &stpi_escp2_variable_2880_4pl_drops,
    stpi_escp2_2880dpi_reslist, &stpi_escp2_standard_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 33: Stylus Color 880/83/C60 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 4,
    360, 14400, -1, 2880, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, &stpi_escp2_variable_2880_4pl_drops,
    stpi_escp2_2880dpi_reslist, &stpi_escp2_standard_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 34: Stylus Color 980 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    96, 1, 2, 192, 1, 1, 192, 1, 1, 4,
    360, 14400, -1, 2880, 720, 90, 90, 38, 1, 0, 0, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c3pl_dotsizes, sc980_densities, &stpi_escp2_variable_3pl_drops,
    stpi_escp2_2880dpi_reslist, &stpi_escp2_standard_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 35: Stylus Photo 780/790/810/820 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 2880, 720, 90, 90, 0, 1, 0, 55, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, &stpi_escp2_variable_2880_4pl_drops,
    stpi_escp2_2880dpi_reslist, &stpi_escp2_photo_gen2_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 36: Stylus Photo 785/890/895/915/935 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 2880, 720, 90, 90, 0, 1, 0, 55, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, &stpi_escp2_variable_2880_4pl_drops,
    stpi_escp2_2880dpi_reslist, &stpi_escp2_photo_gen2_inkgroup,
    variable_bits, variable_base_res, &standard_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 37: Stylus Photo 1280/1290 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 2880, 720, 90, 90, 0, 1, 0, 55, 0, 0, 0,
    INCH(13), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, &stpi_escp2_variable_2880_4pl_drops,
    stpi_escp2_2880dpi_reslist, &stpi_escp2_photo_gen2_inkgroup,
    variable_bits, variable_base_res, &standard_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 38: Stylus Color 580 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_YES | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    15, 15, 3, 48, 48, 3, 48, 48, 3, 4,
    360, 14400, 360, 1440, 720, 90, 90, 0, 1, 0, 0, -99, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    sc480_dotsizes, sc480_densities, &stpi_escp2_variable_x80_6pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_x80_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 39: Stylus Color Pro XL */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    16, 1, 4, 16, 1, 4, 16, 1, 4, 4,
    360, 14400, -1, 720, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(13), INCH(1200), INCH(2), INCH(2),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    1, 7, 0, 0,
    g1_dotsizes, g1_densities, &stpi_escp2_simple_drops,
    stpi_escp2_720dpi_reslist, &stpi_escp2_standard_inkgroup,
    standard_bits, standard_base_res, &default_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },
  /* 40: Stylus Pro 5500 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_YES |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(13), INCH(1200), INCH(2), INCH(2),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    1, 7, 0, 0,
    spro_pigment_dotsizes, spro_pigment_densities, &stpi_escp2_simple_drops,
    stpi_escp2_pro_reslist, &stpi_escp2_photo_pigment_inkgroup,
    standard_bits, pro_base_res, &spro5000_input_slot_list,
    &standard_quality_list, NULL, NULL,
    &stpi_escp2_pro7500_printer_weave_list
  },
  /* 41: Stylus Pro 10000 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_YES |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(44), INCH(1200), INCH(7), INCH(7),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 9, 9, 9, 9, 9, 9, -1, -1, 0, 0,
    1, 7, 0, 0,
    spro10000_dotsizes, spro10000_densities, &stpi_escp2_spro10000_drops,
    stpi_escp2_pro_reslist, &stpi_escp2_photo_gen2_inkgroup,
    variable_bits, pro_base_res, &pro_roll_feed_input_slot_list,
    &standard_quality_list, NULL, NULL,
    &stpi_escp2_pro7000_printer_weave_list
  },
  /* 42: Stylus C20SX/C20UX */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_YES | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    15, 15, 3, 48, 48, 3, 48, 48, 3, 4,
    360, 14400, -1, 720, 720, 90, 90, 0, 1, 0, 0, -99, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    sc480_dotsizes, sc480_densities, &stpi_escp2_variable_x80_6pl_drops,
    stpi_escp2_720dpi_soft_reslist, &stpi_escp2_x80_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 43: Stylus C40SX/C40UX/C41SX/C41UX/C42SX/C42UX */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_YES | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    15, 15, 3, 48, 48, 3, 48, 48, 3, 4,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, -99, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    sc480_dotsizes, sc480_densities, &stpi_escp2_variable_x80_6pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_x80_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 44: Stylus C70/C80 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    60, 60, 2, 180, 180, 2, 180, 180, 2, 4,
    360, 14400, -1, 2880, 1440, 360, 180, 0, 1, 0, 0, -240, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    4, 15, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_densities, &stpi_escp2_variable_3pl_pigment_drops,
    stpi_escp2_2880_1440dpi_reslist, &stpi_escp2_c80_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 45: Stylus Color Pro */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_NO |
     MODEL_PRINT_TO_CD_NO),
    16, 1, 4, 16, 1, 4, 16, 1, 4, 4,
    360, 14400, -1, 720, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    1, 7, 0, 0,
    g1_dotsizes, g1_densities, &stpi_escp2_simple_drops,
    stpi_escp2_720dpi_reslist, &stpi_escp2_standard_inkgroup,
    standard_bits, standard_base_res, &default_input_slot_list,
    &standard_quality_list, NULL, NULL,
    NULL
  },
  /* 46: Stylus Photo 950/960 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_YES | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_YES),
    96, 96, 2, 96, 96, 2, 24, 24, 1, 6,
    360, 14400, -1, 2880, 1440, 360, 180, 0, 1, 0, 190, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 0, 0,
    4, 15, 0, 0,
    c2pl_dotsizes, c2pl_densities, &stpi_escp2_variable_2pl_drops,
    stpi_escp2_superfine_reslist, &stpi_escp2_f360_photo_inkgroup,
    stp950_bits, stp950_base_res, &cd_cutter_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 47: Stylus Photo 2100/2200 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_YES | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_YES),
    96, 96, 2, 96, 96, 2, 192, 192, 1, 7,
    360, 14400, -1, 2880, 1440, 360, 180, 0, 1, 0, 190, 0, 0, 0,
    INCH(13), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 0, 0,
    4, 15, 0, 0,
    c4pl_pigment_dotsizes, c4pl_pigment_densities, &stpi_escp2_variable_ultrachrome_drops,
    stpi_escp2_superfine_reslist, &stpi_escp2_f360_ultrachrome_inkgroup,
    ultrachrome_bits, ultrachrome_base_res, &cd_cutter_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 48: Stylus Pro 7600 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_PRO | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_YES |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 7,
    360, 14400, -1, 2880, 1440, 360, 180, 0, 1, 0, 0, 0, 0, 0,
    INCH(24), INCH(1200), INCH(7), INCH(7),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    spro_c4pl_pigment_dotsizes, c4pl_pigment_densities, &stpi_escp2_variable_ultrachrome_drops,
    stpi_escp2_pro_reslist, &stpi_escp2_ultrachrome_inkgroup,
    ultrachrome_bits, pro_base_res, &pro_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    &stpi_escp2_pro7600_printer_weave_list
  },
  /* 49: Stylus Pro 9600 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_PRO | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_YES |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 7,
    360, 14400, -1, 2880, 1440, 360, 180, 0, 1, 0, 0, 0, 0, 0,
    INCH(44), INCH(1200), INCH(7), INCH(7),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    spro_c4pl_pigment_dotsizes, c4pl_pigment_densities, &stpi_escp2_variable_ultrachrome_drops,
    stpi_escp2_pro_reslist, &stpi_escp2_ultrachrome_inkgroup,
    ultrachrome_bits, pro_base_res, &pro_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    &stpi_escp2_pro7600_printer_weave_list
  },
  /* 50: Stylus Photo 825/830 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 2880, 1440, 90, 90, 0, 1, 0, 55, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, &stpi_escp2_variable_2880_4pl_drops,
    stpi_escp2_2880_1440dpi_reslist, &stpi_escp2_photo_gen2_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 51: Stylus Photo 925 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 2880, 1440, 90, 90, 0, 1, 0, 55, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, &stpi_escp2_variable_2880_4pl_drops,
    stpi_escp2_2880_1440dpi_reslist, &stpi_escp2_photo_gen2_inkgroup,
    variable_bits, variable_base_res, &cutter_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 52: Stylus Color C62 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 4,
    360, 14400, -1, 2880, 1440, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, &stpi_escp2_variable_2880_4pl_drops,
    stpi_escp2_2880_1440dpi_reslist, &stpi_escp2_standard_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 53: Japanese PM-950C */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_YES | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_YES),
    96, 96, 2, 96, 96, 2, 24, 24, 1, 6,
    360, 14400, -1, 2880, 1440, 360, 180, 0, 1, 0, 190, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 0, 0,
    4, 15, 0, 0,
    c2pl_dotsizes, c2pl_densities, &stpi_escp2_variable_2pl_drops,
    stpi_escp2_superfine_reslist, &stpi_escp2_f360_photo7_japan_inkgroup,
    stp950_bits, stp950_base_res, &cd_cutter_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 54: Stylus Photo EX3 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 6,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, 0, 0, 0,
    INCH(13), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    sp720_dotsizes, c6pl_densities, &stpi_escp2_variable_6pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_photo_gen1_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 55: Stylus C82/CX-5200 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    59, 60, 2, 180, 180, 2, 180, 180, 2, 4,
    360, 14400, -1, 2880, 1440, 360, 180, 0, 1, 0, 0, -240, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    4, 15, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_densities, &stpi_escp2_variable_3pl_pigment_drops,
    stpi_escp2_2880_1440dpi_reslist, &stpi_escp2_c80_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 56: Stylus C50 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    15, 15, 3, 48, 48, 3, 48, 48, 3, 4,
    360, 14400, -1, 1440, 720, 90, 90, 0, 1, 0, 0, -99, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_densities, &stpi_escp2_variable_x80_6pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_x80_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 57: Japanese PM-970C */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_YES | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    180, 180, 2, 360, 360, 1, 360, 360, 1, 6,
    360, 14400, -1, 2880, 2880, 720, 360, 0, 1, 0, 190, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    4, 15, 0, 0,
    c1_8pl_dotsizes, c1_8pl_densities, &stpi_escp2_variable_2pl_drops,
    stpi_escp2_superfine_reslist, &stpi_escp2_f360_photo7_japan_inkgroup,
    c1_8_bits, c1_8_base_res, &cutter_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 58: Japanese PM-930C */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    90, 90, 2, 90, 90, 2, 90, 90, 2, 6,
    360, 14400, -1, 2880, 2880, 720, 360, 0, 1, 0, 190, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    4, 15, 0, 0,
    c1_8pl_dotsizes, c1_8pl_densities, &stpi_escp2_variable_2pl_drops,
    stpi_escp2_superfine_reslist, &stpi_escp2_photo_gen2_inkgroup,
    c1_8_bits, c1_8_base_res, &cutter_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 59: Stylus C43SX/C43UX/C44SX/C44UX (WRONG -- see 43!) */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_YES | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    15, 15, 3, 48, 48, 3, 48, 48, 3, 4,
    360, 14400, -1, 2880, 720, 90, 90, 0, 1, 0, 0, -99, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_densities, &stpi_escp2_variable_x80_6pl_drops,
    stpi_escp2_1440dpi_reslist, &stpi_escp2_x80_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 60: Stylus C84 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    59, 60, 2, 180, 180, 2, 180, 180, 2, 4,
    360, 14400, -1, 2880, 1440, 360, 180, 0, 1, 0, 0, -240, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    4, 15, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_densities, &stpi_escp2_variable_3pl_pigment_drops,
    stpi_escp2_2880_1440dpi_reslist, &stpi_escp2_c80_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 61: Stylus Color C63/C64 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_NO),
    29, 30, 3, 90, 90, 3, 90, 90, 3, 4,
    360, 14400, -1, 2880, 1440, 360, 120, 0, 1, 0, 0, -180, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0,
    4, 15, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_densities, &stpi_escp2_variable_3pl_pigment_drops,
    stpi_escp2_2880_1440dpi_reslist, &stpi_escp2_c64_inkgroup,
    variable_bits, variable_base_res, &default_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 62: Stylus Photo 900 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_YES),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 2880, 720, 90, 90, 0, 1, 0, 55, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 399, 394, 595, 842,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, &stpi_escp2_variable_2880_4pl_drops,
    stpi_escp2_2880dpi_reslist, &stpi_escp2_photo_gen2_inkgroup,
    variable_bits, variable_base_res, &cd_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 63: Stylus Photo R300 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_YES),
    90, 1, 3, 90, 1, 3, 90, 1, 3, 6,
    360, 14400, -1, 2880, 1440, 360, 120, 0, 1, 0, 190, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 595, 842,
    4, 15, 0, 0,
    p3pl_dotsizes, p3pl_densities, &stpi_escp2_variable_3pl_pmg_drops,
    stpi_escp2_superfine_reslist, &stpi_escp2_photo_gen3_inkgroup,
    variable_bits, variable_base_res, &cd_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
  /* 64: PM-G800 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO | MODEL_SEND_ZERO_ADVANCE_YES |
     MODEL_SUPPORTS_INK_CHANGE_NO | MODEL_PACKET_MODE_YES |
     MODEL_PRINT_TO_CD_YES),
    180, 1, 2, 180, 1, 2, 180, 1, 2, 8,
    360, 14400, -1, 2880, 1440, 360, 180, 0, 1, 0, 190, 0, 0, 0,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 595, 842,
    4, 15, 0, 0,
    p1_5pl_dotsizes, p1_5pl_densities, &stpi_escp2_variable_1_5pl_drops,
    stpi_escp2_superfine_reslist, &stpi_escp2_cmykrb_inkgroup,
    variable_bits, c1_5_base_res, &cd_cutter_roll_feed_input_slot_list,
    &standard_quality_list, &new_init_sequence, &je_deinit_sequence,
    NULL
  },
};

const int stpi_escp2_model_limit =
sizeof(stpi_escp2_model_capabilities) / sizeof(stpi_escp2_printer_t);
