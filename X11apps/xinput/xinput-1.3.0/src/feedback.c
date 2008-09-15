/*
 * Copyright 1996 by Frederic Lepied, France. <Frederic.Lepied@sugix.frmug.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  Frederic   Lepied not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     Frederic  Lepied   makes  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * FREDERIC  LEPIED DISCLAIMS ALL   WARRANTIES WITH REGARD  TO  THIS SOFTWARE,
 * INCLUDING ALL IMPLIED   WARRANTIES OF MERCHANTABILITY  AND   FITNESS, IN NO
 * EVENT  SHALL FREDERIC  LEPIED BE   LIABLE   FOR ANY  SPECIAL, INDIRECT   OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER  IN  AN ACTION OF  CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING    OUT OF OR   IN  CONNECTION  WITH THE USE    OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "xinput.h"

int
set_ptr_feedback(Display	*display,
		 int		argc,
		 char		*argv[],
		 char		*name,
		 char		*desc)
{
    XDeviceInfo		*info;
    XDevice		*device;
    XPtrFeedbackControl	feedback;
    XFeedbackState	*state;
    int			num_feedbacks;
    int			loop;
    int			id;

    if (argc != 4) {
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

    /* We will match the first Ptr Feedback class. Can there be more? */
    id = -1;
    state = XGetFeedbackControl(display, device, &num_feedbacks);
    for(loop=0; loop<num_feedbacks; loop++) {
	if (state->class == PtrFeedbackClass) {
	   id = state->id;
	}
	state = (XFeedbackState*) ((char*) state + state->length);
    }

    if (id == -1) {
       fprintf(stderr, "unable to find PtrFeedbackClass for %s\n", argv[0]);
       return 1;
    }

    feedback.class       = PtrFeedbackClass;
    feedback.length      = sizeof(XPtrFeedbackControl);
    feedback.id	         = id;
    feedback.threshold	 = atoi(argv[1]);
    feedback.accelNum	 = atoi(argv[2]);
    feedback.accelDenom  = atoi(argv[3]);

    XChangeFeedbackControl(display, device, DvAccelNum|DvAccelDenom|DvThreshold,
			   (XFeedbackControl*) &feedback);
    return EXIT_SUCCESS;
}


int
get_feedbacks(Display	*display,
	      int	argc,
	      char	*argv[],
	      char	*name,
	      char	*desc)
{
    XDeviceInfo		*info;
    XDevice		*device;
    XFeedbackState	*state;
    int			num_feedbacks;
    int			loop;
    XPtrFeedbackState	*p;
    XKbdFeedbackState	*k;
    XBellFeedbackState	*b;
    XLedFeedbackState	*l;
    XIntegerFeedbackState *i;
    XStringFeedbackState *s;

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

    state = XGetFeedbackControl(display, device, &num_feedbacks);

    printf("%d feedback class%s\n", num_feedbacks,
	   (num_feedbacks > 1) ? "es" : "");

    for(loop=0; loop<num_feedbacks; loop++) {
	switch (state->class) {
	case KbdFeedbackClass:
	    k = (XKbdFeedbackState*) state;
	    printf("KbdFeedbackClass id=%ld\n", state->id);
	    printf("\tclick is %d\n", k->click);
	    printf("\tpercent is %d\n", k->percent);
	    printf("\tpitch is %d\n", k->pitch);
	    printf("\tduration is %d\n", k->duration);
	    printf("\tled_mask is %d\n", k->led_mask);
	    printf("\tglobal_auto_repeat is %d\n", k->global_auto_repeat);
	    break;

	case PtrFeedbackClass:
	    p = (XPtrFeedbackState*) state;
	    printf("PtrFeedbackClass id=%ld\n", state->id);
	    printf("\taccelNum is %d\n", p->accelNum);
	    printf("\taccelDenom is %d\n", p->accelDenom);
	    printf("\tthreshold is %d\n", p->threshold);
	    break;

	case StringFeedbackClass:
	    s = (XStringFeedbackState*) state;
	    printf("XStringFeedbackControl id=%ld\n", state->id);
	    printf("\tmax_symbols is %d\n", s->max_symbols);
	    printf("\tnum_syms_supported is %d\n", s->num_syms_supported);
	    break;

	case IntegerFeedbackClass:
	    i = (XIntegerFeedbackState*) state;
	    printf("XIntegerFeedbackControl id=%ld\n", state->id);
	    printf("\tresolution is %d\n", i->resolution);
	    printf("\tminVal is %d\n", i->minVal);
	    printf("\tmaxVal is %d\n", i->maxVal);
	    break;

	case LedFeedbackClass:
	    l = (XLedFeedbackState*) state;
	    printf("XLedFeedbackState id=%ld\n", state->id);
	    printf("\tled_values is %d\n", l->led_values);
	    break;

	case BellFeedbackClass:
	    b = (XBellFeedbackState*) state;
	    printf("XBellFeedbackControl id=%ld\n", state->id);
	    printf("\tpercent is %d\n", b->percent);
	    printf("\tpitch is %d\n", b->pitch);
	    printf("\tduration is %d\n", b->duration);
	    break;
	}
	state = (XFeedbackState*) ((char*) state + state->length);
    }
    return EXIT_SUCCESS;
}

/* end of ptrfdbk.c */
