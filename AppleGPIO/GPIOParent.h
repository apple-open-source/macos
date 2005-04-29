/*
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _GPIOPARENT_H
#define _GPIOPARENT_H

// platform function symbols

// simple reads and writes
#define kSymGPIOParentWriteGPIO		"GPIOParent_writeGPIO"
#define kSymGPIOParentReadGPIO		"GPIOParent_readGPIO"

// interrupt notification
#include "GPIOEventHandler.h"	// callback prototype

#define kSymGPIOParentIntCapable	"GPIOParent_intCapable"
#define kSymGPIOParentRegister		"GPIOParent_register"
#define kSymGPIOParentUnregister	"GPIOParent_unregister"
#define kSymGPIOParentEvtEnable		"GPIOParent_evtEnable"
#define kSymGPIOParentEvtDisable	"GPIOParent_evtDisable"

#endif // _GPIOPARENT_H