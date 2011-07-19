/*
 * Copyright (c) 2007-2011 Apple Inc. All rights reserved.
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

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netdb.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "autofs.h"
#include "automount.h"

static int hosts_match(const char *host, size_t hostlen, const char *thishost);
static int get_local_host_name(char *localhost, size_t localhost_len);
static int convert_to_write_lock(void);
static void get_my_host_names(void);
static void free_hostinfo_list(void);

/*
 * XXX - is self_check() sufficient for this?  It'll handle the "localhost"
 * case (as 127.0.0.1 will be one of our addresses), and it should
 * handle our primary host name, with or without qualifications.  We
 * need to call it anyway, to handle multi-homing and .local hostnames,
 * and if we can just use self_check(), that avoids a gethostname() system
 * call and some compares.
 *
 * One problem with self_check() is that it can be expensive if you're
 * using it on a lot of names, as it looks up the host name and compares
 * all the IP addresses for that host name with all of the IP addresses
 * for the machine.  If that's done for all the entries in the -fstab
 * map, as would be the case for an "ls -l" done on /Network/Servers or
 * if the Finder's looking at everything in /Network/Servers - as it would
 * in column view when you're looking at anything under /Network/Servers -
 * then, the first time that happens, it does a host name lookup for every
 * server listed there, meaning it could do a DNS lookup for every such
 * host.
 *
 * We use host_is_us() as a "fast path" check; instead of trying to look
 * up the host name, we do a quick comparison against the result of
 * gethostname() (the primary DNS host name) and against the result
 * of SCDynamicStoreCopyLocalHostName() (the local host name) and
 * "localhost", and then check whether it matches the result of a
 * reverse lookup of any of our IP addresses.
 *
 * When we have a loopback file system, so we can use that for entries
 * in -fstab that refer to us, rather than making those entries a symlink
 * to /, we should be able to avoid this hack.
 */

static u_int num_hostinfo;
static struct hostent **hostinfo_list;
static u_int num_host_names;
static char **my_host_names;
static int have_my_host_names;

/*
 * Read/write lock on the host name information.
 */
static pthread_rwlock_t host_name_cache_lock = PTHREAD_RWLOCK_INITIALIZER;

int
host_is_us(const char *host, size_t hostlen)
{
	int err;
	static const char localhost[] = "localhost";
	static char ourhostname[MAXHOSTNAMELEN];
	static char ourlocalhostname[MAXHOSTNAMELEN];
	size_t ourlocalhostnamelen;
	u_int i;

	/*
	 * This is, by definition, us.
	 */
	if (hostlen == sizeof localhost - 1 &&
	    strncasecmp(host, localhost, sizeof localhost - 1) == 0)
		return (1);

	/*
	 * Get our hostname, and compare the counted string we were
	 * handed with the host name - and the first component of
	 * the host name, if it has more than one component.
	 *
	 * For now, we call gethostname() every time, as that
	 * should be reasonably cheap and it avoids us having
	 * to catch notifications for the host name changing.
	 */
	if (gethostname(ourhostname, sizeof ourhostname) == 0) {
		if (hosts_match(host, hostlen, ourhostname))
			return (1);
	}

	/*
	 * Try to get the local host name.  If that works, compare the
	 * counted string we were handed with the host name.
	 *
	 * For now, we call get_local_host_name() every time, as
	 * that should be reasonably cheap (although, if it contacts
	 * configd, it's probably not as cheap as gethostname())
	 * and it avoids us having to catch notifications for the
	 * local host name changing.
	 */
	if (get_local_host_name(ourlocalhostname, sizeof ourlocalhostname)
	    == 0) {
		ourlocalhostnamelen = strlen(ourlocalhostname);
		if (hostlen == ourlocalhostnamelen &&
		    strncasecmp(host, ourlocalhostname, ourlocalhostnamelen)
		       == 0)
			return (1);
	}

	/*
	 * Now we check against all the host names for this host.
	 * We cache those, as that's potentially a bit more
	 * expensive; we do flush that cache if we get a
	 * cache flush notification from automount, as it gets
	 * run with the "-c" flag whenever there's a network
	 * change, so that should be sufficient to catch changes
	 * in our IP addresses.
	 */

	/*
	 * Get a read lock, so the cache doesn't get modified out
	 * from under us.
	 */
	err = pthread_rwlock_rdlock(&host_name_cache_lock);
	if (err != 0) {
		pr_msg("Can't get read lock on host name cache: %s",
		    strerror(err));
		return (0);
	}

	/*
	 * OK, now get the names for all the IP addresses for this host.
	 */
	if (!have_my_host_names) {
		/*
		 * We have to get the local host name; convert the read lock
		 * to a write lock, if we haven't done so already.
		 */
		if (!convert_to_write_lock()) {
			/* convert_to_write_lock() released the read lock */
			return (0);
		}
		get_my_host_names();
	}

	/*
	 * Check against all of those names.
	 */
	if (have_my_host_names) {
		for (i = 0; i < num_host_names; i++) {
			if (hosts_match(host, hostlen, my_host_names[i])) {
				pthread_rwlock_unlock(&host_name_cache_lock);
				return (1);
			}
		}
	}

	/* Not us. */
	pthread_rwlock_unlock(&host_name_cache_lock);
	return (0);
}

