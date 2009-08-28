/*
 * Copyright 1996-1997 by Frederic Lepied, France. <Frederic.Lepied@sugix.frmug.org>
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

#include "xinput.h"

int
query_state(Display	*display,
	    int		argc,
	    char	*argv[],
	    char	*name,
	    char	*desc)
{
    XDeviceInfo		*info;
    XDevice		*device;
    XDeviceState	*state;
    int			loop;
    int			loop2;
    XInputClass		*cls;
    XValuatorState	*val_state;
    XKeyState		*key_state;
    XButtonState	*but_state;

    if (argc != 1) {
	fprintf(stderr, "usage: xinput %s %s\n", name, desc);
	return 1;
    }

    info = find_device_info(display, argv[0], True);

    if (!info) {
	fprintf(stderr, "unable to find device %s\n", argv[0]);
	return 1;
    }

    device = XOpenDevice(display, info->id);

    if (!device) {
	fprintf(stderr, "unable to open device %s\n", argv[0]);
	return 1;
    }

    state = XQueryDeviceState(display, device);

    if (state) {
        cls = state->data;
	printf("%d class%s :\n", state->num_classes,
	       (state->num_classes > 1) ? "es" : "");
	for(loop=0; loop<state->num_classes; loop++) {
	  switch(cls->class) {
	  case ValuatorClass:
	    val_state = (XValuatorState *) cls;
	    printf("ValuatorClass Mode=%s Proximity=%s\n",
		   val_state->mode & 1 ? "Absolute" : "Relative",
		   val_state->mode & 2 ? "Out" : "In");
	    for(loop2=0; loop2<val_state->num_valuators; loop2++) {
	      printf("\tvaluator[%d]=%d\n", loop2, val_state->valuators[loop2]);
	    }
	    break;

	  case ButtonClass:
	    but_state = (XButtonState *) cls;
	    printf("ButtonClass\n");
	    for(loop2=1; loop2<=but_state->num_buttons; loop2++) {
	      printf("\tbutton[%d]=%s\n", loop2,
		     (but_state->buttons[loop2 / 8] & (1 << (loop2 % 8))) ? "down" : "up" );
	    }
	    break;

	  case KeyClass:
	    key_state = (XKeyState *) cls;
	    printf("KeyClass\n");
	    for(loop2=0; loop2<key_state->num_keys; loop2++) {
	      printf("\tkey[%d]=%s\n", loop2,
		     (key_state->keys[loop2 / 8] & (1 << (loop2 % 8))) ? "down" : "up" );
	    }

	    break;

	  }
	  cls = (XInputClass *) ((char *) cls + cls->length);
	}
	XFreeDeviceState(state);
    }
    return EXIT_SUCCESS;
}

/* end of state.c */
