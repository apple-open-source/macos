/*
 * 
 * (c) Copyright 1990 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1990 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1990 DIGITAL EQUIPMENT CORPORATION
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 *                 permission to use, copy, modify, and distribute this
 * file for any purpose is hereby granted without fee, provided that
 * the above copyright notices and this notice appears in all source
 * code copies, and that none of the names of Open Software
 * Foundation, Inc., Hewlett-Packard Company, or Digital Equipment
 * Corporation be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Neither Open Software Foundation, Inc., Hewlett-
 * Packard Company, nor Digital Equipment Corporation makes any
 * representations about the suitability of this software for any
 * purpose.
 * 
 */
/*
**
**  NAME:
**
**      pthread_np.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Non-standard pthread routines on which the NCK runtime depends.
**
*/

#include <pthread.h>
#include <pthread_np.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/time.h>

/*
 *  FUNCTIONAL DESCRIPTION:
 *
 *      Convert a delta timespec to absolute (offset by current time)
 *
 *  FORMAL PARAMETERS:
 *
 *      delta   struct timespec; input delta time
 *
 *      abstime struct timespec; output absolute time
 *
 *  IMPLICIT INPUTS:
 *
 *      current time
 *
 *  IMPLICIT OUTPUTS:
 *
 *      none
 *
 *  FUNCTION VALUE:
 *
 *      0 if successful, else -1 and errno set to error code
 *
 *  SIDE EFFECTS:
 *
 *      none
 */

extern int 
pthread_get_expiration_np(delta, abstime)
struct timespec         *delta;
struct timespec         *abstime;
{
    struct timeval   now;

    if (delta->tv_nsec >= (1000 * 1000000) || delta->tv_nsec < 0) 
    {
        errno = EINVAL;                   
        return -1;
    }

    gettimeofday(&now, NULL);

    abstime->tv_nsec    = delta->tv_nsec + (now.tv_usec * 1000);
    abstime->tv_sec     = delta->tv_sec + now.tv_sec;
        
    /* 
     * check for carry 
     */
    if (abstime->tv_nsec >= (1000 * 1000000)) 
    {   
        abstime->tv_nsec -= (1000 * 1000000);
        abstime->tv_sec += 1;
    }

    return 0;
}

extern void
pthread_delay_np(interval)
struct timespec *interval;
{
    struct timeval tv;

    tv.tv_sec = (interval)->tv_sec;
    tv.tv_usec = (interval)->tv_nsec/1000;

    select(0, 0, 0, 0, &tv);

   /*
    * Place a cancellation point here to allow reception of cancels after
    * select.
    */
   pthread_testcancel();
}

void pthread_lock_global_np  (void){return;}
void pthread_unlock_global_np  (void){return;}

