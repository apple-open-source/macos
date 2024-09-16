/*
 * Copyright (c) 2007-2020 Apple Inc. All Rights Reserved.
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
/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: vm_mmap.c 1.6 91/10/21$
 *
 *	@(#)vm_mmap.c	8.10 (Berkeley) 2/19/95
 */
/*
 * NOTICE: This file was modified by SPARTA, Inc. in 2005 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 */

/*
 * Mapped file (mmap) interface to VM
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/proc_internal.h>
#include <sys/kauth.h>
#include <sys/resourcevar.h>
#include <sys/vnode_internal.h>
#include <sys/acct.h>
#include <sys/wait.h>
#include <sys/file_internal.h>
#include <sys/vadvise.h>
#include <sys/trace.h>
#include <sys/mman.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/ubc.h>
#include <sys/ubc_internal.h>
#include <sys/sysproto.h>

#include <sys/syscall.h>
#include <sys/kdebug.h>
#include <sys/bsdtask_info.h>

#include <security/audit/audit.h>
#include <bsm/audit_kevents.h>

#include <mach/mach_types.h>
#include <mach/mach_traps.h>
#include <mach/vm_sync.h>
#include <mach/vm_behavior.h>
#include <mach/vm_inherit.h>
#include <mach/vm_statistics.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>
#include <mach/host_priv.h>
#include <mach/sdt.h>
#include <mach-o/loader.h>
#include <mach/vm_types_unsafe.h>

#include <machine/machine_routines.h>

#include <kern/cpu_number.h>
#include <kern/host.h>
#include <kern/task.h>
#include <kern/page_decrypt.h>

#include <IOKit/IOReturn.h>
#include <IOKit/IOBSD.h>

#include <vm/vm_kern_xnu.h>
#include <vm/vm_map_xnu.h>
#include <vm/vm_pager_xnu.h>
#include <vm/vm_sanitize_internal.h>

#if CONFIG_MACF
#include <security/mac_framework.h>
#endif
#include <os/overflow.h>

/*
 * this function implements the same logic as dyld's "dyld_fall_2020_os_versions"
 * from dyld_priv.h. Basically, we attempt to draw the line of: "was this code
 * compiled with an SDK from fall of 2020 or later?""
 */
static bool
proc_2020_fall_os_sdk_or_later(void)
{
	const uint32_t proc_sdk_ver = proc_sdk(current_proc());

	switch (proc_platform(current_proc())) {
	case PLATFORM_MACOS:
		return proc_sdk_ver >= 0x000a1000; // DYLD_MACOSX_VERSION_10_16
	case PLATFORM_IOS:
	case PLATFORM_IOSSIMULATOR:
	case PLATFORM_MACCATALYST:
		return proc_sdk_ver >= 0x000e0000; // DYLD_IOS_VERSION_14_0
	case PLATFORM_BRIDGEOS:
		return proc_sdk_ver >= 0x00050000; // DYLD_BRIDGEOS_VERSION_5_0
	case PLATFORM_TVOS:
	case PLATFORM_TVOSSIMULATOR:
		return proc_sdk_ver >= 0x000e0000; // DYLD_TVOS_VERSION_14_0
	case PLATFORM_WATCHOS:
	case PLATFORM_WATCHOSSIMULATOR:
		return proc_sdk_ver >= 0x00070000; // DYLD_WATCHOS_VERSION_7_0
	default:
		/*
		 * tough call, but let's give new platforms the benefit of the doubt
		 * to avoid a re-occurence of rdar://89843927
		 */
		return true;
	}
}

static inline kern_return_t
mmap_sanitize(
	vm_map_t                user_map,
	vm_prot_ut              prot_u,
	vm_addr_struct_t        pos_u,
	vm_size_struct_t        len_u,
	vm_addr_struct_t        addr_u,
	int                     flags,
	vm_prot_t              *prot,
	vm_object_offset_t     *file_pos,
	vm_object_offset_t     *file_end,
	vm_map_size_t          *file_size,
	vm_map_offset_t        *user_addr,
	vm_map_offset_t        *user_end,
	vm_map_size_t          *user_size)
{
	kern_return_t           kr;
	vm_map_offset_t         user_mask = vm_map_page_mask(user_map);
	vm_sanitize_flags_t     vm_sanitize_flags;

	kr = vm_sanitize_prot_bsd(prot_u, VM_SANITIZE_CALLER_MMAP, prot);
	*prot &= VM_PROT_ALL;
	if (__improbable(kr != KERN_SUCCESS)) {
		return kr;
	}

	/*
	 * Check file_pos doesn't overflow with PAGE_MASK since VM objects use
	 * this page mask internally, and it can be wider than the user_map's.
	 */
	if (flags & MAP_UNIX03) {
		vm_sanitize_flags = VM_SANITIZE_FLAGS_SIZE_ZERO_FAILS;
	} else {
		vm_sanitize_flags = VM_SANITIZE_FLAGS_SIZE_ZERO_FALLTHROUGH;
	}

	kr = vm_sanitize_addr_size(pos_u, len_u, VM_SANITIZE_CALLER_MMAP, PAGE_MASK,
	    vm_sanitize_flags | VM_SANITIZE_FLAGS_GET_UNALIGNED_VALUES,
	    file_pos, file_end, file_size);
	if (__improbable(kr != KERN_SUCCESS)) {
		return kr;
	}

	/*
	 * Check that file_pos is page aligned for the user page size when
	 * UNIX03 compliance is requested.
	 * The user page size may be different from the kernel page size we
	 * use to check for overflows in the sanitizer call above).
	 */
	if ((flags & MAP_UNIX03) && (*file_pos & user_mask)) {
		return KERN_INVALID_ARGUMENT;
	}

	if (flags & MAP_FIXED) {
		kr = vm_sanitize_addr_size(addr_u, len_u, VM_SANITIZE_CALLER_MMAP,
		    user_map,
		    VM_SANITIZE_FLAGS_SIZE_ZERO_FALLTHROUGH,
		    user_addr, user_end, user_size);
		if (__improbable(kr != KERN_SUCCESS)) {
			return kr;
		}

		/*
		 * Further validation since we allowed a misaligned user_addr
		 * for fixed mappings.
		 *
		 * The specified address must have the same remainder
		 * as the file offset taken modulo PAGE_SIZE, so it
		 * should be aligned after adjustment by (file_pos & user_mask).
		 */
		if (!VM_SANITIZE_UNSAFE_IS_EQUAL(addr_u, *user_addr + (*file_pos & user_mask))) {
			return KERN_INVALID_ARGUMENT;
		}
	} else {
		/*
		 * For "anywhere" mappings, the address is only a hint,
		 * mach_vm_map_kernel() will fail with KERN_NO_SPACE
		 * if user_addr + user_size overflows,
		 * and mmap will start scanning again.
		 *
		 * Unlike Mach VM APIs, the hint is taken as a strict
		 * "start" which is why we round the sanitized address up,
		 * rather than truncate.
		 */
		*user_addr = vm_sanitize_addr(user_map,
		    vm_sanitize_compute_unsafe_end(addr_u, user_mask));
		kr = vm_sanitize_size(pos_u, len_u, VM_SANITIZE_CALLER_MMAP,
		    user_map, VM_SANITIZE_FLAGS_SIZE_ZERO_FALLTHROUGH,
		    user_size);
		if (__improbable(kr != KERN_SUCCESS)) {
			return kr;
		}
	}

	return KERN_SUCCESS;
}

