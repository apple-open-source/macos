/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#ifndef _SCPPATH_H
#define _SCPPATH_H

#include <CoreFoundation/CoreFoundation.h>
#include <sys/cdefs.h>

/*!
	@header SCPPath.h
	The SystemConfiguration framework provides access to the data used
		to configure a running system.

	Specifically, the SCPPathXXX() API's allow an application to
		load and store XML configuration data in a controlled
		manner and provides the necessary notifications to other
		applications which need to be aware of configuration
		changes.

	The SCPPathXXX() API's make certain assumptions about the layout
		of the preferences data.  These APIs view the data as a
		collection of dictionaries of key/value pairs and an
		associated path name.  The root path ("/") identifies
		the top-level dictionary.  Additional path components
		specify the keys for sub-dictionaries.

		For example, the following dictionary can be access via
		two paths.  The root ("/") path would return a property
		list with all keys and values.  The path "/path1" would
		only return the dictionary with the "key3" and "key4"
		properties.

		<dict>
			<key>key1</key>
			<string>val1</string>
			<key>key2</key>
			<string>val2</string>
			<key>path1</key>
			<dict>
				<key>key3</key>
				<string>val3</string>
				<key>key4</key>
				<string>val4</string>
			</dict>
		</dict>

	The APIs provided by this framework communicate with the "configd"
		daemon for any tasks requiring synchronization and/or
		notification.
 */


__BEGIN_DECLS

/*!
	@function SCPPathCreateUniqueChild
	@discussion Creates a new path component within the dictionary
		hierarchy.
	@param session Pass the SCPSessionRef handle which should be used to
	 communicate with the APIs.
	@param prefix Pass a string which represents the parent path.
	@param newPath A pointer to memory which will be filled with an
		string representing the new child path.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call. Possible return values include: SCP_OK,
		SCP_NOKEY.
 */
SCPStatus	SCPPathCreateUniqueChild	(SCPSessionRef		session,
						 CFStringRef		prefix,
						 CFStringRef		*newPath);

/*!
	@function SCPPathGetValue
	@discussion Returns the dictionary associated with the specified
		path.
	@param session Pass the SCPSessionRef handle which should be used to
	 communicate with the APIs.
	@param path Pass a string whcih represents the path to be returned.
	@param value A pointer to memory which will be filled with an
		dictionary associated with the specified path.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call. Possible return values include: SCP_OK,
		SCP_NOKEY.
 */
SCPStatus	SCPPathGetValue			(SCPSessionRef		session,
						 CFStringRef		path,
						 CFDictionaryRef	*value);

/*!
	@function SCPPathGetLink
	@discussion Returns the link (if one exists) associatd with the
		specified path.
	@param session Pass the SCPSessionRef handle which should be used to
	 communicate with the APIs.
	@param path Pass a string whcih represents the path to be returned.
	@param link A pointer to memory which will be filled with a
		string reflecting the link found at the specified path.
		If no link was present at the specified path a status
		value of SCP_NOKEY will be returned.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call. Possible return values include: SCP_OK,
		SCP_NOKEY.
 */
SCPStatus	SCPPathGetLink			(SCPSessionRef		session,
						 CFStringRef		path,
						 CFStringRef		*link);

/*!
	@function SCPPathSetValue
	@discussion Associates a dictionary with the specified path.
	@param session Pass the SCPSessionRef handle which should be used to
	 communicate with the APIs.
	@param path Pass a string whcih represents the path to be returned.
	@param value Pass a dictionary which represents the data to be
		stored at the specified path.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call. Possible return values include: SCP_OK.
 */
SCPStatus	SCPPathSetValue			(SCPSessionRef		session,
						 CFStringRef		path,
						 CFDictionaryRef	value);

/*!
	@function SCPPathSetLink
	@discussion Associates a link to a second dictionary at the
		specified path.
	@param session Pass the SCPSessionRef handle which should be used to
	 communicate with the APIs.
	@param path Pass a string whcih represents the path to be returned.
	@param value Pass a string which represents the path to be stored
		at the specified path.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call. Possible return values include: SCP_OK,
		SCP_NOKEY.
 */
SCPStatus	SCPPathSetLink			(SCPSessionRef		session,
						 CFStringRef		path,
						 CFStringRef		link);

/*!
	@function SCPPathRemove
	@discussion Removes the data associated with the specified path.
	@param session Pass the SCPSessionRef handle which should be used to
	 communicate with the APIs.
	@param path Pass a string whcih represents the path to be returned.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call. Possible return values include: SCP_OK,
		SCP_NOKEY.
 */
SCPStatus	SCPPathRemove			(SCPSessionRef		session,
						 CFStringRef		path);

__END_DECLS

#endif /* _SCPPATH_H */
