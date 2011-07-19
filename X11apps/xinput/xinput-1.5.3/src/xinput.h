/*
 * Copyright 1996 by Frederic Lepied, France. <Frederic.Lepied@sugix.frmug.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  the authors  not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     The authors  make  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHORS DISCLAIM ALL   WARRANTIES WITH REGARD  TO  THIS SOFTWARE,
 * INCLUDING ALL IMPLIED   WARRANTIES OF MERCHANTABILITY  AND   FITNESS, IN NO
 * EVENT  SHALL THE AUTHORS  BE   LIABLE   FOR ANY  SPECIAL, INDIRECT   OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER  IN  AN ACTION OF  CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING    OUT OF OR   IN  CONNECTION  WITH THE USE    OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#ifdef HAVE_XI2
#include <X11/extensions/XInput2.h>
#endif
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 1
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 0
#endif

extern int xi_opcode; /* xinput extension op code */
XDeviceInfo* find_device_info( Display *display, char *name, Bool only_extended);
#if HAVE_XI2
XIDeviceInfo* xi2_find_device_info(Display *display, char *name);
int xinput_version(Display* display);
#endif

int get_feedbacks( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int set_ptr_feedback( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int get_button_map( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int set_button_map( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int set_pointer( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);

int set_mode( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int list( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int test( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int version( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int set_integer_feedback( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int query_state( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);

/* X Input 1.5 */
int list_props( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int set_int_prop( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int set_float_prop( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int set_atom_prop( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int watch_props( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int delete_prop( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int set_prop( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);

/* X Input 2.0 */
int create_master( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int remove_master( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int change_attachment( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int float_device( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int set_clientpointer( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);
int test_xi2( Display* display, int argc, char *argv[], char *prog_name, char *prog_desc);

/* end of xinput.h */