/*
 * XXX Internally, we use VM_PROT_* somewhat interchangeably, but the correct
 * XXX usage is PROT_* from an interface perspective.  Thus the values of
 * XXX VM_PROT_* and PROT_* need to correspond.
 */
int
mmap(proc_t p, struct mmap_args *uap, user_addr_t *retval)
{
	/*
	 *	Map in special device (must be SHARED) or file
	 */
	struct fileproc        *fp;
	struct vnode           *vp = NULLVP;
	int                     flags;
	int                     prot;
	int                     err = 0;
	vm_map_t                user_map;
	kern_return_t           result;
	vm_map_kernel_flags_t   vmk_flags = VM_MAP_KERNEL_FLAGS_NONE;
	boolean_t               docow;
	vm_prot_t               maxprot;
	void                   *handle;
	memory_object_t         pager = MEMORY_OBJECT_NULL;
	memory_object_control_t control;
	int                     mapanon = 0;
	int                     fpref = 0;
	int                     error = 0;
	int                     fd = uap->fd;
	int                     num_retries = 0;
	kern_return_t           kr;
	/* page-aligned "user_map" quantities */
	vm_map_offset_t         user_addr, user_end, user_mask;
	vm_map_size_t           user_size;
	/* unaligned "file" quantities */
	vm_object_offset_t      file_pos, file_end;
	vm_map_size_t           file_size;

	/*
	 * Note that for UNIX03 conformance, there is additional parameter checking for
	 * mmap() system call in libsyscall prior to entering the kernel.  The sanity
	 * checks and argument validation done in this function are not the only places
	 * one can get returned errnos.
	 */

	user_map  = current_map();
	flags     = uap->flags;
	user_mask = vm_map_page_mask(user_map);

	AUDIT_ARG(addr, VM_SANITIZE_UNSAFE_UNWRAP(uap->addr));
	AUDIT_ARG(len, VM_SANITIZE_UNSAFE_UNWRAP(uap->len));
	AUDIT_ARG(fd, uap->fd);

	/*
	 * Sanitize any input parameters that are addr/size/protections
	 */
	kr = mmap_sanitize(user_map,
	    uap->prot,
	    uap->pos,
	    uap->len,
	    uap->addr,
	    flags,
	    &prot,
	    &file_pos,
	    &file_end,
	    &file_size,
	    &user_addr,
	    &user_end,
	    &user_size);
	if (__improbable(kr != KERN_SUCCESS)) {
		assert(vm_sanitize_get_kr(kr));
		return EINVAL;
	}

#if 3777787
	/*
	 * Since the hardware currently does not support writing without
	 * read-before-write, or execution-without-read, if the request is
	 * for write or execute access, we must imply read access as well;
	 * otherwise programs expecting this to work will fail to operate.
	 */
	if (prot & (VM_PROT_EXECUTE | VM_PROT_WRITE)) {
		prot |= VM_PROT_READ;
	}
#endif  /* radar 3777787 */

	/*
	 * verify no unknown flags are passed in, and if any are,
	 * fail out early to make sure the logic below never has to deal
	 * with invalid flag values. only do so for processes compiled
	 * with Fall 2020 or later SDK, which is where we drew this
	 * line and documented it as such.
	 */
	if (flags & ~(MAP_SHARED |
	    MAP_PRIVATE |
	    MAP_COPY |
	    MAP_FIXED |
	    MAP_RENAME |
	    MAP_NORESERVE |
	    MAP_RESERVED0080 |                                  //grandfathered in as accepted and ignored
	    MAP_NOEXTEND |
	    MAP_HASSEMAPHORE |
	    MAP_NOCACHE |
	    MAP_JIT |
	    MAP_TPRO |
	    MAP_FILE |
	    MAP_ANON |
	    MAP_RESILIENT_CODESIGN |
	    MAP_RESILIENT_MEDIA |
#if XNU_TARGET_OS_OSX
	    MAP_32BIT |
#endif
	    MAP_TRANSLATED_ALLOW_EXECUTE |
	    MAP_UNIX03)) {
		if (proc_2020_fall_os_sdk_or_later()) {
			return EINVAL;
		}
	}


	if (flags & MAP_UNIX03) {
		/*
		 * Enforce UNIX03 compliance.
		 */
		if (!(flags & (MAP_PRIVATE | MAP_SHARED))) {
			/* need either MAP_PRIVATE or MAP_SHARED */
			return EINVAL;
		}
	}


	if (flags & MAP_JIT) {
		if ((flags & MAP_FIXED) ||
		    (flags & MAP_SHARED) ||
		    !(flags & MAP_ANON) ||
		    (flags & MAP_RESILIENT_CODESIGN) ||
		    (flags & MAP_RESILIENT_MEDIA) ||
		    (flags & MAP_TPRO)) {
			return EINVAL;
		}
	}

	if ((flags & MAP_RESILIENT_CODESIGN) ||
	    (flags & MAP_RESILIENT_MEDIA)) {
		if ((flags & MAP_ANON) ||
		    (flags & MAP_JIT) ||
		    (flags & MAP_TPRO)) {
			return EINVAL;
		}
	}
	if (flags & MAP_RESILIENT_CODESIGN) {
		int reject_prot = ((flags & MAP_PRIVATE) ? VM_PROT_EXECUTE : (VM_PROT_WRITE | VM_PROT_EXECUTE));
		if (prot & reject_prot) {
			/*
			 * Quick sanity check. maxprot is calculated below and
			 * we will test it again.
			 */
			return EPERM;
		}
	}
	if (flags & MAP_SHARED) {
		/*
		 * MAP_RESILIENT_MEDIA is not valid with MAP_SHARED because
		 * there is no place to inject zero-filled pages without
		 * actually adding them to the file.
		 * Since we didn't reject that combination before, there might
		 * already be callers using it and getting a valid MAP_SHARED
		 * mapping but without the resilience.
		 * For backwards compatibility's sake, let's keep ignoring
		 * MAP_RESILIENT_MEDIA in that case.
		 */
		flags &= ~MAP_RESILIENT_MEDIA;
	}
	if (flags & MAP_RESILIENT_MEDIA) {
		if ((flags & MAP_ANON) ||
		    (flags & MAP_SHARED)) {
			return EINVAL;
		}
	}
	if (flags & MAP_TPRO) {
		/*
		 * MAP_TPRO without VM_PROT_WRITE is not valid here because
		 * the TPRO mapping is handled at the PMAP layer with implicit RW
		 * protections.
		 *
		 * This would enable bypassing of file-based protections, i.e.
		 * a file open/mapped as read-only could be written to.
		 */
		if ((prot & VM_PROT_EXECUTE) ||
		    !(prot & VM_PROT_WRITE)) {
			return EPERM;
		}
	}

	/* Entitlement check against code signing monitor */
	if ((flags & MAP_JIT) && (vm_map_csm_allow_jit(user_map) != KERN_SUCCESS)) {
		printf("[%d] code signing monitor denies JIT mapping\n", proc_pid(p));
		return EPERM;
	}

	if (flags & MAP_ANON) {
		maxprot = VM_PROT_ALL;
#if CONFIG_MACF
		/*
		 * Entitlement check.
		 */
		error = mac_proc_check_map_anon(p, current_cached_proc_cred(p),
		    user_addr, user_size, prot, flags, &maxprot);
		if (error) {
			return EINVAL;
		}
#endif /* MAC */

		/*
		 * Mapping blank space is trivial.  Use positive fds as the alias
		 * value for memory tracking.
		 */
		if (fd != -1) {
			/*
			 * Use "fd" to pass (some) Mach VM allocation flags,
			 * (see the VM_FLAGS_* definitions).
			 */
			int vm_flags = fd & (VM_FLAGS_ALIAS_MASK |
			    VM_FLAGS_SUPERPAGE_MASK |
			    VM_FLAGS_PURGABLE |
			    VM_FLAGS_4GB_CHUNK);

			if (vm_flags != fd) {
				/* reject if there are any extra flags */
				return EINVAL;
			}

			/*
			 * vm_map_kernel_flags_set_vmflags() will assume that
			 * the full set of VM flags are passed, which is
			 * problematic for FIXED/ANYWHERE.
			 *
			 * The block handling MAP_FIXED below will do the same
			 * thing again which is fine because it's idempotent.
			 */
			if (flags & MAP_FIXED) {
				vm_flags |= VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE;
			} else {
				vm_flags |= VM_FLAGS_ANYWHERE;
			}
			vm_map_kernel_flags_set_vmflags(&vmk_flags, vm_flags);
		}

#if CONFIG_MAP_RANGES
		/*
		 * if the client specified a tag, let the system policy apply.
		 *
		 * otherwise, force the heap range.
		 */
		if (vmk_flags.vm_tag) {
			vm_map_kernel_flags_update_range_id(&vmk_flags, user_map, user_size);
		} else {
			vmk_flags.vmkf_range_id = UMEM_RANGE_ID_HEAP;
		}
#endif /* CONFIG_MAP_RANGES */

		handle = NULL;
		file_pos = 0;
		mapanon = 1;
	} else {
		struct vnode_attr va;
		vfs_context_t ctx = vfs_context_current();

		if (flags & MAP_JIT) {
			return EINVAL;
		}

		/*
		 * Mapping file, get fp for validation. Obtain vnode and make
		 * sure it is of appropriate type.
		 */
		err = fp_lookup(p, fd, &fp, 0);
		if (err) {
			return err;
		}
		fpref = 1;
		switch (FILEGLOB_DTYPE(fp->fp_glob)) {
		case DTYPE_PSXSHM:
			error = pshm_mmap(p, VM_SANITIZE_UNSAFE_UNWRAP(uap->addr),
			    user_size, prot, flags, fp,
			    vm_map_trunc_page(file_pos, user_mask),
			    file_pos & user_mask, retval);
			goto bad;
		case DTYPE_VNODE:
			break;
		default:
			error = EINVAL;
			goto bad;
		}
		vp = (struct vnode *)fp_get_data(fp);
		error = vnode_getwithref(vp);
		if (error != 0) {
			goto bad;
		}

		if (vp->v_type != VREG && vp->v_type != VCHR) {
			(void)vnode_put(vp);
			error = EINVAL;
			goto bad;
		}

		AUDIT_ARG(vnpath, vp, ARG_VNODE1);

		/*
		 * POSIX: mmap needs to update access time for mapped files
		 */
		if ((vnode_vfsvisflags(vp) & MNT_NOATIME) == 0) {
			VATTR_INIT(&va);
			nanotime(&va.va_access_time);
			VATTR_SET_ACTIVE(&va, va_access_time);
			vnode_setattr(vp, &va, ctx);
		}

		/*
		 * XXX hack to handle use of /dev/zero to map anon memory (ala
		 * SunOS).
		 */
		if (vp->v_type == VCHR || vp->v_type == VSTR) {
			(void)vnode_put(vp);
			error = ENODEV;
			goto bad;
		} else {
			/*
			 * Ensure that file and memory protections are
			 * compatible.  Note that we only worry about
			 * writability if mapping is shared; in this case,
			 * current and max prot are dictated by the open file.
			 * XXX use the vnode instead?  Problem is: what
			 * credentials do we use for determination? What if
			 * proc does a setuid?
			 */
			maxprot = VM_PROT_EXECUTE;      /* TODO: Remove this and restrict maxprot? */
			if (fp->fp_glob->fg_flag & FREAD) {
				maxprot |= VM_PROT_READ;
			} else if (prot & PROT_READ) {
				(void)vnode_put(vp);
				error = EACCES;
				goto bad;
			}
			/*
			 * If we are sharing potential changes (either via
			 * MAP_SHARED or via the implicit sharing of character
			 * device mappings), and we are trying to get write
			 * permission although we opened it without asking
			 * for it, bail out.
			 */

			if ((flags & MAP_SHARED) != 0) {
				if ((fp->fp_glob->fg_flag & FWRITE) != 0 &&
				    /*
				     * Do not allow writable mappings of
				     * swap files (see vm_swapfile_pager.c).
				     */
				    !vnode_isswap(vp)) {
					/*
					 * check for write access
					 *
					 * Note that we already made this check when granting FWRITE
					 * against the file, so it seems redundant here.
					 */
					error = vnode_authorize(vp, NULL, KAUTH_VNODE_CHECKIMMUTABLE, ctx);

					/* if not granted for any reason, but we wanted it, bad */
					if ((prot & PROT_WRITE) && (error != 0)) {
						vnode_put(vp);
						goto bad;
					}

					/* if writable, remember */
					if (error == 0) {
						maxprot |= VM_PROT_WRITE;
					}
				} else if ((prot & PROT_WRITE) != 0) {
					(void)vnode_put(vp);
					error = EACCES;
					goto bad;
				}
			} else {
				maxprot |= VM_PROT_WRITE;
			}

			handle = (void *)vp;
#if CONFIG_MACF
			error = mac_file_check_mmap(vfs_context_ucred(ctx),
			    fp->fp_glob, prot, flags, file_pos, &maxprot);
			if (error) {
				(void)vnode_put(vp);
				goto bad;
			}
#endif /* MAC */
			/*
			 * Consult the file system to determine if this
			 * particular file object can be mapped.
			 *
			 * N.B. If MAP_PRIVATE (i.e. CoW) has been specified,
			 * then we don't check for writeability on the file
			 * object, because it will only ever see reads.
			 */
			error = VNOP_MMAP_CHECK(vp, (flags & MAP_PRIVATE) ?
			    (prot & ~PROT_WRITE) : prot, ctx);
			if (error) {
				(void)vnode_put(vp);
				goto bad;
			}
		}

		/*
		 * No copy-on-read for mmap() mappings themselves.
		 */
		vmk_flags.vmkf_no_copy_on_read = 1;
#if CONFIG_MAP_RANGES && !XNU_PLATFORM_MacOSX
		/* force file ranges on !macOS */
		vmk_flags.vmkf_range_id = UMEM_RANGE_ID_HEAP;
#if XNU_TARGET_OS_IOS && EXTENDED_USER_VA_SUPPORT
		/*
		 * Put allocations on iOS with EXTENDED_USER_VA_SUPPORT
		 * in the large file range, if the process has the "extra jumbo" entitlement.
		 * Otherwise, place allocation into the heap range.
		 */
		vmk_flags.vmkf_range_id = UMEM_RANGE_ID_LARGE_FILE;
#endif /* XNU_TARGET_OS_IOS && EXTENDED_USER_VA_SUPPORT */
#endif /* CONFIG_MAP_RANGES && !XNU_PLATFORM_MacOSX */
	}

	if (user_size == 0) {
		if (!mapanon) {
			(void)vnode_put(vp);
		}
		error = 0;
		goto bad;
	}

	if (flags & MAP_FIXED) {
		/*
		 * mmap(MAP_FIXED) will replace any existing mappings in the
		 * specified range, if the new mapping is successful.
		 * If we just deallocate the specified address range here,
		 * another thread might jump in and allocate memory in that
		 * range before we get a chance to establish the new mapping,
		 * and we won't have a chance to restore the old mappings.
		 * So we use VM_FLAGS_OVERWRITE to let Mach VM know that it
		 * has to deallocate the existing mappings and establish the
		 * new ones atomically.
		 */
		vmk_flags.vmf_fixed = true;
		vmk_flags.vmf_overwrite = true;
	}

	if (flags & MAP_NOCACHE) {
		vmk_flags.vmf_no_cache = true;
	}

	if (flags & MAP_JIT) {
		vmk_flags.vmkf_map_jit = TRUE;
	}

	if (flags & MAP_TPRO) {
		vmk_flags.vmf_tpro = true;
	}

#if CONFIG_ROSETTA
	if (flags & MAP_TRANSLATED_ALLOW_EXECUTE) {
		if (!proc_is_translated(p)) {
			if (!mapanon) {
				(void)vnode_put(vp);
			}
			error = EINVAL;
			goto bad;
		}
		vmk_flags.vmkf_translated_allow_execute = TRUE;
	}
#endif

	if (flags & MAP_RESILIENT_CODESIGN) {
		vmk_flags.vmf_resilient_codesign = true;
	}
	if (flags & MAP_RESILIENT_MEDIA) {
		vmk_flags.vmf_resilient_media = true;
	}

#if XNU_TARGET_OS_OSX
	/* macOS-specific MAP_32BIT flag handling */
	if (flags & MAP_32BIT) {
		vmk_flags.vmkf_32bit_map_va = TRUE;
	}
#endif

	/*
	 * Lookup/allocate object.
	 */
	if (handle == NULL) {
		control = NULL;
#ifdef notyet
/* Hmm .. */
#if defined(VM_PROT_READ_IS_EXEC)
		if (prot & VM_PROT_READ) {
			prot |= VM_PROT_EXECUTE;
		}
		if (maxprot & VM_PROT_READ) {
			maxprot |= VM_PROT_EXECUTE;
		}
#endif
#endif

#if 3777787
		if (prot & (VM_PROT_EXECUTE | VM_PROT_WRITE)) {
			prot |= VM_PROT_READ;
		}
		if (maxprot & (VM_PROT_EXECUTE | VM_PROT_WRITE)) {
			maxprot |= VM_PROT_READ;
		}
#endif  /* radar 3777787 */
map_anon_retry:

		result = mach_vm_map_kernel(user_map,
		    vm_sanitize_wrap_addr_ref(&user_addr), user_size,
		    0, vmk_flags,
		    IPC_PORT_NULL, 0, FALSE,
		    prot, maxprot,
		    (flags & MAP_SHARED) ?
		    VM_INHERIT_SHARE :
		    VM_INHERIT_DEFAULT);

		/* If a non-binding address was specified for this anonymous
		 * mapping, retry the mapping with a zero base
		 * in the event the mapping operation failed due to
		 * lack of space between the address and the map's maximum.
		 */
		if ((result == KERN_NO_SPACE) && ((flags & MAP_FIXED) == 0) && user_addr && (num_retries++ == 0)) {
			user_addr = vm_map_page_size(user_map);
			goto map_anon_retry;
		}
	} else {
		if (vnode_isswap(vp)) {
			/*
			 * Map swap files with a special pager
			 * that returns obfuscated contents.
			 */
			control = NULL;
			pager = swapfile_pager_setup(vp);
			if (pager != MEMORY_OBJECT_NULL) {
				control = swapfile_pager_control(pager);
			}
		} else {
			control = ubc_getobject(vp, UBC_FLAGS_NONE);
		}

		if (control == NULL) {
			(void)vnode_put(vp);
			error = ENOMEM;
			goto bad;
		}

#if FBDP_DEBUG_OBJECT_NO_PAGER
//#define FBDP_PATH_NAME1 "/private/var/db/timezone/tz/2022a.1.1/icutz/"
#define FBDP_PATH_NAME1 "/private/var/db/timezone/tz/202"
#define FBDP_FILE_NAME1 "icutz44l.dat"
#define FBDP_PATH_NAME2 "/private/var/mobile/Containers/Data/InternalDaemon/"
#define FBDP_FILE_NAME_START2 "com.apple.LaunchServices-"
#define FBDP_FILE_NAME_END2 "-v2.csstore"
		if (!strncmp(vp->v_name, FBDP_FILE_NAME1, strlen(FBDP_FILE_NAME1))) {
			char *path;
			int len;
			bool already_tracked;
			len = MAXPATHLEN;
			path = zalloc_flags(ZV_NAMEI, Z_WAITOK | Z_NOFAIL);
			vn_getpath(vp, path, &len);
			if (!strncmp(path, FBDP_PATH_NAME1, strlen(FBDP_PATH_NAME1))) {
				if (memory_object_mark_as_tracked(control,
				    true,
				    &already_tracked) == KERN_SUCCESS &&
				    !already_tracked) {
					printf("FBDP %s:%d marked vp %p \"%s\" moc %p as tracked\n", __FUNCTION__, __LINE__, vp, path, control);
				}
			}
			zfree(ZV_NAMEI, path);
		} else if (!strncmp(vp->v_name, FBDP_FILE_NAME_START2, strlen(FBDP_FILE_NAME_START2)) &&
		    strlen(vp->v_name) > strlen(FBDP_FILE_NAME_START2) + strlen(FBDP_FILE_NAME_END2) &&
		    !strncmp(vp->v_name + strlen(vp->v_name) - strlen(FBDP_FILE_NAME_END2),
		    FBDP_FILE_NAME_END2,
		    strlen(FBDP_FILE_NAME_END2))) {
			char *path;
			int len;
			bool already_tracked;
			len = MAXPATHLEN;
			path = zalloc_flags(ZV_NAMEI, Z_WAITOK | Z_NOFAIL);
			vn_getpath(vp, path, &len);
			if (!strncmp(path, FBDP_PATH_NAME2, strlen(FBDP_PATH_NAME2))) {
				if (memory_object_mark_as_tracked(control,
				    true,
				    &already_tracked) == KERN_SUCCESS &&
				    !already_tracked) {
					printf("FBDP %s:%d marked vp %p \"%s\" moc %p as tracked\n", __FUNCTION__, __LINE__, vp, path, control);
				}
			}
			zfree(ZV_NAMEI, path);
		}
#endif /* FBDP_DEBUG_OBJECT_NO_PAGER */

		/*
		 *  Set credentials:
		 *	FIXME: if we're writing the file we need a way to
		 *      ensure that someone doesn't replace our R/W creds
		 *      with ones that only work for read.
		 */

		ubc_setthreadcred(vp, p, current_thread());
		docow = FALSE;
		if ((flags & (MAP_ANON | MAP_SHARED)) == 0) {
			docow = TRUE;
		}

#ifdef notyet
/* Hmm .. */
#if defined(VM_PROT_READ_IS_EXEC)
		if (prot & VM_PROT_READ) {
			prot |= VM_PROT_EXECUTE;
		}
		if (maxprot & VM_PROT_READ) {
			maxprot |= VM_PROT_EXECUTE;
		}
#endif
#endif /* notyet */

#if 3777787
		if (prot & (VM_PROT_EXECUTE | VM_PROT_WRITE)) {
			prot |= VM_PROT_READ;
		}
		if (maxprot & (VM_PROT_EXECUTE | VM_PROT_WRITE)) {
			maxprot |= VM_PROT_READ;
		}
#endif  /* radar 3777787 */

map_file_retry:
		if (flags & MAP_RESILIENT_CODESIGN) {
			int reject_prot = ((flags & MAP_PRIVATE) ? VM_PROT_EXECUTE : (VM_PROT_WRITE | VM_PROT_EXECUTE));
			if (prot & reject_prot) {
				/*
				 * Would like to use (prot | maxprot) here
				 * but the assignment of VM_PROT_EXECUTE
				 * to maxprot above would always fail the test.
				 *
				 * Skipping the check is ok, however, because we
				 * restrict maxprot to prot just below in this
				 * block.
				 */
				assert(!mapanon);
				vnode_put(vp);
				error = EPERM;
				goto bad;
			}
			/* strictly limit access to "prot" */
			maxprot &= prot;
		}

		result = vm_map_enter_mem_object_control(user_map,
		    vm_sanitize_wrap_addr_ref(&user_addr), user_size,
		    0, vmk_flags,
		    control, vm_map_trunc_page(file_pos, user_mask),
		    docow, prot, maxprot,
		    (flags & MAP_SHARED) ?
		    VM_INHERIT_SHARE :
		    VM_INHERIT_DEFAULT);

		/* If a non-binding address was specified for this file backed
		 * mapping, retry the mapping with a zero base
		 * in the event the mapping operation failed due to
		 * lack of space between the address and the map's maximum.
		 */
		if ((result == KERN_NO_SPACE) && ((flags & MAP_FIXED) == 0) && user_addr && (num_retries++ == 0)) {
			user_addr = vm_map_page_size(user_map);
			goto map_file_retry;
		}
	}

	if (!mapanon) {
		(void)vnode_put(vp);
	}

	switch (result) {
	case KERN_SUCCESS:
		*retval = user_addr + (file_pos & user_mask);
		error = 0;
		break;
	case KERN_INVALID_ADDRESS:
	case KERN_NO_SPACE:
		error =  ENOMEM;
		break;
	case KERN_PROTECTION_FAILURE:
		error =  EACCES;
		break;
	default:
		error =  EINVAL;
		break;
	}
bad:
	if (pager != MEMORY_OBJECT_NULL) {
		/*
		 * Release the reference on the pager.
		 * If the mapping was successful, it now holds
		 * an extra reference.
		 */
		memory_object_deallocate(pager);
	}
	if (fpref) {
		fp_drop(p, fd, fp, 0);
	}

	KERNEL_DEBUG_CONSTANT((BSDDBG_CODE(DBG_BSD_SC_EXTENDED_INFO, SYS_mmap) | DBG_FUNC_NONE), fd, (uint32_t)(*retval), (uint32_t)user_size, error, 0);
#if XNU_TARGET_OS_OSX
	KERNEL_DEBUG_CONSTANT((BSDDBG_CODE(DBG_BSD_SC_EXTENDED_INFO2, SYS_mmap) | DBG_FUNC_NONE), (uint32_t)(*retval >> 32), (uint32_t)(user_size >> 32),
	    (uint32_t)(file_pos >> 32), (uint32_t)file_pos, 0);
#endif /* XNU_TARGET_OS_OSX */
	return error;
}

