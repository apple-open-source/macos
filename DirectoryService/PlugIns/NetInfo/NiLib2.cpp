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

#include <netinfo/ni.h>

#include <arpa/inet.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include "NiLib2.h"
#include "NiLib3.h"

/*
 * Constants
 */
const char ACCESS_USER_SUPER[] = "root";
const char ACCESS_USER_ANYBODY[] = "*";
const char ACCESS_NAME_PREFIX[] = "_writers_";
const char ACCESS_DIR_KEY[] = "_writers";

//------------------------------------------------------------------------------------
//	* 
//------------------------------------------------------------------------------------

enum ni_parse_status ni_parse_server_tag ( char *str, struct sockaddr_in *server, char **t )
{
	// utility to parse a server/tag string

	int len, i;
	char *host, *tag, *slash;
	struct hostent *hent;

	len = strlen( str );

	// find the "/" character
	slash = index( str, '/' );

	// check to see if the "/" is missing
	if ( slash == NULL ) return NI_PARSE_BADFORMAT;

	// find the location of the '/'
	i = slash - str;

	// check if host string is empty
	if ( i == 0 ) return NI_PARSE_NOHOST;

	// check if tag string is empty
	if ( i == ( len - 1 ) ) return NI_PARSE_NOTAG;

	// allocate some space for the host and tag
	host = ( char * )::malloc( i + 1 );
	*t = ( char * )::malloc( len - i );
	tag = *t;

	// copy out the host
	strncpy( host, str, i );
	host[i] = '\0';

	// copy out the tag
	strcpy( tag, slash + 1 );

	// try interpreting the host portion as an address
	server->sin_addr.s_addr = inet_addr( host );

	if ( (long)server->sin_addr.s_addr == -1 )
	{
		// This isn't a valid address.  Is it a known hostname?
 		hent = gethostbyname( host );
		if ( hent != NULL )
		{
			// found a host with that name
			memmove( &server->sin_addr, hent->h_addr, hent->h_length );
		}
		else
		{
//			fprintf( stderr, "Can't find address for %s\n", host );
			free( host );
			free( tag );
			return NI_PARSE_HOSTNOTFOUND;
		}
   }

	free( host );
	return NI_PARSE_OK;
} // 


//------------------------------------------------------------------------------------
//	* 
//------------------------------------------------------------------------------------

const char* ni_parse_error_string ( enum ni_parse_status status )
{
	switch ( status )
	{
		case NI_PARSE_OK:
			return( "Operation succeeded" );

		case NI_PARSE_BADFORMAT:
			return( "Bad format" );

		case NI_PARSE_NOHOST:
			return( "No host" );

		case NI_PARSE_NOTAG:
			return( "No tag" );

		case NI_PARSE_BADADDR:
			return( "Bad address" );

		case NI_PARSE_HOSTNOTFOUND:
			return( "Host not found" );
	}
	return NULL;
} // 


//------------------------------------------------------------------------------------
//	* 
//------------------------------------------------------------------------------------

int do_open ( char *tool, char *name, void **domain, bool bytag, int timeout, char *user, char *passwd )
{
#pragma unused ( tool )
	// do an ni_open or an ni_connect, as appropriate

	char *tag;
	enum ni_parse_status pstatus;
	ni_status status;
	struct sockaddr_in server;
	ni_id rootdir;

	if ( bytag )
	{
		// connect by tag
		// call a function to parse the input arg
		pstatus = ::ni_parse_server_tag( name, &server, &tag );
		if ( pstatus != NI_PARSE_OK )
		{
			if (tag != NULL)
			{
				free(tag);
			}
			return NI_FAILED + 1 + pstatus;
		}

		// connect to the specified server
		*domain = ::ni_connect( &server, tag );
		if ( *domain == NULL )
		{
			if (tag != NULL)
			{
				free(tag);
			}
			return NI_FAILED + 1;
		}
		if (tag != NULL)
		{
			free(tag);
		}
	}

	else
	{
		// open domain
		status = ::ni_open( NULL, name, domain );
		if ( status != NI_OK )
		{
			return status;
		}
	}

	// abort on errors
	ni_setabort( *domain, 1 );
	
	// set timeouts
	ni_setreadtimeout( *domain, timeout );
	ni_setwritetimeout( *domain, timeout );

	// authentication
	if ( user != NULL )
	{
		ni_setuser( *domain, user );
		if ( passwd != NULL ) ::ni_setpassword( *domain, passwd );
	}

	// get the root directory to see if the connection is alive
	status = ::ni_root( *domain, &rootdir );
	if ( status != NI_OK )
	{
		return status;
	}

	return 0;
} // 


//------------------------------------------------------------------------------------
//	* Create
//------------------------------------------------------------------------------------