static int
hosts_match(const char *host, size_t hostlen, const char *matchhost)
{
	size_t matchhost_len;
	const char *p;

	matchhost_len = strlen(matchhost);
	if (hostlen == matchhost_len &&
	    strncasecmp(host, matchhost, matchhost_len) == 0)
		return (1);

	/*
	 * Compare the counted string we were handed with the first
	 * component of the host name, if it has more than one component.
	 */
	p = strchr(matchhost, '.');
	if (p != NULL) {
		matchhost_len = p - matchhost;
		if (hostlen == matchhost_len &&
		    strncasecmp(host, matchhost, matchhost_len) == 0)
			return(1);
	}

	return (0);
}

static int
get_local_host_name(char *ourlocalhostname, size_t ourlocalhostnamelen)
{
	SCDynamicStoreRef store;
	CFStringRef ourlocalhostname_CFString;
	Boolean ret;

	store = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("automountd"),
	    NULL, NULL);
	if (store == NULL)
		return (-1);

	ourlocalhostname_CFString = SCDynamicStoreCopyLocalHostName(store);
	CFRelease(store);
	if (ourlocalhostname_CFString == NULL)
		return (-1);

	ret = CFStringGetCString(ourlocalhostname_CFString, ourlocalhostname,
	    ourlocalhostnamelen, kCFStringEncodingUTF8);
	CFRelease(ourlocalhostname_CFString);
	if (!ret)
		return (-1);

	/*
	 * That won't have ".local" at the end; add it.
	 */
	if (strlcat(ourlocalhostname, ".local", ourlocalhostnamelen)
	    >= ourlocalhostnamelen)
		return (-1);	/* didn't fit in the buffer */
	return (0);
}

static int
convert_to_write_lock(void)
{
	int err;

	pthread_rwlock_unlock(&host_name_cache_lock);
	err = pthread_rwlock_wrlock(&host_name_cache_lock);
	if (err != 0) {
		pr_msg("Error attempting to get write lock on host name cache: %s",
		    strerror(err));
		return (0);
	}
	return (1);
}

