/*
 * Copyright (c) 2025 Apple Computer, Inc. All rights reserved.
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

#ifndef IOPCITraceEventDefinitions_h
#define IOPCITraceEventDefinitions_h


/*
 * Trace Event Codes
 */

enum PCITraceEventCodes {
    PCI_TRACE_EVENT_NOOP = 0,
    PCI_TRACE_EVENT_TEST,

    PCI_TRACE_EVENT_NUM_EVENTS,
};


/*
 * Trace Event Data Structures
 * NOTE: User defines their event data structures needed for processing here
 * NOTE: Packing structs is compiler/standard dependent, this assumes gcc or clang
 */

#if defined(__GNUC__) || defined(__clang__)
    #define PACKED_STRUCT(name) struct __attribute__((packed)) name
#else
    #error "Unknown compiler or standard version"
#endif 


PACKED_STRUCT(PCITraceEventTestEventData) { 
    uint32_t    testData32a;
    uint32_t    testData32b;
    uint64_t    testData64a;
};
    

#endif /* IOPCITraceEventDefinitions_h */

