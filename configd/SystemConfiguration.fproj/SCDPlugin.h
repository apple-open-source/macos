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

#ifndef _SCDPLUGIN_H
#define _SCDPLUGIN_H

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <CoreFoundation/CoreFoundation.h>


/*!
	@header SCDPlugin
 */


/*
	@define	kSCBundleRequires
 */
#define kSCBundleRequires	CFSTR("Requires")


/*
	@define	kSCBundleVerbose
 */
#define kSCBundleVerbose	CFSTR("Verbose")


/*!
	@typedef SCDynamicStoreBundleLoadFunction
	@discussion Type of the load() initialization function that will be
		called when a plug-in is loaded.  This function is called
		before calling the start() function and can be uesd to
		initialize any variables, open any sessions with "configd",
		and register any needed notifications.
	@param bundle The CFBundle being loaded.
	@param verbose A boolean value indicating whether verbose logging has
		been enabled for this bundle.
 */
typedef void	(*SCDynamicStoreBundleLoadFunction)	(CFBundleRef	bundle,
							 Boolean	bundleVerbose);

/*!
	@typedef SCDynamicStoreBundleStartFunction
	@discussion Type of the start() initialization function that will be
		called after all plug-ins have been loaded and their load()
		functions have been called.  This function can initialize
		variables, open sessions with "configd", and register any
		needed notifications.
	@param bundleName The name of the plug-in / bundle.
	@param bundlePath The path name associated with the plug-in / bundle.
 */
typedef void	(*SCDynamicStoreBundleStartFunction)	(const char	*bundleName,
							 const char	*bundlePath);

/*!
	@typedef SCDynamicStoreBundlePrimeFunction
	@discussion Type of the prime() initialization function that will be
		called after all plug-ins have executed their start() function but
		before the main plug-in run loop is started.  This function should
		be used to initialize any configuration information and/or state
		in the store.
 */
typedef void	(*SCDynamicStoreBundlePrimeFunction)	();


/*!
        @typedef SCDPluginExecCallBack
	@discussion Type of the callback function used when a child process
		started by a plug-in has exited.
	@param pid The process id of the child which has exited.
	@param status The exit status of the child which has exited.
	@param rusage A summary of the resources used by the child process
		and all its children.
	@param context The callback argument specified on the call
 		to _SCDPluginExecCommand(). 
 */
typedef void (*SCDPluginExecCallBack)	(pid_t		pid,
                                         int		status,
                                         struct rusage	*rusage,
                                         void		*context);


__BEGIN_DECLS

/*!
	@function _SCDPluginExecCommand
        @discussion Starts a child process.
        @param callout The function to be called when the child
		process exits.  A NULL value can be specified if no
 		callouts are desired.
        @param context A argument which will be passed
 		to the callout function.
        @param uid The desired user id of the child process.
	@param gid The desired group id of the child process.
	@param path The command to be executed.
	@param argv The arguments to be passed to the child process.
        @result The process ID of the child.
 */
pid_t
_SCDPluginExecCommand	(
                        SCDPluginExecCallBack	callout,
                        void			*context,
                        uid_t			uid,
                        gid_t			gid,
                        const char		*path,
                        char * const 		argv[]
                        );

__END_DECLS

#endif /* _SCDPLUGIN_H */