int
msync(__unused proc_t p, struct msync_args *uap, int32_t *retval)
{
	__pthread_testcancel(1);
	return msync_nocancel(p, (struct msync_nocancel_args *)uap, retval);
}

int
msync_nocancel(__unused proc_t p, struct msync_nocancel_args *uap, __unused int32_t *retval)
{
	mach_vm_offset_t addr;
	mach_vm_size_t size;
	int flags;
	vm_map_t user_map;
	int rv;
	vm_sync_t sync_flags = 0;

	user_map = current_map();
	addr = (mach_vm_offset_t) uap->addr;
	size = (mach_vm_size_t) uap->len;
#if XNU_TARGET_OS_OSX
	KERNEL_DEBUG_CONSTANT((BSDDBG_CODE(DBG_BSD_SC_EXTENDED_INFO, SYS_msync) | DBG_FUNC_NONE), (uint32_t)(addr >> 32), (uint32_t)(size >> 32), 0, 0, 0);
#endif /* XNU_TARGET_OS_OSX */
	if (vm_map_range_overflows(user_map, addr, size)) {
		return EINVAL;
	}
	if (addr & vm_map_page_mask(user_map)) {
		/* UNIX SPEC: user address is not page-aligned, return EINVAL */
		return EINVAL;
	}
	if (size == 0) {
		/*
		 * We cannot support this properly without maintaining
		 * list all mmaps done. Cannot use vm_map_entry as they could be
		 * split or coalesced by indepenedant actions. So instead of
		 * inaccurate results, lets just return error as invalid size
		 * specified
		 */
		return EINVAL; /* XXX breaks posix apps */
	}

	flags = uap->flags;
	/* disallow contradictory flags */
	if ((flags & (MS_SYNC | MS_ASYNC)) == (MS_SYNC | MS_ASYNC)) {
		return EINVAL;
	}

	if (flags & MS_KILLPAGES) {
		sync_flags |= VM_SYNC_KILLPAGES;
	}
	if (flags & MS_DEACTIVATE) {
		sync_flags |= VM_SYNC_DEACTIVATE;
	}
	if (flags & MS_INVALIDATE) {
		sync_flags |= VM_SYNC_INVALIDATE;
	}

	if (!(flags & (MS_KILLPAGES | MS_DEACTIVATE))) {
		if (flags & MS_ASYNC) {
			sync_flags |= VM_SYNC_ASYNCHRONOUS;
		} else {
			sync_flags |= VM_SYNC_SYNCHRONOUS;
		}
	}

	sync_flags |= VM_SYNC_CONTIGUOUS;       /* complain if holes */

	rv = mach_vm_msync(user_map, addr, size, sync_flags);

	switch (rv) {
	case KERN_SUCCESS:
		break;
	case KERN_INVALID_ADDRESS:      /* hole in region being sync'ed */
		return ENOMEM;
	case KERN_FAILURE:
		return EIO;
	default:
		return EINVAL;
	}
	return 0;
}

