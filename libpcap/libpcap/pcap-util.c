/*
 * Copyright (c) 2012-2013 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/queue.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "pcap-int.h"
#include "pcap-util.h"

static int null_uu_inited = 0;
static uuid_t null_uu;

void
pcap_if_info_set_clear(struct pcap_if_info_set *if_info_set)
{
	int i;
	
	if (if_info_set->if_infos != NULL) {
		for (i = 0; i < if_info_set->if_info_count; i++)
			pcap_if_info_set_free(if_info_set, if_info_set->if_infos[i]);
		
		free(if_info_set->if_infos);
		if_info_set->if_infos = NULL;
	}
	if_info_set->if_info_count = 0;
	if_info_set->if_dump_id = 0;
}

struct pcap_if_info *
pcap_if_info_set_find_by_name(struct pcap_if_info_set *if_info_set, const char *name)
{
	int i;
	
	for (i = 0; i < if_info_set->if_info_count; i++) {
		if (strcmp(name, if_info_set->if_infos[i]->if_name) == 0)
			return (if_info_set->if_infos[i]);
	}
	return (NULL);
}

struct pcap_if_info *
pcap_if_info_set_find_by_id(struct pcap_if_info_set *if_info_set, int if_id)
{
	int i;
	
	if (if_id == -1)
		return (NULL);
	
	for (i = 0; i < if_info_set->if_info_count; i++) {
		if (if_id == if_info_set->if_infos[i]->if_id)
			return (if_info_set->if_infos[i]);
	}
	return (NULL);
}

void
pcap_if_info_set_free(struct pcap_if_info_set *if_info_set, struct pcap_if_info *if_info)
{
	if (if_info != NULL) {
		int i;
		
		for (i = 0; i < if_info_set->if_info_count; i++) {
			if (if_info_set->if_infos[i] == if_info) {
				if_info_set->if_infos[i] = NULL;
				break;
			}
		}
		
		pcap_freecode(&if_info->if_filter_program);
		free(if_info);
	}
}

struct pcap_if_info *
pcap_if_info_set_add(struct pcap_if_info_set *if_info_set, const char *name,
		 int if_id, int linktype, int snaplen,
		 const char *filter_str, char *errbuf)
{
	struct pcap_if_info *if_info = NULL;
	size_t ifname_len = strlen(name);
	struct pcap_if_info **newarray;

	/*
	 * Stash the interface name after the structure
	 */
	if_info = calloc(1, sizeof(struct pcap_if_info) + ifname_len + 1);
	if (if_info == NULL) {
		if (errbuf != NULL) {
			snprintf(errbuf, PCAP_ERRBUF_SIZE,
				 "%s: calloc() failed", __func__);
		}
		return (NULL);
	}
	if_info->if_name = (char *)(if_info + 1);
	if (ifname_len > 0)
		bcopy(name, if_info->if_name, ifname_len);
	if_info->if_name[ifname_len] = 0;
	if (if_id == -1)
		if_info->if_id = if_info_set->if_info_count;
	else
		if_info->if_id = if_id;
	if_info->if_linktype = linktype;
	if_info->if_snaplen = snaplen;
	
	/*
	 * The compilation of a BPF filter expression depends on
	 * the DLT so we store the program in the if_info
	 */
	if (filter_str != NULL && *filter_str != 0) {
		if (pcap_compile_nopcap(if_info->if_snaplen,
					if_info->if_linktype,
					&if_info->if_filter_program,
					filter_str, 0, PCAP_NETMASK_UNKNOWN) == -1) {
			if (errbuf != NULL) {
				snprintf(errbuf, PCAP_ERRBUF_SIZE,
					 "%s: pcap_compile_nopcap() failed", __func__);
			 }
			free(if_info);
			return (NULL);
		}
	}
	
	/*
	 * Resize pointer array
	 */
	newarray = realloc(if_info_set->if_infos,
			   (if_info_set->if_info_count + 1) * sizeof(struct pcap_if_info *));
	if (newarray == NULL) {
		if (errbuf != NULL) {
			snprintf(errbuf, PCAP_ERRBUF_SIZE,
				 "%s: realloc() failed", __func__);
				 }
		pcap_if_info_set_free(if_info_set, if_info);
		return (NULL);
	}
	if_info_set->if_infos = newarray;
	if_info_set->if_infos[if_info_set->if_info_count] = if_info;
	if_info_set->if_info_count += 1;
	
	return (if_info);
}