static void
get_my_host_names(void)
{
	struct ifaddrs *ifaddrs, *ifaddr;
	struct sockaddr *addr;
	struct sockaddr_in *addr_in;
#if 0
	struct sockaddr_in6 *addr_in6;
#endif
	int error_num;
	struct hostent **hostinfop;
	struct hostent *hostinfo;
	u_int i;
	char **host_namep;
	char **aliasp;

	/*
	 * If we already have the list of host names, presumably
	 * that was fetched by another thread in between releasing
	 * the read lock on the list and getting the write lock;
	 * just return.  (have_my_host_names is modified only
	 * when the write lock is held.)
	 */
	if (have_my_host_names)
		return;

	if (getifaddrs(&ifaddrs) == -1) {
		pr_msg("getifaddrs failed: %s\n", strerror(errno));
		return;
	}

	/*
	 * What's the maximum number of hostinfo structures we'd have?
	 * (This counts all IPv4 and IPv6 addresses; we might not be
	 * able to get information for some of them, so we won't
	 * necessarily store hostinfo pointers for all of them.)
	 */
	num_hostinfo = 0;
	for (ifaddr = ifaddrs; ifaddr != NULL; ifaddr = ifaddr->ifa_next) {
		addr = ifaddr->ifa_addr;
		switch (addr->sa_family) {

		case AF_INET:
		case AF_INET6:
			num_hostinfo++;
			break;

		default:
			break;
		}
	}

	/*
	 * Allocate the array of hostinfo structures.
	 */
	hostinfo_list = malloc(num_hostinfo * sizeof *hostinfo_list);
	if (hostinfo_list == NULL) {
		freeifaddrs(ifaddrs);
		pr_msg("Couldn't allocate array of hostinfo pointers\n");
		return;
	}
	
	/*
	 * Fill in the array of hostinfo pointers, and count how many
	 * hostinfo pointers and host names we have.
	 */
	hostinfop = hostinfo_list;
	num_hostinfo = 0;
	num_host_names = 0;
	for (ifaddr = ifaddrs; ifaddr != NULL; ifaddr = ifaddr->ifa_next) {
		addr = ifaddr->ifa_addr;
		switch (addr->sa_family) {

		case AF_INET:
			addr_in = (struct sockaddr_in *)addr;
			hostinfo = getipnodebyaddr(&addr_in->sin_addr,
			    sizeof addr_in->sin_addr, addr->sa_family,
			    &error_num);
			break;

#if 0	// until IPv6 reverse-DNS lookups are fixed - 8650817
		case AF_INET6:
			addr_in6 = (struct sockaddr_in6 *)addr;
			hostinfo = getipnodebyaddr(&addr_in6->sin6_addr,
			    sizeof addr_in6->sin6_addr, addr->sa_family,
			    &error_num);
			break;
#endif

		default:
			hostinfo = NULL;
			break;
		}
		if (hostinfo != NULL) {
			*hostinfop++ = hostinfo;
			num_hostinfo++;
			num_host_names++;		/* main name */
			for (aliasp = hostinfo->h_aliases; *aliasp != NULL;
			    aliasp++)
				num_host_names++;	/* alias */
		}
	}
	freeifaddrs(ifaddrs);

	/*
	 * Allocate the array of host name pointers.
	 */
	my_host_names = malloc(num_host_names * sizeof *my_host_names);
	if (my_host_names == NULL) {
		free_hostinfo_list();
		pr_msg("Couldn't allocate array of host name pointers\n");
		return;
	}

	/*
	 * Fill in the array of host name pointers.
	 */
	host_namep = my_host_names;
	for (i = 0; i < num_hostinfo; i++) {
		hostinfo = hostinfo_list[i];
		*host_namep++ = hostinfo->h_name;
		for (aliasp = hostinfo->h_aliases; *aliasp != NULL; aliasp++)
			*host_namep++ = *aliasp;
	}

	have_my_host_names = 1;
}

static void
free_hostinfo_list(void)
{
	u_int i;

	for (i = 0; i < num_hostinfo; i++)
		freehostent(hostinfo_list[i]);
	free(hostinfo_list);
	hostinfo_list = NULL;
	num_hostinfo = 0;
}

void
flush_host_name_cache(void)
{
	int err;

	err = pthread_rwlock_wrlock(&host_name_cache_lock);
	if (err != 0) {
		pr_msg("Error attempting to get write lock on host name cache: %s",
		    strerror(err));
		return;
	}

	/*
	 * Discard the host information, if we have any.
	 */
	if (have_my_host_names) {
		free_hostinfo_list();
		free(my_host_names);
		my_host_names = NULL;
		num_host_names = 0;
		have_my_host_names = 0;
	}

	pthread_rwlock_unlock(&host_name_cache_lock);
}
