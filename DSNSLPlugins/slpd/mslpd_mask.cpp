/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * mslpd_mask.c : Minimal SLP v2 mask for optimized query handling.
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
 * (c) Sun Microsystems, 1998, All Rights Reserved.
 * Author: Erik Guttman
 */
 /*
	Portions Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h" /* for safe_malloc */
#include "mslpd_mask.h"

#define BIT(i,pc)  /* (pc[i/8] & (1<<(i%8))) */ getbit(i,pc)

static char   getbit(int i, char *pc);


void   mask_set(Mask *pMask, int bit, int val) {

  int index = bit/8;
  unsigned char mask = 1 << (7-bit%8);
  /*  printf("  SET  bit = %d   index = %d   mask = %u   was = %u  ",
	 bit, index, mask,
	 pMask->pcMask[index]); */

  if (val) pMask->pcMask[index] |= mask; /* set the bit */
  else pMask->pcMask[index] &= ~mask;  /* clear the bit */

  /*  printf("val = %s   is = %u\n",(pMask->pcMask[index] & mask)?"yes":"no",
	 pMask->pcMask[index]); */

}

int    mask_get(Mask *pMask, int bit) {
  return BIT(bit,pMask->pcMask);
}

Mask * mask_create(int iBits) {
  Mask *pm   = (Mask *) safe_malloc(sizeof(Mask),NULL,0);
  assert( pm );
  
  pm->iLen   = iBits/8+1;
  pm->iBits  = iBits;
  pm->pcMask = safe_malloc(sizeof(pm->iLen),NULL,0);
  assert( pm->pcMask );
  
  return pm;
}

Mask * mask_invert(Mask *pm) {
  Mask *pmClone = mask_clone(pm);
  int i;
  for (i = 0; i < pm->iLen; i++)
    pmClone->pcMask[i] = ~(pm->pcMask[i]);
  return pmClone;
}

Mask * mask_clone(Mask *pm) {
  Mask *pmClone = (Mask*) safe_malloc(sizeof(Mask),(char*)pm,sizeof(Mask));
  assert( pmClone );
  
  pmClone->pcMask = safe_malloc(pm->iLen,pm->pcMask,pm->iLen);
  assert( pmClone->pcMask );
  
  return pmClone;
}

void mask_delete(Mask *pm) {
  if (pm) {
    if (pm->pcMask) SLPFree((void*)pm->pcMask);
    SLPFree((void*)pm);
  }
}

Mask * mask_and(Mask *pm1, Mask *pm2) 
{
	int i;
	Mask *pmResult = mask_create(pm1->iBits);
	
	if (pm1->iLen != pm2->iLen || pm1->iBits != pm2->iBits) 
	{
		LOG( SLP_LOG_FAIL, "mask_and: unequal lengths");
		return NULL;
	}

	pmResult->iBits = pm1->iBits;
	
	for (i = 0; i < pm1->iLen; i++)
		pmResult->pcMask[i] = (char) ((char)(0xFF & pm1->pcMask[i]) &
					(char)(0xFF & pm2->pcMask[i]));
	
	return pmResult;
}

Mask * mask_or(Mask *pm1, Mask *pm2) 
{
	int i;
	Mask *pmResult = mask_create(pm1->iBits);
	
	if (pm1->iLen != pm2->iLen || pm1->iBits != pm2->iBits)
  	{
		LOG( SLP_LOG_FAIL, "mask_and: unequal lengths");
		return NULL;
	}
	
	pmResult->iBits = pm1->iBits;
	
	for (i = 0; i < pm1->iLen; i++)
		pmResult->pcMask[i] = pm1->pcMask[i] | pm2->pcMask[i];
	
	return pmResult;
}

int mask_next(Mask *pm, int val) {

  while (pm->iIter < pm->iBits && BIT(pm->iIter,pm->pcMask) != val)
    pm->iIter++;

  if (pm->iIter == pm->iBits) return -1;
  else return pm->iIter++;

}

void   mask_reset(Mask *pm) {
  pm->iIter = 0;
}

/* ------------------------------------------------------------------------ */

static char getbit(int i, char *pc) {
  int index = i/8;
  unsigned char mask = 1 << (7-i%8);
  /*  printf("     i = %d   index = %d   mask = %u   value = %s \n",
	 i, index, mask, (pc[index] & mask)?"yes":"no");
	 */ 
  return (pc[index] & mask)?1:0;
}

/* ------------------------------------------------------------------------ */

#ifndef NDEBUG

void mask_show(Mask *pm) {
  
  int i;
  for (i = 0; i<pm->iBits ; i++ ) printf("%3d",i);
  printf("\n");
  for (i = 0;i<pm->iBits ;i++ ) printf("%3d",mask_get(pm,i));
  printf("\n");
  
}

#endif

