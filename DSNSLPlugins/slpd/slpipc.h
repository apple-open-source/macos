/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 
/*!
 * 	@header slpipc
 *  C calls that bridge into our C++ code
*/

#ifndef _slpipc_
#define _slpipc_
#pragma once

    SLPInternalError	reset_slpd( int argc, char *pcArgv[], struct sockaddr_in *psin, SAState* psa );

    int			handle_udp(SAState *psa, char* pcInBuf, int inBufSz, struct sockaddr_in sinIn);
    int			handle_tcp(SAState *psa, SOCKET sdRqst, struct sockaddr_in sinIn);

    int			InitializeInternalProcessListener( SAState* psa );
    int			InitializeTCPListener( SAState* psa );
    int			InitializeUDPListener( SAState* psa );

    int			RunSLPInternalProcessListener( SAState* psa );
    int			StartSLPUDPListener( SAState* psa );
    void		CancelSLPUDPListener( void );
    int			StartSLPTCPListener( SAState* psa );
    void		CancelSLPTCPListener( void );

#endif
