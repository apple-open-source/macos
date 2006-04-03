/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  UtilitiesCFPrettyPrint.c
 *  DiskImages
 *
 *  Created by Shantonu Sen on 8/11/04.
 *  Copyright 2004 Apple Computer, Inc. All rights reserved.
 *
 */

#include "UtilitiesCFPrettyPrint.h"

typedef struct TAOCFDepth_Counter {
    int depth;
    int arrayIndex;
    char *indent;
    FILE *out;
} TAOCFDepth_Counter;

static void _TAOCFLabelPrettyPrint(char *label, CFTypeRef value, void *context);
static void _TAOCFDictionaryPrettyPrint(const void *key, const void *value, void *context);
static void _TAOCFArrayPrettyPrint(const void *value, void *context);
static void _TAOCFTreePrettyPrint(const void *value, void *context);
static void _TAOCFBagPrettyPrint(const void *value, void *context);
static void _TAOCFScalarPrettyPrint(const void *value, void *context);
static void _TAOCFPrettyPrint_recurse(CFTypeRef inRef, void *h);

static char *	_TAOCFGetDescriptionAsCString(CFTypeRef typeRef);

static void _TAOCFLabelPrettyPrint(char *label, CFTypeRef value, void *context) {
    TAOCFDepth_Counter *h = (TAOCFDepth_Counter *)context;
    int i;
    char *desc;
    
    // we're printing at least label, so tab over
    
    for(i=0; i< h->depth; i++) {
        fprintf(h->out, h->indent);
    }
    
    fprintf(h->out, "%s", label);
    
    if(value == NULL) {
	// our job here is done
	fprintf(h->out, "\n");
	return;
    }
    
    if(label && label[0]) {
	fprintf(h->out, ":");
    }
    
    if(CFGetTypeID(value) == CFDictionaryGetTypeID()
       || CFGetTypeID(value) == CFArrayGetTypeID()
       || CFGetTypeID(value) == CFTreeGetTypeID()
       || CFGetTypeID(value) == CFBagGetTypeID() ) {
        // go to next line, indent, and recurse
        TAOCFDepth_Counter newhelper;
	
	fprintf(h->out, "\n");
	
        newhelper.depth = h->depth + 1;
	newhelper.out = h->out;
	newhelper.arrayIndex = 0;
	newhelper.indent = h->indent;
	_TAOCFPrettyPrint_recurse((CFTypeRef) value, &newhelper);
    } else {
        // just a regular key value pair. print
	desc = _TAOCFGetDescriptionAsCString((CFTypeRef)value);
	if(label && label[0]) {
	    fprintf(h->out, " ");
	}
	if(desc) {
	    fprintf(h->out, "%s\n", desc);
	    free(desc);
	} else {
	    fprintf(h->out, "(NULL)\n");		
	}
    }
}

static void _TAOCFDictionaryPrettyPrint(const void *key, const void *value, void *context) {
    char *desc;
    
    desc = _TAOCFGetDescriptionAsCString((CFTypeRef)key);
    
    _TAOCFLabelPrettyPrint(desc, (CFTypeRef) value, context);
    
    if(desc) {
	free(desc);
    }
}

static void _TAOCFArrayPrettyPrint(const void *value, void *context) {
    char desc[256];
    
    snprintf(desc, 256, "%d", ((TAOCFDepth_Counter *)context)->arrayIndex++);
    
    _TAOCFLabelPrettyPrint(desc, (CFTypeRef)value, context);
}

static void _TAOCFBagPrettyPrint(const void *value, void *context) {
    
    _TAOCFLabelPrettyPrint("", (CFTypeRef)value, context);
}