static inline kern_return_t
munmap_sanitize(
	vm_map_t                user_map,
	vm_addr_struct_t        addr_u,
	vm_size_struct_t        len_u,
	mach_vm_offset_t       *user_addr,
	mach_vm_offset_t       *user_end,
	mach_vm_size_t         *user_size)
{
	return vm_sanitize_addr_size(addr_u, len_u, VM_SANITIZE_CALLER_MUNMAP,
	           user_map,
	           VM_SANITIZE_FLAGS_CHECK_ALIGNED_START | VM_SANITIZE_FLAGS_SIZE_ZERO_FAILS,
	           user_addr, user_end, user_size);
}

int
munmap(__unused proc_t p, struct munmap_args *uap, __unused int32_t *retval)
{
	mach_vm_offset_t        user_addr, user_end;
	mach_vm_size_t          user_size;
	kern_return_t           result;
	vm_map_t                user_map;

	user_map = current_map();

	AUDIT_ARG(addr, VM_SANITIZE_UNSAFE_UNWRAP(uap->addr));
	AUDIT_ARG(len, VM_SANITIZE_UNSAFE_UNWRAP(uap->len));

	/*
	 * Sanitize any input parameters that are addr/size/protections
	 */
	result = munmap_sanitize(user_map,
	    uap->addr,
	    uap->len,
	    &user_addr,
	    &user_end,
	    &user_size);
	if (__improbable(result != KERN_SUCCESS)) {
		assert(vm_sanitize_get_kr(result) ==
		    KERN_INVALID_ARGUMENT);
		return EINVAL;
	}
	if (mach_vm_deallocate(user_map, user_addr, user_size)) {
		return EINVAL;
	}
	return 0;
}

