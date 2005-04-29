/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3virge/s3v_dac.c,v 1.5 2004/02/13 23:58:43 dawes Exp $ */

/*
 * Copyright (C) 1994-1998 The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * s3v_dac.c
 * Port to 4.0 design level
 *
 * S3 ViRGE driver
 *
 *
 * s3vcommonCalcClock from S3gendac.c in pre 4.0 tree.
 *
 */

#include	"s3v.h"


#define BASE_FREQ         14.31818   /* MHz */


	/* function */
void
S3VCommonCalcClock(ScrnInfoPtr pScrn, DisplayModePtr mode,
		   long freq, int min_m, int min_n1, 
		   int max_n1, int min_n2, int max_n2, 
		   long freq_min, long freq_max,
		   unsigned char * mdiv, unsigned char * ndiv)
{
   double ffreq, ffreq_min, ffreq_max, ffreq_min_warn;
   double div, diff, best_diff;
   unsigned int m;
   unsigned char n1, n2;
   unsigned char best_n1=16+2, best_n2=2, best_m=125+2;

   ffreq     = freq     / 1000.0 / BASE_FREQ;
   ffreq_min = freq_min / 1000.0 / BASE_FREQ;
   ffreq_max = freq_max / 1000.0 / BASE_FREQ;

   /* Doublescan modes can run at half the min frequency */
   /* But only use that value for warning and changing */
   /* ffreq, don't change the actual min used for clock calcs below. */
   if(mode->Flags & V_DBLSCAN && ffreq_min)
     ffreq_min_warn = ffreq_min / 2;
   else
     ffreq_min_warn = ffreq_min;

   if (ffreq < ffreq_min_warn / (1<<max_n2)) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "invalid frequency %1.3f MHz  [freq <= %1.3f MHz]\n", 
		 ffreq*BASE_FREQ, ffreq_min_warn*BASE_FREQ / (1<<max_n2));
      ffreq = ffreq_min_warn / (1<<max_n2);
   }
   if (ffreq > ffreq_max / (1<<min_n2)) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,      
		 "invalid frequency %1.3f MHz  [freq >= %1.3f MHz]\n", 
		 ffreq*BASE_FREQ, ffreq_max*BASE_FREQ / (1<<min_n2));
      ffreq = ffreq_max / (1<<min_n2);
   }

   /* work out suitable timings */

   best_diff = ffreq;
   
   for (n2=min_n2; n2<=max_n2; n2++) {
      for (n1 = min_n1+2; n1 <= max_n1+2; n1++) {
	 m = (int)(ffreq * n1 * (1<<n2) + 0.5) ;
	 if (m < min_m+2 || m > 127+2) 
	    continue;
	 div = (double)(m) / (double)(n1);	 
	 if ((div >= ffreq_min) &&
	     (div <= ffreq_max)) {
	    diff = ffreq - div / (1<<n2);
	    if (diff < 0.0) 
	       diff = -diff;
	    if (diff < best_diff) {
	       best_diff = diff;
	       best_m    = m;
	       best_n1   = n1;
	       best_n2   = n2;
	    }
	 }
      }
   }
   
#ifdef EXTENDED_DEBUG
   ErrorF("Clock parameters for %1.6f MHz: m=%d, n1=%d, n2=%d\n",
	  ((double)(best_m) / (double)(best_n1) / (1 << best_n2)) * BASE_FREQ,
	  best_m-2, best_n1-2, best_n2);
#endif
  
   if (max_n1 == 63)
      *ndiv = (best_n1 - 2) | (best_n2 << 6);
   else
      *ndiv = (best_n1 - 2) | (best_n2 << 5);
   *mdiv = best_m - 2;
}

