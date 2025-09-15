/* ifpermit.c
 *
 * Copyright (c) 2024-2025 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Implementation of a permitted interface list object, which maintains a list of
 * interfaces on which we are permitted to provide some service.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>
#include <dns_sd.h>
#include <net/if.h>
#include <inttypes.h>
#include <sys/resource.h>
#include <netinet/icmp6.h>


#include "srp.h"
#include "ifpermit.h"
#include "dns-msg.h"
#include "ioloop.h"
#include "srp-mdns-proxy.h"
#include "srp-strict.h"

// If we aren't able to allocate a permitted interface list, we still need to return a non-NULL value so that we don't
// fail open. So all of these functions need to treat this particular value as special but not dereference it.
#define PERMITTED_INTERFACE_LIST_BLOCKED (ifpermit_list_t *)1

typedef struct ifpermit_name ifpermit_name_t;
struct ifpermit_name {
    ifpermit_name_t *next;
    char *name; // Interface name
    uint32_t ifindex; // Interface index
    int count;        // Number of permittors for this interface
};

struct ifpermit_list {
    int ref_count;
    ifpermit_name_t *names;
};

void
ifpermit_list_add(ifpermit_list_t *permits, const char *name)
{
    if (permits == PERMITTED_INTERFACE_LIST_BLOCKED) {
        ERROR("blocked permit list when adding " PUB_S_SRP, name);
        return;
    }
    ifpermit_name_t **pname = &permits->names;
    ifpermit_name_t *permit_name;
    while (*pname != NULL) {
        permit_name = *pname;
        if (!strcmp(name, permit_name->name)) {
        success:
            permit_name->count++;
            INFO("%d permits for interface " PUB_S_SRP " with index %d", permit_name->count, name, permit_name->ifindex);
            return;
        }
        pname = &permit_name->next;
    }
    permit_name = srp_strict_calloc(1, sizeof(*permit_name));
    if (permit_name != NULL) {
        permit_name->name = srp_strict_strdup(name);
        if (permit_name->name == NULL) {
            srp_strict_free(&permit_name);
            permit_name = NULL;
        } else {
            permit_name->ifindex = if_nametoindex(name);
            if (permit_name->ifindex == 0) {
                ERROR("if_nametoindex for interface " PUB_S_SRP " returned 0.", name);
                srp_strict_free(&permit_name->name);
                srp_strict_free(&permit_name);
                return;
            }
            *pname = permit_name;
            goto success;
        }
    }
    ERROR("no memory to add permit for " PUB_S_SRP, name);
}

void
ifpermit_list_remove(ifpermit_list_t *permits, const char *name)
{
    if (permits == PERMITTED_INTERFACE_LIST_BLOCKED) {
        ERROR("blocked permit list when removing " PUB_S_SRP, name);
        return;
    }
    if (permits == NULL) {
        INFO("no permit list when removing " PUB_S_SRP, name);
        return;
    }
    ifpermit_name_t **pname = &permits->names;
    ifpermit_name_t *permit_name;
    while (*pname != NULL) {
        permit_name = *pname;
        if (!strcmp(name, permit_name->name)) {
            permit_name->count--;
            INFO("%d permits for interface " PUB_S_SRP " with index %d", permit_name->count, name, permit_name->ifindex);
            if (permit_name->count == 0) {
                *pname = permit_name->next;
                srp_strict_free(&permit_name->name);
                srp_strict_free(&permit_name);
            }
            return;
        }
        pname = &permit_name->next;
    }

    FAULT("permit remove for interface " PUB_S_SRP " which does not exist", name);
}

static void
ifpermit_list_finalize(ifpermit_list_t *list)
{
    if (list != NULL && list != PERMITTED_INTERFACE_LIST_BLOCKED) {
        ifpermit_name_t *names = list->names, *next = NULL;
        while (names != NULL) {
            next = names->next;
            srp_strict_free(&names->name);
            srp_strict_free(&names);
            names = next;
        }
        srp_strict_free(&list);
    }
}

void
ifpermit_list_retain_(ifpermit_list_t *list, const char *file, int line)
{
    if (list != NULL && list != PERMITTED_INTERFACE_LIST_BLOCKED) {
        RETAIN(list, ifpermit_list);
    }
}

void
ifpermit_list_release_(ifpermit_list_t *list, const char *file, int line)
{
    if (list != NULL && list != PERMITTED_INTERFACE_LIST_BLOCKED) {
        RELEASE(list, ifpermit_list);
    }
}

ifpermit_list_t *
ifpermit_list_create_(const char *file, int line)
{
    ifpermit_list_t *permits = srp_strict_calloc(1, sizeof(*permits));
    if (permits == NULL) {
        return PERMITTED_INTERFACE_LIST_BLOCKED;
    }
    RETAIN(permits, ifpermit_list);
    return permits;
}

bool
ifpermit_interface_index_is_listed(ifpermit_list_t *permits, uint32_t ifindex)
{
    if (permits != NULL && permits != PERMITTED_INTERFACE_LIST_BLOCKED) {
        for (ifpermit_name_t *name = permits->names; name != NULL; name = name->next) {
            if (name->ifindex == ifindex) {
                return true;
            }
        }
    }
    return false;
}

bool
ifpermit_interface_name_is_listed(ifpermit_list_t *permits, const char *name)
{
    if (permits == NULL) {
        return false;
    }
    if (permits != NULL && permits != PERMITTED_INTERFACE_LIST_BLOCKED) {
        for (ifpermit_name_t *permit = permits->names; permit != NULL; permit = permit->next) {
            if (!strcmp(permit->name, name)) {
                return true;
            }
        }
    }
    return false;
}

void
ifpermit_add_permitted_interface_to_server_(srp_server_t *NONNULL server_state, const char *NONNULL name,
                                            const char *file, int line)
{
    if (server_state->permitted_interfaces == NULL) {
        server_state->permitted_interfaces = ifpermit_list_create_(file, line);
    }
    ifpermit_list_add(server_state->permitted_interfaces, name);
}

void
ifpermit_save_permit_list_to_prefs(ifpermit_list_t *list, const char *preference_name)
{
    char iflist[128]; // Shouldn't really be more than one or perhaps two interfaces on this list.

    if (list != NULL && list->names != NULL) {
        char *listp = iflist;
        char *list_lim = listp + sizeof(iflist);
        for (ifpermit_name_t *permit = list->names; permit != NULL; permit = permit->next) {
            size_t namelen = strlen(permit->name);
            if (listp + namelen + 2 < list_lim) {
                if (listp != iflist) {
                    *listp++ = ',';
                }
                strcpy(listp, permit->name);
                listp += namelen;
                *listp = 0;
            }
        }
    } else {
        iflist[0] = 0;
    }

    CFStringRef app_id = CFSTR("com.apple.srp-mdns-proxy.preferences");
    CFStringRef key = CFStringCreateWithCString(NULL, preference_name, kCFStringEncodingASCII);
    OSStatus error = 1;
    if (key != NULL) {
        error = CFPrefs_SetCString(app_id, key, iflist, strlen(iflist));
    }
    INFO(PUB_S_SRP " '" PUB_S_SRP "' to pref string " PUB_S_SRP,
         error == noErr ? "wrote" : "failed to write", iflist, preference_name);
}

void
ifpermit_load_permit_list_from_prefs(ifpermit_list_t **list, const char *preference_name)
{
    CFStringRef app_id = CFSTR("com.apple.srp-mdns-proxy.preferences");
    CFStringRef key = CFStringCreateWithCString(NULL, preference_name, kCFStringEncodingASCII);
    char iflist[128]; // Shouldn't really be more than one or perhaps two interfaces on this list.
    OSStatus error;
    CFPrefs_GetCString(app_id, key, iflist, sizeof(iflist), &error);
    // Probably no preference set, which is not an error.
    if (error != noErr) {
        INFO("got error %d when reading pref string " PUB_S_SRP, (int)error, preference_name);
        goto out;
    }
    INFO("got '" PUB_S_SRP "' from pref string " PUB_S_SRP, iflist, preference_name);
    if (iflist[0] != 0) {
        char *ifname = iflist;
        while (ifname != NULL) {
            char *ifend = strchr(ifname, ',');
            char *next = ifend;
            if (ifend == NULL) {
                ifend = ifname + strlen(ifname);
            } else {
                // If we found a comma, there must at least be a NUL following it.
                next++;
            }
            *ifend = 0;
            if (*list == NULL) {
                *list = ifpermit_list_create();
                if (list == NULL) {
                    ERROR("no memory for permit list.");
                    goto out;
                }
            }
            ifpermit_list_add(*list, ifname);
            ifname = next;
        }
    }
out:
    srp_cf_forget(&key);
}


// Local Variables:
// mode: C
// tab-width: 4
// c-file-style: "bsd"
// c-basic-offset: 4
// fill-column: 120
// indent-tabs-mode: nil
// End:
