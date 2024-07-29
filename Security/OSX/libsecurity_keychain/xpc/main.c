/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
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

#include <xpc/private.h>
#include <syslog.h>
#include <sys/param.h>
#include <sandbox.h>
#include <dlfcn.h>
#include <sysexits.h>
#include <Security/Security.h>
#include <Security/SecKeychainPriv.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CoreFoundation.h>
#include <sys/types.h>
#include <pwd.h>

#import "utilities/debugging.h"
#import "utilities/SecCFRelease.h"

struct connection_info {
    xpc_connection_t peer;
    int processed;
    int done;
};

// prior to 8723022 we have to do our own idle timeout work
#ifndef XPC_HANDLES_IDLE_TIMEOUT
int current_connections = 0;
// Number of seconds to sit with no clients
#define IDLE_WAIT_TIME 30
#endif

xpc_object_t keychain_prefs_path = NULL;
xpc_object_t home = NULL;

extern xpc_object_t
xpc_create_reply_with_format(xpc_object_t original, const char * format, ...);

static
xpc_object_t create_keychain_search_list_for(xpc_connection_t peer, SecPreferencesDomain domain)
{
    CFArrayRef keychains = NULL;
    pid_t peer_pid = xpc_connection_get_pid(peer);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    OSStatus status = SecKeychainCopyDomainSearchList(domain, &keychains);
#pragma clang diagnostic pop
    if (errSecSuccess != status) {
        secerror("Unable to get keychain search list (domain=%d) on behalf of %d, status=0x%lx", domain, peer_pid, (unsigned long)status);
        return NULL;
    }
    
    xpc_object_t paths = xpc_array_create(NULL, 0);
	CFIndex n_keychains = CFArrayGetCount(keychains);
	CFIndex i;
	for(i = 0; i < n_keychains; i++) {
		char path[MAXPATHLEN];
		
        SecKeychainRef keychain = (SecKeychainRef)CFArrayGetValueAtIndex(keychains, i);
        UInt32 length = MAXPATHLEN;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        OSStatus status = SecKeychainGetPath(keychain, &length, path);
#pragma clang diagnostic pop
        if (errSecSuccess != status) {
            secerror("Unable to get path for keychain#%ld of %ld on behalf of %d, status=0x%lx", i, n_keychains, peer_pid, (unsigned long)status);
            continue;
        }
        xpc_object_t path_as_xpc_string = xpc_string_create(path);
		xpc_array_append_value(paths, path_as_xpc_string);
        xpc_release(path_as_xpc_string);		
	}
    CFRelease(keychains);
    return paths;
}

static
bool keychain_domain_needs_writes(const char *domain_name)
{
	return (0 == strcmp("kSecPreferencesDomainUser", domain_name) || 0 == strcmp("kSecPreferencesDomainDynamic", domain_name));
}

static
void _set_keychain_search_lists_for_domain(xpc_connection_t peer, xpc_object_t all_domains, char *domain_name, SecPreferencesDomain domain_enum)
{
    xpc_object_t keychains_for_domain = create_keychain_search_list_for(peer, domain_enum);
    if (keychains_for_domain) {
        xpc_dictionary_set_value(all_domains, domain_name, keychains_for_domain);
        xpc_release(keychains_for_domain);
    } else {
        secerror("Can't discover keychain paths for domain %s on behalf of %d", domain_name, xpc_connection_get_pid(peer));
    }
}

#define SET_KEYCHAIN_SEARCH_LISTS_FOR_DOMAIN(peer, all_domains, domain) _set_keychain_search_lists_for_domain(peer, all_domains, #domain, domain);

static
xpc_object_t create_keychain_search_lists(xpc_connection_t peer)
{
	xpc_object_t all_domains = xpc_dictionary_create(NULL, NULL, 0);
    
    SET_KEYCHAIN_SEARCH_LISTS_FOR_DOMAIN(peer, all_domains, kSecPreferencesDomainUser);
    SET_KEYCHAIN_SEARCH_LISTS_FOR_DOMAIN(peer, all_domains, kSecPreferencesDomainSystem);
    SET_KEYCHAIN_SEARCH_LISTS_FOR_DOMAIN(peer, all_domains, kSecPreferencesDomainCommon);
    SET_KEYCHAIN_SEARCH_LISTS_FOR_DOMAIN(peer, all_domains, kSecPreferencesDomainDynamic);
	
    return all_domains;
}