static void _TAOCFTreePrettyPrint(const void *value, void *context) {
    //char desc[256];
    CFTreeContext treeContext;
    TAOCFDepth_Counter newhelper;
    
    newhelper.depth = ((TAOCFDepth_Counter *)context)->depth + 1;
    newhelper.out = ((TAOCFDepth_Counter *)context)->out;
    newhelper.indent = ((TAOCFDepth_Counter *)context)->indent;
    newhelper.arrayIndex = 0;
    
    //	snprintf(desc, 256, "%d", ((TAOCFDepth_Counter *)context)->arrayIndex++);
    
    CFTreeGetContext((CFTreeRef)value, &treeContext);
    
    _TAOCFLabelPrettyPrint("", (CFTypeRef)treeContext.info, context);
    
    // It's a little non-standard to recurse here, but otherwise
    // we couldn't print the head of the tree.
    CFTreeApplyFunctionToChildren((CFTreeRef)value,
				  _TAOCFTreePrettyPrint, &newhelper);
}

static void _TAOCFScalarPrettyPrint(const void *value, void *context) {
    char *desc;
    
    desc = _TAOCFGetDescriptionAsCString((CFTypeRef)value);
    
    _TAOCFLabelPrettyPrint(desc, NULL, context);
    
    if(desc) {
	free(desc);
    }
}


// Used to split CFTypes into key:value form
static void _TAOCFPrettyPrint_recurse(CFTypeRef inRef, void *h) {
    
    if(inRef == NULL) {
	_TAOCFScalarPrettyPrint(inRef, h);
    } else if(CFGetTypeID(inRef) == CFDictionaryGetTypeID()) {
	// if we have a dictionary, apply the output function
	CFDictionaryApplyFunction((CFDictionaryRef)inRef,
				  _TAOCFDictionaryPrettyPrint, h);
    } else if(CFGetTypeID(inRef) == CFArrayGetTypeID()) {
	CFArrayApplyFunction((CFArrayRef)inRef,
			     CFRangeMake(0, CFArrayGetCount((CFArrayRef)inRef)),
			     _TAOCFArrayPrettyPrint, h);
    } else if(CFGetTypeID(inRef) == CFTreeGetTypeID()) {
	_TAOCFTreePrettyPrint(inRef, h);
    } else if(CFGetTypeID(inRef) == CFBagGetTypeID()) {
	CFBagApplyFunction(inRef, _TAOCFBagPrettyPrint, h);
    } else {
	_TAOCFScalarPrettyPrint(inRef, h);
    }
    
}


void TAOCFPrettyPrint(CFTypeRef inRef) {
    TAOCFPrettyPrintToFile(inRef, stdout);
}

void TAOCFPrettyPrintWithIndenter(CFTypeRef inRef, char *indent) {
    TAOCFPrettyPrintToFileWithIndenter(inRef, stdout, indent);
}

void TAOCFPrettyPrintToFile(CFTypeRef inRef, FILE *out) {
    TAOCFPrettyPrintToFileWithIndenter(inRef, out, "\t");
}

void TAOCFPrettyPrintToFileWithIndenter(CFTypeRef inRef, FILE *out, char *indent) {
    TAOCFDepth_Counter helper;
    
    helper.depth = 0;
    helper.out = out;
    helper.arrayIndex = 0;
    helper.indent = indent;
    
    _TAOCFPrettyPrint_recurse(inRef, &helper);
    
}


static char *	_TAOCFGetDescriptionAsCString(CFTypeRef typeRef)
{
    char	    *str = NULL;
    CFStringRef	    cfstr = NULL;
    CFIndex	    len;
    
//    if(typeRef == NULL) typeRef = kCFNull;
    cfstr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), typeRef);
    if(cfstr == NULL && typeRef != NULL) cfstr = CFSTR("<error getting string representation>");

    // add one for the NULL that CFStringGetCString will append
    len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr), kCFStringEncodingUTF8) + 1;
    str = malloc(len);			
    if (str) {
	if (!CFStringGetCString(cfstr, str, len, kCFStringEncodingUTF8)) {
	    free(str);
	    str = NULL;
	}
    }

    return str;
}