int
mprotect(__unused proc_t p, struct mprotect_args *uap, __unused int32_t *retval)
{
	vm_prot_t prot;
	mach_vm_offset_t        user_addr;
	mach_vm_size_t  user_size;
	kern_return_t   result;
	vm_map_t        user_map;
#if CONFIG_MACF
	int error;
#endif

	AUDIT_ARG(addr, uap->addr);
	AUDIT_ARG(len, uap->len);
	AUDIT_ARG(value32, uap->prot);

	user_map = current_map();
	user_addr = (mach_vm_offset_t) uap->addr;
	user_size = (mach_vm_size_t) uap->len;
	prot = (vm_prot_t)(uap->prot & (VM_PROT_ALL | VM_PROT_TRUSTED | VM_PROT_STRIP_READ));

	if (vm_map_range_overflows(user_map, user_addr, user_size)) {
		return EINVAL;
	}
	if (user_addr & vm_map_page_mask(user_map)) {
		/* UNIX SPEC: user address is not page-aligned, return EINVAL */
		return EINVAL;
	}

#ifdef notyet
/* Hmm .. */
#if defined(VM_PROT_READ_IS_EXEC)
	if (prot & VM_PROT_READ) {
		prot |= VM_PROT_EXECUTE;
	}
#endif
#endif /* notyet */

#if 3936456
	if (prot & (VM_PROT_EXECUTE | VM_PROT_WRITE)) {
		prot |= VM_PROT_READ;
	}
#endif  /* 3936456 */

#if CONFIG_MACF
	/*
	 * The MAC check for mprotect is of limited use for 2 reasons:
	 * Without mmap revocation, the caller could have asked for the max
	 * protections initially instead of a reduced set, so a mprotect
	 * check would offer no new security.
	 * It is not possible to extract the vnode from the pager object(s)
	 * of the target memory range.
	 * However, the MAC check may be used to prevent a process from,
	 * e.g., making the stack executable.
	 */
	error = mac_proc_check_mprotect(p, user_addr,
	    user_size, prot);
	if (error) {
		return error;
	}
#endif

	if (prot & VM_PROT_TRUSTED) {
#if CONFIG_DYNAMIC_CODE_SIGNING
		/* CODE SIGNING ENFORCEMENT - JIT support */
		/* The special protection value VM_PROT_TRUSTED requests that we treat
		 * this page as if it had a valid code signature.
		 * If this is enabled, there MUST be a MAC policy implementing the
		 * mac_proc_check_mprotect() hook above. Otherwise, Codesigning will be
		 * compromised because the check would always succeed and thusly any
		 * process could sign dynamically. */
		result = vm_map_sign(
			user_map,
			vm_map_trunc_page(user_addr,
			vm_map_page_mask(user_map)),
			vm_map_round_page(user_addr + user_size,
			vm_map_page_mask(user_map)));
		switch (result) {
		case KERN_SUCCESS:
			break;
		case KERN_INVALID_ADDRESS:
			/* UNIX SPEC: for an invalid address range, return ENOMEM */
			return ENOMEM;
		default:
			return EINVAL;
		}
#else
		return ENOTSUP;
#endif
	}
	prot &= ~VM_PROT_TRUSTED;

	result = mach_vm_protect(user_map, user_addr, user_size,
	    FALSE, prot);
	switch (result) {
	case KERN_SUCCESS:
		return 0;
	case KERN_PROTECTION_FAILURE:
		return EACCES;
	case KERN_INVALID_ADDRESS:
		/* UNIX SPEC: for an invalid address range, return ENOMEM */
		return ENOMEM;
	}
	return EINVAL;
}


