/*
 * This software has been licensed to the Centre of Speech Technology, KTH
 * by Microsoft Corp. with the terms in the accompanying file BSD.txt,
 * which is a BSD style license.
 *
 *    "Copyright (c) 1990-1996 Entropic Research Laboratory, Inc. 
 *                   All rights reserved"
 *
 * Written by:  Derek Lin
 * Checked by:
 * Revised by:  David Talkin
 *
 * Brief description:  Estimates F0 using normalized cross correlation and
 *   dynamic programming.
 *
 */


#include "snack.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#ifndef TRUE
# define TRUE 1
# define FALSE 0
#endif
#ifndef FLT_MAX
# define FLT_MAX (3.40282347E+38f) 
#endif
#ifndef M_PI
# define M_PI (3.1415926536f)
#endif
#include "jkGetF0.h"

int	    debug_level = 0;

void free_dp_f0();
static int check_f0_params();

int
Get_f0(Sound *sound, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  float *fdata;
  int done;
  long buff_size, actsize;
  double sf, start_time;
  F0_params *par, *read_f0_params();
  float *f0p, *vuvp, *rms_speech, *acpkp;
  int i, vecsize;
  int init_dp_f0(), dp_f0();
  static int framestep = -1;
  long sdstep = 0, total_samps;
  int ndone = 0;
  Tcl_Obj *list;
  double framestep2 = 0.0, wind_dur;

  int arg, startpos = 0, endpos = -1, fmax, fmin;
  static CONST84 char *subOptionStrings[] = {
    "-start", "-end", "-maxpitch", "-minpitch", "-progress",
    "-framelength", "-method", "-windowlength", NULL
  };
  enum subOptions {
    START, END, F0MAX, F0MIN, PROGRESS, FRAME, METHOD, WINLEN
  };

  if (sound->cmdPtr != NULL) {
    Tcl_DecrRefCount(sound->cmdPtr);
    sound->cmdPtr = NULL;
  }

  par = (F0_params *) ckalloc(sizeof(F0_params));
  par->cand_thresh = 0.3f;
  par->lag_weight = 0.3f;
  par->freq_weight = 0.02f;
  par->trans_cost = 0.005f;
  par->trans_amp = 0.5f;
  par->trans_spec = 0.5f;
  par->voice_bias = 0.0f;
  par->double_cost = 0.35f;
  par->min_f0 = 50;
  par->max_f0 = 550;
  par->frame_step = 0.01f;
  par->wind_dur = 0.0075f;
  par->n_cands = 20;
  par->mean_f0 = 200;     /* unused */
  par->mean_f0_weight = 0.0f;  /* unused */
  par->conditioning = 0;    /*unused */

  for (arg = 2; arg < objc; arg += 2) {
    int index;
	
    if (Tcl_GetIndexFromObj(interp, objv[arg], subOptionStrings,
			    "option", 0, &index) != TCL_OK) {
      return TCL_ERROR;
    }

    if (arg + 1 == objc) {
      Tcl_AppendResult(interp, "No argument given for ",
		       subOptionStrings[index], " option", (char *) NULL);
      return TCL_ERROR;
    }
    
    switch ((enum subOptions) index) {
    case START:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &startpos) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case END:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &endpos) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case F0MAX:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &fmax) != TCL_OK)
	  return TCL_ERROR;
	par->max_f0 = (float) fmax;
	break;
      }
    case F0MIN:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &fmin) != TCL_OK)
	  return TCL_ERROR;
	par->min_f0 = (float) fmin;
	break;
      }
    case PROGRESS:
      {
	char *str = Tcl_GetStringFromObj(objv[arg+1], NULL);
	
	if (strlen(str) > 0) {
	  Tcl_IncrRefCount(objv[arg+1]);
	  sound->cmdPtr = objv[arg+1];
	}
	break;
      }
    case FRAME:
      {
	if (Tcl_GetDoubleFromObj(interp, objv[arg+1], &framestep2)
	    != TCL_OK)
	  return TCL_ERROR;
	par->frame_step = (float) framestep2;
	break;
      }
    case METHOD:
      {
	break;
      }
    case WINLEN:
      {
	if (Tcl_GetDoubleFromObj(interp, objv[arg+1], &wind_dur)
	    != TCL_OK)
	  return TCL_ERROR;
	par->wind_dur = (float) wind_dur;
	break;
      }
    }
  }
  if (startpos < 0) startpos = 0;
  if (endpos >= (sound->length - 1) || endpos == -1)
    endpos = sound->length - 1;
  if (startpos > endpos) return TCL_OK;

  sf = (double) sound->samprate;

  if (framestep > 0)  /* If a value was specified with -S, use it. */
    par->frame_step = (float) (framestep / sf);
  start_time = 0.0f;
  if(check_f0_params(interp, par, sf)){
    Tcl_AppendResult(interp, "invalid/inconsistent parameters -- exiting.", NULL);
    return TCL_ERROR;
  }

  total_samps = endpos - startpos + 1;
  if(total_samps < ((par->frame_step * 2.0) + par->wind_dur) * sf) {
    Tcl_AppendResult(interp, "input range too small for analysis by get_f0.", NULL);
    return TCL_ERROR;
  }
  /* Initialize variables in get_f0.c; allocate data structures;
   * determine length and overlap of input frames to read.
   */
  if (init_dp_f0(sf, par, &buff_size, &sdstep)
      || buff_size > INT_MAX || sdstep > INT_MAX)
  {
    Tcl_AppendResult(interp, "problem in init_dp_f0().", NULL);
    return TCL_ERROR;
  }

  if (debug_level)
    Fprintf(stderr, "init_dp_f0 returned buff_size %ld, sdstep %ld.\n",buff_size, sdstep);

  if (buff_size > total_samps)
    buff_size = total_samps;

  actsize = min(buff_size, sound->length);
  fdata = (float *) ckalloc(sizeof(float) * max(buff_size, sdstep));
  list = Tcl_NewListObj(0, NULL);
  Snack_ProgressCallback(sound->cmdPtr, interp, "Computing pitch", 0.0);
  ndone = startpos;

  while (TRUE) {
    done = (actsize < buff_size) || (total_samps == buff_size);
    Snack_GetSoundData(sound, ndone, fdata, actsize);
    /*if (sound->debug > 0) Snack_WriteLog("dp_f0...\n");*/
    if (dp_f0(fdata, (int) actsize, (int) sdstep, sf, par,
	      &f0p, &vuvp, &rms_speech, &acpkp, &vecsize, done)) {
      Tcl_AppendResult(interp, "problem in dp_f0().", NULL);
      return TCL_ERROR;
    }
    /*if (sound->debug > 0) Snack_WriteLogInt("done dp_f0",vecsize);*/
    for (i = vecsize - 1; i >= 0; i--) {
      Tcl_Obj *frameList;
      frameList = Tcl_NewListObj(0, NULL);
      Tcl_ListObjAppendElement(interp, list, frameList);
      Tcl_ListObjAppendElement(interp, frameList,
			       Tcl_NewDoubleObj((double)f0p[i]));
      Tcl_ListObjAppendElement(interp, frameList,
			       Tcl_NewDoubleObj((double)vuvp[i]));
      Tcl_ListObjAppendElement(interp, frameList,
			       Tcl_NewDoubleObj((double)rms_speech[i]));
      Tcl_ListObjAppendElement(interp, frameList,
			       Tcl_NewDoubleObj((double)acpkp[i]));
    }

    if (done) break;

    ndone += sdstep; 
    actsize = min(buff_size, sound->length - ndone);
    total_samps -= sdstep;

    if (actsize > total_samps)
      actsize = total_samps;

    if (1) {
      int res = Snack_ProgressCallback(sound->cmdPtr, interp, "Computing pitch", (double) ndone / sound->length);
      if (res != TCL_OK) {
	return TCL_ERROR;
      }
    }
  }

  Snack_ProgressCallback(sound->cmdPtr, interp, "Computing pitch", 1.0);

  ckfree((void *)fdata);

  ckfree((void *)par);

  free_dp_f0();
  
  Tcl_SetObjResult(interp, list);

  return TCL_OK;
}