void
pcap_clear_if_infos(pcap_t * pcap)
{
	pcap_if_info_set_clear(&pcap->if_info_set);
}

struct pcap_if_info *
pcap_find_if_info_by_name(pcap_t * pcap, const char *name)
{
	return (pcap_if_info_set_find_by_name(&pcap->if_info_set, name));
}

struct pcap_if_info *
pcap_find_if_info_by_id(pcap_t * pcap, int if_id)
{
	return (pcap_if_info_set_find_by_id(&pcap->if_info_set, if_id));
}

void
pcap_free_if_info(pcap_t * pcap, struct pcap_if_info *if_info)
{
	pcap_if_info_set_free(&pcap->if_info_set, if_info);
}

struct pcap_if_info *
pcap_add_if_info(pcap_t * pcap, const char *name,
		 int if_id, int linktype, int snaplen)
{
	struct pcap_if_info *if_info = NULL;

	pcap->cleanup_extra_op = pcap_ng_init_section_info;

	if_info = pcap_if_info_set_add(&pcap->if_info_set,
		name, if_id, linktype, snaplen,
		pcap->filter_str, pcap->errbuf);
	
	return (if_info);
}

void
pcap_proc_info_set_clear(struct pcap_proc_info_set *proc_info_set)
{
	int i;
	
	if (proc_info_set->proc_infos != NULL) {
		for (i = 0; i < proc_info_set->proc_info_count; i++)
			pcap_proc_info_set_free(proc_info_set, proc_info_set->proc_infos[i]);
		
		free(proc_info_set->proc_infos);
		proc_info_set->proc_infos = NULL;
	}
	proc_info_set->proc_info_count = 0;
	proc_info_set->proc_dump_index = 0;
}

struct pcap_proc_info *
pcap_proc_info_set_find(struct pcap_proc_info_set *proc_info_set,
	uint32_t pid, const char *name)
{
	return (pcap_proc_info_set_find_uuid(proc_info_set, pid, name, NULL));
}

struct pcap_proc_info *
pcap_proc_info_set_find_uuid(struct pcap_proc_info_set *proc_info_set,
	uint32_t pid, const char *name, const uuid_t uu)
{
	int i;

	if (name != NULL && uu != NULL) {
		for (i = 0; i < proc_info_set->proc_info_count; i++) {
			struct pcap_proc_info *proc_info = proc_info_set->proc_infos[i];
		
		if (pid == proc_info->proc_pid &&
		    strcmp(name, proc_info->proc_name) == 0 &&
		    uuid_compare(uu, proc_info->proc_uuid) == 0)
				return (proc_info);
		}
	} else if (name != NULL) {
		for (i = 0; i < proc_info_set->proc_info_count; i++) {
			struct pcap_proc_info *proc_info = proc_info_set->proc_infos[i];
			
			if (pid == proc_info->proc_pid &&
			    strcmp(name, proc_info->proc_name) == 0)
				return (proc_info);
		}
	} else if (uu != NULL) {
		for (i = 0; i < proc_info_set->proc_info_count; i++) {
			struct pcap_proc_info *proc_info = proc_info_set->proc_infos[i];
			
			if (pid == proc_info->proc_pid &&
			    uuid_compare(uu, proc_info->proc_uuid) == 0)
			return (proc_info);
	}
	}
	return (NULL);
}

struct pcap_proc_info *
pcap_proc_info_set_find_by_index(struct pcap_proc_info_set *proc_info_set,
	uint32_t index)
{
	int i;
	
	for (i = 0; i < proc_info_set->proc_info_count; i++) {
		struct pcap_proc_info *proc_info = proc_info_set->proc_infos[i];
		
		if (index == proc_info->proc_index)
			return (proc_info);
	}
	return (NULL);
}

void
pcap_proc_info_set_free(struct pcap_proc_info_set *proc_info_set,
	struct pcap_proc_info *proc_info)
{
	
	if (proc_info != NULL) {
		int i;
		
		for (i = 0; i < proc_info_set->proc_info_count; i++) {
			if (proc_info_set->proc_infos[i] == proc_info) {
				proc_info_set->proc_infos[i] = NULL;
				break;
			}
		}
		free(proc_info);
	}
}

