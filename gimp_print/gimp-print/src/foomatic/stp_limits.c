/*
 * "$Id: stp_limits.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
 *
 *   Dump the per-printer options for Grant Taylor's *-omatic database
 *
 *   Copyright 2000 Robert Krawitz (rlk@alum.mit.edu)
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
#include <stdio.h>
#ifdef INCLUDE_GIMP_PRINT_H
#include INCLUDE_GIMP_PRINT_H
#else
#include <gimp-print/gimp-print.h>
#endif
#include "../../lib/libprintut.h"

int
main(int argc, char **argv)
{
  const stp_vars_t minimum = stp_minimum_settings();
  const stp_vars_t maximum = stp_maximum_settings();
  const stp_vars_t defvars = stp_default_settings();

  printf("$stp_values{'MINVAL'}{'Brightness'} = %.3f\n",
	 stp_get_brightness(minimum));
  printf("$stp_values{'MAXVAL'}{'Brightness'} = %.3f\n",
	 stp_get_brightness(maximum));
  printf("$stp_values{'DEFVAL'}{'Brightness'} = %.3f\n",
	 stp_get_brightness(defvars));

  printf("$stp_values{'MINVAL'}{'Contrast'} = %.3f\n",
	 stp_get_contrast(minimum));
  printf("$stp_values{'MAXVAL'}{'Contrast'} = %.3f\n",
	 stp_get_contrast(maximum));
  printf("$stp_values{'DEFVAL'}{'Contrast'} = %.3f\n",
	 stp_get_contrast(defvars));

  printf("$stp_values{'MINVAL'}{'Density'} = %.3f\n",
	 stp_get_density(minimum));
  printf("$stp_values{'MAXVAL'}{'Density'} = %.3f\n",
	 stp_get_density(maximum));
  printf("$stp_values{'DEFVAL'}{'Density'} = %.3f\n",
	 stp_get_density(defvars));

  printf("$stp_values{'MINVAL'}{'Gamma'} = %.3f\n",
	 stp_get_gamma(minimum));
  printf("$stp_values{'MAXVAL'}{'Gamma'} = %.3f\n",
	 stp_get_gamma(maximum));
  printf("$stp_values{'DEFVAL'}{'Gamma'} = %.3f\n",
	 stp_get_gamma(defvars));

  printf("$stp_values{'MINVAL'}{'Cyan'} = %.3f\n",
	 stp_get_cyan(minimum));
  printf("$stp_values{'MAXVAL'}{'Cyan'} = %.3f\n",
	 stp_get_cyan(maximum));
  printf("$stp_values{'DEFVAL'}{'Cyan'} = %.3f\n",
	 stp_get_cyan(defvars));

  printf("$stp_values{'MINVAL'}{'Magenta'} = %.3f\n",
	 stp_get_magenta(minimum));
  printf("$stp_values{'MAXVAL'}{'Magenta'} = %.3f\n",
	 stp_get_magenta(maximum));
  printf("$stp_values{'DEFVAL'}{'Magenta'} = %.3f\n",
	 stp_get_magenta(defvars));

  printf("$stp_values{'MINVAL'}{'Yellow'} = %.3f\n",
	 stp_get_yellow(minimum));
  printf("$stp_values{'MAXVAL'}{'Yellow'} = %.3f\n",
	 stp_get_yellow(maximum));
  printf("$stp_values{'DEFVAL'}{'Yellow'} = %.3f\n",
	 stp_get_yellow(defvars));

  printf("$stp_values{'MINVAL'}{'Saturation'} = %.3f\n",
	 stp_get_saturation(minimum));
  printf("$stp_values{'MAXVAL'}{'Saturation'} = %.3f\n",
	 stp_get_saturation(maximum));
  printf("$stp_values{'DEFVAL'}{'Saturation'} = %.3f\n",
	 stp_get_saturation(defvars));

  return 0;
}