static
xpc_object_t create_keychain_and_lock_paths(xpc_connection_t peer, xpc_object_t keychain_path_dict)
{
	pid_t peer_pid = xpc_connection_get_pid(peer);
	char *assembly_queue_label = NULL;
    asprintf(&assembly_queue_label, "assembly-for-%d", peer_pid);
    if (!assembly_queue_label) {
        secerror("Unable to create assembly queue label for %d", peer_pid);
        return NULL;
    }
    dispatch_queue_t assembly_queue = dispatch_queue_create(assembly_queue_label, 0);
    free(assembly_queue_label);
    if (!assembly_queue) {
        secerror("Unable to create assembly queue for %d", peer_pid);
        return NULL;
    }
	xpc_object_t return_paths_dict = xpc_dictionary_create(NULL, NULL, 0);
	
	xpc_dictionary_apply(keychain_path_dict, ^(const char *keychain_domain, xpc_object_t keychain_path_array) {
		xpc_object_t return_paths_array = xpc_array_create(NULL, 0);
		dispatch_apply(xpc_array_get_count(keychain_path_array), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(size_t i) {
			xpc_object_t path_as_xpc_string = xpc_array_get_value(keychain_path_array, i);

            // We should not return sandbox exceptions for non-keychain files. Check the file before returning the path.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            SecKeychainRef purportedKeychain = NULL;
            OSStatus openStatus = SecKeychainOpen(xpc_string_get_string_ptr(path_as_xpc_string), &purportedKeychain);

            // Try to fetch the 'keychain version', which will force the file to be opened and parsed as a keychain.
            // If the file doesn't exist, we will get the errSecNoSuchKeychain error. In that case, return a sandbox exception: 
            //     the file might be added later on.
            // If XPCKeychainSandboxCheck does not have sandbox access to the file, we will get errSecNoSuchKeychain. In that case,
            //     add the file as before, and rely on the creation of the sandbox exception to fail later on.
            // If the file exists, is readable, and is not a keychain, we will get an error (usually errSecInvalidKeychain).
            UInt32 keychainVersion = 0xffffffff;
            OSStatus versionStatus = SecKeychainGetKeychainVersion(purportedKeychain, &keychainVersion);

            secinfo("keychain_xpc_sandbox", "after opening %s: openStatus: %d versionStatus: %d keychain version: %d",
                    xpc_string_get_string_ptr(path_as_xpc_string),
                    openStatus,
                    versionStatus,
                    keychainVersion);

            CFReleaseNull(purportedKeychain);
#pragma clang diagnostic pop

            bool returnSandboxExceptionForPath = openStatus == errSecSuccess && (versionStatus == errSecSuccess || versionStatus == errSecNoSuchKeychain);
            if(!returnSandboxExceptionForPath) {
                secnotice("keychain_xpc_sandbox", "Not returning sandbox extension for non-keychain file %s: openStatus: %d versionStatus: %d keychain version: %d",
                          xpc_string_get_string_ptr(path_as_xpc_string),
                          openStatus,
                          versionStatus,
                          keychainVersion);
                return;
            }

			dispatch_sync(assembly_queue, ^{
				xpc_array_append_value(return_paths_array, path_as_xpc_string);
			});
			
			if (!keychain_domain_needs_writes(keychain_domain)) {
				// lock files are only to prevent write-write errors, readers don't hold locks, so they don't need the lock files
				return;
			}
			
            // figure out the base and dir
            const char* path = xpc_array_get_string(keychain_path_array, i);
            char* dir = NULL;
            char* base = NULL;
            
            char buffer[PATH_MAX];
            strcpy(buffer, path);
            
            if (path != NULL) {
                ptrdiff_t i = strlen(buffer) - 1;
                while (i >= 0 && buffer[i] != '/') {
                    i -= 1;
                }
                
                if (i >= 0) {
                    // NULL terminate the dir
                    buffer[i] = 0;
                    dir = buffer;
                    base = buffer + i + 1;
                } else {
                    dir = NULL;
                    base = buffer;
                }
            }

            if (!(path && dir && base)) {
                secerror("Can't get dir or base (likely out of memory) for %s", xpc_array_get_string(keychain_path_array, i));
                return;
            }
			
			// "network style" lock files
			path_as_xpc_string = xpc_string_create_with_format("%s/lck~%s", dir, base);
			dispatch_sync(assembly_queue, ^{
				xpc_array_append_value(return_paths_array, path_as_xpc_string);
			});
			xpc_release(path_as_xpc_string);

            CC_SHA1_CTX sha1Context;
            CC_SHA1_Init(&sha1Context);
            CC_SHA1_Update(&sha1Context, base, (CC_LONG)strlen(base));
            
            unsigned char sha1_result_bytes[CC_SHA1_DIGEST_LENGTH];
            
            CC_SHA1_Final(sha1_result_bytes, &sha1Context);

			path_as_xpc_string = xpc_string_create_with_format("%s/.fl%02X%02X%02X%02X", dir, sha1_result_bytes[0], sha1_result_bytes[1], sha1_result_bytes[2], sha1_result_bytes[3]);
			dispatch_sync(assembly_queue, ^{
				xpc_array_append_value(return_paths_array, path_as_xpc_string);
			});
		});
		xpc_dictionary_set_value(return_paths_dict, keychain_domain, return_paths_array);
		xpc_release(return_paths_array);
		return (bool)true;
	});
	
	dispatch_release(assembly_queue);
	return return_paths_dict;
}

