/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/ati.c,v 1.21 2003/01/01 19:16:30 tsi Exp $ */
/*
 * Copyright 1997 through 2003 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of Marc Aurele La France not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Marc Aurele La France makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *
 * MARC AURELE LA FRANCE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL MARC AURELE LA FRANCE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*************************************************************************/

/*
 * Author:  Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * This is the ATI driver for XFree86.
 *
 * John Donne once said "No man is an island", and I am most certainly not an
 * exception.  Contributions, intentional or not, to this and previous versions
 * of this driver by the following are hereby acknowledged:
 *
 * Thomas Roell, Per Lindqvist, Doug Evans, Rik Faith, Arthur Tateishi,
 * Alain Hebert, Ton van Rosmalen, David Chambers, William Shubert,
 * ATI Technologies Incorporated, Robert Wolff, David Dawes, Mark Weaver,
 * Hans Nasten, Kevin Martin, Frederic Rienthaler, Marc Bolduc, Reuben Sumner,
 * Benjamin T. Yang, James Fast Kane, Randall Hopper, W. Marcus Miller,
 * Henrik Harmsen, Christian Lupien, Precision Insight Incorporated,
 * Mark Vojkovich, Huw D M Davies, Andrew C Aitchison, Ani Joshi,
 * Kostas Gewrgiou, Jakub Jelinek, David S. Miller, A E Lawrence,
 * Linus Torvalds, William Blew, Ignacio Garcia Etxebarria, Patrick Chase,
 * Vladimir Dergachev
 *
 * ... and, many, many others from around the world.
 *
 * In addition, this work would not have been possible without the active
 * support, both moral and otherwise, of the staff and management of Computing
 * and Network Services at the University of Alberta, in Edmonton, Alberta,
 * Canada.
 *
 * The driver is intended to support all ATI adapters since their VGA Wonder
 * V3, including OEM counterparts.
 */

#include "atiident.h"
#include "atioption.h"
#include "atiprobe.h"
#include "ativersion.h"

/* The root of all evil... */
DriverRec ATI =
{
    ATI_VERSION_CURRENT,
    "ati",
    ATIIdentify,
    ATIProbe,
    ATIAvailableOptions,
    NULL,
    0
};
