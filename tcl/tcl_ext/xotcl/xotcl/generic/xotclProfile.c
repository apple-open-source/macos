/* -*- Mode: c++ -*-
 *  $Id: xotclProfile.c,v 1.2 2006/02/18 22:17:33 neumann Exp $
 *  
 *  Extended Object Tcl (XOTcl)
 *
 *  Copyright (C) 1999-2008 Gustaf Neumann, Uwe Zdun
 *
 *
 *  xotclProfile.c --
 *  
 *  Profiling information printout for XOTcl
 *
 *  For profiling infos PROFILE (xotcl.h) flag must be activated
 *  
 */

#include "xotclInt.h"

#if defined(PROFILE)
void
XOTclProfileFillTable(Tcl_HashTable* table, Tcl_DString* key,
		 double totalMicroSec) {
  Tcl_HashEntry* hPtr;
  char* keyStr = Tcl_DStringValue(key);
  long int* value;

  hPtr = Tcl_FindHashEntry(table, keyStr);
  if (!hPtr) {
    int nw;
    hPtr = Tcl_CreateHashEntry(table, keyStr, &nw);
    if (!nw)
      return;
    value = (long int*) ckalloc (sizeof(long int));
    *value = 0;
    Tcl_SetHashValue(hPtr, (ClientData) value);
  } else
    value = (long int*) Tcl_GetHashValue (hPtr);

  *value += totalMicroSec;


  /* {
    long int* d = (long int*) Tcl_GetHashValue (hPtr);
    fprintf(stderr, "Entered %s ... %ld\n", Tcl_GetHashKey(table, hPtr), *d);
    }*/

}

void
XOTclProfileEvaluateData(Tcl_Interp* interp, long int startSec, long int startUsec,
		    XOTclObject* obj, XOTclClass *cl, char *methodName) {
  double totalMicroSec;
  struct timeval trt;
  Tcl_DString objectKey, methodKey;

  XOTclProfile* profile = &RUNTIME_STATE(interp)->profile;

  gettimeofday(&trt, NULL);

  totalMicroSec = (trt.tv_sec - startSec) * 1000000 +
    (trt.tv_usec - startUsec);

  profile->overallTime += totalMicroSec;

  if (obj->teardown == 0 || !obj->id || obj->destroyCalled)
    return;

  ALLOC_DSTRING(&objectKey, ObjStr(obj->cmdName));

  if (cl)
    ALLOC_DSTRING(&methodKey, ObjStr(cl->object.cmdName));
  else
    ALLOC_DSTRING(&methodKey, ObjStr(obj->cmdName));
  Tcl_DStringAppend(&methodKey, "->", 2);
  Tcl_DStringAppend(&methodKey, methodName, -1);
  if (cl)
    Tcl_DStringAppend(&methodKey, " (instproc)", 11);
  else
    Tcl_DStringAppend(&methodKey, " (proc)", 7);

  XOTclProfileFillTable(&profile->objectData, &objectKey, totalMicroSec);
  XOTclProfileFillTable(&profile->methodData, &methodKey, totalMicroSec);
  DSTRING_FREE(&objectKey);
  DSTRING_FREE(&methodKey);
}

void
XOTclProfilePrintTable(Tcl_HashTable* table) {
  Tcl_HashEntry* topValueHPtr;
  long int* topValue;

  do {
    Tcl_HashSearch hSrch;
    Tcl_HashEntry* hPtr = table ?
      Tcl_FirstHashEntry(table, &hSrch) : 0;
    char* topKey = 0;

    topValueHPtr = 0;
    topValue = 0;

    for (; hPtr != 0; hPtr = Tcl_NextHashEntry(&hSrch)) {
      long int *val = (long int*) Tcl_GetHashValue(hPtr);
      if (val && (!topValue || (topValue && *val >= *topValue))) {
	topValue = val;
	topValueHPtr = hPtr;
	topKey =  Tcl_GetHashKey(table, hPtr);
      }
    }

    if (topValueHPtr) {
      fprintf(stderr, "  %15ld   %s\n", *topValue, topKey);
      ckfree((char*) topValue);
      Tcl_DeleteHashEntry(topValueHPtr);
    }
  } while (topValueHPtr);
}

void
XOTclProfilePrintData(Tcl_Interp *interp) {
  XOTclProfile* profile = &RUNTIME_STATE(interp)->profile;

  fprintf(stderr, "------------------------------------------------------------------\n");
  fprintf(stderr, "\nXOTcl Profile Information\n\n");
  fprintf(stderr, "------------------------------------------------------------------\n");
  fprintf(stderr, "Overall Elapsed Time              %ld\n",
	  profile->overallTime);
  fprintf(stderr, "------------------------------------------------------------------\n");
  fprintf(stderr, "     MICROSECONDS   OBJECT-NAME\n");
  XOTclProfilePrintTable(&profile->objectData);
  fprintf(stderr, "------------------------------------------------------------------\n");
  fprintf(stderr, "     MICROSECONDS   (CL/OBJ)->METHOD-NAME\n");
  XOTclProfilePrintTable(&profile->methodData);
  fprintf(stderr, "------------------------------------------------------------------\n");
}

void 
XOTclProfileInit(Tcl_Interp *interp) {
  RUNTIME_STATE(interp)->profile.overallTime = 0;
  Tcl_InitHashTable(&RUNTIME_STATE(interp)->profile.objectData,
		    TCL_STRING_KEYS);
  Tcl_InitHashTable(&RUNTIME_STATE(interp)->profile.methodData,
		    TCL_STRING_KEYS);
}

#endif
