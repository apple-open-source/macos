/*
 * Copyright (c) 2006-2008 Apple Computer, Inc.  All Rights Reserved.
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
 
#include "dt_dof_byteswap.h"

#ifdef __APPLE__

#include <CoreFoundation/CFByteOrder.h>

#include <dt_impl.h>
#include <dt_strtab.h>
#include <dt_program.h>
#include <dt_provider.h>
#include <dt_xlator.h>
#include <dt_dof.h>

#define swapu16(x)	do { x = CFSwapInt16(x); } while(0)
#define swapu32(x)	do { x = CFSwapInt32(x); } while(0)
#define swapu64(x)	do { x = CFSwapInt64(x); } while(0)

// We recursively traverse the DOF, byte-swapping after we've followed
// the pointers down.

static void
byteswap_dof_ecbdesc(uint8_t* base, dof_sec_t* sec)
{
	dof_ecbdesc_t* ecbdesc = (dof_ecbdesc_t*)(base + sec->dofs_offset);
	
	swapu32(ecbdesc->dofe_probes);
	swapu32(ecbdesc->dofe_pred);
	swapu32(ecbdesc->dofe_actions);
	swapu32(ecbdesc->dofe_pad);
	swapu64(ecbdesc->dofe_uarg);

	return;
}

static void
byteswap_dof_probedesc(uint8_t* base, dof_sec_t* sec)
{
	dof_probedesc_t* probedesc = (dof_probedesc_t*)(base + sec->dofs_offset);
	
	swapu32(probedesc->dofp_strtab);
	swapu32(probedesc->dofp_provider);
	swapu32(probedesc->dofp_mod);
	swapu32(probedesc->dofp_func);
	swapu32(probedesc->dofp_name);
	swapu32(probedesc->dofp_id);
	
	return;
}

static void
byteswap_dof_actdescs(uint8_t* base, dof_sec_t* sec)
{
	dof_actdesc_t* actdescs = (dof_actdesc_t*)(base + sec->dofs_offset);
	
	int i;
	
	for(i = 0; i < (sec->dofs_size / sec->dofs_entsize); i++) {
		swapu32(actdescs[i].dofa_difo);
		swapu32(actdescs[i].dofa_strtab);
		swapu32(actdescs[i].dofa_kind);
		swapu32(actdescs[i].dofa_ntuple);
		swapu64(actdescs[i].dofa_arg);
		swapu64(actdescs[i].dofa_uarg);
	}

	return;
}

static void
byteswap_dof_difohdr(uint8_t* base, dof_sec_t* sec)
{
	dof_difohdr_t* difohdr = (dof_difohdr_t*)(base + sec->dofs_offset);
	
	swapu32(difohdr->dofd_rtype.dtdt_size);
	
	int i;
	
	for(i = 0; 
	    i < ((sec->dofs_size - sizeof(*difohdr)) / sizeof(dof_secidx_t)) + 1;
		i++) {
		swapu32(difohdr->dofd_links[i]);
	}
	
	return;
}

static void
byteswap_dof_dif(uint8_t* base, dof_sec_t* sec)
{
	uint32_t* dif = (uint32_t*)(base + sec->dofs_offset);
	
	int i;
	
	for(i = 0; i < (sec->dofs_size / sizeof(uint32_t)); i++)
		swapu32(dif[i]);

	return;
}

static void
byteswap_dof_difvs(uint8_t* base, dof_sec_t* sec)
{
	dtrace_difv_t* difvs = (dtrace_difv_t*)(base + sec->dofs_offset);
	
	int i;
	
	for(i = 0; i < (sec->dofs_size / sec->dofs_entsize); i++) {
		swapu32(difvs[i].dtdv_name);
		swapu32(difvs[i].dtdv_id);
		swapu16(difvs[i].dtdv_flags);
		swapu32(difvs[i].dtdv_type.dtdt_size);
	}

	return;
}

static void
byteswap_dof_relodescs(uint8_t* base, dof_sec_t* sec)
{
	dof_relodesc_t* relodescs = (dof_relodesc_t*)(base + sec->dofs_offset);
	
	int i;
	
	for(i = 0; i < (sec->dofs_size / sec->dofs_entsize); i++) {
		swapu32(relodescs[i].dofr_name);
		swapu32(relodescs[i].dofr_type);
		swapu64(relodescs[i].dofr_offset);
		swapu64(relodescs[i].dofr_data);
	}
	
	return;
}

static void
byteswap_dof_diftypes(uint8_t* base, dof_sec_t* sec)
{
	dtrace_diftype_t* types = (dtrace_diftype_t*)(base + sec->dofs_offset);
	
	int i;
	
	for(i = 0; i < (sec->dofs_size / sec->dofs_entsize); i++)
		swapu32(types[i].dtdt_size);

	return;
}

static void
byteswap_dof_relohdr(uint8_t* base, dof_sec_t* sec)
{
	dof_relohdr_t* relohdr = (dof_relohdr_t*)(base + sec->dofs_offset);
	
	swapu32(relohdr->dofr_strtab);
	swapu32(relohdr->dofr_relsec);
	swapu32(relohdr->dofr_tgtsec);

	return;
}

static void
byteswap_dof_optdescs(uint8_t* base, dof_sec_t* sec)
{
	dof_optdesc_t* optdescs = (dof_optdesc_t*)(base + sec->dofs_offset);
	
	int i;
	
	for(i = 0; i < (sec->dofs_size / sec->dofs_entsize); i++) {
		swapu32(optdescs[i].dofo_option);
		swapu32(optdescs[i].dofo_strtab);
		swapu64(optdescs[i].dofo_value);
	}

	return;
}

static void
byteswap_dof_provider(uint8_t* base, dof_sec_t* sec)
{
	dof_provider_t* provider = (dof_provider_t*)(base + sec->dofs_offset);
	
	swapu32(provider->dofpv_strtab);
	swapu32(provider->dofpv_probes);
	swapu32(provider->dofpv_prargs);
	swapu32(provider->dofpv_proffs);
	swapu32(provider->dofpv_name);
	swapu32(provider->dofpv_provattr);
	swapu32(provider->dofpv_modattr);
	swapu32(provider->dofpv_funcattr);
	swapu32(provider->dofpv_nameattr);
	swapu32(provider->dofpv_argsattr);
	swapu32(provider->dofpv_prenoffs);

	return;
}

static void
byteswap_dof_probes(uint8_t* base, dof_sec_t* sec)
{
	dof_probe_t* probes = (dof_probe_t*)(base + sec->dofs_offset);
	
	int i;
	
	for(i = 0; i < sec->dofs_size / sec->dofs_entsize; i++) {
		swapu64(probes[i].dofpr_addr);
		swapu32(probes[i].dofpr_func);
		swapu32(probes[i].dofpr_name);
		swapu32(probes[i].dofpr_nargv);
		swapu32(probes[i].dofpr_xargv);
		swapu32(probes[i].dofpr_argidx);
		swapu32(probes[i].dofpr_offidx);
		swapu16(probes[i].dofpr_noffs);
		swapu32(probes[i].dofpr_enoffidx);
		swapu16(probes[i].dofpr_nenoffs);
	}

	return;
}

static void
byteswap_dof_uint32s(uint8_t* base, dof_sec_t* sec)
{
	uint32_t* uint32s = (uint32_t*)(base + sec->dofs_offset);
	
	int i;
	
	for(i = 0; i < sec->dofs_size / sizeof(uint32_t); i++)
		swapu32(uint32s[i]);

	return;
}

static void
byteswap_dof_uint64s(uint8_t* base, dof_sec_t* sec)
{
	uint64_t* uint64s = (uint64_t*)(base + sec->dofs_offset);
	
	int i;
	
	for(i = 0; i < sec->dofs_size / sizeof(uint64_t); i++)
		swapu64(uint64s[i]);

	return;
}

static void
byteswap_dof_xlrefs(uint8_t* base, dof_sec_t* sec)
{
	dof_xlref_t* xlrefs = (dof_xlref_t*)(base + sec->dofs_offset);
	
	int i;
	
	for(i = 0; i < sec->dofs_size / sec->dofs_entsize; i++) {
		swapu32(xlrefs[i].dofxr_xlator);
		swapu32(xlrefs[i].dofxr_member);
		swapu32(xlrefs[i].dofxr_argn);
	}

	return;
}

static void
byteswap_dof_xlmembers(uint8_t* base, dof_sec_t* sec)
{
	dof_xlmember_t* xlmembers = (dof_xlmember_t*)(base + sec->dofs_offset);
	
	int i;
	
	for(i = 0; i < sec->dofs_size / sec->dofs_entsize; i++) {
		swapu32(xlmembers[i].dofxm_difo);
		swapu32(xlmembers[i].dofxm_name);
		swapu32(xlmembers[i].dofxm_type.dtdt_size);
	}

	return;
}

static void
byteswap_dof_xlator(uint8_t* base, dof_sec_t* sec)
{
	dof_xlator_t* xlator = (dof_xlator_t*)(base + sec->dofs_offset);
	
	swapu32(xlator->dofxl_members);
	swapu32(xlator->dofxl_strtab);
	swapu32(xlator->dofxl_argv);
	swapu32(xlator->dofxl_argc);
	swapu32(xlator->dofxl_type);
	swapu32(xlator->dofxl_attr);
	
	return;
}

static void
byteswap_dof_secidxs(uint8_t* base, dof_sec_t* sec)
{
	dof_secidx_t* secidxs = (dof_secidx_t*)(base + sec->dofs_offset);
	
	int i;
	
	for(i = 0; i < sec->dofs_size / sizeof(dof_secidx_t); i++)
		swapu32(secidxs[i]);

	return;
}

static void
byteswap_dof_sec(uint8_t* base, dof_sec_t* sec)
{
	#define HANDLE_SEC(sectype, handler)	\
	case sectype:							\
		handler(base, sec);					\
		break;

	switch(sec->dofs_type) {
	
	HANDLE_SEC(DOF_SECT_ECBDESC,	byteswap_dof_ecbdesc);
	HANDLE_SEC(DOF_SECT_PROBEDESC,	byteswap_dof_probedesc);
	HANDLE_SEC(DOF_SECT_ACTDESC,	byteswap_dof_actdescs);
	HANDLE_SEC(DOF_SECT_DIFOHDR,	byteswap_dof_difohdr);
	HANDLE_SEC(DOF_SECT_DIF,		byteswap_dof_dif);
	HANDLE_SEC(DOF_SECT_VARTAB,		byteswap_dof_difvs);
	HANDLE_SEC(DOF_SECT_RELTAB,		byteswap_dof_relodescs);
	HANDLE_SEC(DOF_SECT_TYPTAB,		byteswap_dof_diftypes);
	HANDLE_SEC(DOF_SECT_URELHDR,	byteswap_dof_relohdr);
	HANDLE_SEC(DOF_SECT_KRELHDR,	byteswap_dof_relohdr);
	HANDLE_SEC(DOF_SECT_OPTDESC,	byteswap_dof_optdescs);
	HANDLE_SEC(DOF_SECT_PROVIDER,	byteswap_dof_provider);
	HANDLE_SEC(DOF_SECT_PROBES,		byteswap_dof_probes);
	HANDLE_SEC(DOF_SECT_PROFFS,		byteswap_dof_uint32s);
	HANDLE_SEC(DOF_SECT_INTTAB,		byteswap_dof_uint64s);
	HANDLE_SEC(DOF_SECT_XLTAB,		byteswap_dof_xlrefs);
	HANDLE_SEC(DOF_SECT_XLMEMBERS,	byteswap_dof_xlmembers);
	HANDLE_SEC(DOF_SECT_XLIMPORT,	byteswap_dof_xlator);
	HANDLE_SEC(DOF_SECT_XLEXPORT,	byteswap_dof_xlator);
	HANDLE_SEC(DOF_SECT_PREXPORT,	byteswap_dof_secidxs);
	
	case DOF_SECT_NONE:
	case DOF_SECT_COMMENTS:
	case DOF_SECT_SOURCE:
	case DOF_SECT_STRTAB:
	case DOF_SECT_UTSNAME:
		break;
	}

	swapu32(sec->dofs_type);
	swapu32(sec->dofs_align);
	swapu32(sec->dofs_flags);
	swapu32(sec->dofs_entsize);
	swapu64(sec->dofs_offset);
	swapu64(sec->dofs_size);
}

static void
byteswap_dof_hdr(uint8_t* base, dof_hdr_t* hdr)
{
	uint32_t i;
	dof_sec_t* secs;
	
	secs = (dof_sec_t*)(base + hdr->dofh_secoff);
	
	for(i = 0; i < hdr->dofh_secnum; i++)
		byteswap_dof_sec(base, &secs[i]);

	// Most of the dofh_ident field is endian neutral. DOF_ID_ENCODING is not.
	hdr->dofh_ident[DOF_ID_ENCODING] = (hdr->dofh_ident[DOF_ID_ENCODING] == DOF_ENCODE_LSB) ? DOF_ENCODE_MSB : DOF_ENCODE_LSB;

	swapu32(hdr->dofh_flags);
	swapu32(hdr->dofh_hdrsize);
	swapu32(hdr->dofh_secsize);
	swapu32(hdr->dofh_secnum);
	swapu64(hdr->dofh_secoff);
	swapu64(hdr->dofh_loadsz);
	swapu64(hdr->dofh_filesz);
	swapu64(hdr->dofh_pad);
	
	return;
}

void
dtrace_dof_byteswap(dof_hdr_t* hdr)
{	
	uint8_t* base = (uint8_t*)hdr;
	
	byteswap_dof_hdr(base, hdr);
}

#endif