int
minherit(__unused proc_t p, struct minherit_args *uap, __unused int32_t *retval)
{
	mach_vm_offset_t addr;
	mach_vm_size_t size;
	vm_inherit_t inherit;
	vm_map_t        user_map;
	kern_return_t   result;

	AUDIT_ARG(addr, uap->addr);
	AUDIT_ARG(len, uap->len);
	AUDIT_ARG(value32, uap->inherit);

	user_map = current_map();
	addr = (mach_vm_offset_t)uap->addr;
	size = (mach_vm_size_t)uap->len;
	inherit = uap->inherit;
	if (vm_map_range_overflows(user_map, addr, size)) {
		return EINVAL;
	}
	result = mach_vm_inherit(user_map, addr, size,
	    inherit);
	switch (result) {
	case KERN_SUCCESS:
		return 0;
	case KERN_PROTECTION_FAILURE:
		return EACCES;
	}
	return EINVAL;
}

int
madvise(__unused proc_t p, struct madvise_args *uap, __unused int32_t *retval)
{
	vm_map_t user_map;
	mach_vm_offset_t start;
	mach_vm_size_t size;
	vm_behavior_t new_behavior;
	kern_return_t   result;

	/*
	 * Since this routine is only advisory, we default to conservative
	 * behavior.
	 */
	switch (uap->behav) {
	case MADV_RANDOM:
		new_behavior = VM_BEHAVIOR_RANDOM;
		break;
	case MADV_SEQUENTIAL:
		new_behavior = VM_BEHAVIOR_SEQUENTIAL;
		break;
	case MADV_NORMAL:
		new_behavior = VM_BEHAVIOR_DEFAULT;
		break;
	case MADV_WILLNEED:
		new_behavior = VM_BEHAVIOR_WILLNEED;
		break;
	case MADV_DONTNEED:
		new_behavior = VM_BEHAVIOR_DONTNEED;
		break;
	case MADV_FREE:
		new_behavior = VM_BEHAVIOR_FREE;
		break;
	case MADV_ZERO_WIRED_PAGES:
		new_behavior = VM_BEHAVIOR_ZERO_WIRED_PAGES;
		break;
	case MADV_FREE_REUSABLE:
		new_behavior = VM_BEHAVIOR_REUSABLE;
		break;
	case MADV_FREE_REUSE:
		new_behavior = VM_BEHAVIOR_REUSE;
		break;
	case MADV_CAN_REUSE:
		new_behavior = VM_BEHAVIOR_CAN_REUSE;
		break;
	case MADV_PAGEOUT:
#if MACH_ASSERT
		new_behavior = VM_BEHAVIOR_PAGEOUT;
		break;
#else /* MACH_ASSERT */
		return ENOTSUP;
#endif /* MACH_ASSERT */
	case MADV_ZERO:
		new_behavior = VM_BEHAVIOR_ZERO;
		break;
	default:
		return EINVAL;
	}

	user_map = current_map();
	start = (mach_vm_offset_t) uap->addr;
	size = (mach_vm_size_t) uap->len;
	if (vm_map_range_overflows(user_map, start, size)) {
		return EINVAL;
	}
#if __arm64__
	if (start == 0 &&
	    size != 0 &&
	    (uap->behav == MADV_FREE ||
	    uap->behav == MADV_FREE_REUSABLE)) {
		printf("** %s: %d[%s] "
		    "failing madvise(0x%llx,0x%llx,%s)\n",
		    __func__, proc_getpid(p), p->p_comm, start, size,
		    ((uap->behav == MADV_FREE_REUSABLE)
		    ? "MADV_FREE_REUSABLE"
		    : "MADV_FREE"));
		return EINVAL;
	}
#endif /* __arm64__ */

	result = mach_vm_behavior_set(user_map, start, size, new_behavior);
	switch (result) {
	case KERN_SUCCESS:
		return 0;
	case KERN_INVALID_ADDRESS:
		return EINVAL;
	case KERN_NO_SPACE:
		return ENOMEM;
	case KERN_PROTECTION_FAILURE:
		return EPERM;
	case KERN_NO_ACCESS:
		return ENOTSUP;
	}

	return EINVAL;
}

