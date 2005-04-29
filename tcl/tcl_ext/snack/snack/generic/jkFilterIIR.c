/*
 * jkFilterIIR.c --
 *
 *	  Implementation of Snack IIR filters.
 *
 * Copyright (c) 2000 MusicMatch, Inc.  All rights reserved other than
 * as specified here.

 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 * 
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
 * DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
 * IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
 * NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 * 
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal 
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense, the
 * software shall be classified as "Commercial Computer Software" and the
 * Government shall have only "Restricted Rights" as defined in Clause
 * 252.227-7013 (c) (1) of DFARs.  Notwithstanding the foregoing, the
 * authors grant the U.S. Government and others acting in its behalf
 * permission to use and distribute the software in accordance with the
 * terms specified in this license. 
 *
 */

#include "snack.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Private stuff */

/* far below here for the initialization of this structure */
Snack_FilterType snackIIRType;

/* this structure is a C-coded sub-class of a snack filter */
typedef struct iirFilter {
  configProc *configProc;
  flowProc   *flowProc;
  freeProc   *freeProc;
  Tcl_Interp *interp;
  Snack_Filter prev, next;
  Snack_StreamInfo si;
  double     dataRatio;
  int        reserved[4];
  /* private members */
  int nInTaps;		/* number of numerator taps on filter */
  int nOutTaps;		/* number of denominator taps on filter */
  int width;			/* number of channels in the history.
				   Individual samples are interleaved. */
  double dither;		/* size of triangular dither */
  double noise;		/* size of additive gaussian noise */
  double *itaps;		/* numerator tap weights */
  double *otaps;		/* denominator tap weights */
  int in, out;		/* current position in histories */
  double *inputs;		/* input history for numerator */
  double *outputs;		/* output history for denominator */
} iirFilter, *iirFilter_t;

/*
 * iirConfigProc -- Process the configuration options for an IIR filter.
 * 
 * Arguments:
 *
 * f		Filter structure to configure.
 *
 * interp	Interpreter to use for error messages.
 *
 * objc		Number of options
 *
 * objv		Number of arguments
 * 
 * Returns:
 * TCL_OK or TCL_ERROR as appropriate.  Side effect on f.
 * 
 */
static int
iirConfigProc(Snack_Filter f, Tcl_Interp *interp, int objc,
	       Tcl_Obj *CONST objv[])
{
  int j, n;
  Tcl_Obj **impObj;
  double dither, noise;
  iirFilter_t iir = (iirFilter_t) f;
  
  int arg, arg1 = 0;
  static CONST84 char *optionStrings[] = {
    "-impulse", "-numerator", "-denominator", "-noise", "-dither", NULL
  };
  enum options {
    IMPULSE, TOP, BOTTOM, NOISE, DITHER
  };
  
  /* The default values have already been set if this is a new
     filter, or the structure has already been configured.  Either way,
     our default course of action here, is to leave everything untouched.
  */
  for (arg = arg1; arg < objc; arg += 2) {
    int index;
    
    /* Tcl_GetIndexFromObj is good to use since it remembers the result
       and makes faster decisions next time */
    if (Tcl_GetIndexFromObj(interp, objv[arg], optionStrings, "option", 0,
			    &index) != TCL_OK) {
      return TCL_ERROR;
    }
    
    switch ((enum options) index) {
      /* size of triangular dithering on output */
    case DITHER:
      if (Tcl_GetDoubleFromObj(interp, objv[arg+1], &dither) != TCL_OK) {
	return TCL_ERROR;
      }
      if (dither < 0) dither = -dither;
      iir->dither = dither;
      break;
      
      /* amount of gaussian noise to add */
    case NOISE:
      if (Tcl_GetDoubleFromObj(interp, objv[arg+1], &noise) != TCL_OK) {
	return TCL_ERROR;
      }
      if (noise < 0) noise = -noise;
      iir->noise = noise;
      break;
      
      /* either the impulse response if this is really an FIR or
	 the numerator of a rational filter of the form
	 y = x P(z)/Q(z).  People who are thinking in terms of
	 FIR's will probably prefer to use -impulse while IIR
	 designers will use -numerator and -denominator.
      */
    case IMPULSE:
    case TOP:
      if (Tcl_ListObjGetElements(interp, objv[arg+1], &n, &impObj) != TCL_OK) {
	return TCL_ERROR;
      }
      
      iir->nInTaps = n;
      iir->itaps = (double *) ckalloc( n * sizeof(iir->itaps[0]) );
      
      for (j = 0; j < n; j++) {
	if (Tcl_GetDoubleFromObj(interp, impObj[j], &iir->itaps[j]) != TCL_OK) {
	  return TCL_ERROR;
	}
      }
      
      break;
      
      /* the denominator of a ratio filter */
    case BOTTOM:
      if (Tcl_ListObjGetElements(interp, objv[arg+1], &n, &impObj) != TCL_OK) {
	return TCL_ERROR;
      }
      
      iir->nOutTaps = n;
      iir->otaps = (double *) ckalloc( n * sizeof(iir->otaps[0]) );
      
      for (j = 0; j < n; j++) {
	if (Tcl_GetDoubleFromObj(interp, impObj[j], &iir->otaps[j]) != TCL_OK) {
	  return TCL_ERROR;
	}
      }
      
      break;
    }
  }
  
  return TCL_OK;
}

