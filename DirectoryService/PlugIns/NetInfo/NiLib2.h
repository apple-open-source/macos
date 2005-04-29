/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header NiLib2
 */

#ifndef __NiLib2_h__
#define	__NiLib2_h__		1

class NiLib2 {
public:
	static ni_status	Create					( void *domain, char *pathname );
	static ni_status	CreateProp				( void *domain, char *pathname, const ni_name key, ni_namelist values );
	static ni_status	CreateDirProp			( void *domain, ni_id *dir, const ni_name key, ni_namelist values );
	static ni_status	AppendProp				( void *domain, char *pathname, const ni_name key, ni_namelist values );
	static ni_status	AppendDirProp			( void *domain, ni_id *dir, const ni_name key, ni_namelist values );
	static ni_status	MergeProp				( void *domain, char *pathname, const ni_name key, ni_namelist values );
	static ni_status	MergeDirProp			( void *domain, ni_id *dir, const ni_name key, ni_namelist values );
	static ni_status	InsertVal				( void *domain, char *pathname, const ni_name key, const ni_name value, ni_index where );
	static ni_status	InsertDirVal			( void *domain, ni_id *dir, const ni_name key, const ni_name value, ni_index whereval );
	static ni_status	Destroy					( void *domain, char *pathname );
	static ni_status	Destroy					( void *inDomain, ni_id *inDirID );
	static ni_status	DestroyDir				( void *domain, ni_id *dir, ni_id *parent );
	static ni_status	DestroyProp				( void *domain, char *pathname, ni_namelist keys );
	static ni_status	DestroyDirProp			( void *domain, ni_id *dir, ni_namelist keys );
	static ni_status	DestroyVal				( void *domain, char *pathname, const ni_name key, ni_namelist values );
	static ni_status	DestroyDirVal			( void *domain, ni_id *dir, const ni_name key, ni_namelist values );
	static ni_status	RenameProp				( void *domain, char *pathname, const ni_name oldname, const ni_name newname );
	static ni_status	RenameDirProp			( void *domain, ni_id *dir, const ni_name oldname, const ni_name newname );
	static ni_status	CreatePath				( void *domain, ni_id *dir, char *pathname );
	static ni_status	CreateChild				( void *domain, ni_id *dir, const ni_name dirname );
	static ni_status	PathSearch				( void *domain, ni_id *dir, char *pathname );
	static ni_status	StatProp				( void *domain, char *pathname, const ni_name key, ni_index *where );
	static ni_status	StatPropDir				( void *domain, ni_id *dir, const ni_name key, ni_index *where );
	static ni_status	StatVal					( void *domain, char *pathname, const ni_name key, const ni_name value, ni_index *where );
	static ni_status	StatValDir				( void *domain, ni_id *dir, const ni_name key, const ni_name value, ni_index *where );
	static ni_status	ReapProp				( void *domain, char *pathname, const ni_name key );
	static ni_status	ReappropDir				( void *domain, ni_id *dir, const ni_name key );
	static ni_status	ReapDir					( void *domain, char *pathname );
	static ni_status	Copy					( void *srcdomain, char *srcpath, void*dstdomain, bool recursive );
	static ni_status	Copy					( void *srcdomain, ni_id *inSrcDirID, char *path, void *dstdomain, bool recursive );
	static ni_status	CopyDir					( void *srcdomain, ni_id *srcdir, void*dstdomain, ni_id *dstdir, bool recursive );
	static ni_status	CopyDirToParentDir		( void *srcdomain, ni_id *srcdir, void*dstdomain, ni_id *dstdir, bool recursive );
	static ni_status	LookUpProp				( void *domain, char *pathname, const ni_name key, ni_namelist *values );
	static ni_index		InsertSorted			( ni_namelist *values, const ni_name newvalue );
	static ni_status	InAccessList			( const char* user, ni_namelist access_list);
	static ni_status	ValidateDir				( const char* user, ni_proplist *pl );
	static ni_status	ValidateName			( const char* user, ni_proplist *pl, ni_index prop_index );
};

#endif