int
mincore(__unused proc_t p, struct mincore_args *uap, __unused int32_t *retval)
{
	mach_vm_offset_t addr = 0, first_addr = 0, end = 0, cur_end = 0;
	vm_map_t map = VM_MAP_NULL;
	user_addr_t vec = 0;
	int error = 0;
	int64_t lastvecindex = 0;
	int mincoreinfo = 0;
	int pqueryinfo = 0;
	uint64_t pqueryinfo_vec_size = 0;
	vm_page_info_basic_t info = NULL;
	mach_msg_type_number_t count = 0;
	char *kernel_vec = NULL;
	uint64_t req_vec_size_pages = 0, cur_vec_size_pages = 0, vecindex = 0;
	kern_return_t kr = KERN_SUCCESS;
	int effective_page_shift, effective_page_size;

	map = current_map();

	/*
	 * On systems with 4k kernel space and 16k user space, we will
	 * use the kernel page size to report back the residency information.
	 * This is for backwards compatibility since we already have
	 * processes that depend on this behavior.
	 */
	if (vm_map_page_shift(map) < PAGE_SHIFT) {
		effective_page_shift = vm_map_page_shift(map);
		effective_page_size = vm_map_page_size(map);
	} else {
		effective_page_shift = PAGE_SHIFT;
		effective_page_size = PAGE_SIZE;
	}

	/*
	 * Make sure that the addresses presented are valid for user
	 * mode.
	 */
	first_addr = addr = vm_map_trunc_page(uap->addr,
	    vm_map_page_mask(map));
	end = vm_map_round_page(uap->addr + uap->len,
	    vm_map_page_mask(map));

	if (end < addr) {
		return EINVAL;
	}

	if (end == addr) {
		return 0;
	}

	/*
	 * We are going to loop through the whole 'req_vec_size' pages
	 * range in chunks of 'cur_vec_size'.
	 */

	req_vec_size_pages = (end - addr) >> effective_page_shift;
	cur_vec_size_pages = MIN(req_vec_size_pages, (MAX_PAGE_RANGE_QUERY >> effective_page_shift));
	size_t kernel_vec_size = cur_vec_size_pages;

	kernel_vec = (char *)kalloc_data(kernel_vec_size, Z_WAITOK | Z_ZERO);

	if (kernel_vec == NULL) {
		return ENOMEM;
	}

	/*
	 * Address of byte vector
	 */
	vec = uap->vec;

	pqueryinfo_vec_size = cur_vec_size_pages * sizeof(struct vm_page_info_basic);

	info = (struct vm_page_info_basic *)kalloc_data(pqueryinfo_vec_size, Z_WAITOK);

	if (info == NULL) {
		kfree_data(kernel_vec, kernel_vec_size);
		return ENOMEM;
	}

	while (addr < end) {
		cur_end = addr + (cur_vec_size_pages * effective_page_size);

		count =  VM_PAGE_INFO_BASIC_COUNT;
		kr = vm_map_page_range_info_internal(map,
		    addr,
		    cur_end,
		    effective_page_shift,
		    VM_PAGE_INFO_BASIC,
		    (vm_page_info_t) info,
		    &count);

		assert(kr == KERN_SUCCESS);

		/*
		 * Do this on a map entry basis so that if the pages are not
		 * in the current processes address space, we can easily look
		 * up the pages elsewhere.
		 */
		lastvecindex = -1;

		for (; addr < cur_end; addr += effective_page_size) {
			pqueryinfo = info[lastvecindex + 1].disposition;

			mincoreinfo = 0;

			if (pqueryinfo & VM_PAGE_QUERY_PAGE_PRESENT) {
				mincoreinfo |= MINCORE_INCORE;
			}
			if (pqueryinfo & VM_PAGE_QUERY_PAGE_REF) {
				mincoreinfo |= MINCORE_REFERENCED;
			}
			if (pqueryinfo & VM_PAGE_QUERY_PAGE_DIRTY) {
				mincoreinfo |= MINCORE_MODIFIED;
			}
			if (pqueryinfo & VM_PAGE_QUERY_PAGE_PAGED_OUT) {
				mincoreinfo |= MINCORE_PAGED_OUT;
			}
			if (pqueryinfo & VM_PAGE_QUERY_PAGE_COPIED) {
				mincoreinfo |= MINCORE_COPIED;
			}
			if ((pqueryinfo & VM_PAGE_QUERY_PAGE_EXTERNAL) == 0) {
				mincoreinfo |= MINCORE_ANONYMOUS;
			}
			/*
			 * calculate index into user supplied byte vector
			 */
			vecindex = (addr - first_addr) >> effective_page_shift;
			kernel_vec[vecindex] = (char)mincoreinfo;
			lastvecindex = vecindex;
		}


		assert(vecindex == (cur_vec_size_pages - 1));

		error = copyout(kernel_vec, vec, cur_vec_size_pages * sizeof(char) /* a char per page */);

		if (error) {
			break;
		}

		/*
		 * For the next chunk, we'll need:
		 * - bump the location in the user buffer for our next disposition.
		 * - new length
		 * - starting address
		 */
		vec += cur_vec_size_pages * sizeof(char);
		req_vec_size_pages = (end - addr) >> effective_page_shift;
		cur_vec_size_pages = MIN(req_vec_size_pages, (MAX_PAGE_RANGE_QUERY >> effective_page_shift));

		first_addr = addr;
	}

	kfree_data(info, pqueryinfo_vec_size);
	kfree_data(kernel_vec, kernel_vec_size);

	if (error) {
		return EFAULT;
	}

	return 0;
}

int
mlock(__unused proc_t p, struct mlock_args *uap, __unused int32_t *retvalval)
{
	kern_return_t result;

	AUDIT_ARG(addr, VM_SANITIZE_UNSAFE_UNWRAP(uap->addr));
	AUDIT_ARG(len, VM_SANITIZE_UNSAFE_UNWRAP(uap->len));

	/* have to call vm_map_wire directly to pass "I don't know" protections */
	result = vm_map_wire_kernel(current_map(), uap->addr,
	    vm_sanitize_compute_unsafe_end(uap->addr, uap->len),
	    vm_sanitize_wrap_prot(VM_PROT_NONE), VM_KERN_MEMORY_MLOCK, TRUE);

	switch (result) {
	case KERN_SUCCESS:
		return 0;
	case KERN_INVALID_ARGUMENT:
		return EINVAL;
	case KERN_RESOURCE_SHORTAGE:
		return EAGAIN;
	case KERN_PROTECTION_FAILURE:
		return EPERM;
	default:
		return ENOMEM;
	}
}