/*
 * Some consistency checks on parameter values.
 * Return a positive integer if any errors detected, 0 if none.
 */

static int
check_f0_params(Tcl_Interp *interp, F0_params *par, double sample_freq)
{
  int	  error = 0;
  double  dstep;

  if((par->cand_thresh < 0.01) || (par->cand_thresh > 0.99)) {
    Tcl_AppendResult(interp,
	  "ERROR: cand_thresh parameter must be between [0.01, 0.99].",NULL);
    error++;
  }
  if((par->wind_dur > .1) || (par->wind_dur < .0001)) {
    Tcl_AppendResult(interp,"ERROR: wind_dur parameter must be between [0.0001, 0.1].",NULL);
    error++;
  }
  if((par->n_cands > 100) || (par->n_cands < 3)){
    Tcl_AppendResult(interp,
	    "ERROR: n_cands parameter must be between [3,100].",NULL); 
    error++;
  }
  if((par->max_f0 <= par->min_f0) || (par->max_f0 >= (sample_freq/2.0)) ||
     (par->min_f0 < (sample_freq/10000.0))){
    Tcl_AppendResult(interp,
	    "ERROR: min(max)_f0 parameter inconsistent with sampling frequency.",NULL); 
    error++;
  }
  dstep = ((double)((int)(0.5 + (sample_freq * par->frame_step))))/sample_freq;
  if(dstep != par->frame_step) {
    if(debug_level)
      Tcl_AppendResult(interp,
	      "Frame step set to exactly match signal sample rate.",NULL);
    par->frame_step = (float) dstep;
  }
  if((par->frame_step > 0.1) || (par->frame_step < (1.0/sample_freq))){
    Tcl_AppendResult(interp,
	    "ERROR: frame_step parameter must be between [1/sampling rate, 0.1].",NULL); 
    error++;
  }

  return(error);
}

static void get_cand(), peak(), do_ffir();
static int lc_lin_fir(), downsamp();

/* ----------------------------------------------------------------------- */
void get_fast_cands(fdata, fdsdata, ind, step, size, dec, start, nlags, engref, maxloc, maxval, cp, peaks, locs, ncand, par)
     float *fdata, *fdsdata, *engref, *maxval, *peaks;
     int size, start, nlags, *maxloc, *locs, *ncand, ind, step, dec;
     Cross *cp;
     F0_params *par;
{
  int decind, decstart, decnlags, decsize, i, j, *lp;
  float *corp, xp, yp, lag_wt;
  register float *pe;

  lag_wt = par->lag_weight/nlags;
  decnlags = 1 + (nlags/dec);
  if((decstart = start/dec) < 1) decstart = 1;
  decind = (ind * step)/dec;
  decsize = 1 + (size/dec);
  corp = cp->correl;
    
  crossf(fdsdata + decind, decsize, decstart, decnlags, engref, maxloc,
	maxval, corp);
  cp->maxloc = *maxloc;	/* location of maximum in correlation */
  cp->maxval = *maxval;	/* max. correlation value (found at maxloc) */
  cp->rms = (float) sqrt(*engref/size); /* rms in reference window */
  cp->firstlag = decstart;

  get_cand(cp,peaks,locs,decnlags,ncand,par->cand_thresh); /* return high peaks in xcorr */

  /* Interpolate to estimate peak locations and values at high sample rate. */
  for(i = *ncand, lp = locs, pe = peaks; i--; pe++, lp++) {
    j = *lp - decstart - 1;
    peak(&corp[j],&xp,&yp);
    *lp = (*lp * dec) + (int)(0.5+(xp*dec)); /* refined lag */
    *pe = yp*(1.0f - (lag_wt* *lp)); /* refined amplitude */
  }
  
  if(*ncand >= par->n_cands) {	/* need to prune candidates? */
    register int *loc, *locm, lt;
    register float smaxval, *pem;
    register int outer, inner, lim;
    for(outer=0, lim = par->n_cands-1; outer < lim; outer++)
      for(inner = *ncand - 1 - outer,
	  pe = peaks + (*ncand) -1, pem = pe-1,
	  loc = locs + (*ncand) - 1, locm = loc-1;
	  inner--;
	  pe--,pem--,loc--,locm--)
	if((smaxval = *pe) > *pem) {
	  *pe = *pem;
	  *pem = smaxval;
	  lt = *loc;
	  *loc = *locm;
	  *locm = lt;
	}
    *ncand = par->n_cands-1;  /* leave room for the unvoiced hypothesis */
  }
  crossfi(fdata + (ind * step), size, start, nlags, 7, engref, maxloc,
	  maxval, corp, locs, *ncand);

  cp->maxloc = *maxloc;	/* location of maximum in correlation */
  cp->maxval = *maxval;	/* max. correlation value (found at maxloc) */
  cp->rms = (float) sqrt(*engref/size); /* rms in reference window */
  cp->firstlag = start;
  get_cand(cp,peaks,locs,nlags,ncand,par->cand_thresh); /* return high peaks in xcorr */
    if(*ncand >= par->n_cands) {	/* need to prune candidates again? */
    register int *loc, *locm, lt;
    register float smaxval, *pe, *pem;
    register int outer, inner, lim;
    for(outer=0, lim = par->n_cands-1; outer < lim; outer++)
      for(inner = *ncand - 1 - outer,
	  pe = peaks + (*ncand) -1, pem = pe-1,
	  loc = locs + (*ncand) - 1, locm = loc-1;
	  inner--;
	  pe--,pem--,loc--,locm--)
	if((smaxval = *pe) > *pem) {
	  *pe = *pem;
	  *pem = smaxval;
	  lt = *loc;
	  *loc = *locm;
	  *locm = lt;
	}
    *ncand = par->n_cands - 1;  /* leave room for the unvoiced hypothesis */
  }
}

/* ----------------------------------------------------------------------- */
float *downsample(input,samsin,state_idx,freq,samsout,decimate, first_time, last_time)
     double freq;
     float *input;
      int samsin, *samsout, decimate, state_idx, first_time, last_time;
{
  static float	b[2048];
  static float *foutput = NULL;
  float	beta = 0.0f;
  static int	ncoeff = 127, ncoefft = 0;
  int init;

  if(input && (samsin > 0) && (decimate > 0) && *samsout) {
    if(decimate == 1) {
      return(input);
    }

    if(first_time){
      int nbuff = (samsin/decimate) + (2*ncoeff);

      ncoeff = ((int)(freq * .005)) | 1;
      beta = .5f/decimate;
      foutput = (float*)ckrealloc((void *)foutput, sizeof(float) * nbuff);
      /*      spsassert(foutput, "Can't allocate foutput in downsample");*/
      for( ; nbuff > 0 ;)
	foutput[--nbuff] = 0.0;

      if( !lc_lin_fir(beta,&ncoeff,b)) {
	fprintf(stderr,"\nProblems computing interpolation filter\n");
	ckfree((void *)foutput);
	return(NULL);
      }
      ncoefft = (ncoeff/2) + 1;
    }		    /*  endif new coefficients need to be computed */

    if(first_time) init = 1;
    else if (last_time) init = 2;
    else init = 0;
    
    if(downsamp(input,foutput,samsin,samsout,state_idx,decimate,ncoefft,b,init)) {
      return(foutput);
    } else
      Fprintf(stderr,"Problems in downsamp() in downsample()\n");
  } else
    Fprintf(stderr,"Bad parameters passed to downsample()\n");
  
  return(NULL);
}

