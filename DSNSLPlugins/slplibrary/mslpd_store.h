/*
 * mslpd_store.h : SA Store Definitions 
 *
 *   The SAStore for the SLP v2 minimal implementation maintains
 *   all service information advertised by the mslpd.
 *
 * Version: 1.5
 * Date:    10/05/99
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
 * v_i is used to represent booleans and integers.  It is also used
 *    to store the length of strings and opaques, if the val is interpreted
 *    this way.
 * v_pc is used to point to buffers representing strings and opaques
 *    these buffers are null terminated.
 */
typedef union {
  int   v_i;  /* bools will only be 1 = true, 0 = false, or int val */
  char *v_pc; /* opaques must be interpreted literally (not case folded) */
} Val;

typedef struct Values {
  unsigned char  type;
  int            numvals;
  Val           *pval; /* NULL terminated array of Vals */
} Values; 
  
/*
 * The store will have N entries.  Term N in the array is NULL.
 */
typedef struct store {
  int       size;
  char    **srvtype;  /* srvtype per entry, including the NA (use URL
		         if it is not supplied explicitly) */
  char    **scope;    /* scopes per entry (comma delimited scope list) */
  char    **lang;     /* language per entry */
  int      *life;     /* service lifetime */
  char    **url;      /* url per entry */
  char   ***tag;      /* array of tags per entry */
  Values  **values;   /* for each tag, its values */
  char    **attrlist; /* the original attrlist for forwarding the reg */
} SAStore;

#define TYPE_BOOL     1
#define TYPE_INT      2
#define TYPE_STR      3
#define TYPE_OPAQUE   4
#define TYPE_KEYWORD  5
#define TYPE_UNKNOWN  255

/*
 * visible within mslpd, implemented in mslpd_reader.c
 */
SLPInternalError fill_value(Values *pv, int i, const char *ppc, int *piOffset);