/*
 * iirCreateProc -- Create a filter structure.
 * 
 * Arguments:
 *
 * interp		to report errors in
 *
 * objc			number of arguments
 *
 * objv			number of arguments (for configuration)
 * 
 * Returns:
 * A filter structure all filled in and configurated!
 * 
 */

static Snack_Filter
iirCreateProc(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  iirFilter_t iir;
  
  iir = (iirFilter_t) ckalloc(sizeof(*iir));
  memset(iir, 0, sizeof(*iir));
  
  /* default values set before calling the config */
  iir->dither = 0.75;
  iir->nInTaps = 0;
  iir->nOutTaps = 0;
  
  if (iirConfigProc((Snack_Filter) iir, interp, objc, objv) != TCL_OK) {
    ckfree((char *) iir);
    return (Snack_Filter) NULL;
  }
  
  return (Snack_Filter) iir;
}

/*
 * iirStartProc -- Reinitialize a filter structure.
 * 
 * Arguments:
 *
 * f		The filter to initialize
 *
 * si		Information about a stream.  This is passed to allow the
 *		filter to be customized for the number of channels and such.
 *
 * Returns:
 * TCL_OK or TCL_ERROR as appropriate
 * 
 */

static int
iirStartProc(Snack_Filter f, Snack_StreamInfo si)
{
  int i;
  iirFilter_t iir = (iirFilter_t) f;
  
  if (iir->nInTaps > 0) {
    iir->inputs = (double *) ckalloc(si->outWidth * 
				     iir->nInTaps * sizeof(iir->inputs[0]));
    for (i = 0; i < iir->nInTaps * si->outWidth; i++ ) {
      iir->inputs[i] = 0;
    }
  }
  if (iir->nOutTaps > 0) {
    iir->outputs = (double *) ckalloc(si->outWidth * 
				      iir->nOutTaps * sizeof(iir->outputs[0]));
    for (i = 0; i < iir->nOutTaps * si->outWidth; i++ ) {
      iir->outputs[i] = 0;
    }
  }
  
  iir->in = iir->out = 0;
  
  return TCL_OK;
}

/*
 * name-of-function -- Return a normal random deviate.  The sum of 12
 * 	unit uniform deviates has very close to a normal distribution.
 * 	Changing the sign on 6 of them gives a mean of 0.  This is
 * 	probably slightly faster than the standard method that
 * 	requires half of a sine and half of a log to compute, and it
 * 	is thread safe.  Accuracy is infinitessimally lower.  None of
 * 	these differences matter compared to the fact that I can get
 * 	this one exactly right from memory.
 * 	
 * Arguments:
 *
 * 
 * Returns:
 * A unit normal deviate.
 * 
 */

static double
xdrand48()
{
  return((double)rand()/RAND_MAX);
}

static double
normalDeviate()
{
  return
    xdrand48() + xdrand48() - xdrand48() - xdrand48() +
    xdrand48() + xdrand48() - xdrand48() - xdrand48() +
    xdrand48() + xdrand48() - xdrand48() - xdrand48();
}

/*
 * triangularDeviate -- Return a triangular dither sample.  Adding a
 * 	dither signal from a triangular distribution converts highly
 * 	correlated quantization noise into uncorrelated white noise
 * 	that is generally considered to be less perceptible.
 * 	Theoretically, 1LSB is the optimal size, but in practice,
 * 	somewhat less is actually used.
 * 	
 * Arguments:
 *
 * Returns:
 * A random value from a unit sized triangular distribution.
 * 
 */

static double
triangularDeviate()
{
    return xdrand48() - xdrand48();
}