ni_status NiLib2::Create ( void *domain, char *pathname )
{
	// make a directory with the given pathname
	// do nothing if the directory already exists

	ni_status niStatus;
	ni_id dir;

	// need to be talking to the master
	::ni_needwrite( domain, 1 );

	// see if it already exists
	niStatus = NiLib2::PathSearch( domain, &dir, pathname );
	if ( niStatus == NI_OK )
	{
		return( NI_OK );
	}

	// doesn't exist: create it
	niStatus = ::ni_root( domain, &dir );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	if ( pathname[0] == '/' )
	{
		niStatus = NiLib2::CreatePath( domain, &dir, pathname + 1 );
	}
	else
	{
		niStatus = NiLib2::CreatePath( domain, &dir, pathname );
	}

	return( niStatus ); 

} // Create


//------------------------------------------------------------------------------------
//	* CreateProp
//------------------------------------------------------------------------------------

ni_status NiLib2::CreateProp ( void *domain, char *pathname, const ni_name key, ni_namelist values )
{
	// create a new property with a given key and list of values
	// replaces an existing property if it already exists

	ni_status niStatus;
	ni_id dir;

	// need to be talking to the master
	::ni_needwrite( domain, 1 );

	// see if the directory already exists
	niStatus = NiLib2::PathSearch( domain, &dir, pathname );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	return( NiLib2::CreateDirProp( domain, &dir, key, values ) );

} // CreateProp


//------------------------------------------------------------------------------------
//	* CreateDirProp
//------------------------------------------------------------------------------------