/* ----------------------------------------------------------------------- */
/* Get likely candidates for F0 peaks. */
static void get_cand(cross,peak,loc,nlags,ncand,cand_thresh)
     Cross *cross;
     float *peak, cand_thresh;
     int *loc;
     int  *ncand, nlags;
{
  register int i, lastl, *t;
  register float o, p, q, *r, *s, clip;
  int start, ncan, maxl;

  clip = (float) (cand_thresh * cross->maxval);
  maxl = cross->maxloc;
  lastl = nlags - 2;
  start = cross->firstlag;

  r = cross->correl;
  o= *r++;			/* first point */
  q = *r++;	                /* middle point */
  p = *r++;
  s = peak;
  t = loc;
  ncan=0;
  for(i=1; i < lastl; i++, o=q, q=p, p= *r++){
    if((q > clip) &&		/* is this a high enough value? */
      (q >= p) && (q >= o)){ /* NOTE: this finds SHOLDERS and PLATEAUS
				      as well as peaks (is this a good idea?) */
	*s++ = q;		/* record the peak value */
	*t++ = i + start;	/* and its location */
	ncan++;			/* count number of peaks found */
      }
  }
/*
  o = q;
  q = p;
  if( (q > clip) && (q >=0)){
    *s++ = q;
    *t++ = i+start;
    ncan++;
  }
*/
  *ncand = ncan;
}

/* ----------------------------------------------------------------------- */
/* buffer-to-buffer downsample operation */
/* This is STRICTLY a decimator! (no upsample) */
static int downsamp(in, out, samples, outsamps, state_idx, decimate, ncoef, fc, init)
     float *in, *out;
     int samples, *outsamps, decimate, ncoef, state_idx;
     float fc[];
     int init;
{
  if(in && out) {
    do_ffir(in, samples, out, outsamps, state_idx, ncoef, fc, 0, decimate, init);
    return(TRUE);
  } else
    printf("Bad signal(s) passed to downsamp()\n");
  return(FALSE);
}

/*      ----------------------------------------------------------      */
static void do_ffir(buf,in_samps,bufo,out_samps,idx, ncoef,fc,invert,skip,init)
/* fc contains 1/2 the coefficients of a symmetric FIR filter with unity
    passband gain.  This filter is convolved with the signal in buf.
    The output is placed in buf2.  If(invert), the filter magnitude
    response will be inverted.  If(init&1), beginning of signal is in buf;
    if(init&2), end of signal is in buf.  out_samps is set to the number of
    output points placed in bufo. */
register float	*buf, *bufo;
float *fc;
register int in_samps, ncoef, invert, skip, init, *out_samps;
int idx;
{
  register float *dp1, *dp2, *dp3, sum, integral;
  static float *co=NULL, *mem=NULL;
  static float state[1000];
  static int fsize=0, resid=0;
  register int i, j, k, l;
  register float *sp;
  register float *buf1;

  buf1 = buf;
  if(ncoef > fsize) {/*allocate memory for full coeff. array and filter memory */    fsize = 0;
    i = (ncoef+1)*2;
    if(!((co = (float *)ckrealloc((void *)co, sizeof(float)*i)) &&
	 (mem = (float *)ckrealloc((void *)mem, sizeof(float)*i)))) {
      fprintf(stderr,"allocation problems in do_fir()\n");
      return;
    }
    fsize = ncoef;
  }

  /* fill 2nd half with data */
  for(i=ncoef, dp1=mem+ncoef-1; i-- > 0; )  *dp1++ = *buf++;  

  if(init & 1) {	/* Is the beginning of the signal in buf? */
    /* Copy the half-filter and its mirror image into the coefficient array. */
    for(i=ncoef-1, dp3=fc+ncoef-1, dp2=co, dp1 = co+((ncoef-1)*2),
	integral = 0.0; i-- > 0; )
      if(!invert) *dp1-- = *dp2++ = *dp3--;
      else {
	integral += (sum = *dp3--);
	*dp1-- = *dp2++ = -sum;
      }
    if(!invert)  *dp1 = *dp3;	/* point of symmetry */
    else {
      integral *= 2;
      integral += *dp3;
      *dp1 = integral - *dp3;
    }

    for(i=ncoef-1, dp1=mem; i-- > 0; ) *dp1++ = 0;
  }
  else
    for(i=ncoef-1, dp1=mem, sp=state; i-- > 0; ) *dp1++ = *sp++;

  i = in_samps;
  resid = 0;

  k = (ncoef << 1) -1;	/* inner-product loop limit */

  if(skip <= 1) {       /* never used */
/*    *out_samps = i;	
    for( ; i-- > 0; ) {	
      for(j=k, dp1=mem, dp2=co, dp3=mem+1, sum = 0.0; j-- > 0;
	  *dp1++ = *dp3++ )
	sum += *dp2++ * *dp1;

      *--dp1 = *buf++;	
      *bufo++ = (sum < 0.0)? sum -0.5 : sum +0.5; 
    }
    if(init & 2) {	
      for(i=ncoef; i-- > 0; ) {
	for(j=k, dp1=mem, dp2=co, dp3=mem+1, sum = 0.0; j-- > 0;
	    *dp1++ = *dp3++ )
	  sum += *dp2++ * *dp1;
	*--dp1 = 0.0;
	*bufo++ = (sum < 0)? sum -0.5 : sum +0.5; 
      }
      *out_samps += ncoef;
    }
    return;
*/
  } 
  else {			/* skip points (e.g. for downsampling) */
    /* the buffer end is padded with (ncoef-1) data points */
    for( l=0 ; l < *out_samps; l++ ) {
      for(j=k-skip, dp1=mem, dp2=co, dp3=mem+skip, sum=0.0; j-- >0;
	  *dp1++ = *dp3++)
	sum += *dp2++ * *dp1;
      for(j=skip; j-- >0; *dp1++ = *buf++) /* new data to memory */
	sum += *dp2++ * *dp1;
      *bufo++ = (sum<0.0) ? sum -0.5f : sum +0.5f;
    }
    if(init & 2){
      resid = in_samps - *out_samps * skip;
      for(l=resid/skip; l-- >0; ){
	for(j=k-skip, dp1=mem, dp2=co, dp3=mem+skip, sum=0.0; j-- >0;
	    *dp1++ = *dp3++)
	    sum += *dp2++ * *dp1;
	for(j=skip; j-- >0; *dp1++ = 0.0)
	  sum += *dp2++ * *dp1;
	*bufo++ = (sum<0.0) ? sum -0.5f : sum +0.5f;
	(*out_samps)++;
      }
    }
    else
      for(dp3=buf1+idx-ncoef+1, l=ncoef-1, sp=state; l-- >0; ) *sp++ = *dp3++;
  }
}

/*      ----------------------------------------------------------      */
static int lc_lin_fir(fc,nf,coef)
/* create the coefficients for a symmetric FIR lowpass filter using the
   window technique with a Hanning window. */
register float	fc;
float	*coef;
int	*nf;
{
    register int	i, n;
    register double	twopi, fn, c;

    if(((*nf % 2) != 1))
	*nf = *nf + 1;
    n = (*nf + 1)/2;

    /*  Compute part of the ideal impulse response (the sin(x)/x kernel). */
    twopi = M_PI * 2.0;
    coef[0] = (float) (2.0 * fc);
    c = M_PI;
    fn = twopi * fc;
    for(i=1;i < n; i++) coef[i] = (float)(sin(i * fn)/(c * i));

    /* Now apply a Hanning window to the (infinite) impulse response. */
    /* (Probably should use a better window, like Kaiser...) */
    fn = twopi/(double)(*nf);
    for(i=0;i<n;i++) 
	coef[n-i-1] *= (float)((.5 - (.5 * cos(fn * ((double)i + 0.5)))));
    
    return(TRUE);
}


/* ----------------------------------------------------------------------- */
/* Use parabolic interpolation over the three points defining the peak
 * vicinity to estimate the "true" peak. */
static void peak(y, xp, yp)
     float *y,			/* vector of length 3 defining peak */
       *xp, *yp;  /* x,y values of parabolic peak fitting the input points. */
{
  register float a, c;
  
  a = (float)((y[2]-y[1])+(.5*(y[0]-y[2])));
  if(fabs(a) > .000001) {
    *xp = c = (float)((y[0]-y[2])/(4.0*a));
    *yp = y[1] - (a*c*c);
  } else {
    *xp = 0.0;
    *yp = y[1];
  }
}

