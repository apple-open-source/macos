/*
 * "$Id: print-escp2-data.c,v 1.227 2007/05/28 23:38:16 rlk Exp $"
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
#include <gutenprint/gutenprint.h>
#include "gutenprint-internal.h"
#include <gutenprint/gutenprint-intl-internal.h>
#include "print-escp2.h"
#include <limits.h>

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
 *  8: 5760x2880
 */

/*   0     1     2     3     4     5     6     7     8 */

static const escp2_dot_size_t g1_dotsizes =
{   -2,   -2,   -2,   -2,   -1,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t g2_dotsizes =
{   -2,   -2,   -2,   -2,   -1,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t g3_dotsizes =
{    3,    3,    2,    1,    1,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t c6pl_dotsizes =
{ 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10 };

static const escp2_dot_size_t c4pl_dotsizes =
{ 0x12, 0x12, 0x12, 0x11, 0x10, 0x10, 0x10, 0x10, 0x10 };

static const escp2_dot_size_t c4pl_pigment_dotsizes =
{ 0x12, 0x12, 0x12, 0x11, 0x11, 0x10, 0x10, 0x10, 0x10 };

static const escp2_dot_size_t c3pl_dotsizes =
{ 0x11, 0x11, 0x11, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10 };

static const escp2_dot_size_t c3pl_pigment_dotsizes =
{ 0x10, 0x10, 0x10, 0x11, 0x12, 0x12, 0x12, 0x12, 0x12 };

static const escp2_dot_size_t p3pl_dotsizes =
{ 0x10, 0x10, 0x10, 0x11, 0x12, 0x12, 0x12, 0x12, 0x12 };

static const escp2_dot_size_t p1_5pl_dotsizes =
{ 0x10, 0x10, 0x10, 0x11, 0x12, 0x13, 0x13, 0x13, 0x13 };

static const escp2_dot_size_t claria_dotsizes =
{ 0x33, 0x33, 0x24, 0x24, 0x24, 0x25, 0x25, 0x25, 0x25 };

static const escp2_dot_size_t claria_1400_dotsizes =
{ 0x33, 0x33, 0x21, 0x21, 0x33, 0x25, 0x25, 0x25, 0x25 };

static const escp2_dot_size_t c2pl_dotsizes =
{ 0x12, 0x12, 0x12, 0x11, 0x13,   -1, 0x10, 0x10, 0x10 };

static const escp2_dot_size_t c1_8pl_dotsizes =
{ 0x10, 0x10, 0x10, 0x10, 0x11, 0x12, 0x12, 0x13, 0x13 };

static const escp2_dot_size_t p3_5pl_dotsizes =
{ 0x10, 0x10, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12 };

static const escp2_dot_size_t sc440_dotsizes =
{    3,    3,    2,    1,   -1,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t sc480_dotsizes =
{ 0x13, 0x13, 0x13, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10 };

static const escp2_dot_size_t sc600_dotsizes =
{    4,    4,    3,    2,    1,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t sc640_dotsizes =
{    3,    3,    2,    1,    1,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t sc660_dotsizes =
{    3,    3,    0,    0,    0,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t sc670_dotsizes =
{ 0x12, 0x12, 0x12, 0x11, 0x11,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t sp700_dotsizes =
{    3,    3,    2,    1,    4,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t sp720_dotsizes =
{ 0x12, 0x12, 0x11, 0x11, 0x11,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t sp2000_dotsizes =
{ 0x11, 0x11, 0x11, 0x10, 0x10,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t spro_dye_dotsizes =
{    3,    3,    3,    1,    1,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t spro_pigment_dotsizes =
{    3,    3,    2,    1,    1,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t spro10000_dotsizes =
{    4, 0x11, 0x11, 0x10, 0x10,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t spro5000_dotsizes =
{    3,    3,    2,    1,    4,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t spro_c4pl_pigment_dotsizes =
{ 0x11, 0x11, 0x11, 0x10, 0x10,   -1,    5,    5,    5 };

static const escp2_dot_size_t picturemate_dotsizes =
{   -1,   -1,   -1,   -1, 0x12, 0x12, 0x12, 0x12,   -1 };

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
 *  8: 5760x2880
 */

/*   0     1     2     3     4     5     6     7     8 */

static const escp2_bits_t variable_bits =
{    2,    2,    2,    2,    2,    2,    2,    2,    2 };

static const escp2_bits_t stp950_bits =
{    2,    2,    2,    2,    2,    2,    1,    1,    1 };

static const escp2_bits_t ultrachrome_bits =
{    2,    2,    2,    2,    2,    1,    1,    1,    1 };

static const escp2_bits_t standard_bits =
{    1,    1,    1,    1,    1,    1,    1,    1,    1 };

static const escp2_bits_t c1_8_bits =
{    2,    2,    2,    2,    2,    1,    1,    1,    1 };

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
 *  8: 5760x2880
 */

/*   0     1     2     3     4     5     6     7     8 */

static const escp2_base_resolutions_t standard_base_res =
{  720,  720,  720,  720,  720,  720,  720,  720,  720 };

static const escp2_base_resolutions_t g3_base_res =
{  720,  720,  720,  720,  360,  360,  360,  360,  360 };

static const escp2_base_resolutions_t variable_base_res =
{  360,  360,  360,  360,  360,  360,  360,  360,  360 };

static const escp2_base_resolutions_t stp950_base_res =
{  360,  360,  360,  360,  360,  720,  720,  720,  720 };

static const escp2_base_resolutions_t ultrachrome_base_res =
{  360,  360,  360,  360,  360,  720,  720,  720,  720 };

static const escp2_base_resolutions_t c1_8_base_res =
{  360,  360,  720,  720,  720, 1440, 1440, 1440, 1440 };

static const escp2_base_resolutions_t c1_5_base_res =
{  360,  360,  720,  720,  720,  720,  720,  720,  720 };

static const escp2_base_resolutions_t claria_1400_base_res =
{  360,  360,  720,  720,  360,  720,  720,  720,  720 };

static const escp2_base_resolutions_t stc900_base_res =
{  360,  360,  360,  360,  180,  180,  360,  360,  360 };

static const escp2_base_resolutions_t pro_base_res =
{ 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 5760 };

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
 *  8: 5760x2880
 */

/*  0    1     2       3    4      5      6      7      8 */

static const escp2_densities_t g1_densities =
{ 2.6, 1.3,  1.3,  0.568, 0.0,   0.0,   0.0,   0.0,   0.0   };

static const escp2_densities_t g3_densities =
{ 2.6, 1.3,  0.65, 0.775, 0.388, 0.0,   0.0,   0.0,   0.0   };

static const escp2_densities_t c6pl_densities =
{ 4.0, 2.0,  1.0,  0.568, 0.568, 0.568, 0.0,   0.0,   0.0   };

static const escp2_densities_t c4pl_2880_densities =
{ 2.6, 1.3,  0.65, 0.650, 0.650, 0.650, 0.32,  0.0,   0.0   };

static const escp2_densities_t c4pl_densities =
{ 2.6, 1.3,  0.65, 0.568, 0.523, 0.792, 0.396, 0.0,   0.0   };

static const escp2_densities_t c4pl_pigment_densities =
{ 2.3, 1.15, 0.58, 0.766, 0.388, 0.958, 0.479, 0.0,   0.0   };

static const escp2_densities_t c3pl_pigment_densities =
{ 2.4, 1.2,  0.60, 0.600, 0.512, 0.512, 0.512, 0.0,   0.0   };

static const escp2_densities_t c3pl_pigment_c66_densities =
{ 2.8, 1.4,  0.70, 0.600, 0.512, 0.512, 0.512, 0.0,   0.0   };

static const escp2_densities_t c3pl_densities =
{ 2.6, 1.3,  0.65, 0.730, 0.7,   0.91,  0.455, 0.0,   0.0   };

static const escp2_densities_t p3pl_densities =
{ 4.0, 2.0,  1.00, 0.679, 0.657, 0.684, 0.566, 0.283, 0.0   };

static const escp2_densities_t p1_5pl_densities =
{ 2.8, 1.4,  1.00, 1.000, 0.869, 0.942, 0.471, 0.500, 0.530 };

static const escp2_densities_t claria_densities =
{ 2.8, 1.4,  2.00, 1.000, 0.500, 0.812, 0.406, 0.546, 0.440 };

static const escp2_densities_t claria_1400_densities =
{ 2.8, 1.4,  2.00, 1.000, 0.500, 0.812, 0.406, 0.546, 0.440 };

static const escp2_densities_t p3_5pl_densities =
{ 2.8, 1.4,  1.77, 0.886, 0.443, 0.221, 0.240, 0.293, 0.146 };

static const escp2_densities_t c2pl_densities =
{ 2.0, 1.0,  0.5,  0.650, 0.650, 0.0,   0.650, 0.325, 0.0   };

static const escp2_densities_t c1_8pl_densities =
{ 2.3, 1.15, 0.57, 0.650, 0.650, 0.0,   0.650, 0.360, 0.0   };

static const escp2_densities_t sc1500_densities =
{ 2.6, 1.3,  1.3,  0.631, 0.0,   0.0,   0.0,   0.0,   0.0   };

static const escp2_densities_t sc440_densities =
{ 4.0, 2.0,  1.0,  0.900, 0.45,  0.0,   0.0,   0.0,   0.0   };

static const escp2_densities_t sc480_densities =
{ 2.8, 1.4,  0.7,  0.710, 0.710, 0.546, 0.0,   0.0,   0.0   };

static const escp2_densities_t sc660_densities =
{ 4.0, 2.0,  1.0,  0.646, 0.323, 0.0,   0.0,   0.0,   0.0   };

static const escp2_densities_t sc980_densities =
{ 2.6, 1.3,  0.65, 0.511, 0.49,  0.637, 0.455, 0.0,   0.0   };

static const escp2_densities_t sp700_densities =
{ 2.6, 1.3,  1.3,  0.775, 0.55,  0.0,   0.0,   0.0,   0.0   };

static const escp2_densities_t sp2000_densities =
{ 2.6, 1.3,  0.65, 0.852, 0.438, 0.219, 0.0,   0.0,   0.0   };

static const escp2_densities_t spro_dye_densities =
{ 2.6, 1.3,  1.3,  0.775, 0.388, 0.275, 0.0,   0.0,   0.0   };

static const escp2_densities_t spro_pigment_densities =
{ 3.0, 1.5,  0.78, 0.775, 0.388, 0.194, 0.0,   0.0,   0.0   };

static const escp2_densities_t spro10000_densities =
{ 2.6, 1.3,  0.65, 0.431, 0.216, 0.392, 0.0,   0.0,   0.0   };

static const escp2_densities_t picturemate_densities =
{   0,   0,     0,     0, 1.596, 0.798, 0.650, 0.530, 0.0   };


static const stp_raw_t new_init_sequence = STP_RAW_STRING("\0\0\0\033\001@EJL 1284.4\n@EJL     \n\033@");

static const stp_raw_t je_deinit_sequence = STP_RAW_STRING("JE\001\000\000");

/* These sequences provided by Epson.  No, I don't know what
   most of them mean. */

static const stp_raw_t bsc64_borderless_sequence = STP_RAW_STRING("SN\114\000\000\011\026\000\000\000\000\000\000\000\003\000\000\000\260\004\352\004\064\001\016\002\000\000\000\000\064\010\150\020\030\025\310\031\340\075\314\020\214\012\024\005\214\000\012\001\054\001\000\000\017\017\017\017\017\017\017\017\004\012\004\017\017\017\017\017\006\004\000\001\001\001\000\000\020\010");

static const stp_raw_t bsc66_borderless_sequence = STP_RAW_STRING("SN\114\000\000\011\026\000\000\000\000\000\000\000\003\000\000\000\260\004\352\004\064\001\016\002\000\000\000\000\064\010\150\020\030\025\310\031\340\075\314\020\214\012\024\005\214\000\012\001\054\001\000\000\017\017\017\017\017\017\017\017\004\012\004\017\017\017\017\017\006\004\000\001\001\001\000\000\020\010");

static const stp_raw_t bsc68_borderless_sequence = STP_RAW_STRING("SN\114\000\000\011\026\000\000\000\000\000\000\000\003\000\000\000\260\004\352\004\064\001\016\002\000\000\000\000\064\010\150\020\030\025\310\031\340\075\314\020\214\012\024\005\214\000\012\001\054\001\000\000\017\017\017\017\017\017\017\017\004\012\004\017\017\017\017\017\006\004\000\001\001\001\000\000\020\010");

static const stp_raw_t bsc82_borderless_sequence = STP_RAW_STRING("SN\062\000\000\006\013\000\000\000\000\000\000\000\001\002\026\003\276\000\064\007\000\000\154\007\352\011\352\011\226\000\000\000\226\000\064\007\023\020\025\031\001\021\004\021\021\021\001\001\000\000\174\005");

static const stp_raw_t bsc84_borderless_sequence = STP_RAW_STRING("SN\114\000\000\011\027\000\000\000\000\000\000\000\003\000\000\001\260\004\336\004\064\001\000\002\000\000\000\000\064\010\150\020\030\025\310\031\340\075\314\020\214\012\024\005\214\000\012\001\054\001\000\000\017\017\017\017\017\017\017\017\004\012\004\017\017\017\017\017\006\004\000\001\001\001\000\000\370\007");

static const stp_raw_t bsc86_borderless_sequence = STP_RAW_STRING("SN\114\000\000\011\027\000\000\000\000\000\000\000\003\000\000\001\260\004\336\004\064\001\000\002\000\000\000\000\064\010\150\020\030\025\310\031\340\075\314\020\214\012\024\005\214\000\012\001\054\001\000\000\017\017\017\017\017\017\017\017\004\012\004\017\017\017\017\017\006\004\000\001\001\001\000\000\370\007");

static const stp_raw_t bsc88_borderless_sequence = STP_RAW_STRING("SN\114\000\000\011\027\000\000\000\000\000\000\000\003\000\000\001\260\004\336\004\064\001\000\002\000\000\000\000\064\010\150\020\030\025\310\031\340\075\314\020\214\012\024\005\214\000\012\001\054\001\000\000\017\017\017\017\017\017\017\017\004\012\004\017\017\017\017\017\006\004\000\001\001\001\000\000\370\007");

static const stp_raw_t cx6400_borderless_sequence = STP_RAW_STRING("SN\062\000\000\006\026\000\000\000\000\000\000\001\000\000\027\003\276\000\077\007\000\000\334\005\366\011\366\011\226\000\000\000\226\000\077\007\031\030\031\031\004\031\004\031\031\031\004\004\000\000\135\006");

static const stp_raw_t cx6600_borderless_sequence = STP_RAW_STRING("SN\062\000\000\006\026\000\000\000\000\000\000\001\000\000\027\003\276\000\077\007\000\000\334\005\366\011\366\011\226\000\000\000\226\000\077\007\031\030\031\031\004\031\004\031\031\031\004\004\000\000\135\006");

static const stp_raw_t pm830c_borderless_sequence = STP_RAW_STRING("SN\054\000\000\001\027\000\000\000\000\000\000\001\003\000\243\156\000\223\170\220\065\002\000\000\005\277\001\270\006\144\000\164\016\032\004\042\005\310\031\000\100\000\022\143\102\007");

static const stp_raw_t pm930c_borderless_sequence = STP_RAW_STRING("SN\070\000\000\007\027\000\000\000\000\000\000\001\003\000\330\006\124\001\264\015\042\013\110\007\060\011\316\022\054\001\251\013\054\001\002\003\363\027\031\030\031\031\031\031\004\031\031\031\001\004\103\000\000\001\001\001\360\006");

static const stp_raw_t pm970c_borderless_sequence = STP_RAW_STRING("SN\070\000\000\007\033\000\000\000\000\000\000\001\003\000\330\006\124\001\264\015\054\013\110\007\060\011\316\022\054\001\251\013\054\001\002\003\363\027\031\030\031\031\031\031\004\031\031\031\001\004\103\000\000\001\001\001\364\006");

static const stp_raw_t sp1280_borderless_sequence = STP_RAW_STRING("SN\003\000\000\011\001");

static const stp_raw_t sp780_borderless_sequence = STP_RAW_STRING("SN\003\000\000\000\002SN\003\000\000\001\001SN\003\000\000\011\001");

static const stp_raw_t sp820_borderless_sequence = STP_RAW_STRING("SN\003\000\000\011\001");

static const stp_raw_t sp820u_borderless_sequence = STP_RAW_STRING("SN\003\000\000\011\001");

static const stp_raw_t sp825_borderless_sequence = STP_RAW_STRING("SN\003\000\000\011\001");

static const stp_raw_t sp890_borderless_sequence = STP_RAW_STRING("SN\003\000\000\000\010SN\003\000\000\001\001SN\003\000\000\002\000SN\003\000\000\007\000SN\003\000\000\011\001");

static const stp_raw_t sp900_borderless_sequence = STP_RAW_STRING("SN\064\000\000\003\026\000\000\000\000\000\000\001\003\000\007\144\050\002\152\215\000\063\344\000\040\120\000\207\150\020\212\003\070\002\360\001\324\100\000\001\010\001\142\141\141\140\141\024\002\025\027\061\132\011");

static const stp_raw_t sp925_borderless_sequence = STP_RAW_STRING("SN\054\000\000\001\027\000\000\000\000\000\000\001\003\000\243\156\000\223\170\220\065\002\000\000\005\277\001\270\006\144\000\024\036\032\004\042\005\310\031\000\100\000\022\143\362\006");

static const stp_raw_t sp960_borderless_sequence = STP_RAW_STRING("SN\064\000\000\002\027\000\000\000\000\000\000\001\003\000\204\003\252\000\204\006\270\004\364\006\166\005\230\011\226\000\304\004\226\000\206\002\270\012\031\030\031\031\031\031\004\064\064\064\004\001\000\000\040\011");

static const stp_raw_t spr300_borderless_sequence = STP_RAW_STRING("SN\120\000\000\014\027\000\000\000\000\000\000\000\003\000\001\001\130\002\320\004\107\001\107\002\000\000\000\000\120\010\204\020\030\025\310\031\340\075\240\017\214\012\060\005\214\000\012\001\054\001\000\000\000\000\017\017\017\017\017\017\017\017\004\012\004\017\017\017\017\017\006\004\017\017\000\000\001\001\000\001\060\010");

static const stp_raw_t spr320_borderless_sequence = STP_RAW_STRING("SN\120\000\000\014\027\000\000\000\000\000\000\000\003\000\001\001\130\002\320\004\107\001\107\002\000\000\000\000\120\010\204\020\030\025\310\031\340\075\240\017\214\012\060\005\214\000\012\001\054\001\000\000\000\000\017\017\017\017\017\017\017\017\004\012\004\017\017\017\017\017\006\004\017\017\000\000\001\001\000\001\060\010");

static const stp_raw_t spr800_borderless_sequence = STP_RAW_STRING("SN\124\000\000\012\033\000\000\000\000\000\000\001\003\000\001\001\235\007\124\001\120\012\252\000\363\006\077\002\120\012\277\007\050\002\045\013\054\001\253\000\037\001\041\000\040\001\322\000\241\000\000\000\017\000\036\000\030\031\031\031\031\031\031\031\003\033\033\143\143\143\143\143\143\143\143\143\143\143\002\000\001\000\001\001\362\014");

static const stp_raw_t sprx500_borderless_sequence = STP_RAW_STRING("SN\114\000\000\011\026\000\000\000\000\000\000\000\003\000\000\001\260\004\336\004\064\001\000\002\000\000\000\000\064\010\150\020\030\025\310\031\340\075\314\020\214\012\024\005\214\000\012\001\054\001\000\000\017\017\017\017\017\017\017\017\004\012\004\017\017\017\017\017\006\004\000\001\001\001\000\000\367\007");

static const stp_raw_t sprx600_borderless_sequence = STP_RAW_STRING("SN\114\000\000\011\026\000\000\000\000\000\000\000\003\000\000\001\260\004\336\004\064\001\000\002\000\000\000\000\064\010\150\020\030\025\310\031\340\075\314\020\214\012\024\005\214\000\012\001\054\001\000\000\017\017\017\017\017\017\017\017\004\012\004\017\017\017\017\017\006\004\000\001\001\001\000\000\367\007");

static const stp_raw_t sprx620_borderless_sequence = STP_RAW_STRING("SN\114\000\000\011\026\000\000\000\000\000\000\000\003\000\000\001\260\004\336\004\064\001\000\002\000\000\000\000\064\010\150\020\030\025\310\031\340\075\314\020\214\012\024\005\214\000\012\001\054\001\000\000\017\017\017\017\017\017\017\017\004\012\004\017\017\017\017\017\006\004\000\001\001\001\000\000\367\007");

#define INCH(x)		(72 * x)

const stpi_escp2_printer_t stpi_escp2_model_capabilities[] =
{
  /* FIRST GENERATION PRINTERS */
  /* 0: Stylus Color */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    15, 1, 4, 15, 1, 4, 15, 1, 4, 4,
    360, 14400, -1, 720, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    g1_dotsizes, g1_densities, "simple",
    "720dpi", "standard",
    standard_bits, standard_base_res, "default",
    "standard", NULL, NULL,
    NULL, NULL, "standard"
  },
  /* 1: Stylus Color 400/500 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 4,
    360, 14400, -1, 720, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    g2_dotsizes, g1_densities, "simple",
    "sc500", "standard",
    standard_bits, standard_base_res, "default",
    "standard", NULL, NULL,
    NULL, NULL, "standard"
  },
  /* 2: Stylus Color 1500 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 4,
    360, 14400, -1, 720, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17), INCH(44), INCH(2), INCH(2),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    g1_dotsizes, sc1500_densities, "simple",
    "sc500", "cmy",
    standard_bits, standard_base_res, "standard_roll_feed",
    "standard", NULL, NULL,
    NULL, NULL, "standard"
  },
  /* 3: Stylus Color 600 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 4,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 8, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    8, 9, 0, 30, 8, 9, 0, 30, 8, 9, 0, 0, 8, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    sc600_dotsizes, g3_densities, "simple",
    "g3", "standard",
    standard_bits, g3_base_res, "default",
    "standard", NULL, NULL,
    NULL, NULL, "standard"
  },
  /* 4: Stylus Color 800 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    64, 1, 2, 64, 1, 2, 64, 1, 2, 4,
    360, 14400, -1, 1440, 720, 180, 180,
    0, 1, 4, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    8, 9, 9, 40, 8, 9, 9, 40, 8, 9, 0, 0, 8, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    g3_dotsizes, g3_densities, "simple",
    "g3", "standard",
    standard_bits, g3_base_res, "default",
    "standard", NULL, NULL,
    NULL, NULL, "standard"
  },
  /* 5: Stylus Color 850 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    64, 1, 2, 64, 1, 2, 64, 1, 2, 4,
    360, 14400, -1, 1440, 720, 180, 180,
    0, 1, 4, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    g3_dotsizes, g3_densities, "simple",
    "g3", "standard",
    standard_bits, g3_base_res, "default",
    "standard", NULL, NULL,
    NULL, NULL, "standard"
  },
  /* 6: Stylus Color 1520 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    64, 1, 2, 64, 1, 2, 64, 1, 2, 4,
    360, 14400, -1, 1440, 720, 180, 180,
    0, 1, 4, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17), INCH(44), INCH(2), INCH(2),
    8, 9, 9, 40, 8, 9, 9, 40, 8, 9, 0, 0, 8, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    g3_dotsizes, g3_densities, "simple",
    "g3", "standard",
    standard_bits, g3_base_res, "standard_roll_feed",
    "standard", NULL, NULL,
    NULL, NULL, "standard"
  },

  /* SECOND GENERATION PRINTERS */
  /* 7: Stylus Photo 700 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 6,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 8, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 30, 9, 9, 0, 30, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 15, 0, 0,		/* Is it really 15 pairs??? */
    sp700_dotsizes, sp700_densities, "simple",
    "g3", "photo_gen1",
    standard_bits, g3_base_res, "default",
    "standard", NULL, NULL,
    NULL, NULL, "photo"
  },
  /* 8: Stylus Photo EX */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_NO | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 6,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 8, 1, 28800, 720 * 720,
    INCH(118 / 10), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 30, 9, 9, 0, 30, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    sp700_dotsizes, sp700_densities, "simple",
    "g3", "photo_gen1",
    standard_bits, g3_base_res, "default",
    "standard", NULL, NULL,
    NULL, NULL, "photo"
  },
  /* 9: Stylus Photo */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 6,
    360, 14400, -1, 720, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 8, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 30, 9, 9, 0, 30, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    sp700_dotsizes, sp700_densities, "simple",
    "g3_720dpi", "photo_gen1",
    standard_bits, g3_base_res, "default",
    "standard", NULL, NULL,
    NULL, NULL, "photo"
  },

  /* THIRD GENERATION PRINTERS */
  /* 10: Stylus Color 440/460 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    21, 1, 4, 21, 1, 4, 21, 1, 4, 4,
    360, 14400, -1, 720, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 8, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 15, 0, 0,
    sc440_dotsizes, sc440_densities, "simple",
    "g3_720dpi", "standard",
    standard_bits, standard_base_res, "default",
    "standard", NULL, NULL,
    NULL, NULL, "standard"
  },
  /* 11: Stylus Color 640 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 4,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 8, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 15, 0, 0,
    sc640_dotsizes, sc440_densities, "simple",
    "sc640", "standard",
    standard_bits, standard_base_res, "default",
    "standard", NULL, NULL,
    NULL, NULL, "standard"
  },
  /* 12: Stylus Color 740/Stylus Scan 2000/Stylus Scan 2500 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 4,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    c6pl_dotsizes, c6pl_densities, "variable_6pl",
    "1440dpi", "standard",
    variable_bits, variable_base_res, "default",
    "standard", NULL, NULL,
    NULL, NULL, "standard"
  },
  /* 13: Stylus Color 900 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    96, 1, 2, 192, 1, 1, 192, 1, 1, 4,
    360, 14400, -1, 1440, 720, 180, 180,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    c3pl_dotsizes, c3pl_densities, "variable_3pl",
    "1440dpi", "standard",
    variable_bits, stc900_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 14: Stylus Photo 750 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    c6pl_dotsizes, c6pl_densities, "variable_6pl",
    "1440dpi", "photo_gen1",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "photo"
  },
  /* 15: Stylus Photo 1200 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(13), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    c6pl_dotsizes, c6pl_densities, "variable_6pl",
    "1440dpi", "photo_gen1",
    variable_bits, variable_base_res, "standard_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "photo"
  },
  /* 16: Stylus Color 860 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 4,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_densities, "variable_1440_4pl",
    "1440dpi", "standard",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 17: Stylus Color 1160 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 4,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(13), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_densities, "variable_1440_4pl",
    "1440dpi", "standard",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 18: Stylus Color 660 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 4,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 8, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 9, 9, 9, 9, 9, 26, 9, 9, 9, 0, 9, 9, 9, 0, -1, -1, 0, 0, 0,
    1, 15, 0, 0,
    sc660_dotsizes, sc660_densities, "simple",
    "sc640", "standard",
    standard_bits, standard_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 19: Stylus Color 760 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 4,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_densities, "variable_1440_4pl",
    "1440dpi", "standard",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 20: Stylus Photo 720 (Australia) */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 6,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    sp720_dotsizes, c6pl_densities, "variable_6pl",
    "1440dpi", "photo_gen1",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "photo"
  },
  /* 21: Stylus Color 480 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_YES |
     MODEL_PACKET_MODE_YES),
    15, 15, 3, 48, 48, 3, 48, 48, 3, 4,
    360, 14400, 360, 720, 720, 90, 90,
    0, 1, 0, 0, 0, -99, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    sc480_dotsizes, sc480_densities, "variable_x80_6pl",
    "720dpi_soft", "x80",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 22: Stylus Photo 870/875 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_densities, "variable_1440_4pl",
    "1440dpi", "photo_gen2",
    variable_bits, variable_base_res, "standard_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "photo"
  },
  /* 23: Stylus Photo 1270 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(13), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_densities, "variable_1440_4pl",
    "1440dpi", "photo_gen2",
    variable_bits, variable_base_res, "standard_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "photo"
  },
  /* 24: Stylus Color 3000 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    64, 1, 2, 64, 1, 2, 64, 1, 2, 4,
    360, 14400, -1, 1440, 720, 180, 180,
    0, 1, 4, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17), INCH(44), INCH(2), INCH(2),
    8, 9, 9, 40, 8, 9, 9, 40, 8, 9, 0, 0, 8, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    g3_dotsizes, g3_densities, "simple",
    "g3", "standard",
    standard_bits, g3_base_res, "standard_roll_feed",
    "standard", NULL, NULL,
    NULL, NULL, "standard"
  },
  /* 25: Stylus Color 670 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    32, 1, 4, 64, 1, 2, 64, 1, 2, 4,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    sc670_dotsizes, c6pl_densities, "variable_6pl",
    "1440dpi", "standard",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 26: Stylus Photo 2000P */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 6,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(13), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    2, 15, 0, 0,
    sp2000_dotsizes, sp2000_densities, "variable_2000p",
    "1440dpi", "photo_pigment",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "photo"
  },
  /* 27: Stylus Pro 5000 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    64, 1, 2, 64, 1, 2, 64, 1, 2, 6,
    360, 14400, -1, 1440, 720, 180, 180,
    0, 1, 0, 0, 0, 0, 0, 4, 1, 28800, 720 * 720,
    INCH(13), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 30, 9, 9, 0, 30, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    spro5000_dotsizes, sp700_densities, "simple",
    "1440dpi", "photo_gen1",
    standard_bits, g3_base_res, "spro5000",
    "standard", NULL, NULL,
    NULL, NULL, "photo"
  },
  /* 28: Stylus Pro 7000 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 6,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(24), INCH(1200), INCH(7), INCH(7),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 9, 9, 9, 9, 9, 9, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    spro_dye_dotsizes, spro_dye_densities, "simple",
    "pro", "photo_gen1",
    standard_bits, pro_base_res, "pro_roll_feed",
    "standard", NULL, NULL,
    NULL, "pro7000", "photo"
  },
  /* 29: Stylus Pro 7500 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_YES | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 6,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(24), INCH(1200), INCH(7), INCH(7),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 9, 9, 9, 9, 9, 9, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    spro_pigment_dotsizes, spro_pigment_densities, "simple",
    "pro", "photo_pigment",
    standard_bits, pro_base_res, "pro_roll_feed",
    "standard", NULL, NULL,
    NULL, "pro7500", "photo"
  },
  /* 30: Stylus Pro 9000 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 6,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(44), INCH(1200), INCH(7), INCH(7),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 9, 9, 9, 9, 9, 9, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    spro_dye_dotsizes, spro_dye_densities, "simple",
    "pro", "photo_gen1",
    standard_bits, pro_base_res, "pro_roll_feed",
    "standard", NULL, NULL,
    NULL, "pro7000", "photo"
  },
  /* 31: Stylus Pro 9500 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_YES | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 6,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(44), INCH(1200), INCH(7), INCH(7),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 9, 9, 9, 9, 9, 9, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    spro_pigment_dotsizes, spro_pigment_densities, "simple",
    "pro", "photo_pigment",
    standard_bits, pro_base_res, "pro_roll_feed",
    "standard", NULL, NULL,
    NULL, "pro7500", "photo"
  },
  /* 32: Stylus Color 777/680 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 4,
    360, 14400, -1, 2880, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, "variable_2880_4pl",
    "2880dpi", "standard",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 33: Stylus Color 880/83/C60 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 4,
    360, 14400, -1, 2880, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, "variable_2880_4pl",
    "2880dpi", "standard",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 34: Stylus Color 980 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    96, 1, 2, 192, 1, 1, 192, 1, 1, 4,
    360, 14400, -1, 2880, 720, 180, 180,
    38, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    c3pl_dotsizes, sc980_densities, "variable_3pl",
    "2880dpi", "standard",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 35: Stylus Photo 780/790 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 2880, 720, 90, 90,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 24,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, "variable_2880_4pl",
    "2880dpi", "photo_gen2",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &sp780_borderless_sequence, NULL, "photo"
  },
  /* 36: Stylus Photo 785/890/895/915/935 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 2880, 720, 90, 90,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 24,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, "variable_2880_4pl",
    "2880dpi", "photo_gen2",
    variable_bits, variable_base_res, "standard_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &sp890_borderless_sequence, NULL, "photo"
  },
  /* 37: Stylus Photo 1280/1290 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 2880, 720, 90, 90,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(13), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 24,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, "variable_2880_4pl",
    "2880dpi", "photo_gen2",
    variable_bits, variable_base_res, "standard_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &sp1280_borderless_sequence, NULL, "photo"
  },
  /* 38: Stylus Color 580 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_YES |
     MODEL_PACKET_MODE_YES),
    15, 15, 3, 48, 48, 3, 48, 48, 3, 4,
    360, 14400, 360, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, -99, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    sc480_dotsizes, sc480_densities, "variable_x80_6pl",
    "1440dpi", "x80",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 39: Stylus Color Pro XL */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    16, 1, 4, 16, 1, 4, 16, 1, 4, 4,
    360, 14400, -1, 720, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(13), INCH(1200), INCH(2), INCH(2),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    g1_dotsizes, g1_densities, "simple",
    "720dpi", "standard",
    standard_bits, standard_base_res, "default",
    "standard", NULL, NULL,
    NULL, NULL, "standard"
  },
  /* 40: Stylus Pro 5500 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_YES | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 6,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(13), INCH(1200), INCH(2), INCH(2),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    spro_pigment_dotsizes, spro_pigment_densities, "simple",
    "pro", "photo_pigment",
    standard_bits, pro_base_res, "spro5000",
    "standard", NULL, NULL,
    NULL, "pro7500", "photo"
  },
  /* 41: Stylus Pro 10000 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_YES | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 6,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(44), INCH(1200), INCH(7), INCH(7),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 9, 9, 9, 9, 9, 9, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    spro10000_dotsizes, spro10000_densities, "spro10000",
    "pro", "photo_gen2",
    variable_bits, pro_base_res, "pro_roll_feed",
    "standard", NULL, NULL,
    NULL, "pro7000", "photo"
  },
  /* 42: Stylus C20SX/C20UX */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_YES |
     MODEL_PACKET_MODE_YES),
    15, 15, 3, 48, 48, 3, 48, 48, 3, 4,
    360, 14400, -1, 720, 720, 90, 90,
    0, 1, 0, 0, 0, -99, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    sc480_dotsizes, sc480_densities, "variable_x80_6pl",
    "720dpi_soft", "x80",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 43: Stylus C40SX/C40UX/C41SX/C41UX/C42SX/C42UX */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_YES |
     MODEL_PACKET_MODE_YES),
    15, 15, 3, 48, 48, 3, 48, 48, 3, 4,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, -99, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    sc480_dotsizes, sc480_densities, "variable_x80_6pl",
    "1440dpi", "x80",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 44: Stylus C70/C80 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    60, 60, 2, 180, 180, 2, 180, 180, 2, 4,
    360, 14400, -1, 2880, 1440, 360, 180,
    0, 1, 0, 0, 0, -240, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    4, 15, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_densities, "variable_3pl_pigment",
    "2880_1440dpi", "c80",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 45: Stylus Color Pro */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_NO),
    16, 1, 4, 16, 1, 4, 16, 1, 4, 4,
    360, 14400, -1, 720, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(44), INCH(2), INCH(2),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    1, 7, 0, 0,
    g1_dotsizes, g1_densities, "simple",
    "720dpi", "standard",
    standard_bits, standard_base_res, "default",
    "standard", NULL, NULL,
    NULL, NULL, "standard"
  },
  /* 46: Stylus Photo 950/960 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_YES |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    96, 96, 2, 96, 96, 2, 24, 24, 1, 6,
    360, 14400, -1, 2880, 1440, 360, 180,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 0, 0, 24,
    4, 15, 0, 0,
    c2pl_dotsizes, c2pl_densities, "variable_2pl",
    "superfine", "f360_photo",
    stp950_bits, stp950_base_res, "cd_cutter_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &sp960_borderless_sequence, NULL, "sp960"
  },
  /* 47: Stylus Photo 2100/2200 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_YES |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    96, 96, 2, 96, 96, 2, 192, 192, 1, 7,
    360, 14400, -1, 2880, 1440, 360, 180,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(13), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 0, 0, 0,
    4, 15, 0, 0,
    c4pl_pigment_dotsizes, c4pl_pigment_densities, "variable_ultrachrome",
    "superfine", "f360_ultrachrome",
    ultrachrome_bits, ultrachrome_base_res, "cd_cutter_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "sp2200"
  },
  /* 48: Stylus Pro 7600 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_PRO | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_YES | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 7,
    360, 14400, -1, 2880, 1440, 360, 180,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(24), INCH(1200), INCH(7), INCH(7),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    spro_c4pl_pigment_dotsizes, c4pl_pigment_densities, "variable_ultrachrome",
    "pro", "ultrachrome",
    ultrachrome_bits, pro_base_res, "pro_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, "pro7600", "photo"
  },
  /* 49: Stylus Pro 9600 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_PRO | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_YES | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    1, 1, 1, 1, 1, 1, 1, 1, 1, 7,
    360, 14400, -1, 2880, 1440, 360, 180,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(44), INCH(1200), INCH(7), INCH(7),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    spro_c4pl_pigment_dotsizes, c4pl_pigment_densities, "variable_ultrachrome",
    "pro", "ultrachrome",
    ultrachrome_bits, pro_base_res, "pro_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, "pro7600", "photo"
  },
  /* 50: Stylus Photo 825/830 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 2880, 1440, 90, 90,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 24,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, "variable_2880_4pl",
    "2880_1440dpi", "photo_gen2",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &sp1280_borderless_sequence, NULL, "photo"
  },
  /* 51: Stylus Photo 925 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 2880, 1440, 90, 90,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 24,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, "variable_2880_4pl",
    "2880_1440dpi", "photo_gen2",
    variable_bits, variable_base_res, "cutter_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &sp925_borderless_sequence, NULL, "photo"
  },
  /* 52: Stylus Color C62 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 144, 1, 1, 144, 1, 1, 4,
    360, 14400, -1, 2880, 1440, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, "variable_2880_4pl",
    "2880_1440dpi", "standard",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 53: Japanese PM-950C */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_YES |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    96, 96, 2, 96, 96, 2, 24, 24, 1, 6,
    360, 14400, -1, 2880, 1440, 360, 180,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 0, 0, 24,
    4, 15, 0, 0,
    c2pl_dotsizes, c2pl_densities, "variable_2pl",
    "superfine", "f360_photo7_japan",
    stp950_bits, stp950_base_res, "cd_cutter_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &sp960_borderless_sequence, NULL, "pm_950c"
  },
  /* 54: Stylus Photo EX3 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    32, 1, 4, 32, 1, 4, 32, 1, 4, 6,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(13), INCH(44), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    sp720_dotsizes, c6pl_densities, "variable_6pl",
    "1440dpi", "photo_gen1",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "photo"
  },
  /* 55: Stylus C82/CX-5200 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    59, 60, 2, 180, 180, 2, 180, 180, 2, 4,
    360, 14400, -1, 2880, 1440, 360, 180,
    0, 1, 0, 0, 0, -240, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    4, 15, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_densities, "variable_3pl_pigment",
    "2880_1440dpi", "c82",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 56: Stylus C50 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    15, 15, 3, 48, 48, 3, 48, 48, 3, 4,
    360, 14400, -1, 1440, 720, 90, 90,
    0, 1, 0, 0, 0, -99, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_densities, "variable_x80_6pl",
    "1440dpi", "x80",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 57: Japanese PM-970C */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_YES |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    180, 180, 2, 360, 360, 1, 360, 360, 1, 7,
    360, 14400, -1, 2880, 2880, 720, 360,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 24,
    4, 15, 0, 0,
    c1_8pl_dotsizes, c1_8pl_densities, "variable_2pl",
    "superfine", "f360_photo7_japan",
    c1_8_bits, c1_8_base_res, "cutter_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &pm970c_borderless_sequence, NULL, "pm_950c"
  },
  /* 58: Japanese PM-930C */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    90, 90, 2, 90, 90, 2, 90, 90, 2, 6,
    360, 14400, -1, 2880, 2880, 720, 360,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 24,
    4, 15, 0, 0,
    c1_8pl_dotsizes, c1_8pl_densities, "variable_2pl",
    "superfine", "photo_gen2",
    c1_8_bits, c1_8_base_res, "cutter_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &pm930c_borderless_sequence, NULL, "photo"
  },
  /* 59: Stylus C43SX/C43UX/C44SX/C44UX (WRONG -- see 43!) */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_NO | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_YES |
     MODEL_PACKET_MODE_YES),
    15, 15, 3, 48, 48, 3, 48, 48, 3, 4,
    360, 14400, -1, 2880, 720, 90, 90,
    0, 1, 0, 0, 0, -99, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_densities, "variable_x80_6pl",
    "1440dpi", "x80",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 60: Stylus C84 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    59, 60, 2, 180, 180, 2, 180, 180, 2, 4,
    360, 14400, -1, 2880, 1440, 360, 180,
    0, 1, 0, 80, 42, -240, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 24,
    4, 15, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_densities, "variable_3pl_pigment",
    "2880_1440dpi", "c82",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &bsc84_borderless_sequence, NULL, "standard"
  },
  /* 61: Stylus Color C63/C64 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    29, 30, 3, 90, 90, 3, 90, 90, 3, 4,
    360, 14400, -1, 2880, 1440, 360, 120,
    0, 1, 0, 80, 42, -180, 0, 0, 1, 28800, 1440 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 24,
    4, 15, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_densities, "variable_3pl_pigment",
    "2880_1440dpi", "c64",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &bsc64_borderless_sequence, NULL, "standard"
  },
  /* 62: Stylus Photo 900 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 2880, 720, 90, 90,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, 399, 394, 595, 842, 24,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, "variable_2880_4pl",
    "2880dpi", "photo_gen2",
    variable_bits, variable_base_res, "cd_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &sp900_borderless_sequence, NULL, "photo"
  },
  /* 63: Stylus Photo R300 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_FULL | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    90, 1, 3, 90, 1, 3, 90, 1, 3, 6,
    360, 14400, -1, 2880, 1440, 360, 120,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 595, 842, 24,
    4, 15, 0, 0,
    p3pl_dotsizes, p3pl_densities, "variable_3pl_pmg",
    "superfine", "photo_gen3",
    variable_bits, variable_base_res, "cd_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &spr300_borderless_sequence, NULL, "photo"
  },
  /* 64: PM-G800/Stylus Photo R800 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_FULL | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    180, 1, 2, 180, 1, 2, 180, 1, 2, 8,
    360, 28800, -1, 5760, 2880, 360, 180,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 180, 5760 * 2880,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 11, 9, 9, 0, 11, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 595, 842, 24,
    4, 15, 0, 0,
    p1_5pl_dotsizes, p1_5pl_densities, "variable_1_5pl",
    "superfine", "cmykrb",
    variable_bits, c1_5_base_res, "r1800",
    "p1_5", &new_init_sequence, &je_deinit_sequence,
    &spr800_borderless_sequence, NULL, "r800"
  },
  /* 65: Stylus Photo CX4600 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    90, 1, 3, 90, 1, 3, 90, 1, 3, 4,
    360, 14400, -1, 5760, 1440, 360, 120,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 180, 1440 * 1440,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 595, 842, 0,
    4, 15, 0, 0,
    p3pl_dotsizes, p3pl_densities, "variable_3pl_pmg",
    "superfine", "cx3650",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "mfp2005"
  },
  /* 66: Stylus Color C65/C66 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    29, 30, 3, 90, 90, 3, 90, 90, 3, 4,
    360, 14400, -1, 2880, 1440, 360, 120,
    0, 1, 0, 80, 42, -180, 0, 0, 1, 28800, 1440 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 24,
    4, 15, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_c66_densities, "variable_3pl_pigment_c66",
    "2880_1440dpi", "c64",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &bsc66_borderless_sequence, NULL, "standard"
  },
  /* 67: Stylus Photo R1800 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_FULL | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    180, 1, 2, 180, 1, 2, 180, 1, 2, 8,
    360, 28800, -1, 5760, 2880, 360, 180,
    0, 1, 0, 96, 42, 0, 0, 0, 1, 180, 5760 * 2880,
    INCH(13), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 11, 9, 9, 0, 11, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 595, 842, 24,
    4, 15, 0, 0,
    p1_5pl_dotsizes, p1_5pl_densities, "variable_1_5pl",
    "superfine", "cmykrb",
    variable_bits, c1_5_base_res, "r1800",
    "p1_5", &new_init_sequence, &je_deinit_sequence,
    &spr800_borderless_sequence, NULL, "r800"
  },
  /* 68: PM-G820 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_FULL | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    180, 1, 2, 180, 1, 2, 180, 1, 2, 8,
    360, 14400, -1, 5760, 2880, 360, 180,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 180, 5760 * 2880,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 11, 9, 9, 0, 11, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 595, 842, 24,
    4, 15, 0, 0,
    p1_5pl_dotsizes, p1_5pl_densities, "variable_1_5pl",
    "superfine", "photo_gen3",
    variable_bits, c1_5_base_res, "cd_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &spr800_borderless_sequence, NULL, "r800"
  },
  /* 69: Stylus C86 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    59, 60, 2, 180, 180, 2, 180, 180, 2, 4,
    360, 14400, -1, 2880, 2880, 360, 180,
    0, 1, 0, 80, 42, -240, 0, 0, 1, 28800, 1440 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 24,
    4, 15, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_densities, "variable_3pl_pigment",
    "2880_1440dpi", "c82",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &bsc86_borderless_sequence, NULL, "standard"
  },
  /* 70: Stylus Photo RX700 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_FULL | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    180, 1, 2, 180, 1, 2, 180, 1, 2, 6,
    360, 28800, -1, 5760, 2880, 360, 180,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 204, 263, 595, 842, 0,
    4, 15, 0, 0,
    p1_5pl_dotsizes, p1_5pl_densities, "variable_1_5pl",
    "superfine", "photo_gen3",
    variable_bits, c1_5_base_res, "rx700",
    "p1_5", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "rx700"
  },
  /* 71: Stylus Photo R2400 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_FULL | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    180, 1, 2, 180, 1, 2, 180, 1, 2, 8,
    360, 14400, -1, 5760, 2880, 360, 180,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 180, 1440 * 1440,
    INCH(13), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 595, 842, 0,
    4, 15, 0, 0,
    p3_5pl_dotsizes, p3_5pl_densities, "variable_r2400",
    "superfine", "f360_ultrachrome_k3",
    variable_bits, c1_5_base_res, "r2400",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "r2400"
  },
  /* 72: Stylus CX3700/3800/3810 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    29, 30, 3, 90, 90, 3, 90, 90, 3, 4,
    360, 14400, -1, 2880, 1440, 360, 120,
    0, 1, 0, 80, 42, -180, 0, 0, 1, 28800, 1440 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    4, 15, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_c66_densities, "variable_3pl_pigment_c66",
    "2880_1440dpi", "c64",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "cx3800"
  },
  /* 73: E-100/PictureMate */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_FULL | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    90, 1, 3, 90, 1, 3, 90, 1, 3, 6,
    360, 28800, -1, 5760, 1440, 1440, 720,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(4), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 595, 842, 0,
    4, 15, 0, 0,
    picturemate_dotsizes, picturemate_densities, "variable_picturemate",
    "picturemate", "picturemate",
    variable_bits, c1_5_base_res, "default",
    "picturemate", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "picturemate"
  },
  /* 74: PM-A650 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    90, 90, 3, 90, 90, 3, 90, 90, 3, 4,
    360, 14400, -1, 5760, 1440, 360, 120,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 0,
    4, 15, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_c66_densities, "variable_3pl_pigment_c66",
    "superfine", "c64",
    variable_bits, variable_base_res, "cd_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 75: Japanese PM-A750 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_YES |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    90, 90, 3, 90, 90, 3, 90, 90, 3, 4,
    360, 14400, -1, 5760, 1440, 360, 120,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 0, 0, 0,
    4, 15, 0, 0,
    c2pl_dotsizes, c2pl_densities, "variable_2pl",
    "superfine", "c64",
    variable_bits, variable_base_res, "cd_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 76: Japanese PM-A890 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_YES |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    90, 90, 3, 90, 90, 3, 90, 90, 3, 6,
    360, 14400, -1, 5760, 1440, 360, 120,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 0, 0, 0,
    4, 15, 0, 0,
    c2pl_dotsizes, c2pl_densities, "variable_2pl",
    "superfine", "photo_gen3",
    variable_bits, variable_base_res, "cd_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "standard"
  },
  /* 77: Japanese PM-D600 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    90, 1, 3, 90, 1, 3, 90, 1, 3, 4,
    360, 14400, -1, 2880, 1440, 360, 120,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 595, 842, 0,
    4, 15, 0, 0,
    p3pl_dotsizes, p3pl_densities, "variable_3pl_pmg",
    "superfine", "c64",
    variable_bits, variable_base_res, "cd_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "photo"
  },
  /* 78: Stylus Photo 810/820 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    48, 1, 3, 48, 1, 3, 48, 1, 3, 6,
    360, 14400, -1, 2880, 720, 90, 90,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 24,
    3, 15, 0, 0,
    c4pl_dotsizes, c4pl_2880_densities, "variable_2880_4pl",
    "2880dpi", "photo_gen2",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &sp1280_borderless_sequence, NULL, "photo"
  },
  /* 79: Stylus CX6400 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    59, 60, 2, 180, 180, 2, 180, 180, 2, 4,
    360, 14400, -1, 2880, 1440, 360, 180,
    0, 1, 0, 80, 42, -240, 0, 0, 1, 28800, 720 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 24,
    4, 15, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_densities, "variable_3pl_pigment",
    "2880_1440dpi", "c82",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &cx6400_borderless_sequence, NULL, "standard"
  },
  /* 80: Stylus CX6600 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    59, 60, 2, 180, 180, 2, 180, 180, 2, 4,
    360, 14400, -1, 2880, 2880, 360, 180,
    0, 1, 0, 80, 42, -240, 0, 0, 1, 28800, 1440 * 720,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, -1, -1, 0, 0, 24,
    4, 15, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_densities, "variable_3pl_pigment",
    "2880_1440dpi", "c82",
    variable_bits, variable_base_res, "default",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &cx6600_borderless_sequence, NULL, "standard"
  },
  /* 81: Stylus Photo R260 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    90, 1, 4, 90, 1, 4, 90, 1, 4, 6,
    360, 14400, -1, 5760, 2880, 360, 90,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 5760 * 2880,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, 204, 189, 595, 842, 24,
    4, 15, 0, 0,
    claria_dotsizes, claria_densities, "variable_claria",
    "superfine", "claria",
    variable_bits, c1_5_base_res, "cd_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "photo"
  },
  /* 82: Stylus Photo 1400 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_YES | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    90, 1, 4, 90, 1, 4, 90, 1, 4, 6,
    360, 14400, -1, 5760, 2880, 360, 90,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 5760 * 2880,
    INCH(13), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, 204, 263, 595, 842, 24,
    4, 15, 0, 0,
    claria_1400_dotsizes, claria_1400_densities, "variable_claria_1400",
    "claria_1400", "claria",
    variable_bits, claria_1400_base_res, "cd_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    NULL, NULL, "photo"
  },
  /* 83: Stylus Photo R240 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_FULL | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    90, 1, 3, 90, 1, 3, 90, 1, 3, 4,
    360, 14400, -1, 5760, 1440, 360, 120,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 595, 842, 24,
    4, 15, 0, 0,
    p3pl_dotsizes, p3pl_densities, "variable_3pl_pmg",
    "superfine", "photo_gen3_4",
    variable_bits, variable_base_res, "cd_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &spr300_borderless_sequence, NULL, "standard"
  },
  /* 84: Stylus Photo RX500 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ZEROMARGIN_FULL | MODEL_VACUUM_NO | MODEL_FAST_360_NO |
     MODEL_SEND_ZERO_ADVANCE_YES | MODEL_SUPPORTS_INK_CHANGE_NO |
     MODEL_PACKET_MODE_YES),
    90, 1, 3, 90, 1, 3, 90, 1, 3, 6,
    360, 14400, -1, 2880, 1440, 360, 120,
    0, 1, 0, 80, 42, 0, 0, 0, 1, 28800, 1440 * 1440,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(2),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0, 204, 191, 595, 842, 24,
    4, 15, 0, 0,
    p3pl_dotsizes, p3pl_densities, "variable_3pl_pmg",
    "superfine", "photo_gen3",
    variable_bits, variable_base_res, "cd_roll_feed",
    "standard", &new_init_sequence, &je_deinit_sequence,
    &sprx500_borderless_sequence, NULL, "photo"
  },
};

const int stpi_escp2_model_limit =
sizeof(stpi_escp2_model_capabilities) / sizeof(stpi_escp2_printer_t);