ni_status NiLib2::CreateDirProp ( void *domain, ni_id *dir, const ni_name key, ni_namelist values )
{
	// createprop given a directory rather than a pathname

	ni_status niStatus;
	ni_property p;
	ni_namelist nl;
	ni_index where;

	// need to be talking to the master
	::ni_needwrite( domain, 1 );

	// fetch list of property keys from directory
	NI_INIT( &nl );
	niStatus = ::ni_listprops( domain, dir, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// check for existing property with this key
	where = ::ni_namelist_match( nl, key );
	::ni_namelist_free( &nl );

	// if property doesn't exist, create it
	if ( where == NI_INDEX_NULL )
	{
		NI_INIT( &p );
		p.nip_name = ::ni_name_dup( key );
		p.nip_val = ::ni_namelist_dup( values );
		niStatus = ::ni_createprop( domain, dir, p, NI_INDEX_NULL );
		ni_prop_free( &p );
		return( niStatus );
	}

	// property exists: replace the existing values
	niStatus = ::ni_writeprop( domain, dir, where, values );

	return( niStatus );

} // CreateDirProp


//------------------------------------------------------------------------------------
//	* AppendProp
//------------------------------------------------------------------------------------

ni_status NiLib2::AppendProp ( void *domain, char *pathname, const ni_name key, ni_namelist values )
{
	// append a list of values to a property
	// a new property is created if it doesn't exist

	ni_status niStatus;
	ni_id dir;

	// need to be talking to the master
	::ni_needwrite( domain, 1 );

	// see if the directory already exists
	niStatus = NiLib2::PathSearch( domain, &dir, pathname );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	return( NiLib2::AppendDirProp( domain, &dir, key, values ) );

} // AppendProp


//------------------------------------------------------------------------------------
//	* AppendDirProp
//------------------------------------------------------------------------------------

ni_status NiLib2::AppendDirProp ( void *domain, ni_id *dir, const ni_name key, ni_namelist values )
{
	// appendprop given a directory rather than a pathname

	ni_status niStatus;
	ni_property p;
	ni_namelist nl;
	ni_index where;
	int i;

	// need to be talking to the master
	::ni_needwrite( domain, 1 );

	// fetch list of property keys from directory
	NI_INIT( &nl );
	niStatus = ::ni_listprops( domain, dir, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// check for existing property with this key
	where = ::ni_namelist_match( nl, key );
	::ni_namelist_free( &nl );

	// if property doesn't exist, create it
	if ( where == NI_INDEX_NULL )
	{
		NI_INIT( &p );
		p.nip_name = ::ni_name_dup( key );
		p.nip_val = ::ni_namelist_dup( values );
		niStatus = ::ni_createprop( domain, dir, p, NI_INDEX_NULL );
		ni_prop_free( &p );
		return( niStatus );
	}


	// property exists: replace the existing values
	// fetch existing namelist for this property
	NI_INIT( &nl );
	niStatus = ::ni_readprop( domain, dir, where, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// append new values
	for ( i = 0; i < (int)values.ni_namelist_len; i++ )
	{
		ni_namelist_insert( &nl, values.ni_namelist_val[i], NI_INDEX_NULL );
	}

	// write the new list back
	niStatus = ::ni_writeprop( domain, dir, where, nl );

	::ni_namelist_free( &nl );

	return( niStatus );

} // AppendDirProp


//------------------------------------------------------------------------------------
//	* InsertVal
//------------------------------------------------------------------------------------

ni_status NiLib2::InsertVal ( void *domain, char *pathname, const ni_name key, const ni_name value, ni_index where )
{
	// insert a new value into a property
	// the property is created if it doesn't exist

	ni_status niStatus;
	ni_id dir;

	// need to be talking to the master
	::ni_needwrite( domain, 1 );

	// see if the directory exists
	niStatus = NiLib2::PathSearch( domain, &dir, pathname );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	return( NiLib2::InsertDirVal( domain, &dir, key, value, where ) );

} // InsertVal


//------------------------------------------------------------------------------------
//	* InsertDirVal
//------------------------------------------------------------------------------------

ni_status NiLib2::InsertDirVal ( void			*domain,
								 ni_id			*dir,
								 const ni_name	key,
								 const ni_name	value,
								 ni_index		whereval )
{
	// insertval given a directory rather than a pathname

	ni_status niStatus;
	ni_property p;
	ni_namelist nl;
	ni_index where;

	// need to be talking to the master
	::ni_needwrite( domain, 1 );

	// fetch list of property keys from directory
	NI_INIT( &nl );
	niStatus = ::ni_listprops( domain, dir, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// check for existing property with this key
	where = ::ni_namelist_match( nl, key );
	::ni_namelist_free( &nl );

	// if property doesn't exist, create it
	if ( where == NI_INDEX_NULL )
	{
		NI_INIT( &nl );
		ni_namelist_insert( &nl, value, NI_INDEX_NULL );
		NI_INIT( &p );
		p.nip_name = ::ni_name_dup( key );
		p.nip_val = ::ni_namelist_dup( nl );
		niStatus = ::ni_createprop( domain, dir, p, NI_INDEX_NULL );
		::ni_namelist_free( &nl );
		ni_prop_free( &p );
		return( niStatus );
	}

	// property exists: replace the existing values
	// fetch existing namelist for this property
	NI_INIT( &nl );
	niStatus = ::ni_readprop( domain, dir, where, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// insert new value
	ni_namelist_insert( &nl, value, whereval );

	// write the new list back
	niStatus = ::ni_writeprop( domain, dir, where, nl );
	::ni_namelist_free( &nl );

	return( niStatus );

} // InsertDirVal


//------------------------------------------------------------------------------------
//	* MergeProp
//------------------------------------------------------------------------------------

ni_status NiLib2::MergeProp ( void *domain, char *pathname, const ni_name key, ni_namelist values )
{
	// merge a list of values into a property ( to prevent duplicates )
	// creates the property if it doesn't already exist

	ni_status niStatus;
	ni_id dir;

	// need to be talking to the master
	::ni_needwrite( domain, 1 );

	// see if the directory already exists
	niStatus = NiLib2::PathSearch( domain, &dir, pathname );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	return NiLib2::MergeDirProp( domain, &dir, key, values );

} // MergeProp


//------------------------------------------------------------------------------------
//	* MergeDirProp
//------------------------------------------------------------------------------------

ni_status NiLib2::MergeDirProp ( void *domain, ni_id *dir, const ni_name key, ni_namelist values )
{
	// mergeprop given a directory rather than a pathname

	ni_status niStatus;
	ni_property p;
	ni_namelist nl;
	ni_index where, whereval;
	int i;

	// need to be talking to the master
	::ni_needwrite( domain, 1 );

	// fetch list of property keys from directory
	NI_INIT( &nl );
	niStatus = ::ni_listprops( domain, dir, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// check for existing property with this key
	where = ::ni_namelist_match( nl, key );
	::ni_namelist_free( &nl );

	// if property doesn't exist, create it
	if ( where == NI_INDEX_NULL )
	{
		NI_INIT( &p );
		p.nip_name = ::ni_name_dup( key );
		p.nip_val = ::ni_namelist_dup( values );
		niStatus = ::ni_createprop( domain, dir, p, NI_INDEX_NULL );
		ni_prop_free( &p );
		return( niStatus );
	}


	// property exists: replace the existing values
	// fetch existing namelist for this property
	NI_INIT( &nl );
	niStatus = ::ni_readprop( domain, dir, where, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// merge new values
	for ( i = 0; i < (int)values.ni_namelist_len; i++ )
	{
		whereval = ::ni_namelist_match( nl, values.ni_namelist_val[i] );
		if ( whereval == NI_INDEX_NULL )
		{
			ni_namelist_insert( &nl, values.ni_namelist_val[i], NI_INDEX_NULL );
		}
	}

	// write the new list back
	niStatus = ::ni_writeprop( domain, dir, where, nl );
	::ni_namelist_free( &nl );

	return( niStatus );

} // MergeDirProp


//------------------------------------------------------------------------------------
//	* Destroy
//------------------------------------------------------------------------------------

ni_status NiLib2::Destroy ( void *inDomain, ni_id *inDirID )
{
	// destroy a directory
	// this version recursively destroys all subdirectories as well

	ni_status	niStatus;
	ni_id		parent;
	ni_index pi;

	// need to be talking to the master
	::ni_needwrite( inDomain, 1 );

	// get the parent directory index ( nii_object )
	niStatus = ::ni_parent( inDomain, inDirID, &pi );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// get the parent directory id
	parent.nii_object = pi;
	niStatus = ::ni_self( inDomain, &parent );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	return( NiLib2::DestroyDir( inDomain, inDirID, &parent ) );

} // Destroy


//------------------------------------------------------------------------------------
//	* Destroy
//------------------------------------------------------------------------------------

ni_status NiLib2::Destroy ( void *domain, char *pathname )
{
	// destroy a directory
	// this version recursively destroys all subdirectories as well

	ni_status niStatus;
	ni_id dir, parent;
	ni_index pi;

	// need to be talking to the master
	ni_needwrite( domain, 1 );

	// see if the directory exists
	niStatus = NiLib2::PathSearch( domain, &dir, pathname );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// get the parent directory index ( nii_object )
	niStatus = ::ni_parent( domain, &dir, &pi );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// get the parent directory id
	parent.nii_object = pi;
	niStatus = ::ni_self( domain, &parent );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	return( NiLib2::DestroyDir( domain, &dir, &parent ) );

} // Destroy


//------------------------------------------------------------------------------------
//	* DestroyDir
//------------------------------------------------------------------------------------

ni_status NiLib2::DestroyDir ( void *domain, ni_id *dir, ni_id *parent )
{
	// destroy a directory and all it's subdirectories
	// this is the recursive workhorse

	ni_status niStatus;
	int i;
	ni_idlist children;
	ni_id child;

	// need to be talking to the master
	ni_needwrite( domain, 1 );

	// get a list of all my children
	NI_INIT( &children );
	niStatus = ::ni_children( domain, dir, &children );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// destroy each child
	for ( i = 0; i < (int)children.ni_idlist_len; i++ )
	{
		child.nii_object = children.ni_idlist_val[i];
		niStatus = ::ni_self( domain, &child );
		if ( niStatus != NI_OK )
		{
			return( niStatus );
		}
		niStatus = NiLib2::DestroyDir( domain, &child, dir );
		if ( niStatus != NI_OK )
		{
			return( niStatus );
		}
	}

	// free list of child ids
	::ni_idlist_free( &children );

	// destroy myself
	return ::ni_destroy( domain, parent, *dir );
} // 


//------------------------------------------------------------------------------------
//	* DestroyProp
//------------------------------------------------------------------------------------

ni_status NiLib2::DestroyProp ( void *domain, char *pathname, ni_namelist keys )
{
	// destroy a property
	// destroys all properties with the same key

	ni_status niStatus;
	ni_id dir;

	// need to be talking to the master
	ni_needwrite( domain, 1 );

	// see if the directory exists
	niStatus = NiLib2::PathSearch( domain, &dir, pathname );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	return( NiLib2::DestroyDirProp( domain, &dir, keys ) );

} // DestroyProp


//------------------------------------------------------------------------------------
//	* DestroyDirProp
//------------------------------------------------------------------------------------

ni_status NiLib2::DestroyDirProp ( void *domain, ni_id *dir, ni_namelist keys )
{
	// destroyprop given a directory rather than a pathname

	ni_status niStatus;
	ni_index where;
	ni_namelist nl;
	int i;

	// need to be talking to the master
	ni_needwrite( domain, 1 );

	// fetch list of property keys from directory
	NI_INIT( &nl );
	niStatus = ::ni_listprops( domain, dir, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// destroy all occurrences of each key
	for ( i = 0; i < (int)keys.ni_namelist_len; i++ )
	{
		where = ::ni_namelist_match( nl, keys.ni_namelist_val[i] );

		// keep looking for all occurrences
		while ( where != NI_INDEX_NULL )
		{
			niStatus = ::ni_destroyprop( domain, dir, where );
			if ( niStatus != NI_OK )
			{
				::ni_namelist_free( &nl );
				return( niStatus );
			}

			// update the namelist  
			::ni_namelist_delete( &nl, where );
			where = ::ni_namelist_match( nl, keys.ni_namelist_val[i] );
		}
	}
	::ni_namelist_free( &nl );

	return( NI_OK );

} // DestroyDirProp


//------------------------------------------------------------------------------------
//	* DestroyVal
//------------------------------------------------------------------------------------

ni_status NiLib2::DestroyVal( void *domain, char *pathname, const ni_name key, ni_namelist values )
{
	// destroy all occurances of a value in a property

	ni_status niStatus;
	ni_id dir;

	// need to be talking to the master
	ni_needwrite( domain, 1 );

	// see if the directory exists
	niStatus = NiLib2::PathSearch( domain, &dir, pathname );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	return NiLib2::DestroyDirVal( domain, &dir, key, values );
} // 


//------------------------------------------------------------------------------------
//	* DestroyVal
//------------------------------------------------------------------------------------

ni_status NiLib2::DestroyDirVal ( void *domain, ni_id *dir, const ni_name key, ni_namelist values )
{
	// destroyval given a directory rather than a pathname

	ni_status niStatus;
	ni_namelist nl;
	ni_index where, whereval;
	int i;

	// need to be talking to the master
	ni_needwrite( domain, 1 );

	// fetch list of property keys from directory
	NI_INIT( &nl );
	niStatus = ::ni_listprops( domain, dir, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// check for existing property with this key
	where = ::ni_namelist_match( nl, key );
	::ni_namelist_free( &nl );

	// if property doesn't exist, nothing to do
	if ( where == NI_INDEX_NULL )
	{
		return( NI_OK );
	}

	// fetch existing namelist for this property
	NI_INIT( &nl );
	niStatus = ::ni_readprop( domain, dir, where, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// delete values
	for ( i = 0; i < (int)values.ni_namelist_len; i++ )
	{
		whereval = ::ni_namelist_match( nl, values.ni_namelist_val[i] );
		while ( whereval != NI_INDEX_NULL )
		{
			::ni_namelist_delete( &nl, whereval );
			whereval = ::ni_namelist_match( nl, values.ni_namelist_val[i] );
		}
	}

	// write the new list back
	niStatus = ::ni_writeprop( domain, dir, where, nl );
	::ni_namelist_free( &nl );

	return( niStatus );

} // DestroyDirVal


//------------------------------------------------------------------------------------
//	* RenameProp
//------------------------------------------------------------------------------------

ni_status NiLib2::RenameProp ( void *domain, char *pathname, const ni_name oldname, const ni_name newname )
{
	// rename a property

	ni_status niStatus;
	ni_id dir;

	// need to be talking to the master
	::ni_needwrite( domain, 1 );

	// see if the directory already exists
	niStatus = NiLib2::PathSearch( domain, &dir, pathname );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	return( NiLib2::RenameDirProp( domain, &dir, oldname, newname ) );

} // RenameProp


//------------------------------------------------------------------------------------
//	* RenameDirProp
//------------------------------------------------------------------------------------

ni_status NiLib2::RenameDirProp ( void *domain, ni_id *dir, const ni_name oldname, const ni_name newname )
{
	// renameprop given a directory rather than a pathname

	ni_status niStatus;
	ni_index where;
	ni_namelist nl;

	// need to be talking to the master
	::ni_needwrite( domain, 1 );

	// fetch list of property keys from directory
	NI_INIT( &nl );
	niStatus = ::ni_listprops( domain, dir, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// look up old name
	where = ::ni_namelist_match( nl, oldname );
	::ni_namelist_free( &nl );

	// if it's not there, return an error
	if ( where == NI_INDEX_NULL )
	{
		return NI_NOPROP;
	}

	return( ::ni_renameprop( domain, dir, where, newname ) );

} // RenameDirProp


//------------------------------------------------------------------------------------
//	* PathSearch
//------------------------------------------------------------------------------------

ni_status NiLib2::PathSearch( void *domain, ni_id *dir, char *pathname )
{
	// same as pathsearch, but if pathname is an integer
	// then use it as a directory id

	int i, len;
	bool is_id;

	len = strlen( pathname );
	is_id = true;

	for ( i = 0; i < len && is_id; i++ )
		if ( !isdigit( pathname[i] ) ) is_id = false;

	if ( is_id )
	{
		char *endPtr = NULL;
		dir->nii_object = ( unsigned long )strtol( pathname, &endPtr, 10 );
		return( ::ni_self( domain, dir ) );
	}
	else
	{
		return( ::ni_pathsearch( domain, dir, pathname ) );
	}
} // PathSearch


//------------------------------------------------------------------------------------
//	* CreatePath
//------------------------------------------------------------------------------------

ni_status NiLib2::CreatePath ( void *domain, ni_id *dir, char *pathname )
{
	// make a directory with the given pathname

	ni_status niStatus;
	ni_id checkdir;
	int i, j, len;
	char *dirname = NULL;
	bool simple;

	// pull out every pathname component and create the directory
	i = 0;
	while ( pathname[i] != '\0' )
	{

		// search forward for a path component ( a directory )
		simple = true;
		for ( j = i; pathname[j] != '\0' && simple; j++ )
		{
			if ( pathname[j] == '\\' && pathname[j+1] == '/' ) j+=2;
			if ( pathname[j] == '/' ) simple = false;
		}

		len = j - i;
		if ( !simple ) len--;
		dirname = (char *)::malloc( len + 1 );
		strncpy( dirname, pathname+i, len );
		dirname[len] = '\0';

		// advance the pointer
		i = j;

		// does this directory exist?
		checkdir = *dir;
		niStatus = ::ni_pathsearch( domain, dir, dirname );

		// if it doesn't exist, create it
		if ( niStatus == NI_NODIR )
		{
			*dir = checkdir;
			niStatus = NiLib2::CreateChild( domain, dir, dirname );
			if ( niStatus != NI_OK )
			{
				return( niStatus );
			}
		}
		free( dirname );
	}

	return( NI_OK );

} // CreatePath


//------------------------------------------------------------------------------------
//	* CreateChild
//------------------------------------------------------------------------------------

ni_status NiLib2::CreateChild ( void *domain, ni_id *dir, const ni_name dirname )
{
	// make a child directory with the given name

	ni_status	niStatus;
	ni_proplist p;
	ni_id		child;
	int			i, j, k, len;
	char	   *key = NULL;
	char	   *value = NULL;

	// if the name contains "=", then we've got "foo=bar"
	// property key is "foo", not "name"

	len = 0;
	for ( i = 0; dirname[i] != '\0' && dirname[i] != '='; i++ )
		;

	if ( dirname[i] == '=' )
	{
		len = i;
	}

	if ( len > 0 )
	{
		key = (char *)::malloc( len + 1 );
		// check for backslashes in property key
		for ( i = 0, j = 0; i < len; i++, j++ )
		{
			if ( dirname[i] == '\\' && dirname[i+1] == '/' ) i++;
			key[j] = dirname[i];
		}
		key[j] = '\0';
		i = len + 1;
	}
	else
	{
		key = (char *)::malloc( 5 );
		strcpy( key, "name" );
		i = 0;
	}

	// compress out backslashes in value
	j = strlen( dirname );
	len = j - i;
	value = (char *)::malloc( len + 1 );
	for ( k = 0; i < j; k++, i++ )
	{
		if ( dirname[i] == '\\' && dirname[i+1] == '/' )
		{
			i++;
		}
		value[k] = dirname[i];
	}
	value[k] = '\0';
		
	// set up the new directory
	NI_INIT( &p );
	NiLib3::AppendProp( &p, key, value );

	// create it
	niStatus = ::ni_create( domain, dir, p, &child, NI_INDEX_NULL );
	if ( niStatus == NI_OK )
	{
		*dir = child;
	}

	ni_proplist_free( &p );
	free( key );
	free( value );

	return( niStatus );

} // CreateChild


//------------------------------------------------------------------------------------
//	* StatProp
//------------------------------------------------------------------------------------

ni_status NiLib2::StatProp ( void *domain, char *pathname, const ni_name key, ni_index *where )
{
	// match a property in a given property in a given directory

	ni_status niStatus;
	ni_id dir;

	// assume there's no match
	*where = NI_INDEX_NULL;

	// see if the directory exists
	niStatus = NiLib2::PathSearch( domain, &dir, pathname );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	return( NiLib2::StatPropDir( domain, &dir, key, where ) );

} // StatProp


//------------------------------------------------------------------------------------
//	* StatPropDir
//------------------------------------------------------------------------------------

ni_status NiLib2::StatPropDir ( void *domain, ni_id *dir, const ni_name key, ni_index *where )
{
	// statprop given a directory rather than a pathname

	ni_status niStatus;
	ni_namelist nl;

	// assume there's no match
	*where = NI_INDEX_NULL;

	// fetch list of property keys from directory
	NI_INIT( &nl );
	niStatus = ::ni_listprops( domain, dir, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// check for property with this key
	*where = ::ni_namelist_match( nl, key );
	::ni_namelist_free( &nl );

	// if property doesn't exist, no match
	if ( *where == NI_INDEX_NULL )
	{
		return NI_NOPROP;
	}

	return( NI_OK );

} // StatPropDir


//------------------------------------------------------------------------------------
//	* StatVal
//------------------------------------------------------------------------------------

ni_status NiLib2::StatVal ( void *domain, char *pathname, const ni_name key, const ni_name value, ni_index *where )
{
	// match a value in a given property in a given directory

	ni_status niStatus;
	ni_id dir;

	// assume there's no match
	*where = NI_INDEX_NULL;

	// see if the directory exists
	niStatus = NiLib2::PathSearch( domain, &dir, pathname );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	return( NiLib2::StatValDir( domain, &dir, key, value, where ) );

} // StatVal


//------------------------------------------------------------------------------------
//	* StatValDir
//------------------------------------------------------------------------------------

ni_status NiLib2::StatValDir ( void *domain, ni_id *dir, const ni_name key, const ni_name value, ni_index *where )
{
	// statval given a directory rather than a pathname

	ni_status niStatus;
	ni_namelist nl;
	ni_index wh;

	// assume there's no match
	*where = NI_INDEX_NULL;

	// fetch list of property keys from directory
	NI_INIT( &nl );
	niStatus = ::ni_listprops( domain, dir, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// check for property with this key
	wh = ::ni_namelist_match( nl, key );
	::ni_namelist_free( &nl );

	// if property doesn't exist, no match
	if ( wh == NI_INDEX_NULL )
	{
		return NI_NOPROP;
	}

	// fetch existing namelist for this property
	NI_INIT( &nl );
	niStatus = ::ni_readprop( domain, dir, wh, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// check for this value
	wh = ::ni_namelist_match( nl, value );
	::ni_namelist_free( &nl );

	// if value doesn't exist, no match
	if ( wh == NI_INDEX_NULL )
	{
		return NI_NONAME;
	}

	*where = wh;

	return( NI_OK );

} // StatValDir


//------------------------------------------------------------------------------------
//	* ReapProp
//------------------------------------------------------------------------------------

ni_status NiLib2::ReapProp ( void *domain, char *pathname, const ni_name key )
{
	// remove a property in a given directory if the property is empty

	ni_status niStatus;
	ni_id dir;

	// see if the directory exists
	niStatus = NiLib2::PathSearch( domain, &dir, pathname );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	return( NiLib2::ReappropDir( domain, &dir, key ) );

} // ReapProp


//------------------------------------------------------------------------------------
//	* ReappropDir
//------------------------------------------------------------------------------------

ni_status NiLib2::ReappropDir ( void *domain, ni_id *dir, const ni_name key )
{
	// reapprop given a directory rather than a pathname

	ni_status niStatus;
	ni_namelist nl;
	ni_index where;

	// fetch list of property keys from directory
	NI_INIT( &nl );
	niStatus = ::ni_listprops( domain, dir, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// check for property with this key
	where = ::ni_namelist_match( nl, key );
	::ni_namelist_free( &nl );

	// if property doesn't exist, return
	if ( where == NI_INDEX_NULL )
	{
		return( NI_OK );
	}

	// fetch existing namelist for this property
	NI_INIT( &nl );
	niStatus = ::ni_readprop( domain, dir, where, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// if the property contains any values, leave it alone
	if ( nl.ni_namelist_len > 0 )
	{
		::ni_namelist_free( &nl );
		return( NI_OK );
	}

	// property is empty, delete it
	::ni_namelist_free( &nl );

	return( ::ni_destroyprop( domain, dir, where ) );

} // ReappropDir


//------------------------------------------------------------------------------------
//	* ReapDir
//------------------------------------------------------------------------------------

ni_status NiLib2::ReapDir ( void *domain, char *pathname )
{
	// destroy a directory if it has nothing but a name

	ni_status niStatus;
	ni_id dir;
	ni_namelist nl;

	// see if the directory exists
	niStatus = NiLib2::PathSearch( domain, &dir, pathname );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// fetch list of property keys from directory
	NI_INIT( &nl );
	niStatus = ::ni_listprops( domain, &dir, &nl );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// if more than one property, leave it alone
	if ( nl.ni_namelist_len > 1 )
	{
		::ni_namelist_free( &nl );
		return( NI_OK );
	}

	// directory is empty ( except for name ), delete it
	::ni_namelist_free( &nl );

	return( NiLib2::Destroy( domain, pathname ) );

} // ReapDir


//------------------------------------------------------------------------------------
//	* Copy
//------------------------------------------------------------------------------------

ni_status NiLib2::Copy ( void *srcdomain, char *path, void *dstdomain, bool recursive )
{
	// copy a directory from src to dst

	ni_status		niStatus;
	ni_id			srcdir;
	ni_id			dstdir;

	// see if src directory exists
	niStatus = NiLib2::PathSearch( srcdomain, &srcdir, path );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// create dstdir if necessary
	niStatus = NiLib2::Create( dstdomain, path );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// get dstdir
	niStatus = NiLib2::PathSearch( dstdomain, &dstdir, path );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	niStatus = NiLib2::CopyDir( srcdomain, &srcdir, dstdomain, &dstdir, recursive );

	return( niStatus );

} // Copy


//------------------------------------------------------------------------------------
//	* Copy
//------------------------------------------------------------------------------------

ni_status NiLib2::Copy ( void *srcdomain, ni_id *inSrcDirID, char *path, void *dstdomain, bool recursive )
{
	// copy a directory from src to dst

	ni_status		niStatus;
	ni_id			dstdir;

	// create dstdir if necessary
	niStatus = NiLib2::Create( dstdomain, path );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// get dstdir
	niStatus = NiLib2::PathSearch( dstdomain, &dstdir, path );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	niStatus = NiLib2::CopyDir( srcdomain, inSrcDirID, dstdomain, &dstdir, recursive );

	return( niStatus );

} // Copy


//------------------------------------------------------------------------------------
//	* CopyDir
//------------------------------------------------------------------------------------

ni_status NiLib2::CopyDir ( void *srcdomain, ni_id *srcdir, void *dstdomain, ni_id *dstdir , bool recursive )
{
	ni_status	niStatus;
	ni_idlist	children;
	int			i;
	int			len;
	ni_proplist	p;
	ni_id		dir;

	NI_INIT( &p );
	
	// get proplist from src dir
	niStatus = ::ni_read( srcdomain, srcdir, &p );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// write the property list to the dst dir
	niStatus = ::ni_write( dstdomain, dstdir, p );
	if ( niStatus != NI_OK )
	{
		ni_proplist_free( &p );
		return( niStatus );
	}
	
	ni_proplist_free( &p );

	if ( recursive )
	{
		NI_INIT( &children );

		// get list of children
		niStatus = ::ni_children( srcdomain, srcdir, &children );
		if ( niStatus != NI_OK )
		{
			return( niStatus );
		}

		len = children.ni_idlist_len;
		for ( i = 0; i < len; i++ )
		{
			dir.nii_object = children.ni_idlist_val[i];
			niStatus = ::ni_self( srcdomain, &dir );
			if ( niStatus != NI_OK )
			{
				::ni_idlist_free( &children );
				return( niStatus );
			}
			niStatus = NiLib2::CopyDirToParentDir( srcdomain,&dir,dstdomain,dstdir,recursive );
		}
	
		::ni_idlist_free( &children );
	}

	return( NI_OK );

} // CopyDir


//------------------------------------------------------------------------------------
//	* CopyDirToParentDir
//------------------------------------------------------------------------------------

ni_status NiLib2::CopyDirToParentDir ( void *srcdomain, ni_id *srcdir, void*dstdomain, ni_id *dstdir , bool recursive )
{
	ni_status niStatus;
	ni_idlist children;
	int i, len;
	ni_proplist p;
	ni_id dir, newdstdir;

	NI_INIT( &p );
	
	// get proplist from src dir
	niStatus = ::ni_read( srcdomain, srcdir, &p );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	// create the destination dir
	niStatus = ::ni_create( dstdomain, dstdir, p, &newdstdir, NI_INDEX_NULL );
	if ( niStatus != NI_OK )
	{
		ni_proplist_free( &p );
		return( niStatus );
	}
	
	ni_proplist_free( &p );

	if ( recursive )
	{
		NI_INIT( &children );

		// get list of children
		niStatus = ::ni_children( srcdomain, srcdir, &children );
		if ( niStatus != NI_OK )
		{
			return( niStatus );
		}

		len = children.ni_idlist_len;
		for ( i = 0; i < len; i++ )
		{
			dir.nii_object = children.ni_idlist_val[i];
			niStatus = ::ni_self( srcdomain, &dir );
			if ( niStatus != NI_OK )
			{
				::ni_idlist_free( &children );
				return( niStatus );
			}
			niStatus = NiLib2::CopyDirToParentDir( srcdomain,&dir,dstdomain, &newdstdir,recursive );
		}
	
		::ni_idlist_free( &children );
	}

	return( NI_OK );

} // CopyDirToParentDir


//------------------------------------------------------------------------------------
//	* LookUpProp
//------------------------------------------------------------------------------------

ni_status NiLib2::LookUpProp ( void *domain, char *pathname, const ni_name key, ni_namelist *values )
{
	// read a property

	ni_status niStatus;
	ni_id dir;

	// see if the directory exists
	niStatus = NiLib2::PathSearch( domain, &dir, pathname );
	if ( niStatus != NI_OK )
	{
		return( niStatus );
	}

	return( ::ni_lookupprop( domain, &dir, key, values ) );

} // LookUpProp


//------------------------------------------------------------------------------------
//	* InsertSorted
//------------------------------------------------------------------------------------

ni_index NiLib2::InsertSorted ( ni_namelist *values, const ni_name newvalue )
{
	int i, len;

	len = values->ni_namelist_len;
	for ( i = 0; i < len; i++ )
	{
		if ( strcmp( newvalue, values->ni_namelist_val[i] ) <= 0 )
		{
			ni_namelist_insert( values, newvalue, ( ni_index )i );
			return ( ni_index )i;
		}
	}

	ni_namelist_insert( values, newvalue, NI_INDEX_NULL );

	return( NI_INDEX_NULL );

} // InsertSorted


//------------------------------------------------------------------------------------
//	* InAccessList
//------------------------------------------------------------------------------------

ni_status NiLib2::InAccessList( const char* user, ni_namelist access_list)
{
	if (ni_namelist_match(access_list, ACCESS_USER_ANYBODY) != NI_INDEX_NULL)
	{
		return NI_OK;
	}

	if (user == NULL || user[0] == '\0')
	{
		return NI_PERM;
	}

	if (ni_namelist_match(access_list, user) != NI_INDEX_NULL)
	{
		return NI_OK;
	}

	return NI_PERM;
} // InAccessList


//------------------------------------------------------------------------------------
//	* ValidateDir
//------------------------------------------------------------------------------------

ni_status NiLib2::ValidateDir( const char* user, ni_proplist *pl )
{
	ni_index i;

	if ((user != NULL) && (strcmp(user, ACCESS_USER_SUPER) == 0))
	{
		return NI_OK;
	}

	for (i = 0; i < pl->nipl_len; i++)
	{
		if (strcmp(pl->nipl_val[i].nip_name, ACCESS_DIR_KEY) == 0)
		{
			return NiLib2::InAccessList(user, pl->nipl_val[i].nip_val);
		}
	}

	return NI_PERM;
} // ValidateDir


//------------------------------------------------------------------------------------
//	* ValidateName
//------------------------------------------------------------------------------------

ni_status NiLib2::ValidateName( const char* user, ni_proplist *pl, ni_index prop_index )
{
	ni_name key = NULL;
	ni_name propkey;
	ni_index i;

	if ((user != NULL) && (ni_name_match(user, ACCESS_USER_SUPER)))
	{
		return NI_OK;
	}

	propkey = pl->nipl_val[prop_index].nip_name;
	if (propkey == NULL)
	{
		return NI_PERM;
	}
	key = (char*)::calloc(strlen(ACCESS_NAME_PREFIX) + strlen(propkey) + 1, 1);
	sprintf(key, "%s%s", ACCESS_NAME_PREFIX, propkey);

	for (i = 0; i < pl->nipl_len; i++)
	{
		if (ni_name_match(pl->nipl_val[i].nip_name, key))
		{
			ni_name_free(&key);
			return NiLib2::InAccessList(user, pl->nipl_val[i].nip_val);
		}
	}

	ni_name_free(&key);
	return NI_PERM;
} // ValidateName