/* A fundamental frequency estimation algorithm using the normalized
   cross correlation function and dynamic programming.  The algorithm
   implemented here is similar to that presented by B. Secrest and
   G. Doddington, "An integrated pitch tracking algorithm for speech
   systems", Proc. ICASSP-83, pp.1352-1355.  It is fully described
   by D. Talkin, "A robust algorithm for ptich tracking (RAPT)", in
   W. B. Kleijn & K. K. Paliwal (eds.) Speech Coding and Synthesis,
   (New York: Elsevier, 1995). */

/* For each frame, up to par->n_cands cross correlation peaks are
   considered as F0 intervals.  Each is scored according to its within-
   frame properties (relative amplitude, relative location), and
   according to its connectivity with each of the candidates in the
   previous frame.  An unvoiced hypothesis is also generated at each
   frame and is considered in the light of voicing state change cost,
   the quality of the cross correlation peak, and frequency continuity. */

/* At each frame, each candidate has associated with it the following
   items:
	its peak value
	its peak value modified by its within-frame properties
	its location
	the candidate # in the previous frame yielding the min. err.
		(this is the optimum path pointer!)
	its cumulative cost: (local cost + connectivity cost +
		cumulative cost of its best-previous-frame-match). */

/* Dynamic programming is then used to pick the best F0 trajectory and voicing
   state given the local and transition costs for the entire utterance. */

/* To avoid the necessity of computing the full crosscorrelation at
   the input sample rate, the signal is downsampled; a full ccf is
   computed at the lower frequency; interpolation is used to estimate the
   location of the peaks at the higher sample rate; and the fine-grained
   ccf is computed only in the vicinity of these estimated peak
   locations. */


/*
 * READ_SIZE: length of input data frame in sec to read
 * DP_CIRCULAR: determines the initial size of DP circular buffer in sec
 * DP_HIST: stored frame history in second before checking for common path 
 *      DP_CIRCULAR > READ_SIZE, DP_CIRCULAR at least 2 times of DP_HIST 
 * DP_LIMIT: in case no convergence is found, DP frames of DP_LIMIT secs
 *      are kept before output is forced by simply picking the lowest cost
 *      path
 */

#define READ_SIZE 0.2
#define DP_CIRCULAR 1.5
#define DP_HIST 0.5
#define DP_LIMIT 1.0

/* 
 * stationarity parameters -
 * STAT_WSIZE: window size in sec used in measuring frame energy/stationarity
 * STAT_AINT: analysis interval in sec in measuring frame energy/stationarity
 */
#define STAT_WSIZE 0.030
#define STAT_AINT 0.020

/*
 * headF points to current frame in the circular buffer, 
 * tailF points to the frame where tracks start
 * cmpthF points to starting frame of converged path to backtrack
 */

static Frame *headF = NULL, *tailF = NULL, *cmpthF = NULL;

static  int *pcands = NULL;	/* array for backtracking in convergence check */
static int cir_buff_growth_count = 0;

static int size_cir_buffer,	/* # of frames in circular DP buffer */
           size_frame_hist,	/* # of frames required before convergence test */
           size_frame_out,	/* # of frames before forcing output */
           num_active_frames,	/* # of frames from tailF to headF */
           output_buf_size;	/* # of frames allocated to output buffers */

/* 
 * DP parameters
 */
static float tcost, tfact_a, tfact_s, frame_int, vbias, fdouble, wdur, ln2,
             freqwt, lagwt;
static int step, size, nlags, start, stop, ncomp, *locs = NULL;
static short maxpeaks;

static int wReuse = 0;  /* number of windows seen before resued */
static Windstat *windstat = NULL;

static float *f0p = NULL, *vuvp = NULL, *rms_speech = NULL, 
             *acpkp = NULL, *peaks = NULL;
static int first_time = 1, pad;


/*--------------------------------------------------------------------*/
int
get_Nframes(buffsize, pad, step)
    long    buffsize;
    int     pad, step;
{
  if (buffsize < pad)
    return (0);
  else
    return ((buffsize - pad)/step);
}


/*--------------------------------------------------------------------*/
int
init_dp_f0(freq, par, buffsize, sdstep)
    double	freq;
    F0_params	*par;
    long	*buffsize, *sdstep;
{
  int nframes;
  int i;
  int stat_wsize, agap, ind, downpatch;

/*
 * reassigning some constants 
 */

  tcost = par->trans_cost;
  tfact_a = par->trans_amp;
  tfact_s = par->trans_spec;
  vbias = par->voice_bias;
  fdouble = par->double_cost;
  frame_int = par->frame_step;
  
  step = eround(frame_int * freq);
  size = eround(par->wind_dur * freq);
  frame_int = (float)(((float)step)/freq);
  wdur = (float)(((float)size)/freq);
  start = eround(freq / par->max_f0);
  stop = eround(freq / par->min_f0);
  nlags = stop - start + 1;
  ncomp = size + stop + 1; /* # of samples required by xcorr
			      comp. per fr. */
  maxpeaks = 2 + (nlags/2);	/* maximum number of "peaks" findable in ccf */
  ln2 = (float)log(2.0);
  size_frame_hist = (int) (DP_HIST / frame_int);
  size_frame_out = (int) (DP_LIMIT / frame_int);

/*
 * SET UP THE D.P. WEIGHTING FACTORS:
 *      The intent is to make the effectiveness of the various fudge factors
 *      independent of frame rate or sampling frequency.                
 */
  
  /* Lag-dependent weighting factor to emphasize early peaks (higher freqs)*/
  lagwt = par->lag_weight/stop;
  
  /* Penalty for a frequency skip in F0 per frame */
  freqwt = par->freq_weight/frame_int;
  
  i = (int) (READ_SIZE *freq);
  if(ncomp >= step) nframes = ((i-ncomp)/step ) + 1;
  else nframes = i / step;

  /* *buffsize is the number of samples needed to make F0 computation
     of nframes DP frames possible.  The last DP frame is patched with
     enough points so that F0 computation on it can be carried.  F0
     computaion on each frame needs enough points to do

     1) xcross or cross correlation measure:
           enough points to do xcross - ncomp

     2) stationarity measure:
           enough to make 30 msec windowing possible - ind

     3) downsampling:
           enough to make filtering possible -- downpatch
 
     So there are nframes whole DP frames, padded with pad points
     to make the last frame F0 computation ok.

  */

  /* last point in data frame needs points of 1/2 downsampler filter length 
     long, 0.005 is the filter length used in downsampler */
  downpatch = (((int) (freq * 0.005))+1) / 2;

  stat_wsize = (int) (STAT_WSIZE * freq);
  agap = (int) (STAT_AINT * freq);
  ind = ( agap - stat_wsize ) / 2;
  i = stat_wsize + ind;
  pad = downpatch + ((i>ncomp) ? i:ncomp);
  *buffsize = nframes * step + pad;
  *sdstep = nframes * step;
  
  /* Allocate space for the DP storage circularly linked data structure */

  size_cir_buffer = (int) (DP_CIRCULAR / frame_int);

  /* creating circularly linked data structures */
  tailF = alloc_frame(nlags, par->n_cands);
  headF = tailF;

  /* link them up */
  for(i=1; i<size_cir_buffer; i++){
    headF->next = alloc_frame(nlags, par->n_cands);
    headF->next->prev = headF;
    headF = headF->next;
  }
  headF->next = tailF;
  tailF->prev = headF;

  headF = tailF;

  /* Allocate sscratch array to use during backtrack convergence test. */
  if( ! pcands ) {
    pcands = (int *) ckalloc( par->n_cands * sizeof(int));
    /*    spsassert(pcands,"can't allocate pathcands");*/
  }

  /* Allocate arrays to return F0 and related signals. */

  /* Note: remember to compare *vecsize with size_frame_out, because
     size_cir_buffer is not constant */
  output_buf_size = size_cir_buffer;
  rms_speech = (float*)ckalloc(sizeof(float) * output_buf_size);
  /*  spsassert(rms_speech,"rms_speech ckalloc failed");*/
  f0p = (float*)ckalloc(sizeof(float) * output_buf_size);
  /*  spsassert(f0p,"f0p ckalloc failed");*/
  vuvp = (float*)ckalloc(sizeof(float)* output_buf_size);
  /*  spsassert(vuvp,"vuvp ckalloc failed");*/
  acpkp = (float*)ckalloc(sizeof(float) * output_buf_size);
  /*  spsassert(acpkp,"acpkp ckalloc failed");*/

  /* Allocate space for peak location and amplitude scratch arrays. */
  peaks = (float*)ckalloc(sizeof(float) * maxpeaks);
  /*  spsassert(peaks,"peaks ckalloc failed");*/
  locs = (int*)ckalloc(sizeof(int) * maxpeaks);
  /*  spsassert(locs, "locs ckalloc failed");*/
  
  /* Initialise the retrieval/saving scheme of window statistic measures */
  wReuse = agap / step;
  if (wReuse){
      windstat = (Windstat *) ckalloc( wReuse * sizeof(Windstat));
      /*      spsassert(windstat, "windstat ckalloc failed");*/
      for(i=0; i<wReuse; i++){
	  windstat[i].err = 0;
	  windstat[i].rms = 0;
      }
  }

  if(debug_level){
    Fprintf(stderr, "done with initialization:\n");
    Fprintf(stderr,
	    " size_cir_buffer:%d  xcorr frame size:%d start lag:%d nlags:%d\n",
	    size_cir_buffer, size, start, nlags);
  }

  num_active_frames = 0;
  first_time = 1;

  return(0);
}