/*
 * iirFlowProc -- Filter a block of samples, producing a block of
 * 	output samples.
 * 
 * Arguments:
 *
 * f		The filter to apply to the samples.
 *
 * si 		stream info structure.  Should tell us how many channels
 *		to process.  
 *
 * in		Input frame.  An array of (*inFrames) samples.
 *
 * out		Output frame.  An array of samples.
 * 
 * inFrames	A pointer to the number of input frames.  We always
 *		process all of our input, so we never change this.
 * 
 * outFrames	A pointer to the number of output frames.  This must be
 *		at least >= *inFrames.  We return the number of samples
 *		that we process.
 * 
 * Returns:
 * TCL_OK or TCL_ERROR as appropriate
 * 
 * Note:
 */

static int
iirFlowProc(Snack_Filter f, Snack_StreamInfo si, float *in, float *out,
	    int *inFrames, int *outFrames)
{
  iirFilter_t iir = (iirFilter_t) f;
  int i, j, k, wi, iir_in = 0, iir_out = 0;
  double insmp, outsmp;
  
  /* deal with stream width sanity check */
  
  /* so... for each input, process it into the filter */
  
  for (wi = 0; wi < si->outWidth; wi++) {
    iir_in = iir->in;
    iir_out = iir->out;
    for (i = 0; i < *inFrames && i < *outFrames; i++) {
      /* get value and cast to a very safe working copy */
      insmp = (double) in[i*si->outWidth+wi];
      iir->inputs[iir_in*si->outWidth+wi] = insmp;
      
      /* output is initially zero.  We add up the contribution for
	 the input history elements */
      outsmp = 0;
      if (iir->itaps != NULL) {
	/* this is the inner convolution loop.  We should unroll
	   this or something if we care about speed.  Of course,
	   we don't care until we know that the code really works!	   
	   before undertaking a full unrolling, we could just
	   split the loop into two loops to avoid the %
	   operation. 
	*/
	k = iir_in;
	for (j = 0 ; j < iir->nInTaps ; j++) {
	  outsmp += iir->itaps[j] * iir->inputs[k*si->outWidth+wi];
	  k = (k+1)%(iir->nInTaps);
	}
	iir_in = (iir_in+1)%(iir->nInTaps);
      }
      
      if (iir->otaps != NULL) {
	/* now we process the output history.  What happens here
	   is that we rearrange the normal formula from this form
	   
	        P(z)
	   y = ------ x
	        Q(z)
	   
	   to isolate the current output value y_n and put all
	   delayed outputs and all current and delayed inputs
	   together on the RHS.  Thus,
	   
	   q_0 y_n = P(z) x - [Q(z) - q_0] y
	   
	   1  [                                                   ]
	   y_n =  --- [ sum_{i=0..} p_i x_{n-i} - sum_{i=1..} q_i y_{n-i} ]
	   q_0 [                                                   ]
	   
	   
	   So clever, so droll.
	*/
	k = iir_out;
	
	/* note that we skip the 0'th element here */
	for (j = 1 ; j < iir->nOutTaps ; j++) {
	  outsmp -= iir->otaps[j] * iir->outputs[k*si->outWidth+wi];
	  k = (k+1)%(iir->nInTaps);
	}
	iir_out = (iir_out+1)%(iir->nOutTaps);
	
	/* here is where we use the zero'th element */
	outsmp /= iir->otaps[0];
	iir->outputs[iir_out*si->outWidth+wi] = outsmp;
      } 
      out[i*si->outWidth+wi] = (float) (outsmp + iir->noise * normalDeviate()
					+ iir->dither * triangularDeviate());
    }
  }
  iir->in = iir_in;
  iir->out = iir_out;
  
  return TCL_OK;
}

/*
 * iirFreeProc -- Deallocate a filter structure.
 * 
 * Arguments:
 *
 * f		The structure to deallocate.
 * 
 * Returns:
 * void
 */

static void
iirFreeProc(Snack_Filter f)
{
  iirFilter_t iir = (iirFilter_t) f;
  
  if (iir->itaps != NULL) ckfree((void *) iir->itaps);
  if (iir->otaps != NULL) ckfree((void *) iir->otaps);
  if (iir->inputs != NULL) ckfree((void *) iir->inputs);
  if (iir->outputs != NULL) ckfree((void *) iir->outputs);
}

Snack_FilterType snackIIRType = {
  "iir",
  iirCreateProc,
  iirConfigProc,
  iirStartProc,
  iirFlowProc,
  iirFreeProc,
  (Snack_FilterType *) NULL
};

void
createIIRFilter() {
  Snack_CreateFilterType(&snackIIRType);
}
