
/*
 * mslpd_mask.h: SLP v2 Header for minimal SA, internal header for mask
 *            implementation.
 *
 * Version: 1.2
 * Date:    09/29/98
 *
 * Licensee will, at its expense,  defend and indemnify Sun Microsystems,
 * Inc.  ("Sun")  and  its  licensors  from  and  against any third party
 * claims, including costs and reasonable attorneys' fees,  and be wholly
 * responsible for  any liabilities  arising  out  of  or  related to the
 * Licensee's use of the Software or Modifications.   The Software is not
 * designed  or intended for use in  on-line  control  of  aircraft,  air
 * traffic,  aircraft navigation,  or aircraft communications;  or in the
 * design, construction, operation or maintenance of any nuclear facility
 * and Sun disclaims any express or implied warranty of fitness  for such
 * uses.  THE SOFTWARE IS PROVIDED TO LICENSEE "AS IS" AND ALL EXPRESS OR
 * IMPLIED CONDITION AND WARRANTIES, INCLUDING  ANY  IMPLIED  WARRANTY OF
 * MERCHANTABILITY,   FITNESS  FOR  WARRANTIES,   INCLUDING  ANY  IMPLIED
 * WARRANTY  OF  MERCHANTABILITY,  FITNESS FOR PARTICULAR PURPOSE OR NON-
 * INFRINGEMENT, ARE DISCLAIMED. IN NO EVENT WILL SUN BE LIABLE HEREUNDER
 * FOR ANY DIRECT DAMAGES OR ANY INDIRECT, PUNITIVE, SPECIAL, INCIDENTAL
 * OR CONSEQUENTIAL DAMAGES OF ANY KIND.
 *
 * (c) Copyright Sun Microsystems, 1998, All Rights Reserved
 * Author: Erik Guttman
 */

typedef struct mask {
  int   iIter;
  int   iLen;
  int   iBits;
  char  *pcMask;
} Mask;

Mask * mask_create(int i);
void   mask_delete(Mask *pm);
void   mask_set(Mask *pMask, int bit, int iVal);
int    mask_get(Mask *pMask, int bit);
Mask * mask_clone(Mask *pm);
Mask * mask_invert(Mask *pm);
Mask * mask_and(Mask *pm1, Mask *pm2);
Mask * mask_or(Mask *pm1, Mask *pm2);
int    mask_next(Mask *pm,int val);
void   mask_reset(Mask *pm);

#ifndef NDEBUG
void   mask_show(Mask *pm);
#endif





