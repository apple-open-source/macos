/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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

#ifndef _SYS_CODESIGN_H_
#define _SYS_CODESIGN_H_

#include <kern/cs_blobs.h>

/* MAC flags used by F_ADDFILESIGS_* */
#define MAC_VNODE_CHECK_DYLD_SIM 0x1   /* tells the MAC framework that dyld-sim is being loaded */

#define CLEAR_LV_ENTITLEMENT "com.apple.private.security.clear-library-validation"
#define OVERRIDE_PLUGIN_HOST_ENTITLEMENT "com.apple.private.security.override-plugin-host-detection"

/* csops  operations */
#define CS_OPS_STATUS           0       /* return status */
#define CS_OPS_MARKINVALID      1       /* invalidate process */
#define CS_OPS_MARKHARD         2       /* set HARD flag */
#define CS_OPS_MARKKILL         3       /* set KILL flag (sticky) */
#ifdef KERNEL_PRIVATE
/* CS_OPS_PIDPATH		4	*/
#endif
#define CS_OPS_CDHASH           5       /* get code directory hash */
#define CS_OPS_PIDOFFSET        6       /* get offset of active Mach-o slice */
#define CS_OPS_ENTITLEMENTS_BLOB 7      /* get entitlements blob */
#define CS_OPS_MARKRESTRICT     8       /* set RESTRICT flag (sticky) */
#define CS_OPS_SET_STATUS       9       /* set codesign flags */
#define CS_OPS_BLOB             10      /* get codesign blob */
#define CS_OPS_IDENTITY         11      /* get codesign identity */
#define CS_OPS_CLEARINSTALLER   12      /* clear INSTALLER flag */
#define CS_OPS_CLEARPLATFORM 13 /* clear platform binary status (DEVELOPMENT-only) */
#define CS_OPS_TEAMID       14  /* get team id */
#define CS_OPS_CLEAR_LV     15  /* clear the library validation flag */
#define CS_OPS_DER_ENTITLEMENTS_BLOB 16  /* get der entitlements blob */
#define CS_OPS_VALIDATION_CATEGORY 17   /* get process validation category */

#define CS_MAX_TEAMID_LEN       64

#ifndef KERNEL

#include <sys/types.h>
#include <mach/message.h>

__BEGIN_DECLS
/* code sign operations */
int csops(pid_t pid, unsigned int  ops, void * useraddr, size_t usersize);
int csops_audittoken(pid_t pid, unsigned int  ops, void * useraddr, size_t usersize, audit_token_t * token);
__END_DECLS

#else /* !KERNEL */

#include <mach/machine.h>
#include <mach/vm_types.h>

#include <sys/cdefs.h>
#include <sys/_types/_off_t.h>

struct vnode;
struct cs_blob;
struct fileglob;

__BEGIN_DECLS
int     cs_valid(struct proc *);
int     cs_process_enforcement(struct proc *);
int cs_process_global_enforcement(void);
int cs_system_enforcement(void);
int cs_vm_supports_4k_translations(void);
int     cs_require_lv(struct proc *);
int csproc_forced_lv(struct proc* p);
int     cs_system_require_lv(void);
uint32_t cs_entitlement_flags(struct proc *p);
int     cs_entitlements_blob_get_vnode(struct vnode *, off_t, void **, size_t *);
int     cs_entitlements_dictionary_copy_vnode(struct vnode *, off_t, void **);
int     cs_entitlements_blob_get(struct proc *, void **, size_t *);
#ifdef KERNEL_PRIVATE
int     cs_entitlements_dictionary_copy(struct proc *, void **);
#endif
int     cs_restricted(struct proc *);
uint8_t *__counted_by_or_null(CS_CDHASH_LEN) cs_get_cdhash(struct proc *);
cs_launch_type_t launch_constraint_data_get_launch_type(launch_constraint_data_t lcd);

struct cs_blob * csproc_get_blob(struct proc *);
struct cs_blob * csvnode_get_blob(struct vnode *, off_t);
void             csvnode_print_debug(struct vnode *);

off_t           csblob_get_base_offset(struct cs_blob *);
vm_size_t       csblob_get_size(struct cs_blob *);
vm_address_t    csblob_get_addr(struct cs_blob *);
const char *    csblob_get_teamid(struct cs_blob *);
const char *    csblob_get_identity(struct cs_blob *);
const uint8_t * csblob_get_cdhash(struct cs_blob *);
const CS_CodeDirectory* csblob_get_code_directory(struct cs_blob *csblob);
int             csblob_get_platform_binary(struct cs_blob *);
void            csblob_invalidate_flags(struct cs_blob *blob);
void            csvnode_invalidate_flags(struct vnode * vp);
unsigned int    csblob_get_flags(struct cs_blob *);
uint8_t         csblob_get_hashtype(struct cs_blob const *);
unsigned int    csblob_get_signer_type(struct cs_blob *);
#if DEVELOPMENT || DEBUG
void            csproc_clear_platform_binary(struct proc *);
#endif

