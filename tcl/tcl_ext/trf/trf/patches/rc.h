/* -*- c -*-
 *
 * rc.h - header containing a declaration of Tcl_ReplaceChannel
 * suitable for 'c2man'.
 *
 * Distributed at @mDate@.
 *
 * Copyright (c) 1999 Andreas Kupries (a.kupries@westend.com)
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL I BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS
 * SOFTWARE AND ITS DOCUMENTATION, EVEN IF I HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * I SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND
 * I HAVE NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
 * ENHANCEMENTS, OR MODIFICATIONS.
 *
 * CVS: $Id: rc.h,v 1.1 1999/06/29 19:29:03 aku Exp $
 */

#include <tcl.h>

/*
 * Exported tcl level procedures.
 *
 * ATTENTION:
 * due to the fact that cpp - processing with gcc 2.5.8 removes any comments
 * in macro-arguments (even if called with option '-C') i have to use the
 * predefined macro __C2MAN__ to distinguish real compilation and manpage
 * generation, removing _ANSI_ARGS_ in the latter case.
 */

/*
 * Replaces an entry for an existing channel.
 * Replaces an entry for an existing channel in both the global list
 * of channels and the hashtable of channels for the given
 * interpreter. The replacement is a new channel with same name, it
 * supercedes the replaced channel. From now on both input and output
 * of the superceded channel will go through the newly created
 * channel, thus allowing the arbitrary filtering/manipulation of the
 * data. It is the responsibility of the newly created channel to
 * forward the filtered/manipulated data to the channel he supercedes
 * at his leisure. The result of the command is the token for the new
 * channel.
 */

Tcl_Channel
Tcl_ReplaceChannel (Tcl_Interp *interp /* An interpreter having access
					* to the channel to supercede,
					* see 'prevChan' */,
		    Tcl_ChannelType *typePtr /* The channel type record
					      * for the new channel. */,
		    ClientData instanceData /* Instance specific data
					     * for the new channel. */,
		    int mask /* TCL_READABLE & TCL_WRITABLE to
			      * indicate whether the new channel
			      * should be readable and/or
			      * writable. This mask is mixed (by &)
			      * with the same information from the
			      * superceded channel to prevent the
			      * execution of invalid operations */,
		    Tcl_Channel prevChan /* The token of the channel to
					  * replace */);
/*
 * This is the reverse operation to 'Tcl_ReplaceChannel'.
 * This is the reverse operation to 'Tcl_ReplaceChannel'. It takes the
 * given channel and uncovers a superceded channel. If there is no
 * superceded channel this operation is equivalent to 'Tcl_Close'. The
 * superceding channel is destroyed.
 */

void
Tcl_UndoReplaceChannel (Tcl_Interp *interp /* An interpreter having access
					    * to the channel to unstack,
					    * see 'chan' */,
			Tcl_Channel chan /* The channel to remove */);