static
xpc_object_t create_one_sandbox_extension(xpc_object_t path, bool read_only)
{
	const char * extension_class = read_only ? APP_SANDBOX_READ : APP_SANDBOX_READ_WRITE;

    secinfo("keychain_xpc_sandbox", "Returning sandbox extension for %s", xpc_string_get_string_ptr(path));
	char *sandbox_extension = sandbox_extension_issue_file(extension_class, xpc_string_get_string_ptr(path), SANDBOX_EXTENSION_CANONICAL);
	if (sandbox_extension) {
		xpc_object_t sandbox_extension_as_xpc_string = xpc_string_create(sandbox_extension);
        free(sandbox_extension);
        return sandbox_extension_as_xpc_string;
	} else {
		secerror("Can't get sandbox fs extension for %s", xpc_string_get_string_ptr(path));
	}
	return NULL;
}

static
xpc_object_t create_all_sandbox_extensions(xpc_object_t path_dict)
{
    xpc_object_t extensions = xpc_array_create(NULL, 0);
	
	xpc_object_t sandbox_extension = create_one_sandbox_extension(keychain_prefs_path, true);
	if (sandbox_extension) {
		xpc_array_append_value(extensions, sandbox_extension);
		xpc_release(sandbox_extension);
	}

	xpc_dictionary_apply(path_dict, ^(const char *keychain_domain, xpc_object_t path_array) {
		bool read_only = true;
		if (keychain_domain_needs_writes(keychain_domain)) {
			read_only = false;
		}
		xpc_array_apply(path_array, ^(size_t index, xpc_object_t path) {
			xpc_object_t sandbox_extension = create_one_sandbox_extension(path, read_only);
			if (sandbox_extension) {
				xpc_array_append_value(extensions, sandbox_extension);
				xpc_release(sandbox_extension);
			}
			return (bool)true;
		});
		return (bool)true;
	});
	    
    return extensions;
}

