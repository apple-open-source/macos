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
 * @header NiLib3
 */

#include <string.h>
#include <netinfo/ni.h>

#include "NiLib3.h"

//------------------------------------------------------------------------------------
//	* _createprop
//------------------------------------------------------------------------------------

static ni_index _createprop ( ni_proplist *l, const ni_name n )
{
	/* property list utility */
	/* add a property to a property list */

	ni_property p;

	NI_INIT( &p );
	p.nip_name = ::ni_name_dup( n );
	p.nip_val.ninl_len = 0;
	p.nip_val.ninl_val = NULL;
	ni_proplist_insert( l, p, NI_INDEX_NULL );
	ni_prop_free( &p );

	// return its index in the proplist
	return( ::ni_proplist_match( *l, n, NULL ) );

} // _createprop


//------------------------------------------------------------------------------------
//	* CreateProp
//------------------------------------------------------------------------------------

void NiLib3::CreateProp ( ni_proplist *l, const ni_name n, const ni_name v )
{
	/* property list utility */
	/* define a value to a property in a property list
	 * destroying any current values. */

	ni_index where;

	if ( NI_INDEX_NULL == ( where = ::ni_proplist_match( *l, n, NULL ) ) )
	{
		where = _createprop( l, n );
	}
	else
	{
		ni_namelist_free ( &( l->nipl_val[where].nip_val ) );
	}

	if ( v != NULL )
	{
		ni_namelist_insert( &( l->nipl_val[where].nip_val ), v, NI_INDEX_NULL );
	}
} // CreateProp


//------------------------------------------------------------------------------------
//	* AppendProp
//------------------------------------------------------------------------------------

void NiLib3::AppendProp ( ni_proplist *l, const ni_name n, const ni_name	v )
{
	/* property list utility */
	/* append a value to a property in a property list */

	ni_index where;

	if ( NI_INDEX_NULL == ( where = ::ni_proplist_match( *l, n, NULL ) ) )
	{
		where = _createprop( l, n );
	}
	ni_namelist_insert( &( l->nipl_val[where].nip_val ), v, NI_INDEX_NULL );

} // AppendProp


//------------------------------------------------------------------------------------
//	* MergeProp
//------------------------------------------------------------------------------------

int NiLib3::MergeProp ( ni_proplist *l, const ni_name n, const ni_name v )
{
	/* property list utility */
	/* merge a value into a property in a property list */

	ni_index where;

	if ( NI_INDEX_NULL == ( where = ::ni_proplist_match( *l, n, NULL ) ) )
	{
		where = _createprop( l, n );
	}

	if ( NI_INDEX_NULL == ::ni_namelist_match( l->nipl_val[where].nip_val, v ) )
	{
		ni_namelist_insert( &( l->nipl_val[where].nip_val ), v, NI_INDEX_NULL );
		return( 0 );
	}

	return( NI_INDEX_NULL );

} // MergeProp


//------------------------------------------------------------------------------------
//	* DestroyProp
//------------------------------------------------------------------------------------

int NiLib3::DestroyProp ( ni_proplist *l, const ni_name n )
{
	/* property list utility */
	/* Delete a property from a property list */

	ni_index where;

	if ( NI_INDEX_NULL == ( where = ::ni_proplist_match ( *l, n, NULL ) ) )
	{
		return NI_INDEX_NULL;
	}

	ni_proplist_delete ( l, where );

	return( 0 );

} // DestroyProp


//------------------------------------------------------------------------------------
//	* DestroyVal
//------------------------------------------------------------------------------------

int NiLib3::DestroyVal ( ni_proplist *l, const ni_name n, const ni_name v )
{
	/* property list utility */
	/* delete a value from a property list */

	ni_index where;
	ni_namelist *nlp;

	if ( NI_INDEX_NULL == ( where = ::ni_proplist_match ( *l, n, NULL ) ) )
	{
		return NI_INDEX_NULL;
	}

	nlp = &( l->nipl_val[where].nip_val );
	if ( NI_INDEX_NULL == ( where = ::ni_namelist_match ( *nlp, v ) ) )
	{
		return NI_INDEX_NULL;
	}

	ni_namelist_delete ( nlp, where );

	return( 0 );

} // DestroyVal


//------------------------------------------------------------------------------------
//	* Name
//------------------------------------------------------------------------------------

char* NiLib3::Name ( ni_proplist l )
{
	/* property list utility */
	/* Return the first value for the name property in a property list */

	ni_index where;

	if ( NI_INDEX_NULL == ( where = ::ni_proplist_match ( l, "name", NULL ) ) )
	{
		return NULL;
	}

	return( l.nipl_val[where].nip_val.ninl_val[0] );

} // Name


//------------------------------------------------------------------------------------
//	* FindPropVals
//------------------------------------------------------------------------------------

ni_namelist* NiLib3::FindPropVals ( ni_proplist l, const ni_name n )
{
	/* property list utility */
	/* Return the list of values for a property in a property list */

	ni_index where;

	if ( NI_INDEX_NULL == ( where = ::ni_proplist_match ( l, n, NULL ) ) )
	{
		return NULL;
	}

	return &( l.nipl_val[where].nip_val );

} // FindPropVals


//------------------------------------------------------------------------------------
//	* 
//------------------------------------------------------------------------------------

char* NiLib3::FindPropVal ( ni_proplist l, const ni_name n, char *out )
{
	/* property list utility */
	/* Return the first value for a property in a property list */

	ni_index where;

	if ( NI_INDEX_NULL == ( where = ::ni_proplist_match ( l, n, NULL ) ) )
	{
		return NULL;
	}

	if ( out != NULL )
	{
		strcpy ( out, l.nipl_val[where].nip_val.ninl_val[0] );
	}

	return( l.nipl_val[where].nip_val.ninl_val[0] );

} // FindPropVal