static Stat *get_stationarity();

/*--------------------------------------------------------------------*/
int
dp_f0(fdata, buff_size, sdstep, freq,
      par, f0p_pt, vuvp_pt, rms_speech_pt, acpkp_pt, vecsize, last_time)
    float	*fdata;
    int		buff_size, sdstep;
    double	freq;
    F0_params	*par;		/* analysis control parameters */
    float	**f0p_pt, **vuvp_pt, **rms_speech_pt, **acpkp_pt;
    int		*vecsize, last_time;
{
  float  maxval, engref, *sta, *rms_ratio, *dsdata, *downsample();
  register float ttemp, ftemp, ft1, ferr, err, errmin;
  register int  i, j, k, loc1, loc2;
  int   nframes, maxloc, ncand, ncandp, minloc,
        decimate, samsds;

  Stat *stat = NULL;

  nframes = get_Nframes((long) buff_size, pad, step); /* # of whole frames */

  if(debug_level)
    Fprintf(stderr,
	    "******* Computing %d dp frames ******** from %d points\n", nframes, buff_size);

  /* Now downsample the signal for coarse peak estimates. */

  decimate = (int)(freq/2000.0);    /* downsample to about 2kHz */
  if (decimate <= 1)
    dsdata = fdata;
  else {
    samsds = ((nframes-1) * step + ncomp) / decimate;
    dsdata = downsample(fdata, buff_size, sdstep, freq, &samsds, decimate, 
			first_time, last_time);
    if (!dsdata) {
      Fprintf(stderr, "can't get downsampled data.\n");
      return 1;
    }
  }

  /* Get a function of the "stationarity" of the speech signal. */

  stat = get_stationarity(fdata, freq, buff_size, nframes, step, first_time);
  if (!stat) { 
    Fprintf(stderr, "can't get stationarity\n");
    return(1);
  }
  sta = stat->stat;
  rms_ratio = stat->rms_ratio;

  /***********************************************************************/
  /* MAIN FUNDAMENTAL FREQUENCY ESTIMATION LOOP */
  /***********************************************************************/
  if(!first_time && nframes > 0) headF = headF->next;

  for(i = 0; i < nframes; i++) {
 
    /* NOTE: This buffer growth provision is probably not necessary.
       It was put in (with errors) by Derek Lin and apparently never
       tested.  My tests and analysis suggest it is completely
       superfluous. DT 9/5/96 */
    /* Dynamically allocating more space for the circular buffer */
    if(headF == tailF->prev){
      Frame *frm;

      if(cir_buff_growth_count > 5){
	Fprintf(stderr,
		"too many requests (%d) for dynamically allocating space.\n   There may be a problem in finding converged path.\n",cir_buff_growth_count);
	return(1);
      }
      if(debug_level) 
	Fprintf(stderr, "allocating %d more frames for DP circ. buffer.\n", size_cir_buffer);
      frm = alloc_frame(nlags, par->n_cands);
      headF->next = frm;
      frm->prev = headF;
      for(k=1; k<size_cir_buffer; k++){
	frm->next = alloc_frame(nlags, par->n_cands);
	frm->next->prev = frm;
	frm = frm->next;
      }
      frm->next = tailF;
      tailF->prev = frm;
      cir_buff_growth_count++;
    }

    headF->rms = stat->rms[i];
    get_fast_cands(fdata, dsdata, i, step, size, decimate, start,
		   nlags, &engref, &maxloc,
		   &maxval, headF->cp, peaks, locs, &ncand, par);
    
    /*    Move the peak value and location arrays into the dp structure */
    {
      register float *ftp1, *ftp2;
      register short *sp1;
      register int *sp2;
      
      for(ftp1 = headF->dp->pvals, ftp2 = peaks,
	  sp1 = headF->dp->locs, sp2 = locs, j=ncand; j--; ) {
	*ftp1++ = *ftp2++;
	*sp1++ = *sp2++;
      }
      *sp1 = -1;		/* distinguish the UNVOICED candidate */
      *ftp1 = maxval;
      headF->dp->mpvals[ncand] = vbias+maxval; /* (high cost if cor. is high)*/
    }

    /* Apply a lag-dependent weight to the peaks to encourage the selection
       of the first major peak.  Translate the modified peak values into
       costs (high peak ==> low cost). */
    for(j=0; j < ncand; j++){
      ftemp = 1.0f - ((float)locs[j] * lagwt);
      headF->dp->mpvals[j] = 1.0f - (peaks[j] * ftemp);
    }
    ncand++;			/* include the unvoiced candidate */
    headF->dp->ncands = ncand;

    /*********************************************************************/
    /*    COMPUTE THE DISTANCE MEASURES AND ACCUMULATE THE COSTS.       */
    /*********************************************************************/

    ncandp = headF->prev->dp->ncands;
    for(k=0; k<ncand; k++){	/* for each of the current candidates... */
      minloc = 0;
      errmin = FLT_MAX;
      if((loc2 = headF->dp->locs[k]) > 0) { /* current cand. is voiced */
	for(j=0; j<ncandp; j++){ /* for each PREVIOUS candidate... */
	  /*    Get cost due to inter-frame period change. */
	  loc1 = headF->prev->dp->locs[j];
	  if (loc1 > 0) { /* prev. was voiced */
	    ftemp = (float) log(((double) loc2) / loc1);
	    ttemp = (float) fabs(ftemp);
	    ft1 = (float) (fdouble + fabs(ftemp + ln2));
	    if (ttemp > ft1)
	      ttemp = ft1;
	    ft1 = (float) (fdouble + fabs(ftemp - ln2));
	    if (ttemp > ft1)
	      ttemp = ft1;
	    ferr = ttemp * freqwt;
	  } else {		/* prev. was unvoiced */
	    ferr = tcost + (tfact_s * sta[i]) + (tfact_a / rms_ratio[i]);
	  }
	  /*    Add in cumulative cost associated with previous peak. */
	  err = ferr + headF->prev->dp->dpvals[j];
	  if(err < errmin){	/* find min. cost */
	    errmin = err;
	    minloc = j;
	  }
	}
      } else {			/* this is the unvoiced candidate */
	for(j=0; j<ncandp; j++){ /* for each PREVIOUS candidate... */
	  
	  /*    Get voicing transition cost. */
	  if (headF->prev->dp->locs[j] > 0) { /* previous was voiced */
	    ferr = tcost + (tfact_s * sta[i]) + (tfact_a * rms_ratio[i]);
	  }
	  else
	    ferr = 0.0;
	  /*    Add in cumulative cost associated with previous peak. */
	  err = ferr + headF->prev->dp->dpvals[j];
	  if(err < errmin){	/* find min. cost */
	    errmin = err;
	    minloc = j;
	  }
	}
      }
      /* Now have found the best path from this cand. to prev. frame */
      if (first_time && i==0) {		/* this is the first frame */
	headF->dp->dpvals[k] = headF->dp->mpvals[k];
	headF->dp->prept[k] = 0;
      } else {
	headF->dp->dpvals[k] = errmin + headF->dp->mpvals[k];
	headF->dp->prept[k] = minloc;
      }
    } /*    END OF THIS DP FRAME */

    if (i < nframes - 1)
      headF = headF->next;
    
    if (debug_level >= 2) {
      Fprintf(stderr,"%d engref:%10.0f max:%7.5f loc:%4d\n",
	      i,engref,maxval,maxloc);
    }
    
  } /* end for (i ...) */

  /***************************************************************/
  /* DONE WITH FILLING DP STRUCTURES FOR THE SET OF SAMPLED DATA */
  /*    NOW FIND A CONVERGED DP PATH                             */
  /***************************************************************/

  *vecsize = 0;			/* # of output frames returned */

  num_active_frames += nframes;

  if( num_active_frames >= size_frame_hist  || last_time ){
    Frame *frm;
    int  num_paths, best_cand, frmcnt, checkpath_done = 1;
    float patherrmin;
      
    if(debug_level)
      Fprintf(stderr, "available frames for backtracking: %d\n",
num_active_frames);
      
    patherrmin = FLT_MAX;
    best_cand = 0;
    num_paths = headF->dp->ncands;

    /* Get the best candidate for the final frame and initialize the
       paths' backpointers. */
    frm = headF;
    for(k=0; k < num_paths; k++) {
      if (patherrmin > headF->dp->dpvals[k]){
	patherrmin = headF->dp->dpvals[k];
	best_cand = k;	/* index indicating the best candidate at a path */
      }
      pcands[k] = frm->dp->prept[k];
    }

    if(last_time){     /* Input data was exhausted. force final outputs. */
      cmpthF = headF;		/* Use the current frame as starting point. */
    } else {
      /* Starting from the most recent frame, trace back each candidate's
	 best path until reaching a common candidate at some past frame. */
      frmcnt = 0;
      while (1) {
	frm = frm->prev;
	frmcnt++;
	checkpath_done = 1;
	for(k=1; k < num_paths; k++){ /* Check for convergence. */
	  if(pcands[0] != pcands[k])
	    checkpath_done = 0;
	}
	if( ! checkpath_done) { /* Prepare for checking at prev. frame. */
	  for(k=0; k < num_paths; k++){
	    pcands[k] = frm->dp->prept[pcands[k]];
	  }
	} else {	/* All paths have converged. */
	  cmpthF = frm;
	  best_cand = pcands[0];
	  if(debug_level)
	    Fprintf(stderr,
		    "paths went back %d frames before converging\n",frmcnt);
	  break;
	}
	if(frm == tailF){	/* Used all available data? */
	  if( num_active_frames < size_frame_out) { /* Delay some more? */
	    checkpath_done = 0; /* Yes, don't backtrack at this time. */
	    cmpthF = NULL;
	  } else {		/* No more delay! Force best-guess output. */
	    checkpath_done = 1;
	    cmpthF = headF;
	    /*	    Fprintf(stderr,
		    "WARNING: no converging path found after going back %d frames, will use the lowest cost path\n",num_active_frames);*/
	  }
	  break;
	} /* end if (frm ...) */
      }	/* end while (1) */
    } /* end if (last_time) ... else */

    /*************************************************************/
    /* BACKTRACKING FROM cmpthF (best_cand) ALL THE WAY TO tailF    */
    /*************************************************************/
    i = 0;
    frm = cmpthF;	/* Start where convergence was found (or faked). */
    while( frm != tailF->prev && checkpath_done){
      if( i == output_buf_size ){ /* Need more room for outputs? */
	output_buf_size *= 2;
	if(debug_level)
	  Fprintf(stderr,
		  "reallocating space for output frames: %d\n",
		  output_buf_size);
	rms_speech = (float *) ckrealloc((void *) rms_speech,
				       sizeof(float) * output_buf_size);
	/*	spsassert(rms_speech, "rms_speech realloc failed in dp_f0()");*/
	f0p = (float *) ckrealloc((void *) f0p,
				sizeof(float) * output_buf_size);
	/*	spsassert(f0p, "f0p realloc failed in dp_f0()");*/
	vuvp = (float *) ckrealloc((void *) vuvp, sizeof(float) * output_buf_size);
	/*	spsassert(vuvp, "vuvp realloc failed in dp_f0()");*/
	acpkp = (float *) ckrealloc((void *) acpkp, sizeof(float) * output_buf_size);
	/*	spsassert(acpkp, "acpkp realloc failed in dp_f0()");*/
      }
      rms_speech[i] = frm->rms;
      acpkp[i] =  frm->dp->pvals[best_cand];
      loc1 = frm->dp->locs[best_cand];
      vuvp[i] = 1.0;
      best_cand = frm->dp->prept[best_cand];
      ftemp = (float) loc1;
      if(loc1 > 0) {		/* Was f0 actually estimated for this frame? */
	if (loc1 > start && loc1 < stop) { /* loc1 must be a local maximum. */
	  float cormax, cprev, cnext, den;
		  
	  j = loc1 - start;
	  cormax = frm->cp->correl[j];
	  cprev = frm->cp->correl[j+1];
	  cnext = frm->cp->correl[j-1];
	  den = (float) (2.0 * ( cprev + cnext - (2.0 * cormax) ));
	  /*
	   * Only parabolic interpolate if cormax is indeed a local 
	   * turning point. Find peak of curve that goes though the 3 points
	   */
		  
	  if (fabs(den) > 0.000001)
	    ftemp += 2.0f - ((((5.0f*cprev)+(3.0f*cnext)-(8.0f*cormax))/den));
	}
	f0p[i] = (float) (freq/ftemp);
      } else {		/* No valid estimate; just fake some arbitrary F0. */
	f0p[i] = 0;
	vuvp[i] = 0.0;
      }
      frm = frm->prev;
	  
      if (debug_level >= 2)
	Fprintf(stderr," i:%4d%8.1f%8.1f\n",i,f0p[i],vuvp[i]);
      /* f0p[i] starts from the most recent one */ 
      /* Need to reverse the order in the calling function */
      i++;
    } /* end while() */
    if (checkpath_done){
      *vecsize = i;
      tailF = cmpthF->next;
      num_active_frames -= *vecsize;
    }
  } /* end if() */

  if (debug_level)
    Fprintf(stderr, "writing out %d frames.\n", *vecsize);
  
  *f0p_pt = f0p;
  *vuvp_pt = vuvp;
  *acpkp_pt = acpkp;
  *rms_speech_pt = rms_speech;
  /*  *acpkp_pt = acpkp;*/
  
  if(first_time) first_time = 0;
  return(0);
}