#define XNU_CSBLOB_HAS_VALIDATION_CATEGORY 1
int             csblob_set_validation_category(struct cs_blob *, unsigned int);
unsigned int    csblob_get_validation_category(struct cs_blob *);

#include <uuid/uuid.h>
#define XNU_SUPPORTS_PROVISIONING_PROFILE_UUID 1
int csblob_register_profile_uuid(struct cs_blob *, const uuid_t, void*, vm_size_t);

void csproc_disable_enforcement(struct proc* p);
void csproc_mark_invalid_allowed(struct proc* p);
int csproc_check_invalid_allowed(struct proc* p);
int csproc_hardened_runtime(struct proc* p);

int             csblob_get_entitlements(struct cs_blob *, void **, size_t *);
int             csblob_get_der_entitlements(struct cs_blob *, const CS_GenericBlob **, size_t *);
#define XNU_HAS_GET_DER_ENTITLEMENTS_UNSAFE 1
const CS_GenericBlob* csblob_get_der_entitlements_unsafe(struct cs_blob *);

const CS_GenericBlob *
    csblob_find_blob(struct cs_blob *, uint32_t, uint32_t);
const CS_GenericBlob *
    csblob_find_blob_bytes(const uint8_t *, size_t, uint32_t, uint32_t);
void *          csblob_entitlements_dictionary_copy(struct cs_blob *csblob);
void            csblob_entitlements_dictionary_set(struct cs_blob *csblob, void * entitlements);

// New APIs
void            csblob_os_entitlements_set(struct cs_blob *csblob, void * entitlements);
void *          csblob_os_entitlements_copy(struct cs_blob *csblob);
void *          csblob_os_entitlements_get(struct cs_blob *csblob);
void *          csblob_get_storage_addr(struct cs_blob *csblob);
/*
 * Mostly convenience functions below
 */

const   char * csproc_get_teamid(struct proc *);
const   char * csproc_get_identity(struct proc *);
const   char * csvnode_get_teamid(struct vnode *, off_t);
int     csproc_get_platform_binary(struct proc *);
int csproc_get_prod_signed(struct proc *);
const   char * csfg_get_teamid(struct fileglob *);
const   char * csfg_get_supplement_teamid(struct fileglob *);
int     csfg_get_path(struct fileglob *, char *, int *);
int     csfg_get_platform_binary(struct fileglob *);
int     csfg_get_supplement_platform_binary(struct fileglob *);
uint8_t * csfg_get_cdhash(struct fileglob *, uint64_t, size_t *);
uint8_t * csfg_get_supplement_cdhash(struct fileglob *, uint64_t, size_t *);
const uint8_t * csfg_get_supplement_linkage_cdhash(struct fileglob *, uint64_t, size_t *);
int csfg_get_prod_signed(struct fileglob *);
int csfg_get_supplement_prod_signed(struct fileglob *fg);
unsigned int csfg_get_signer_type(struct fileglob *);
unsigned int csfg_get_supplement_signer_type(struct fileglob *);
unsigned int csfg_get_validation_category(struct fileglob *fg, uint64_t offset);
unsigned int csfg_get_supplement_validation_category(struct fileglob *fg, uint64_t offset);
const char *csfg_get_identity(struct fileglob *fg, off_t offset);
unsigned int csproc_get_signer_type(struct proc *);

uint8_t csfg_get_platform_identifier(struct fileglob *, off_t);
uint8_t csvnode_get_platform_identifier(struct vnode *, off_t);
uint8_t csproc_get_platform_identifier(struct proc *);

struct cs_blob* csfg_get_csblob(struct fileglob*, uint64_t);
struct cs_blob* csfg_get_supplement_csblob(struct fileglob*, uint64_t);

extern int cs_debug;
extern int cs_debug_fail_on_unsigned_code;
extern unsigned int cs_debug_unsigned_exec_failures;
extern unsigned int cs_debug_unsigned_mmap_failures;

int cs_blob_create_validated(vm_address_t* addr, vm_size_t size,
    struct cs_blob ** ret_blob, CS_CodeDirectory const **ret_cd);

void cs_blob_free(struct cs_blob *blob);

#ifdef XNU_KERNEL_PRIVATE

int     cs_allow_invalid(struct proc *);
int     cs_invalid_page(addr64_t vaddr, boolean_t *cs_killed);
void    cs_process_invalidated(struct proc *);
int     csproc_get_platform_path(struct proc *);
int     csproc_get_validation_category(struct proc *, unsigned int *);

#if !SECURE_KERNEL
extern int cs_enforcement_panic;
#endif

#endif /* XNU_KERNEL_PRIVATE */

__END_DECLS

#endif /* KERNEL */

#endif /* _SYS_CODESIGN_H_ */
