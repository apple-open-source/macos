/*
 * (c) Copyright 2001 by Steve Grubb
 * All rights reserved.  The file named COPYRIGHT specifies the terms
 * and conditions for redistribution.
 */
#ifndef COMPAT_H
#define COMPAT_H

#if !defined(HAVE_GCVT)
int APPEND(FUNC_PREFIX,fcvt_r)(FLOAT_TYPE value,int ndigit,int *decpt,int *sign,char *buf,size_t len);
char *APPEND(FUNC_PREFIX,gcvt)(FLOAT_TYPE value,int ndigit,char *buf);
char *APPEND(FUNC_PREFIX,ecvt)(FLOAT_TYPE value,int ndigit,int *decpt,int *sign);
char *APPEND(FUNC_PREFIX,fcvt)(FLOAT_TYPE value,int ndigit,int *decpt,int *sign);
weak_extern2(FLOOR)weak_extern2(LOG10)weak_extern2(FABS)weak_extern2(EXP)int APPEND(FUNC_PREFIX,ecvt_r)(FLOAT_TYPE value,int ndigit,int *decpt,int *sign,char *buf,size_t len);
#endif


#endif