/*--------------------------------------------------------------------*/
Frame *
alloc_frame(nlags, ncands)
    int     nlags, ncands;
{
  Frame *frm;
  int j;

  frm = (Frame*)ckalloc(sizeof(Frame));
  frm->dp = (Dprec *) ckalloc(sizeof(Dprec));
  /*  spsassert(frm->dp,"frm->dp ckalloc failed in alloc_frame");*/
  frm->dp->ncands = 0;
  frm->cp = (Cross *) ckalloc(sizeof(Cross));
  /*  spsassert(frm->cp,"frm->cp ckalloc failed in alloc_frame");*/
  frm->cp->correl = (float *) ckalloc(sizeof(float) * nlags);
  /*  spsassert(frm->cp->correl, "frm->cp->correl ckalloc failed");*/
  /* Allocate space for candidates and working arrays. */
  frm->dp->locs = (short*)ckalloc(sizeof(short) * ncands);
  /*  spsassert(frm->dp->locs,"frm->dp->locs ckalloc failed in alloc_frame()");*/
  frm->dp->pvals = (float*)ckalloc(sizeof(float) * ncands);
/*  spsassert(frm->dp->pvals,"frm->dp->pvals ckalloc failed in alloc_frame()");*/
  frm->dp->mpvals = (float*)ckalloc(sizeof(float) * ncands);
  /*  spsassert(frm->dp->mpvals,"frm->dp->mpvals ckalloc failed in alloc_frame()");*/
  frm->dp->prept = (short*)ckalloc(sizeof(short) * ncands);
  /*  spsassert(frm->dp->prept,"frm->dp->prept ckalloc failed in alloc_frame()");*/
  frm->dp->dpvals = (float*)ckalloc(sizeof(float) * ncands);
  /*  spsassert(frm->dp->dpvals,"frm->dp->dpvals ckalloc failed in alloc_frame()");*/
    
  /*  Initialize the cumulative DP costs to zero */
  for(j = ncands-1; j >= 0; j--)
    frm->dp->dpvals[j] = 0.0;

  return(frm);
}