struct pcap_proc_info *
pcap_proc_info_set_add_uuid(struct pcap_proc_info_set *proc_info_set,
	uint32_t pid, const char *name, const uuid_t uu, char *errbuf)
{
	struct pcap_proc_info *proc_info = NULL;
	size_t name_len = name != NULL ? strlen(name) : 0;
	struct pcap_proc_info **newarray;
	uuid_string_t uu_str;
	
	if (uu == NULL && null_uu_inited == 0) {
		uuid_clear(null_uu);
		null_uu_inited = 1;
	}
	
	/*
	 * Stash the process name after the structure
	 */
	proc_info = calloc(1, sizeof(struct pcap_proc_info) + name_len + 1);
	if (proc_info == NULL) {
		if (errbuf != NULL) {
			snprintf(errbuf, PCAP_ERRBUF_SIZE,
			 "%s: calloc() failed", __func__);
		}
		return (NULL);
	}
	proc_info->proc_name = (char *)(proc_info + 1);
	if (name_len > 0)
		bcopy(name, proc_info->proc_name, name_len);
	proc_info->proc_name[name_len] = 0;
	proc_info->proc_pid = pid;
	proc_info->proc_index = proc_info_set->proc_info_count;
	uuid_copy(proc_info->proc_uuid, uu != NULL ? uu : null_uu);

	uuid_unparse_lower(proc_info->proc_uuid, uu_str);
	
	/*
	 * Resize pointer array
	 */
	newarray = realloc(proc_info_set->proc_infos,
			   (proc_info_set->proc_info_count + 1) * sizeof(struct pcap_proc_info *));
	if (newarray == NULL) {
		if (errbuf != NULL) {
			snprintf(errbuf, PCAP_ERRBUF_SIZE,
			 "%s: malloc() failed", __func__);
		}
		free(proc_info);
		return (NULL);
	}
	proc_info_set->proc_infos = newarray;
	proc_info_set->proc_infos[proc_info_set->proc_info_count] = proc_info;
	proc_info_set->proc_info_count += 1;

	return (proc_info);
}

void
pcap_clear_proc_infos(pcap_t * pcap)
{
	pcap_proc_info_set_clear(&pcap->proc_info_set);
}

struct pcap_proc_info *
pcap_find_proc_info(pcap_t * pcap, uint32_t pid, const char *name)
{
	return (pcap_proc_info_set_find(&pcap->proc_info_set, pid, name));
}

struct pcap_proc_info *
pcap_find_proc_info_uuid(pcap_t * pcap, uint32_t pid, const char *name, const uuid_t uu)
{
	return (pcap_proc_info_set_find_uuid(&pcap->proc_info_set, pid, name, uu));
}

struct pcap_proc_info *
pcap_find_proc_info_by_index(pcap_t * pcap, uint32_t index)
{
	return (pcap_proc_info_set_find_by_index(&pcap->proc_info_set, index));
}

void
pcap_free_proc_info(pcap_t * pcap, struct pcap_proc_info *proc_info)
{
	pcap_proc_info_set_free(&pcap->proc_info_set, proc_info);
}

struct pcap_proc_info *
pcap_add_proc_info_uuid(pcap_t * pcap, uint32_t pid, const char *name, const uuid_t uu)
{
	struct pcap_proc_info *proc_info = NULL;

	pcap->cleanup_extra_op = pcap_ng_init_section_info;

	proc_info = pcap_proc_info_set_add_uuid(&pcap->proc_info_set,
						pid, name,
						uu,
						pcap->errbuf);
	
	return (proc_info);
}


struct pcap_proc_info *
pcap_add_proc_info(pcap_t * pcap, uint32_t pid, const char *name)
{
	return (pcap_add_proc_info_uuid(pcap, pid, name, NULL));
}

int
pcap_set_filter_info(pcap_t *pcap, const char *str, int optimize, bpf_u_int32 netmask)
{
	if (pcap->filter_str != NULL)
		free(pcap->filter_str);

	if (str == NULL) {
		pcap->filter_str = NULL;
	} else {
		pcap->filter_str = strdup(str);
		if (pcap->filter_str == NULL)
			return (PCAP_ERROR);
	}

	return (0);
}

void
pcap_ng_init_section_info(pcap_t *p)
{
	p->shb_added = 0;
	pcap_clear_if_infos(p);
	pcap_clear_proc_infos(p);
}

