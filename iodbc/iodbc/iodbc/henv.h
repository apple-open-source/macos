/*
 *  henv.h
 *
 *  $Id: henv.h,v 1.1.1.1 2002/04/08 22:48:10 miner Exp $
 *
 *  Environment object management functions
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
#ifndef	_HENV_H
#define	_HENV_H

#include <iodbc.h>
#include <dlproc.h>

#include <sql.h>
#include <sqlext.h>
#include <ithread.h>


enum odbcapi_t
  {
    en_NullProc = 0
#define FUNCDEF(A, B, C)	,B
#include "henv.ci"
#undef FUNCDEF
    , __LAST_API_FUNCTION__
  } ;


typedef struct
  {
    int type;			/* must be 1st field */
    HERR herr;			/* err list          */
    SQLRETURN rc;

    HENV henv;			/* driver's env list */
    HDBC hdbc;			/* driver's dbc list */
    int state;
#if (ODBCVER >= 0x300)
    SQLINTEGER odbc_ver;    /* ODBC version of the application */
#endif    

    SQLSMALLINT err_rec;
  }
GENV_t;


typedef struct
  {
    HENV next;			/* next attached env handle */
    int refcount;		/* Driver's bookkeeping reference count */
    HPROC dllproc_tab[__LAST_API_FUNCTION__];	/* driver api calls  */
    HENV dhenv;			/* driver env handle    */
    HDLL hdll;			/* driver share library handle */

    SWORD thread_safe;		/* Is the driver threadsafe? */
    MUTEX_DECLARE (drv_lock);	/* Used only when driver is not threadsafe */

#if (ODBCVER >= 0x300)
    SQLINTEGER dodbc_ver;	/* driver's ODBC version */
#endif    
  }
ENV_t;


#define IS_VALID_HENV(x) \
	((x) != SQL_NULL_HENV && ((GENV_t FAR *)(x))->type == SQL_HANDLE_ENV)


/*
 * Multi threading
 */
#if defined (IODBC_THREADING)
extern SPINLOCK_DECLARE(iodbcdm_global_lock);
#define ODBC_LOCK()	SPINLOCK_LOCK(iodbcdm_global_lock)
#define ODBC_UNLOCK()	SPINLOCK_UNLOCK(iodbcdm_global_lock)
#else
#define ODBC_LOCK()
#define ODBC_UNLOCK()
#endif


/* Note:
 *
 *  - ODBC applications only know about global environment handle, 
 *    a void pointer points to a GENV_t object. There is only one
 *    this object per process(however, to make the library reentrant,
 *    we still keep this object on heap). Applications only know 
 *    address of this object and needn't care about its detail.
 *
 *  - ODBC driver manager knows about instance environment handles,
 *    void pointers point to ENV_t objects. There are maybe more
 *    than one this kind of objects per process. However, multiple
 *    connections to a same data source(i.e. call same share library)
 *    will share one instance environment object.
 *
 *  - ODBC driver manager knows about their own environment handle,
 *    a void pointer point to a driver defined object. Every driver
 *    keeps one of its own environment object and driver manager
 *    keeps address of it by the 'dhenv' field in the instance
 *    environment object without care about its detail.
 *
 *  - Applications can get driver's environment object handle by
 *    SQLGetInfo() with fInfoType equals to SQL_DRIVER_HENV
 */
#endif