/*--------------------------------------------------------------------*/
/* push window stat to stack, and pop the oldest one */

static int
save_windstat(rho, order, err, rms)
    float   *rho;
    int     order;
    float   err;
    float   rms;
{
    int i,j;

    if(wReuse > 1){               /* push down the stack */
	for(j=1; j<wReuse; j++){
	    for(i=0;i<=order; i++) windstat[j-1].rho[i] = windstat[j].rho[i];
	    windstat[j-1].err = windstat[j].err;
	    windstat[j-1].rms = windstat[j].rms;
	}
	for(i=0;i<=order; i++) windstat[wReuse-1].rho[i] = rho[i]; /*save*/
	windstat[wReuse-1].err = (float) err;
	windstat[wReuse-1].rms = (float) rms;
	return 1;
    } else if (wReuse == 1) {
	for(i=0;i<=order; i++) windstat[0].rho[i] = rho[i];  /* save */
	windstat[0].err = (float) err;
	windstat[0].rms = (float) rms;
	return 1;
    } else 
	return 0;
}


/*--------------------------------------------------------------------*/
static int
retrieve_windstat(rho, order, err, rms)
    float   *rho;
    int     order;
    float   *err;
    float   *rms;
{
    Windstat wstat;
    int i;
	
    if(wReuse){
	wstat = windstat[0];
	for(i=0; i<=order; i++) rho[i] = wstat.rho[i];
	*err = wstat.err;
	*rms = wstat.rms;
	return 1;
    }
    else return 0;
}


/*--------------------------------------------------------------------*/
static float
get_similarity(order, size, pdata, cdata,
	       rmsa, rms_ratio, pre, stab, w_type, init)
    int     order, size;
    float   *pdata, *cdata;
    float   *rmsa, *rms_ratio, pre, stab;
    int     w_type, init;
{
  float rho3[BIGSORD+1], err3, rms3, rmsd3, b0, t, a2[BIGSORD+1], 
      rho1[BIGSORD+1], a1[BIGSORD+1], b[BIGSORD+1], err1, rms1, rmsd1;
  float xitakura(), wind_energy();
  void xa_to_aca ();
  int xlpc();

/* (In the lpc() calls below, size-1 is used, since the windowing and
   preemphasis function assumes an extra point is available in the
   input data array.  This condition is apparently no longer met after
   Derek's modifications.) */

  /* get current window stat */
  xlpc(order, stab, size-1, cdata,
      a2, rho3, (float *) NULL, &err3, &rmsd3, pre, w_type);
  rms3 = wind_energy(cdata, size, w_type);
  
  if(!init) {
      /* get previous window stat */
      if( !retrieve_windstat(rho1, order, &err1, &rms1)){
	  xlpc(order, stab, size-1, pdata,
	      a1, rho1, (float *) NULL, &err1, &rmsd1, pre, w_type);
	  rms1 = wind_energy(pdata, size, w_type);
      }
      xa_to_aca(a2+1,b,&b0,order);
      t = xitakura(order,b,&b0,rho1+1,&err1) - .8f;
      if(rms1 > 0.0)
	  *rms_ratio = (0.001f + rms3)/rms1;
      else
	  if(rms3 > 0.0)
	      *rms_ratio = 2.0;	/* indicate some energy increase */
	  else
	      *rms_ratio = 1.0;	/* no change */
  } else {
      *rms_ratio = 1.0;
      t = 10.0;
  }
  *rmsa = rms3;
  save_windstat( rho3, order, err3, rms3);
  return((float)(0.2/t));
}


/* -------------------------------------------------------------------- */
/* This is an ad hoc signal stationarity function based on Itakura
 * distance and relative amplitudes.
 */
/* 
  This illustrates the window locations when the very first frame is read.
  It shows an example where each frame step |  .  | is 10 msec.  The
  frame step size is variable.  The window size is always 30 msec.
  The window centers '*' is always 20 msec apart.
  The windows cross each other right at the center of the DP frame, or
  where the '.' is.

                          ---------*---------   current window

              ---------*---------  previous window

  |  .  |  .  |  .  |  .  |  .  |  .  |  .  |  .  |  .  |
              ^           ^  ^
              ^           ^  ^
              ^           ^  fdata
              ^           ^
              ^           q
	      p

                          ---
                          ind

  fdata, q, p, ind, are variables used below.
   
*/

static Stat *stat = NULL;
static float *mem = NULL;

static Stat*
get_stationarity(fdata, freq, buff_size, nframes, frame_step, first_time)
    float   *fdata;
    double  freq;
    int     buff_size, nframes, frame_step, first_time;
{
  static int nframes_old = 0, memsize;
  float preemp = 0.4f, stab = 30.0f;
  float *p, *q, *r, *datend;
  int ind, i, j, m, size, order, agap, w_type = 3;

  agap = (int) (STAT_AINT *freq);
  size = (int) (STAT_WSIZE * freq);
  ind = (agap - size) / 2;

  if( nframes_old < nframes || !stat || first_time){
    /* move this to init_dp_f0() later */
    nframes_old = nframes;
    if(stat){
      ckfree((char *) stat->stat);
      ckfree((char *) stat->rms);
      ckfree((char *) stat->rms_ratio);
      ckfree((char *) stat);
    }
    if (mem) ckfree((void *)mem); 
    stat = (Stat *) ckalloc(sizeof(Stat));
    /*    spsassert(stat,"stat ckalloc failed in get_stationarity");*/
    stat->stat = (float*)ckalloc(sizeof(float)*nframes);
    /*    spsassert(stat->stat,"stat->stat ckalloc failed in get_stationarity");*/
    stat->rms = (float*)ckalloc(sizeof(float)*nframes);
    /*    spsassert(stat->rms,"stat->rms ckalloc failed in get_stationarity");*/
    stat->rms_ratio = (float*)ckalloc(sizeof(float)*nframes);
    /*    spsassert(stat->rms_ratio,"stat->ratio ckalloc failed in get_stationarity");*/
    memsize = (int) (STAT_WSIZE * freq) + (int) (STAT_AINT * freq);
    mem = (float *) ckalloc( sizeof(float) * memsize);
    /*    spsassert(mem, "mem ckalloc failed in get_stationarity()");*/
    for(j=0; j<memsize; j++) mem[j] = 0;
  }
  
  if(nframes == 0) return(stat);

  q = fdata + ind;
  datend = fdata + buff_size;

  if((order = (int) (2.0 + (freq/1000.0))) > BIGSORD) {
    Fprintf(stderr,
	    "Optimim order (%d) exceeds that allowable (%d); reduce Fs\n",order, BIGSORD);
    order = BIGSORD;
  }

  /* prepare for the first frame */
  for(j=memsize/2, i=0; j<memsize; j++, i++) mem[j] = fdata[i];

  /* never run over end of frame, should already taken care of when read */

  for(j=0, p = q - agap; j < nframes; j++, p += frame_step, q += frame_step){
      if( (p >= fdata) && (q >= fdata) && ( q + size <= datend) )
	  stat->stat[j] = get_similarity(order,size, p, q, 
					     &(stat->rms[j]),
					     &(stat->rms_ratio[j]),preemp,
					     stab,w_type, 0);
      else {
	  if(first_time) {
	      if( (p < fdata) && (q >= fdata) && (q+size <=datend) )
		  stat->stat[j] = get_similarity(order,size, NULL, q,
						     &(stat->rms[j]),
						     &(stat->rms_ratio[j]),
						     preemp,stab,w_type, 1);
	      else{
		  stat->rms[j] = 0.0;
		  stat->stat[j] = 0.01f * 0.2f;   /* a big transition */
		  stat->rms_ratio[j] = 1.0;   /* no amplitude change */
	      }
	  } else {
	      if( (p<fdata) && (q+size <=datend) ){
		  stat->stat[j] = get_similarity(order,size, mem, 
						     mem + (memsize/2) + ind,
						     &(stat->rms[j]),
						     &(stat->rms_ratio[j]),
						     preemp, stab,w_type, 0);
		  /* prepare for the next frame_step if needed */
		  if(p + frame_step < fdata ){
		      for( m=0; m<(memsize-frame_step); m++) 
			  mem[m] = mem[m+frame_step];
		      r = q + size;
		      for( m=0; m<frame_step; m++) 
			  mem[memsize-frame_step+m] = *r++;
		  }
	      }
	  }
      }
  }

  /* last frame, prepare for next call */
  for(j=(memsize/2)-1, p=fdata + (nframes * frame_step)-1; j>=0 && p>=fdata; j--)
    mem[j] = *p--;
  return(stat);
}


