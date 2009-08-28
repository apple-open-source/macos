/* -*- c -*-
 * patchlevel.h --
 *
 * Distributed at DEC-06-2008.
 *
 * This file does nothing except to define a "patch level" for TRF.
 * The patch level has the form "X.YpZ" where X.Y is the base
 * release, and Z is a serial number that is used to sequence
 * patches for a given release.  Thus 7.4p1 is the first patch
 * to release 7.4, 7.4p2 is the patch that follows 7.4p1, and
 * so on.  The "pZ" is omitted in an original new release, and
 * it is replaced with "bZ" ("aZ") for beta resp. alpha releases.
 * The patch level ensures that patches are applied in the correct
 * order and only to appropriate sources.
 *
 * Copyright (c) 1995 Andreas Kupries (a.kupries@westend.com).
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * CVS $Id: patchlevel.h,v 1.4 2008/12/05 21:36:11 andreas_kupries Exp $
 */

#define TRF_PATCH_LEVEL	"2.1.3"
