
/*
 * mslpd_query.h : Mini SLP v2 Header for minimal SA, external header for
 * the implementation and other query handling functions.
 *
 * Version: 1.3
 * Date:    03/07/99
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

#if 0		// I don't think any of this is used
typedef struct mask {

  // the following parts are public 

  struct mask * (* invert) ();  // creates an inverted mask 
  struct mask * (* clone)  ();  // creates a copy of the mask
  void (* and) (struct mask *); // conjoins two lists - only those in both are set
  void (* or) (struct mask *);  // disjoins two lists - those in either are set
  int  (* next0) ();     // iterates to next unset field in mask, -1 = done
  int  (* next1) ();     // iterates to next set field in mask, -1 = done
  void (* reset) ();     // resets iteration
  void (* release) ();   // release pcMask, for cleaning up

} Mask;

Mask * createMask(int size);

/* comparison operations in scan */
#define EQ_OP       8
#define NE_OP       9
#define LT_OP      10
#define LE_OP      11
#define GT_OP      12
#define GE_OP      13

#endif

SLPInternalError HandlePluginInfoRequest( SAState* psa, const char* buffer, int length, char** reply, int *replySize );