/* -------------------------------------------------------------------- */
/*	Round the argument to the nearest integer.			*/
/*
int
eround(flnum)
    double  flnum;
{
  return((flnum >= 0.0) ? (int)(flnum + 0.5) : (int)(flnum - 0.5));
}

*/
void free_dp_f0()
{
  int i;
  Frame *frm, *next;
  
  ckfree((void *)pcands);
  pcands = NULL;
  
  ckfree((void *)rms_speech);
  rms_speech = NULL;
  
  ckfree((void *)f0p);
  f0p = NULL;
  
  ckfree((void *)vuvp);
  vuvp = NULL;
  
  ckfree((void *)acpkp);
  acpkp = NULL;
  
  ckfree((void *)peaks);
  peaks = NULL;
  
  ckfree((void *)locs);
  locs = NULL;
  
  if (wReuse) {
    ckfree((void *)windstat);
    windstat = NULL;
  }
  
  frm = headF;
  
  for(i = 0; i < size_cir_buffer; i++) {
    next = frm->next;
    ckfree((void *)frm->cp->correl);
    ckfree((void *)frm->dp->locs);
    ckfree((void *)frm->dp->pvals);
    ckfree((void *)frm->dp->mpvals);
    ckfree((void *)frm->dp->prept);
    ckfree((void *)frm->dp->dpvals);
    ckfree((void *)frm->cp);
    ckfree((void *)frm->dp);
    ckfree((void *)frm);
    frm = next;
  }
  headF = NULL;
  tailF = NULL;
  
  ckfree((void *)stat->stat);
  ckfree((void *)stat->rms);
  ckfree((void *)stat->rms_ratio);

  ckfree((void *)stat);
  stat = NULL;

  ckfree((void *)mem);
  mem = NULL;
}

int
cGet_f0(Sound *sound, Tcl_Interp *interp, float **outlist, int *length)
{
  float *fdata;
  int done;
  long buff_size, actsize;
  double sf, start_time;
  F0_params *par, *read_f0_params();
  float *f0p, *vuvp, *rms_speech, *acpkp;
  int i, vecsize;
  int init_dp_f0(), dp_f0();
  static int framestep = -1;
  long sdstep = 0, total_samps;
  int ndone = 0;
  Tcl_Obj *list;
  float *tmp = (float *)ckalloc(sizeof(float) * (5 + sound->length / 80));
  int count = 0;
  int startpos = 0, endpos = -1;

  if (sound->cmdPtr != NULL) {
    Tcl_DecrRefCount(sound->cmdPtr);
    sound->cmdPtr = NULL;
  }

  par = (F0_params *) ckalloc(sizeof(F0_params));
  par->cand_thresh = 0.3f;
  par->lag_weight = 0.3f;
  par->freq_weight = 0.02f;
  par->trans_cost = 0.005f;
  par->trans_amp = 0.5f;
  par->trans_spec = 0.5f;
  par->voice_bias = 0.0f;
  par->double_cost = 0.35f;
  par->min_f0 = 50;
  par->max_f0 = 550;
  par->frame_step = 0.01f;
  par->wind_dur = 0.0075f;
  par->n_cands = 20;
  par->mean_f0 = 200;          /* unused */
  par->mean_f0_weight = 0.0f;  /* unused */
  par->conditioning = 0;       /* unused */

  if (startpos < 0) startpos = 0;
  if (endpos >= (sound->length - 1) || endpos == -1)
    endpos = sound->length - 1;
  if (startpos > endpos) return TCL_OK;

  sf = (double) sound->samprate;

  if (framestep > 0)  /* If a value was specified with -S, use it. */
    par->frame_step = (float) (framestep / sf);
  start_time = 0.0f;
  if(check_f0_params(interp, par, sf)){
    Tcl_AppendResult(interp, "invalid/inconsistent parameters -- exiting.", NULL);
    return TCL_ERROR;
  }

  total_samps = endpos - startpos + 1;
  if(total_samps < ((par->frame_step * 2.0) + par->wind_dur) * sf) {
    Tcl_AppendResult(interp, "input range too small for analysis by get_f0.", NULL);
    return TCL_ERROR;
  }
  /* Initialize variables in get_f0.c; allocate data structures;
   * determine length and overlap of input frames to read.
   */
  if (init_dp_f0(sf, par, &buff_size, &sdstep)
      || buff_size > INT_MAX || sdstep > INT_MAX)
  {
    Tcl_AppendResult(interp, "problem in init_dp_f0().", NULL);
    return TCL_ERROR;
  }

  if (debug_level)
    Fprintf(stderr, "init_dp_f0 returned buff_size %ld, sdstep %ld.\n",buff_size, sdstep);

  if (buff_size > total_samps)
    buff_size = total_samps;

  actsize = min(buff_size, sound->length);
  fdata = (float *) ckalloc(sizeof(float) * max(buff_size, sdstep));
  list = Tcl_NewListObj(0, NULL);
  /*  Snack_ProgressCallback(sound->cmdPtr, interp, "Computing pitch", 0.0);*/
  ndone = startpos;

  while (TRUE) {
    done = (actsize < buff_size) || (total_samps == buff_size);
    Snack_GetSoundData(sound, ndone, fdata, actsize);
    /*if (sound->debug > 0) Snack_WriteLog("dp_f0...\n");*/
    if (dp_f0(fdata, (int) actsize, (int) sdstep, sf, par,
	      &f0p, &vuvp, &rms_speech, &acpkp, &vecsize, done)) {
      Tcl_AppendResult(interp, "problem in dp_f0().", NULL);
      return TCL_ERROR;
    }
    /*if (sound->debug > 0) Snack_WriteLogInt("done dp_f0",vecsize);*/
    for (i = vecsize - 1; i >= 0; i--) {
      tmp[count] = f0p[i];
      count++;
    }

    if (done) break;

    ndone += sdstep; 
    actsize = min(buff_size, sound->length - ndone);
    total_samps -= sdstep;

    if (actsize > total_samps)
      actsize = total_samps;

    /*    if (1) {
      int res = Snack_ProgressCallback(sound->cmdPtr, interp, "Computing pitch", (double) ndone / sound->length);
      if (res != TCL_OK) {
	return TCL_ERROR;
      }
      }*/
  }

  /*Snack_ProgressCallback(sound->cmdPtr, interp, "Computing pitch", 1.0);*/

  ckfree((void *)fdata);

  ckfree((void *)par);

  free_dp_f0();

  *outlist = tmp;
  *length = count;  
  /*Tcl_SetObjResult(interp, list);*/

  return TCL_OK;
}
