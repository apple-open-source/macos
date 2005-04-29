/* 
 * Copyright (C) 2001-2002 Jonas Beskow <beskow@speech.kth.se>
 *
 * This file is part of the Snack Sound Toolkit.
 * The latest version can be found at http://www.speech.kth.se/snack/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "snack.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(WIN) || defined(MAC)
#  define M_PI 3.14159265358979323846
#endif

/* linear interpolation macro */
#define LIN(X,FRAC) (X[0]+(X[1]-X[0])*FRAC)

struct formantFilter {
  configProc *configProc;
  startProc  *startProc;
  flowProc   *flowProc;
  freeProc   *freeProc;
  Tcl_Interp *interp;
  Snack_Filter prev, next;
  Snack_StreamInfo si;
  double dataRatio;
  int    reserved[4];
  /* formant filter params */
  double bw;
  double freq;
  double a0,b0,c0; /* saved coeffs */
  float mem[2];    /* last two samples */
} formantFilter;

typedef struct formantFilter *formantFilter_t;

static int formantConfigProc(Snack_Filter f, Tcl_Interp *interp, int objc,
			     Tcl_Obj *CONST objv[]) {
  formantFilter_t mf = (formantFilter_t) f;
  /*  fprintf(stderr,"formantConfigProc\n");*/

  switch (objc) {
  case 1:    
    if (Tcl_GetDoubleFromObj(interp, objv[0], &(mf->freq)) != TCL_OK) {
      return TCL_ERROR;
    }
    break;
  case 2:
    if (Tcl_GetDoubleFromObj(interp, objv[0], &(mf->freq)) != TCL_OK) {
      return TCL_ERROR;
    }
    if (Tcl_GetDoubleFromObj(interp, objv[1], &(mf->bw)) != TCL_OK) {
      return TCL_ERROR;
    }
    break;
  default:
    Tcl_SetResult(interp,"wrong # args. should be \"filter configure freq ?bandwidth?\"",TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

static void calcCoeffs(double f,double bw, double *a, double *b, double *c) {
  double tmp = exp(-M_PI*bw);
  *c = -tmp*tmp;
  *b = 2.0*tmp*cos(2*M_PI*f);
  *a = 1.0 - *b - *c;
}

static Snack_Filter 
formantCreateProc(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  formantFilter_t mf;
  /*  fprintf(stderr,"formantCreateProc\n");*/
  
  mf = (formantFilter_t) ckalloc(sizeof(formantFilter));

  mf->freq = 0;
  mf->bw = 1;

  if (formantConfigProc((Snack_Filter) mf, interp, objc, objv) != TCL_OK) {
    return (Snack_Filter) NULL;
  }
  return (Snack_Filter) mf;
}

static int
formantStartProc(Snack_Filter f, Snack_StreamInfo si)
{
  formantFilter_t mf = (formantFilter_t) f;

  /* only mono sounds... */
  if (si->outWidth != 1) return TCL_ERROR;
 
  calcCoeffs(1.0*mf->freq/si->rate,
	     1.0*mf->bw/si->rate,
	     &mf->a0,&mf->b0,&mf->c0);
  mf->mem[0] = 0.0;
  mf->mem[1] = 0.0;
  /*  fprintf(stderr,"formantStartProc, width = %d\n",si->outWidth);*/

  return TCL_OK;
}

static int
formantFlowProc(Snack_Filter f, Snack_StreamInfo si, float *in, float *out,
		int *inFrames, int *outFrames) {
  int i,nf = 0;
  double a[2],b[2],c[2],frac,z;
  formantFilter_t mf = (formantFilter_t) f;
  
  /*fprintf(stderr,"entering   formantFlowProc, *inFrames = %d\t*outFrames = %d, in = %p, out = %p\n",*inFrames,*outFrames,in,out); */
  
  calcCoeffs(1.0*mf->freq/si->rate,
	     1.0*mf->bw/si->rate,
	     &a[1],&b[1],&c[1]);
  a[0]=mf->a0;b[0]=mf->b0;c[0]=mf->c0;
  /* only mono sounds... */
  if (si->outWidth != 1) {
    *outFrames = 0;
    *inFrames = 0;
    return TCL_ERROR;
  }
  
  nf = (*inFrames)<(*outFrames)?(*inFrames):(*outFrames);
  if (nf != 0) {
    z = 1.0/nf;
    if (nf>=1) {
      out[0] = (float)(LIN(a,0*z)*in[0]+LIN(b,0*z)*mf->mem[0]+LIN(c,0*z)*mf->mem[1]);
    }
    if (nf>=2) {
      out[1] = (float)(LIN(a,1*z)*in[1]+LIN(b,1*z)*out[0]+LIN(c,1*z)*mf->mem[0]);
    }
    for (i=2;i<nf;i++) {
      frac = i*z;
      out[i] = (float)(LIN(a,frac)*in[i] + LIN(b,frac)*out[i-1] + LIN(c,frac)*out[i-2]);
    }
  }
  if (nf>=1) mf->mem[0]=out[nf-1];
  if (nf>=2) mf->mem[1]=out[nf-2];
  mf->a0=a[1];
  mf->b0=b[1];
  mf->c0=c[1];
  
  *outFrames = nf;
  *inFrames = nf;
  /*fprintf(stderr,"leaving    formantFlowProc, *inFrames = %d\t*outFrames = %d\n",*inFrames,*outFrames); */
  return TCL_OK;
}

static void
formantFreeProc(Snack_Filter f) {
  ckfree((char*)f);
}

static Snack_FilterType snackFormantType = {
  "formant",
  formantCreateProc,
  formantConfigProc,
  formantStartProc,
  formantFlowProc,
  formantFreeProc,
  (Snack_FilterType *) NULL
};


#define RECTANGLE 1
#define TRIANGLE 2
#define SINE 3
#define NOISE 4
#define SAMPLED 5


#define MAX_SAMPLES 1600

struct generatorFilter {
  /* general filter stuff */
  configProc *configProc;
  startProc  *startProc;
  flowProc   *flowProc;
  freeProc   *freeProc;
  Tcl_Interp *interp;
  Snack_Filter prev, next;
  Snack_StreamInfo si;
  double dataRatio;
  int    reserved[4];
  /* generator filter stuff */
  double freq[2];
  double ampl[2];
  double shape[2];     /* pulse width or peak */
  int type;            /* TRIANGLE, RECTANGLE, SINE, NOISE or SAMPLED*/
  double _phase;
  float samples[MAX_SAMPLES];
  float maxval;
  int nSamples;
  int ntot;
  int ngen;
} generatorFilter;

typedef struct generatorFilter *generatorFilter_t;



static int generatorConfigProc(Snack_Filter f, Tcl_Interp *interp, int objc,
			     Tcl_Obj *CONST objv[]) {
  generatorFilter_t mf = (generatorFilter_t) f;
  char *typestr;

  /*  fprintf(stderr,"generatorConfigProc, objc = %d\n",objc);*/

  /*
    syntax: generator configure freq ?ampl? ?shape? ?type? ?ntot?
  */
  switch (objc) {
  case 5:
    if (Tcl_GetIntFromObj(interp,objv[4],&mf->ntot)==TCL_ERROR) {
      return TCL_ERROR;
    }
    
  case 4:
    typestr = Tcl_GetStringFromObj(objv[3], NULL);
    if (strncmp(typestr,"rec",3)==0) {
      mf->type = RECTANGLE;
    } else if (strncmp(typestr,"tri",3)==0) {
      mf->type = TRIANGLE;
    } else if (strncmp(typestr,"sin",3)==0) {
      mf->type = SINE;
    } else if (strncmp(typestr,"noi",3)==0) {
      mf->type = NOISE;
    } else if (strncmp(typestr,"sam",3)==0) {
      mf->type = SAMPLED;
    } else {
      Tcl_SetResult(interp,
		    "bad waveform type, must be rectangle, triangle, sine, noise or sampled",
		    TCL_STATIC);
      return TCL_ERROR;
    }
    
  case 3:
    if (Tcl_GetDoubleFromObj(interp,objv[2],&mf->shape[1])==TCL_ERROR) {
      return TCL_ERROR;
    }
    
  case 2:
    if (Tcl_GetDoubleFromObj(interp,objv[1],&mf->ampl[1])==TCL_ERROR) {
      return TCL_ERROR;
    }
    
  case 1:
    if (Tcl_GetDoubleFromObj(interp,objv[0],&mf->freq[1])==TCL_ERROR) {
      return TCL_ERROR;
    }
    break;
    
  default:
    Tcl_SetResult(interp, "wrong # args, should be \"generator configure freq ?ampl? ?shape? ?type?\"", TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

static Snack_Filter 
generatorCreateProc(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  generatorFilter_t mf;
  /*  fprintf(stderr,"generatorCreateProc, objc = %d\n",objc);*/
  mf = (generatorFilter_t) ckalloc(sizeof(generatorFilter));
  mf->freq[1] = 0;
  mf->ampl[1] = 0;
  mf->type = SINE;
  mf->shape[1] = 0.5;
  mf->_phase = 0.0;
  mf->ntot = -1;
  if (generatorConfigProc((Snack_Filter) mf, interp, objc, objv) != TCL_OK) {
    return (Snack_Filter) NULL;
  }
return (Snack_Filter) mf;
}


static int
generatorStartProc(Snack_Filter f, Snack_StreamInfo si) {
  
  generatorFilter_t mf = (generatorFilter_t) f;
  /*  fprintf(stderr,"entering generatorStartProc\n"); */

  mf->freq[0]  = mf->freq[1];
  mf->ampl[0]  = mf->ampl[1];
  mf->shape[0] = mf->shape[1];
  mf->_phase   = 0.0;
  mf->nSamples = 0;
  mf->ngen     = 0;
  mf->maxval   = 1.0;

  return TCL_OK;
}

static int
generatorFlowProc(Snack_Filter f, Snack_StreamInfo si, float *in, float *out,
		  int *inFrames, int *outFrames) {
  
  generatorFilter_t mf = (generatorFilter_t) f;
  int i, i0, i1, ii, fr, wi;
  double y = 1.0/RAND_MAX, z = 1.0/(*outFrames), ph = mf->_phase;
  double frac,a,b,v[2],tmp;
  
  /*  fprintf(stderr,"entering generatorFlowProc, *inFrames = %d\t*outFrames = %d, in = %p, out = %p\n",*inFrames,*outFrames,in,out); */
  
  if (mf->ntot>0 && mf->ngen + (*outFrames) > mf->ntot) {
    *outFrames = mf->ntot - mf->ngen;
  }
  
  /* read input samples (if any) - this is used by the SAMPLED wave type*/
  for (i=0;i<*inFrames;i++) {
    ii = i+mf->nSamples;
    if (ii>=MAX_SAMPLES) break;
    tmp = mf->samples[ii] = in[i];
    if (fabs(tmp) > mf->maxval) { mf->maxval = (float) fabs(tmp);}
  }
  mf->nSamples += i;
  *inFrames = i;

  i = 0;
  switch (mf->type) {
  case NOISE:
    
    for (fr = 0; fr < *outFrames; fr++) {
      frac = fr*z;
      for (wi = 0; wi < si->outWidth; wi++) {
	out[i++]=(float)(LIN(mf->ampl,frac)*2*(y*rand()-.5));
      }
      i += (si->streamWidth - si->outWidth);
    }
    *inFrames = 0;
    break;

  case RECTANGLE:
    for (fr = 0; fr < *outFrames; fr++) {
      frac = fr*z;
      ph = fmod (ph + LIN(mf->freq,frac)/si->rate,1.0);
      for (wi = 0; wi < si->outWidth; wi++) {
	out[i++] = (float)(LIN(mf->ampl,frac)*(ph<LIN(mf->shape,frac)?-1:1));
      }
      i += (si->streamWidth - si->outWidth);
    }
    *inFrames = 0;
    break;

  case TRIANGLE:
    for (fr = 0; fr < *outFrames; fr++) {
      frac = fr*z;
      ph = fmod (ph + LIN(mf->freq,frac)/si->rate,1.0);
      for (wi = 0; wi < si->outWidth; wi++) {
	if (ph < LIN(mf->shape,frac)) {
	  out[i++] = (float)(LIN(mf->ampl,frac)*(-1+2*(ph)/LIN(mf->shape,frac)));
	} else if (ph > LIN(mf->shape,frac)) {
	  out[i++] = (float)(LIN(mf->ampl,frac)*(1-2*(ph-LIN(mf->shape,frac))/(1-LIN(mf->shape,frac))));
	} else {
	  out[i++] = (float)LIN(mf->ampl,frac);
	}
      }
      i += (si->streamWidth - si->outWidth);
    }
    *inFrames = 0;
    break;

  case SINE:
    for (fr = 0; fr < *outFrames; fr++) {
      frac = fr*z;
      ph = fmod (ph + LIN(mf->freq,frac)/si->rate,1.0);
      a = sin(ph * 2 * M_PI);
      b = 2*LIN(mf->shape,frac)-1;
      a = a>b?a:b;
      for (wi = 0; wi < si->outWidth; wi++) {
	if (1-b==0.0) {
	  out[i++] = 0.0;
	} else {
	  out[i++] = (float)(LIN(mf->ampl,frac)*(a-.5-.5*b)/(1-b));
	}
      }
      i += (si->streamWidth - si->outWidth);
      *inFrames = 0;
    }
    break;

  case SAMPLED:
    if (mf->nSamples > 0) {
      for (fr = 0; fr < *outFrames; fr++) {
	frac = fr*z;
	ph = fmod (ph + LIN(mf->freq,frac)/si->rate,1.0);
	a = ph*(mf->nSamples);
	i0 = (int)floor(a);
	i1 = (int)ceil(a)%mf->nSamples;
	v[0] = mf->samples[i0];
	v[1] = mf->samples[i1];
	frac = (a-i0);
	for (wi = 0; wi < si->outWidth; wi++) {
	  out[i++] = (float)(LIN(v,frac)*LIN(mf->ampl,frac)/mf->maxval);
	}
	i += (si->streamWidth - si->outWidth);
      }
    } else {
      for (fr = 0; fr < *outFrames; fr++) {
	for (wi = 0; wi < si->outWidth; wi++) {
	  out[i++] = 0;
	}
	i += (si->streamWidth - si->outWidth);
      }
    }
  }
  mf->_phase = ph; /* save current phase value */
  mf->freq[0] = mf->freq[1];
  mf->ampl[0] = mf->ampl[1];
  mf->shape[0] = mf->shape[1];

  mf->ngen += *outFrames;
  
  /*  fprintf(stderr,"leaving  generatorFlowProc, *inFrames = %d\t*outFrames = %d (i=%d)\n-----------------------------------------\n",*inFrames,*outFrames,i); */
  return TCL_OK;
}


static void
generatorFreeProc(Snack_Filter f) {
  ckfree((char*)f);
}

static Snack_FilterType snackGeneratorType = {
  "generator",
  generatorCreateProc,
  generatorConfigProc,
  generatorStartProc,
  generatorFlowProc,
  generatorFreeProc,
  (Snack_FilterType *) NULL
};


void
createSynthesisFilters() {
  Snack_CreateFilterType(&snackGeneratorType);
  Snack_CreateFilterType(&snackFormantType);
}
