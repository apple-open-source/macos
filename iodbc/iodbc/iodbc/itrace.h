/*
 *  itrace.h
 *
 *  $Id: itrace.h,v 1.1.1.1 2002/04/08 22:48:10 miner Exp $
 *
 *  Trace functions
 *
 *  The iODBC driver manager.
 *  
 *  Copyright (C) 1995 by Ke Jin <kejin@empress.com> 
 *  Copyright (C) 1996-2002 by OpenLink Software <iodbc@openlinksw.com>
 *  All Rights Reserved.
 *
 *  This software is released under the terms of either of the following
 *  licenses:
 *
 *      - GNU Library General Public License (see LICENSE.LGPL) 
 *      - The BSD License (see LICENSE.BSD).
 *
 *  While not mandated by the BSD license, any patches you make to the
 *  iODBC source code may be contributed back into the iODBC project
 *  at your discretion. Contributions will benefit the Open Source and
 *  Data Access community as a whole. Submissions may be made at:
 *
 *      http://www.iodbc.org
 *
 *
 *  GNU Library Generic Public License Version 2
 *  ============================================
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *  The BSD License
 *  ===============
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *  3. Neither the name of OpenLink Software Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL OPENLINK OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_ITRACE_H
#define _ITRACE_H

#ifdef	DEBUG

#ifndef NO_TRACE
#define NO_TRACE
#endif

#endif

#define TRACE_TYPE_APP2DM	1
#define TRACE_TYPE_DM2DRV	2
#define TRACE_TYPE_DRV2DM	3

#define TRACE_TYPE_RETURN	4

extern HPROC _iodbcdm_gettrproc (void FAR * stm, int procid, int type);

#ifdef NO_TRACE
#define TRACE_CALL( stm, trace_on, procid, plist )
#else
#define TRACE_CALL( stm, trace_on, plist )\
	{\
		if( trace_on)\
		{\
			HPROC	hproc;\
\
			hproc = _iodbcdm_gettrproc(stm, procid, TRACE_TYPE_APP2DM);\
\
			if( hproc )\
				hproc plist;\
		}\
	}
#endif

#ifdef NO_TRACE
#define TRACE_DM2DRV( stm, procid, plist )
#else
#define TRACE_DM2DRV( stm, procid, plist )\
	{\
		HPROC	hproc;\
\
		hproc = _iodbcdm_gettrproc(stm, procid, TRACE_TYPE_DM2DRV);\
\
		if( hproc )\
			hproc plist;\
	}
#endif

#ifdef NO_TRACE
#define TRACE_DRV2DM( stm, procid, plist )
#else
#define TRACE_DRV2DM( stm, procid, plist ) \
	{\
		HPROC	hproc;\
\
		hproc = _iodbcdm_gettrproc( stm, procid, TRACE_TYPE_DRV2DM);\
\
		if( hproc )\
				hproc plist;\
	}
#endif

#ifdef NO_TRACE
#define TRACE_RETURN( stm, trace_on, ret )
#else
#define TRACE_RETURN( stm, trace_on, ret )\
	{\
		if( trace_on ) {\
			HPROC hproc;\
\
			hproc = _iodbcdm_gettrproc( stm, 0, TRACE_TYPE_RETURN);\
\
			if( hproc )\
				hproc( stm, ret );\
		}\
	}
#endif

#define CALL_DRIVER_FUNC( hdbc, errHandle, ret, proc, plist ) \
    { \
      ret = proc plist; \
      if (errHandle) ((GENV_t FAR *)(errHandle))->rc = ret; \
    }

#ifdef	NO_TRACE
#define CALL_DRIVER( hdbc, errHandle, ret, proc, procid, plist ) \
	{\
		DBC_t FAR*	pdbc = (DBC_t FAR*)(hdbc);\
		ENV_t FAR*      penv = (ENV_t FAR*)(pdbc->henv);\
\
	        if (!penv->thread_safe)\
			MUTEX_LOCK (penv->drv_lock);\
\
		CALL_DRIVER_FUNC( hdbc, errHandle, ret, proc, plist )
\
	        if (!penv->thread_safe)\
			MUTEX_UNLOCK (penv->drv_lock);\
\
	}
#else
#define CALL_DRIVER( hdbc, errHandle, ret, proc, procid, plist ) \
	{\
		DBC_t FAR*	pdbc = (DBC_t FAR*)(hdbc);\
		ENV_t FAR*      penv = (ENV_t FAR*)(pdbc->henv);\
\
	        if (!penv->thread_safe)\
			MUTEX_LOCK (penv->drv_lock);\
\
		if( pdbc->trace ) {\
			TRACE_DM2DRV( pdbc->tstm, procid, plist )\
			CALL_DRIVER_FUNC( hdbc, errHandle, ret, proc, plist );\
			TRACE_DRV2DM( pdbc->tstm, procid, plist )\
			TRACE_RETURN( pdbc->tstm, 1, ret )\
		}\
		else\
			CALL_DRIVER_FUNC( hdbc, errHandle, ret, proc, plist );\
\
	        if (!penv->thread_safe)\
			MUTEX_UNLOCK (penv->drv_lock);\
\
	}
#endif


#endif