static
void handle_request_event(struct connection_info *info, xpc_object_t event)
{
    xpc_connection_t peer = xpc_dictionary_get_remote_connection(event);
    xpc_type_t xtype = xpc_get_type(event);
    if (info->done) {
        secerror("event %p while done", event);
        return;
    }
	if (xtype == XPC_TYPE_ERROR) {
		if (event == XPC_ERROR_TERMINATION_IMMINENT) {
			// launchd would like us to die, but we have open transactions.   When we finish with them xpc_service_main
			// will exit for us, so there is nothing for us to do here.
			return;
		}
		
        if (!info->done) {
            info->done = true;
            xpc_release(info->peer);
#ifndef XPC_HANDLES_IDLE_TIMEOUT
            if (0 == __sync_add_and_fetch(&current_connections, -1)) {
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * IDLE_WAIT_TIME), dispatch_get_main_queue(), ^(void) {
                    if (0 == current_connections) {
                        exit(0);
                    }
                });
            }
#endif
        }
        if (peer == NULL && XPC_ERROR_CONNECTION_INVALID == event && 0 != info->processed) {
            // this is a normal shutdown on a connection that has processed at least
            // one request.   Nothing intresting to log.
            return;
        }
		secerror("listener event error (connection %p): %s", peer, xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION));
	} else if (xtype == XPC_TYPE_DICTIONARY) {
        const char *operation = xpc_dictionary_get_string(event, "op");
        if (operation && !strcmp(operation, "GrantKeychainPaths")) {
            secinfo("xpc_keychain_sandbox", "granting keychain sandbox extensions for %@", peer);
            xpc_object_t keychain_paths = create_keychain_search_lists(peer);
			xpc_object_t all_paths = create_keychain_and_lock_paths(peer, keychain_paths);
            xpc_object_t sandbox_extensions = NULL;
            if (all_paths) {
                sandbox_extensions = create_all_sandbox_extensions(all_paths);
            }
			
            xpc_object_t reply = xpc_create_reply_with_format(event, "{keychain-paths: %value, all-paths: %value, extensions: %value, keychain-home: %value}",
															  keychain_paths, all_paths, sandbox_extensions, home);
            xpc_connection_send_message(peer, reply);
            xpc_release(reply);
            if (keychain_paths) {
                xpc_release(keychain_paths);
            }
            if (all_paths) {
                xpc_release(all_paths);
            }
            if (sandbox_extensions) {
                xpc_release(sandbox_extensions);
            }
            if (INT32_MAX != info->processed) {
                info->processed++;
            }
        } else {
            secerror("Unknown op=%s request from pid %d", operation, xpc_connection_get_pid(peer));
        }
    } else {
		secerror("Unhandled request event=%p type=%p", event, xtype);
    }
}

static
void finalize_connection(void *not_used)
{
#ifdef XPC_HANDLES_IDLE_TIMEOUT
	xpc_transaction_end();
#endif
}

static
void handle_connection_event(const xpc_connection_t peer)
{
#ifndef XPC_HANDLES_IDLE_TIMEOUT
    __sync_add_and_fetch(&current_connections, 1);
#endif
    __block struct connection_info info;
    info.peer = peer;
    info.processed = 0;
    info.done = false;
    
    xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
        handle_request_event(&info, event);
    });

    //  unlike dispatch objects xpc objects don't need a context set in order to run a finalizer.   (we use our finalizer to 
    // end the transaction we are about to begin...this keeps xpc from idle exiting us while we have a live connection)
    xpc_connection_set_finalizer_f(peer, finalize_connection);
#ifdef XPC_HANDLES_IDLE_TIMEOUT
    xpc_transaction_begin();
#endif
    
    // enable the peer connection to receive messages
    xpc_connection_resume(peer);
    xpc_retain(peer);
}


static const char* g_path_to_plist = "/Library/Preferences/com.apple.security.plist";



int main(int argc, const char *argv[])
{
    // get the home directory
    const char* home_dir = getenv("HOME");

    if (home_dir == NULL || strlen(home_dir) == 0) {
        struct passwd* pwd = getpwuid(getuid());
        home_dir = pwd->pw_dir; // look it up in directory services, sort of...
    }

    size_t home_dir_length = strlen(home_dir);
    size_t path_to_plist_length = strlen(g_path_to_plist);
    
    size_t total_length = home_dir_length + path_to_plist_length + 1; // compensate for terminating zero
    if (total_length > PATH_MAX) {
        // someone is spoofing us, just exit
        return -1;
    }
    
    // make storage for the real path
    char buffer[PATH_MAX];
    strlcpy(buffer, home_dir, sizeof(buffer));
    strlcat(buffer, g_path_to_plist, sizeof(buffer));
    keychain_prefs_path = xpc_string_create(buffer);
    home = xpc_string_create(home_dir);

    xpc_main(handle_connection_event);
    
    return EX_OSERR;
}
