/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
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

#include "dnssd_ipc.h"


void put_flags(const DNSServiceFlags flags, char **ptr)
    {
    memcpy(*ptr, &flags, sizeof(DNSServiceFlags));
    *ptr += sizeof(flags);
    }

DNSServiceFlags get_flags(char **ptr)
    {
    DNSServiceFlags flags;
	
    flags = *(DNSServiceFlags *)*ptr;
    *ptr += sizeof(DNSServiceFlags);
    return flags;
    }

void put_long(const uint32_t l, char **ptr)
    {

    *(uint32_t *)(*ptr) = l;
    *ptr += sizeof(uint32_t);
    }

uint32_t get_long(char **ptr)
    {
    uint32_t l;
	
    l = *(uint32_t *)(*ptr);
    *ptr += sizeof(uint32_t);
    return l;
    }

void put_error_code(const DNSServiceErrorType error, char **ptr)
    {
    memcpy(*ptr, &error, sizeof(error));
    *ptr += sizeof(DNSServiceErrorType);
    }

DNSServiceErrorType get_error_code(char **ptr)
    {
    DNSServiceErrorType error;
	
    error = *(DNSServiceErrorType *)(*ptr);
    *ptr += sizeof(DNSServiceErrorType);
    return error;
    }

void put_short(const uint16_t s, char **ptr)
    {
    *(uint16_t *)(*ptr) = s;
    *ptr += sizeof(uint16_t);
    }

uint16_t get_short(char **ptr)
    {
    uint16_t s;

    s = *(uint16_t *)(*ptr);
    *ptr += sizeof(uint16_t);
    return s;
    }


int put_string(const char *str, char **ptr)
    {
    if (!str) str = "";
    strcpy(*ptr, str);
    *ptr += strlen(str) + 1;
    return 0;
    }

// !!!KRS we don't properly handle the case where the string is longer than the buffer!!!	
int get_string(char **ptr, char *buffer, int buflen)
    {
    int overrun;
    
    overrun = (int)strlen(*ptr) <  buflen ? 0 : -1;
    strncpy(buffer, *ptr,  buflen - 1);
    buffer[buflen - 1] = '\0';
    *ptr += strlen(buffer) + 1;
    return overrun;
    }	

void put_rdata(const int rdlen, const char *rdata, char **ptr)
    {
    memcpy(*ptr, rdata, rdlen);
    *ptr += rdlen;	
    }

char *get_rdata(char **ptr, int rdlen)
    {
    char *rd;
		
    rd = *ptr;
    *ptr += rdlen;
    return rd;
    }