int
munlock(__unused proc_t p, struct munlock_args *uap, __unused int32_t *retval)
{
	kern_return_t result;

	AUDIT_ARG(addr, VM_SANITIZE_UNSAFE_UNWRAP(uap->addr));
	AUDIT_ARG(len, VM_SANITIZE_UNSAFE_UNWRAP(uap->len));

	/* JMM - need to remove all wirings by spec - this just removes one */
	result = vm_map_unwire(current_map(), uap->addr,
	    vm_sanitize_compute_unsafe_end(uap->addr, uap->len), TRUE);

	switch (result) {
	case KERN_SUCCESS:
		return 0;
	case KERN_INVALID_ARGUMENT:
		return EINVAL;
	default:
		return ENOMEM;
	}
}


int
mlockall(__unused proc_t p, __unused struct mlockall_args *uap, __unused int32_t *retval)
{
	return ENOSYS;
}

int
munlockall(__unused proc_t p, __unused struct munlockall_args *uap, __unused int32_t *retval)
{
	return ENOSYS;
}

#if CONFIG_CODE_DECRYPTION
static inline kern_return_t
mremap_encrypted_sanitize(
	vm_map_t                user_map,
	vm_addr_struct_t        addr_u,
	vm_size_struct_t        len_u,
	mach_vm_offset_t       *user_addr,
	mach_vm_offset_t       *user_end,
	mach_vm_size_t         *user_size)
{
	return vm_sanitize_addr_size(addr_u, len_u,
	           VM_SANITIZE_CALLER_MREMAP_ENCRYPTED, user_map,
	           VM_SANITIZE_FLAGS_CHECK_ALIGNED_START | VM_SANITIZE_FLAGS_SIZE_ZERO_FALLTHROUGH,
	           user_addr, user_end, user_size);
}

int
mremap_encrypted(__unused struct proc *p, struct mremap_encrypted_args *uap, __unused int32_t *retval)
{
	mach_vm_offset_t    user_addr, user_end;
	mach_vm_size_t      user_size;
	kern_return_t       result;
	vm_map_t    user_map;
	uint32_t    cryptid;
	cpu_type_t  cputype;
	cpu_subtype_t       cpusubtype;
	pager_crypt_info_t  crypt_info;
	const char * cryptname = 0;
	char *vpath;
	int len, ret;
	struct proc_regioninfo_internal pinfo;
	vnode_t vp;
	uintptr_t vnodeaddr;
	uint32_t vid;

	AUDIT_ARG(addr, VM_SANITIZE_UNSAFE_UNWRAP(uap->addr));
	AUDIT_ARG(len, VM_SANITIZE_UNSAFE_UNWRAP(uap->len));

	user_map   = current_map();
	cryptid    = uap->cryptid;
	cputype    = uap->cputype;
	cpusubtype = uap->cpusubtype;

	/*
	 * Sanitize any input parameters that are addr/size/protections
	 */
	result = mremap_encrypted_sanitize(user_map,
	    uap->addr,
	    uap->len,
	    &user_addr,
	    &user_end,
	    &user_size);
	if (__improbable(result != KERN_SUCCESS)) {
		assert(vm_sanitize_get_kr(result));
		return EINVAL;
	}

	switch (cryptid) {
	case CRYPTID_NO_ENCRYPTION:
		/* not encrypted, just an empty load command */
		return 0;
	case CRYPTID_APP_ENCRYPTION:
	case CRYPTID_MODEL_ENCRYPTION:
		cryptname = "com.apple.unfree";
		break;
	case 0x10:
		/* some random cryptid that you could manually put into
		 * your binary if you want NULL */
		cryptname = "com.apple.null";
		break;
	default:
		return EINVAL;
	}

	if (NULL == text_crypter_create) {
		return ENOTSUP;
	}

	ret = fill_procregioninfo_onlymappedvnodes( proc_task(p), user_addr, &pinfo, &vnodeaddr, &vid);
	if (ret == 0 || !vnodeaddr) {
		/* No really, this returns 0 if the memory address is not backed by a file */
		return EINVAL;
	}

	vp = (vnode_t)vnodeaddr;
	if ((vnode_getwithvid(vp, vid)) == 0) {
		vpath = zalloc(ZV_NAMEI);

		len = MAXPATHLEN;
		ret = vn_getpath(vp, vpath, &len);
		if (ret) {
			zfree(ZV_NAMEI, vpath);
			vnode_put(vp);
			return ret;
		}

		vnode_put(vp);
	} else {
		return EINVAL;
	}

#if 0
	kprintf("%s vpath %s cryptid 0x%08x cputype 0x%08x cpusubtype 0x%08x range 0x%016llx size 0x%016llx\n",
	    __FUNCTION__, vpath, cryptid, cputype, cpusubtype, (uint64_t)user_addr, (uint64_t)user_size);
#endif

	if (user_size == 0) {
		printf("%s:%d '%s': user_addr 0x%llx user_size 0x%llx cryptid 0x%x ignored\n", __FUNCTION__, __LINE__, vpath, user_addr, user_size, cryptid);
		zfree(ZV_NAMEI, vpath);
		return 0;
	}

	/* set up decrypter first */
	crypt_file_data_t crypt_data = {
		.filename = vpath,
		.cputype = cputype,
		.cpusubtype = cpusubtype,
		.origin = CRYPT_ORIGIN_LIBRARY_LOAD,
	};
	result = text_crypter_create(&crypt_info, cryptname, (void*)&crypt_data);
#if VM_MAP_DEBUG_APPLE_PROTECT
	if (vm_map_debug_apple_protect) {
		printf("APPLE_PROTECT: %d[%s] map %p [0x%llx:0x%llx] %s(%s) -> 0x%x\n",
		    proc_getpid(p), p->p_comm,
		    user_map,
		    (uint64_t) user_addr,
		    (uint64_t) (user_addr + user_size),
		    __FUNCTION__, vpath, result);
	}
#endif /* VM_MAP_DEBUG_APPLE_PROTECT */
	zfree(ZV_NAMEI, vpath);

	if (result) {
		printf("%s: unable to create decrypter %s, kr=%d\n",
		    __FUNCTION__, cryptname, result);
		if (result == kIOReturnNotPrivileged) {
			/* text encryption returned decryption failure */
			return EPERM;
		} else {
			return ENOMEM;
		}
	}

	/* now remap using the decrypter */
	vm_object_offset_t crypto_backing_offset;
	crypto_backing_offset = -1; /* i.e. use map entry's offset */
	result = vm_map_apple_protected(user_map,
	    user_addr,
	    user_addr + user_size,
	    crypto_backing_offset,
	    &crypt_info,
	    cryptid);
	if (result) {
		printf("%s: mapping failed with %d\n", __FUNCTION__, result);
	}

	if (result) {
		return EPERM;
	}
	return 0;
}
#endif /* CONFIG_CODE_DECRYPTION */
