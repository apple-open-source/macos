/*
 * mslpd_stack.h : Defines stack operations for the mslpd_query routines.
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

typedef enum { AND_STATE=1, OR_STATE=2, NOT_STATE=3, INIT_STATE=4,
TERM_STATE=5 } MSLPQState;

typedef struct mslpqframe {
  MSLPQState  state;
  Mask       *pmask;
} MSLPQframe;

typedef struct mslpqstack {
  MSLPQframe *pframe;
  int         iSize;
  int         iMax;
} MSLPQStack;

extern MSLPQStack * stack_init(Mask *pmUseMask);
extern void         stack_push(MSLPQStack *pstack, Mask *pmask, MSLPQState state);
extern MSLPQframe * stack_pop(MSLPQStack *pstack);
extern MSLPQframe * stack_curr(MSLPQStack *pstack);
extern MSLPQframe * stack_prev(MSLPQStack *pstack);
extern void         stack_delete(MSLPQStack *pstack);
