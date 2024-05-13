/*
 * Copyright (c) 2000-2019 Apple Inc. All rights reserved.
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

#ifndef _KERN_DEBUG_H_
#define _KERN_DEBUG_H_

#include <kern/kcdata.h>

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdarg.h>
#include <uuid/uuid.h>
#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/vm_types.h>

#ifndef XNU_KERNEL_PRIVATE
#include <TargetConditionals.h>
#endif

__BEGIN_DECLS

#ifdef __APPLE_API_PRIVATE
#ifdef __APPLE_API_UNSTABLE

struct thread_snapshot {
	uint32_t                snapshot_magic;
	uint32_t                nkern_frames;
	uint32_t                nuser_frames;
	uint64_t                wait_event;
	uint64_t                continuation;
	uint64_t                thread_id;
	uint64_t                user_time;
	uint64_t                system_time;
	int32_t                 state;
	int32_t                 priority;    /*	static priority */
	int32_t                 sched_pri;   /* scheduled (current) priority */
	int32_t                 sched_flags; /* scheduler flags */
	char                    ss_flags;
	char                    ts_qos;      /* effective qos */
	char                    ts_rqos;     /* requested qos */
	char                    ts_rqos_override; /* requested qos override */
	char                    io_tier;
	/*
	 * In microstackshots, the low two bytes are the start of the first async
	 * frame in the thread's user space call stack.  If the call stack lacks
	 * async stack frames, it's `UINT16_MAX`.
	 */
	char                    _reserved[3]; /* pad for 4 byte alignement packing */

	/*
	 * I/O Statistics
	 * XXX: These fields must be together
	 */
	uint64_t                disk_reads_count;
	uint64_t                disk_reads_size;
	uint64_t                disk_writes_count;
	uint64_t                disk_writes_size;
	uint64_t                io_priority_count[STACKSHOT_IO_NUM_PRIORITIES];
	uint64_t                io_priority_size[STACKSHOT_IO_NUM_PRIORITIES];
	uint64_t                paging_count;
	uint64_t                paging_size;
	uint64_t                non_paging_count;
	uint64_t                non_paging_size;
	uint64_t                data_count;
	uint64_t                data_size;
	uint64_t                metadata_count;
	uint64_t                metadata_size;
	/* XXX: I/O Statistics end */

	uint64_t                voucher_identifier; /* obfuscated voucher identifier */
	uint64_t                total_syscalls;
	char                    pth_name[STACKSHOT_MAX_THREAD_NAME_SIZE];
} __attribute__((packed));

/* old, non kcdata format */
struct task_snapshot {
	uint32_t snapshot_magic;
	int32_t pid;
	uint64_t                uniqueid;
	uint64_t                user_time_in_terminated_threads;
	uint64_t                system_time_in_terminated_threads;
	uint8_t                 shared_cache_identifier[16];
	uint64_t                shared_cache_slide;
	uint32_t                nloadinfos;
	int                     suspend_count;
	int                     task_size;      /* pages */
	int                     faults;         /* number of page faults */
	int                     pageins;        /* number of actual pageins */
	int                     cow_faults;     /* number of copy-on-write faults */
	uint32_t                ss_flags;
	/*
	 * In microstackshots, `p_start_sec` is actually the resource coalition ID
	 * that this thread belongs to.
	 */
	uint64_t                p_start_sec;    /* from the bsd proc struct */
	/*
	 * In microstackshots, `p_stat_usec` is actually the resource coalition ID
	 * that this thread is doing work on behalf of.
	 */
	uint64_t                p_start_usec;   /* from the bsd proc struct */

	/*
	 * We restrict ourselves to a statically defined
	 * (current as of 2009) length for the
	 * p_comm string, due to scoping issues (osfmk/bsd and user/kernel
	 * binary compatibility).
	 */
	char                    p_comm[17];
	uint32_t                was_throttled;
	uint32_t                did_throttle;
	uint32_t                latency_qos;
	/*
	 * I/O Statistics
	 * XXX: These fields must be together.
	 */
	uint64_t                disk_reads_count;
	uint64_t                disk_reads_size;
	uint64_t                disk_writes_count;
	uint64_t                disk_writes_size;
	uint64_t                io_priority_count[STACKSHOT_IO_NUM_PRIORITIES];
	uint64_t                io_priority_size[STACKSHOT_IO_NUM_PRIORITIES];
	uint64_t                paging_count;
	uint64_t                paging_size;
	uint64_t                non_paging_count;
	uint64_t                non_paging_size;
	uint64_t                data_count;
	uint64_t                data_size;
	uint64_t                metadata_count;
	uint64_t                metadata_size;
	/* XXX: I/O Statistics end */

	uint32_t                donating_pid_count;
} __attribute__ ((packed));



struct micro_snapshot {
	uint32_t                snapshot_magic;
	uint32_t                ms_cpu;  /* cpu number this snapshot was recorded on */
	uint64_t                ms_time; /* time at sample (seconds) */
	uint64_t                ms_time_microsecs;
	uint8_t                 ms_flags;
	uint16_t                ms_opaque_flags;        /* managed by external entity, e.g. fdrmicrod */
} __attribute__ ((packed));


/*
 * mirrors the dyld_cache_header struct defined in dyld_cache_format.h from dyld source code
 */
struct _dyld_cache_header {
	char        magic[16];                          // e.g. "dyld_v0    i386"
	uint32_t    mappingOffset;      // file offset to first dyld_cache_mapping_info
	uint32_t    mappingCount;       // number of dyld_cache_mapping_info entries
	uint32_t    imagesOffset;       // file offset to first dyld_cache_image_info
	uint32_t    imagesCount;        // number of dyld_cache_image_info entries
	uint64_t    dyldBaseAddress;    // base address of dyld when cache was built
	uint64_t    codeSignatureOffset;// file offset of code signature blob
	uint64_t    codeSignatureSize;  // size of code signature blob (zero means to end of file)
	uint64_t    slideInfoOffset;    // file offset of kernel slid info
	uint64_t    slideInfoSize;      // size of kernel slid info
	uint64_t    localSymbolsOffset; // file offset of where local symbols are stored
	uint64_t    localSymbolsSize;   // size of local symbols information
	uint8_t     uuid[16];           // unique value for each shared cache file
	uint64_t    cacheType;          // 0 for development, 1 for production
	uint32_t    branchPoolsOffset;  // file offset to table of uint64_t pool addresses
	uint32_t    branchPoolsCount;   // number of uint64_t entries
	uint64_t    accelerateInfoAddr; // (unslid) address of optimization info
	uint64_t    accelerateInfoSize; // size of optimization info
	uint64_t    imagesTextOffset;   // file offset to first dyld_cache_image_text_info
	uint64_t    imagesTextCount;    // number of dyld_cache_image_text_info entries
	uint64_t    dylibsImageGroupAddr;// (unslid) address of ImageGroup for dylibs in this cache
	uint64_t    dylibsImageGroupSize;// size of ImageGroup for dylibs in this cache
	uint64_t    otherImageGroupAddr;// (unslid) address of ImageGroup for other OS dylibs
	uint64_t    otherImageGroupSize;// size of oImageGroup for other OS dylibs
	uint64_t    progClosuresAddr;   // (unslid) address of list of program launch closures
	uint64_t    progClosuresSize;   // size of list of program launch closures
	uint64_t    progClosuresTrieAddr;// (unslid) address of trie of indexes into program launch closures
	uint64_t    progClosuresTrieSize;// size of trie of indexes into program launch closures
	uint32_t    platform;           // platform number (macOS=1, etc)
	uint32_t    formatVersion        : 8,// dyld3::closure::kFormatVersion
	    dylibsExpectedOnDisk : 1,      // dyld should expect the dylib exists on disk and to compare inode/mtime to see if cache is valid
	    simulator            : 1,      // for simulator of specified platform
	    locallyBuiltCache    : 1,      // 0 for B&I built cache, 1 for locally built cache
	    padding              : 21;     // TBD
};

/*
 * mirrors the dyld_cache_image_text_info struct defined in dyld_cache_format.h from dyld source code
 */
struct _dyld_cache_image_text_info {
	uuid_t      uuid;
	uint64_t    loadAddress;        // unslid address of start of __TEXT
	uint32_t    textSegmentSize;
	uint32_t    pathOffset;         // offset from start of cache file
};


enum micro_snapshot_flags {
	kInterruptRecord        = 0x1,
	kTimerArmingRecord      = 0x2,
	kUserMode               = 0x4, /* interrupted usermode, or armed by usermode */
	kIORecord               = 0x8,
	kPMIRecord              = 0x10,
	kMACFRecord             = 0x20, /* armed by MACF policy */
};

/*
 * Flags used in the following assortment of snapshots.
 */
enum generic_snapshot_flags {
	kUser64_p               = 0x1, /* Userspace uses 64 bit pointers */
	kKernel64_p             = 0x2  /* The kernel uses 64 bit pointers */
};

#define VM_PRESSURE_TIME_WINDOW 5 /* seconds */

__options_decl(stackshot_flags_t, uint64_t, {
	STACKSHOT_GET_DQ                           = 0x01,
	STACKSHOT_SAVE_LOADINFO                    = 0x02,
	STACKSHOT_GET_GLOBAL_MEM_STATS             = 0x04,
	STACKSHOT_SAVE_KEXT_LOADINFO               = 0x08,
	/*
	 * 0x10, 0x20, 0x40 and 0x80 are reserved.
	 *
	 * See microstackshot_flags_t whose members used to be part of this
	 * declaration.
	 */
	STACKSHOT_ACTIVE_KERNEL_THREADS_ONLY       = 0x100,
	STACKSHOT_GET_BOOT_PROFILE                 = 0x200,
	STACKSHOT_DO_COMPRESS                      = 0x400,
	STACKSHOT_SAVE_IMP_DONATION_PIDS           = 0x2000,
	STACKSHOT_SAVE_IN_KERNEL_BUFFER            = 0x4000,
	STACKSHOT_RETRIEVE_EXISTING_BUFFER         = 0x8000,
	STACKSHOT_KCDATA_FORMAT                    = 0x10000,
	STACKSHOT_ENABLE_BT_FAULTING               = 0x20000,
	STACKSHOT_COLLECT_DELTA_SNAPSHOT           = 0x40000,
	/* Include the layout of the system shared cache */
	STACKSHOT_COLLECT_SHAREDCACHE_LAYOUT       = 0x80000,
	/*
	 * Kernel consumers of stackshot (via stack_snapshot_from_kernel) can ask
	 * that we try to take the stackshot lock, and fail if we don't get it.
	 */
	STACKSHOT_TRYLOCK                          = 0x100000,
	STACKSHOT_ENABLE_UUID_FAULTING             = 0x200000,
	STACKSHOT_FROM_PANIC                       = 0x400000,
	STACKSHOT_NO_IO_STATS                      = 0x800000,
	/* Report owners of and pointers to kernel objects that threads are blocked on */
	STACKSHOT_THREAD_WAITINFO                  = 0x1000000,
	STACKSHOT_THREAD_GROUP                     = 0x2000000,
	STACKSHOT_SAVE_JETSAM_COALITIONS           = 0x4000000,
	STACKSHOT_INSTRS_CYCLES                    = 0x8000000,
	STACKSHOT_ASID                             = 0x10000000,
	STACKSHOT_PAGE_TABLES                      = 0x20000000,
	STACKSHOT_DISABLE_LATENCY_INFO             = 0x40000000,
	STACKSHOT_SAVE_DYLD_COMPACTINFO            = 0x80000000,
	STACKSHOT_INCLUDE_DRIVER_THREADS_IN_KERNEL = 0x100000000,
	/* Include Exclave stacks in Stackshot - currently unused/on-by-default */
	STACKSHOT_EXCLAVES                         = 0x200000000,
	/* Skip Exclaves stack collection */
	STACKSHOT_SKIP_EXCLAVES                    = 0x400000000,
}); // Note: Add any new flags to kcdata.py (stackshot_in_flags)

__options_decl(microstackshot_flags_t, uint32_t, {
	STACKSHOT_GET_MICROSTACKSHOT               = 0x10,
	STACKSHOT_GLOBAL_MICROSTACKSHOT_ENABLE     = 0x20,
	STACKSHOT_GLOBAL_MICROSTACKSHOT_DISABLE    = 0x40,
	STACKSHOT_SET_MICROSTACKSHOT_MARK          = 0x80,
});

#define STACKSHOT_THREAD_SNAPSHOT_MAGIC     0xfeedface
#define STACKSHOT_TASK_SNAPSHOT_MAGIC       0xdecafbad
#define STACKSHOT_MEM_AND_IO_SNAPSHOT_MAGIC 0xbfcabcde
#define STACKSHOT_MICRO_SNAPSHOT_MAGIC      0x31c54011

#define STACKSHOT_PAGETABLES_MASK_ALL           ~0

__options_closed_decl(kf_override_flag_t, uint32_t, {
	KF_SERIAL_OVRD                            = 0x2,
	KF_PMAPV_OVRD                             = 0x4,
	KF_MATV_OVRD                              = 0x8,
	KF_STACKSHOT_OVRD                         = 0x10,
	KF_COMPRSV_OVRD                           = 0x20,
	KF_INTERRUPT_MASKED_DEBUG_OVRD            = 0x40,
	KF_TRAPTRACE_OVRD                         = 0x80,
	KF_IOTRACE_OVRD                           = 0x100,
	KF_INTERRUPT_MASKED_DEBUG_STACKSHOT_OVRD  = 0x200,
	KF_SCHED_HYGIENE_DEBUG_PMC_OVRD           = 0x400,
	KF_RW_LOCK_DEBUG_OVRD                     = 0x800,
	KF_MADVISE_FREE_DEBUG_OVRD                = 0x1000,
	KF_DISABLE_FP_POPC_ON_PGFLT               = 0x2000,
	KF_DISABLE_PROD_TRC_VALIDATION            = 0x4000,
	KF_IO_TIMEOUT_OVRD                        = 0x8000,
	KF_PREEMPTION_DISABLED_DEBUG_OVRD         = 0x10000,
});

boolean_t kern_feature_override(kf_override_flag_t fmask);

__options_decl(eph_panic_flags_t, uint64_t, {
	EMBEDDED_PANIC_HEADER_FLAG_COREDUMP_COMPLETE              = 0x01,                               /* INFO: coredump completed */
	EMBEDDED_PANIC_HEADER_FLAG_STACKSHOT_SUCCEEDED            = 0x02,                               /* INFO: stackshot completed */
	EMBEDDED_PANIC_HEADER_FLAG_STACKSHOT_FAILED_DEBUGGERSYNC  = 0x04,                               /* ERROR: stackshot failed to sync with external debugger */
	EMBEDDED_PANIC_HEADER_FLAG_STACKSHOT_FAILED_ERROR         = 0x08,                               /* ERROR: stackshot failed */
	EMBEDDED_PANIC_HEADER_FLAG_STACKSHOT_FAILED_INCOMPLETE    = 0x10,                               /* ERROR: stackshot is partially complete */
	EMBEDDED_PANIC_HEADER_FLAG_STACKSHOT_FAILED_NESTED        = 0x20,                               /* ERROR: stackshot caused a nested panic */
	EMBEDDED_PANIC_HEADER_FLAG_NESTED_PANIC                   = 0x40,                               /* ERROR: panic handler encountered a panic */
	EMBEDDED_PANIC_HEADER_FLAG_BUTTON_RESET_PANIC             = 0x80,                               /* INFO: force-reset panic: user held power button to force shutdown */
	EMBEDDED_PANIC_HEADER_FLAG_COPROC_INITIATED_PANIC         = 0x100,                              /* INFO: panic was triggered by a companion processor (not Xnu) */
	EMBEDDED_PANIC_HEADER_FLAG_COREDUMP_FAILED                = 0x200,                              /* ERROR: coredump failed to complete */
	EMBEDDED_PANIC_HEADER_FLAG_COMPRESS_FAILED                = 0x400,                              /* ERROR: stackshot failed to compress */
	EMBEDDED_PANIC_HEADER_FLAG_STACKSHOT_DATA_COMPRESSED      = 0x800,                              /* INFO: stackshot data is compressed */
	EMBEDDED_PANIC_HEADER_FLAG_ENCRYPTED_COREDUMP_SKIPPED     = 0x1000,                             /* ERROR: coredump policy requires encryption, but encryptions is not initialized or available */
	EMBEDDED_PANIC_HEADER_FLAG_KERNEL_COREDUMP_SKIPPED_EXCLUDE_REGIONS_UNAVAILABLE   = 0x2000,      /* ERROR: coredump region exclusion list is not available */
	EMBEDDED_PANIC_HEADER_FLAG_COREFILE_UNLINKED              = 0x4000,                             /* ERROR: coredump output file is not linked */
	EMBEDDED_PANIC_HEADER_FLAG_INCOHERENT_PANICLOG            = 0x8000,                             /* ERROR: paniclog integrity check failed (a warning to consumer code i.e. DumpPanic) */
	EMBEDDED_PANIC_HEADER_FLAG_EXCLAVE_PANIC                  = 0x10000,                            /* INFO: panic originated from exclaves */
});

#define EMBEDDED_PANIC_HEADER_CURRENT_VERSION 5
#define EMBEDDED_PANIC_MAGIC 0x46554E4B /* FUNK */
#define EMBEDDED_PANIC_HEADER_OSVERSION_LEN 32

/*
 * Any updates to this header should be also updated in astris as it can not
 * grab this header from the SDK.
 *
 * NOTE: DO NOT REMOVE OR CHANGE THE MEANING OF ANY FIELDS FROM THIS STRUCTURE.
 *       Any modifications should add new fields at the end, bump the version number
 *       and be done alongside astris and DumpPanic changes.
 */
struct embedded_panic_header {
	uint32_t eph_magic;                /* EMBEDDED_PANIC_MAGIC if valid */
	uint32_t eph_crc;                  /* CRC of everything following the ph_crc in the header and the contents */
	uint32_t eph_version;              /* embedded_panic_header version */
	eph_panic_flags_t eph_panic_flags; /* Flags indicating any state or relevant details */
	uint32_t eph_panic_log_offset;     /* Offset of the beginning of the panic log from the beginning of the header */
	uint32_t eph_panic_log_len;        /* length of the panic log */
	uint32_t eph_stackshot_offset;     /* Offset of the beginning of the panic stackshot from the beginning of the header */
	uint32_t eph_stackshot_len;        /* length of the panic stackshot (0 if not valid ) */
	uint32_t eph_other_log_offset;     /* Offset of the other log (any logging subsequent to the stackshot) from the beginning of the header */
	uint32_t eph_other_log_len;        /* length of the other log */
	union {
		struct {
			uint64_t eph_x86_power_state:8,
			    eph_x86_efi_boot_state:8,
			    eph_x86_system_state:8,
			    eph_x86_unused_bits:40;
		}; // anonymous struct to group the bitfields together.
		uint64_t eph_x86_do_not_use; /* Used for offsetof/sizeof when parsing header */
	} __attribute__((packed));
	char eph_os_version[EMBEDDED_PANIC_HEADER_OSVERSION_LEN];
	char eph_macos_version[EMBEDDED_PANIC_HEADER_OSVERSION_LEN];
	uuid_string_t eph_bootsessionuuid_string;                      /* boot session UUID */
	uint64_t eph_roots_installed;                                  /* bitmap indicating which roots are installed on this system */
	uint32_t eph_ext_paniclog_offset;
	uint32_t eph_ext_paniclog_len;
} __attribute__((packed));


#define MACOS_PANIC_HEADER_CURRENT_VERSION 3
#define MACOS_PANIC_MAGIC 0x44454544 /* DEED */

__options_decl(mph_panic_flags_t, uint64_t, {
	MACOS_PANIC_HEADER_FLAG_NESTED_PANIC                   = 0x01,                                /* ERROR: panic handler encountered a panic */
	MACOS_PANIC_HEADER_FLAG_COPROC_INITIATED_PANIC         = 0x02,                                /* INFO: panic was triggered by a companion processor (not Xnu) */
	MACOS_PANIC_HEADER_FLAG_STACKSHOT_SUCCEEDED            = 0x04,                                /* INFO: stackshot completed */
	MACOS_PANIC_HEADER_FLAG_STACKSHOT_DATA_COMPRESSED      = 0x08,                                /* INFO: stackshot data is compressed */
	MACOS_PANIC_HEADER_FLAG_STACKSHOT_FAILED_DEBUGGERSYNC  = 0x10,                                /* ERROR: stackshot failed to sync with external debugger */
	MACOS_PANIC_HEADER_FLAG_STACKSHOT_FAILED_ERROR         = 0x20,                                /* ERROR: stackshot failed */
	MACOS_PANIC_HEADER_FLAG_STACKSHOT_FAILED_INCOMPLETE    = 0x40,                                /* ERROR: stackshot is partially complete */
	MACOS_PANIC_HEADER_FLAG_STACKSHOT_FAILED_NESTED        = 0x80,                                /* ERROR: stackshot caused a nested panic */
	MACOS_PANIC_HEADER_FLAG_COREDUMP_COMPLETE              = 0x100,                               /* INFO: coredump completed */
	MACOS_PANIC_HEADER_FLAG_COREDUMP_FAILED                = 0x200,                               /* ERROR: coredump failed to complete */
	MACOS_PANIC_HEADER_FLAG_STACKSHOT_KERNEL_ONLY          = 0x400,                               /* ERROR: stackshot contains only kernel data (e.g. due to space limitations) */
	MACOS_PANIC_HEADER_FLAG_STACKSHOT_FAILED_COMPRESS      = 0x800,                               /* ERROR: stackshot failed to compress */
	MACOS_PANIC_HEADER_FLAG_ENCRYPTED_COREDUMP_SKIPPED     = 0x1000,                              /* ERROR: coredump policy requires encryption, but encryptions is not initialized or available */
	MACOS_PANIC_HEADER_FLAG_KERNEL_COREDUMP_SKIPPED_EXCLUDE_REGIONS_UNAVAILABLE     = 0x2000,     /* ERROR: coredump region exclusion list is not available */
	MACOS_PANIC_HEADER_FLAG_COREFILE_UNLINKED              = 0x4000,                              /* ERROR: coredump output file is not linked */
	MACOS_PANIC_HEADER_FLAG_INCOHERENT_PANICLOG            = 0x8000                               /* ERROR: paniclog integrity check failed (a warning to consumer code i.e. DumpPanic) */
});

struct macos_panic_header {
	uint32_t mph_magic;                   /* MACOS_PANIC_MAGIC if valid */
	uint32_t mph_crc;                     /* CRC of everything following mph_crc in the header and the contents */
	uint32_t mph_version;                 /* macos_panic_header version */
	uint32_t mph_padding;                 /* unused */
	mph_panic_flags_t mph_panic_flags;    /* Flags indicating any state or relevant details */
	uint32_t mph_panic_log_offset;        /* Offset of the panic log from the beginning of the header */
	uint32_t mph_panic_log_len;           /* length of the panic log */
	uint32_t mph_stackshot_offset;        /* Offset of the panic stackshot from the beginning of the header */
	uint32_t mph_stackshot_len;           /* length of the panic stackshot */
	uint32_t mph_other_log_offset;        /* Offset of the other log (any logging subsequent to the stackshot) from the beginning of the header */
	uint32_t mph_other_log_len;           /* length of the other log */
	uint64_t mph_roots_installed;         /* bitmap indicating which roots are installed on this system */
	char     mph_data[];                  /* panic data -- DO NOT ACCESS THIS FIELD DIRECTLY. Use the offsets above relative to the beginning of the header */
} __attribute__((packed));

/*
 * Any change to the below structure should mirror the structure defined in MacEFIFirmware
 * (and vice versa)
 */

struct efi_aurr_panic_header {
	uint32_t efi_aurr_magic;
	uint32_t efi_aurr_crc;
	uint32_t efi_aurr_version;
	uint32_t efi_aurr_reset_cause;
	uint32_t efi_aurr_reset_log_offset;
	uint32_t efi_aurr_reset_log_len;
	char efi_aurr_panic_data[];
} __attribute__((packed));

/*
 * EXTENDED_/DEBUG_BUF_SIZE can't grow without updates to SMC and iBoot to store larger panic logs on co-processor systems
 */
#define EXTENDED_DEBUG_BUF_SIZE 0x0013ff80

#define EFI_AURR_PANIC_STRING_MAX_LEN 112
#define EFI_AURR_EXTENDED_LOG_SIZE (EXTENDED_DEBUG_BUF_SIZE - sizeof(struct efi_aurr_panic_header) - EFI_AURR_PANIC_STRING_MAX_LEN)

struct efi_aurr_extended_panic_log {
	char efi_aurr_extended_log_buf[EFI_AURR_EXTENDED_LOG_SIZE];
	uint32_t efi_aurr_log_tail; /* Circular buffer indices */
	uint32_t efi_aurr_log_head; /* ditto.. */
} __attribute__((packed));

#endif /* __APPLE_API_UNSTABLE */
#endif /* __APPLE_API_PRIVATE */

/*
 * If non-zero, this physical address had an ECC error that led to a panic.
 */
extern uint64_t ecc_panic_physical_address;

#ifdef KERNEL

__abortlike __printflike(1, 2)
extern void panic(const char *string, ...);

#endif /* KERNEL */

#ifdef KERNEL_PRIVATE
#if DEBUG
#ifndef DKPR
#define DKPR 1
#endif
#endif

#if DKPR
/*
 * For the DEBUG kernel, support the following:
 *	sysctl -w debug.kprint_syscall=<syscall_mask>
 *	sysctl -w debug.kprint_syscall_process=<p_comm>
 * <syscall_mask> should be an OR of the masks below
 * for UNIX, MACH, MDEP, or IPC. This debugging aid
 * assumes the task/process is locked/wired and will
 * not go away during evaluation. If no process is
 * specified, all processes will be traced
 */
extern int debug_kprint_syscall;
extern int debug_kprint_current_process(const char **namep);
#define DEBUG_KPRINT_SYSCALL_PREDICATE_INTERNAL(mask, namep)                    \
	( (debug_kprint_syscall & (mask)) && debug_kprint_current_process(namep) )
#define DEBUG_KPRINT_SYSCALL_MASK(mask, fmt, args...)   do {                    \
	        const char *dks_name = NULL;                                                                    \
	        if (DEBUG_KPRINT_SYSCALL_PREDICATE_INTERNAL(mask, &dks_name)) { \
	                kprintf("[%s%s%p]" fmt, dks_name ? dks_name : "",                       \
	                                dks_name ? "@" : "", current_thread(), args);                   \
	        }                                                                                                                               \
	} while (0)
#else /* !DEBUG */
#define DEBUG_KPRINT_SYSCALL_PREDICATE_INTERNAL(mask, namep) (0)
#define DEBUG_KPRINT_SYSCALL_MASK(mask, fmt, args...) do { } while (0) /* kprintf(fmt, args) */
#endif /* !DEBUG */

enum {
	DEBUG_KPRINT_SYSCALL_UNIX_MASK = 1 << 0,
	DEBUG_KPRINT_SYSCALL_MACH_MASK = 1 << 1,
	DEBUG_KPRINT_SYSCALL_MDEP_MASK = 1 << 2,
	DEBUG_KPRINT_SYSCALL_IPC_MASK  = 1 << 3
};

#define DEBUG_KPRINT_SYSCALL_PREDICATE(mask)                            \
	DEBUG_KPRINT_SYSCALL_PREDICATE_INTERNAL(mask, NULL)
#define DEBUG_KPRINT_SYSCALL_UNIX(fmt, args...)                         \
	DEBUG_KPRINT_SYSCALL_MASK(DEBUG_KPRINT_SYSCALL_UNIX_MASK,fmt,args)
#define DEBUG_KPRINT_SYSCALL_MACH(fmt, args...)                         \
	DEBUG_KPRINT_SYSCALL_MASK(DEBUG_KPRINT_SYSCALL_MACH_MASK,fmt,args)
#define DEBUG_KPRINT_SYSCALL_MDEP(fmt, args...)                         \
	DEBUG_KPRINT_SYSCALL_MASK(DEBUG_KPRINT_SYSCALL_MDEP_MASK,fmt,args)
#define DEBUG_KPRINT_SYSCALL_IPC(fmt, args...)                          \
	DEBUG_KPRINT_SYSCALL_MASK(DEBUG_KPRINT_SYSCALL_IPC_MASK,fmt,args)

/* Debug boot-args */
#define DB_HALT         0x1
//#define DB_PRT          0x2 -- obsolete
#define DB_NMI          0x4
#define DB_KPRT         0x8
#define DB_KDB          0x10
#define DB_ARP          0x40
#define DB_KDP_BP_DIS   0x80
//#define DB_LOG_PI_SCRN  0x100 -- obsolete
#define DB_KDP_GETC_ENA 0x200

#define DB_KERN_DUMP_ON_PANIC           0x400 /* Trigger core dump on panic*/
#define DB_KERN_DUMP_ON_NMI             0x800 /* Trigger core dump on NMI */
#define DB_DBG_POST_CORE                0x1000 /*Wait in debugger after NMI core */
#define DB_PANICLOG_DUMP                0x2000 /* Send paniclog on panic,not core*/
#define DB_REBOOT_POST_CORE             0x4000 /* Attempt to reboot after
	                                        * post-panic crashdump/paniclog
	                                        * dump.
	                                        */
#define DB_NMI_BTN_ENA          0x8000  /* Enable button to directly trigger NMI */
/* 0x10000 was DB_PRT_KDEBUG (kprintf kdebug events), feature removed */
#define DB_DISABLE_LOCAL_CORE   0x20000 /* ignore local kernel core dump support */
#define DB_DISABLE_GZIP_CORE    0x40000 /* don't gzip kernel core dumps */
#define DB_DISABLE_CROSS_PANIC  0x80000 /* x86 only - don't trigger cross panics. Only
	                                 * necessary to enable x86 kernel debugging on
	                                 * configs with a dev-fused co-processor running
	                                 * release bridgeOS.
	                                 */
#define DB_REBOOT_ALWAYS        0x100000 /* Don't wait for debugger connection */
#define DB_DISABLE_STACKSHOT_TO_DISK 0x200000 /* Disable writing stackshot to local disk */
#define DB_DEBUG_IP_INIT        0x400000 /* iBoot specific: Allow globally enabling debug IPs during init */
#define DB_SOC_HALT_ENABLE      0x800000 /* iBoot specific: Enable SoC Halt during init */

/*
 * Values for a 64-bit mask that's passed to the debugger.
 */
#define DEBUGGER_OPTION_NONE                        0x0ULL
#define DEBUGGER_OPTION_PANICLOGANDREBOOT           0x1ULL /* capture a panic log and then reboot immediately */
#define DEBUGGER_OPTION_INITPROC_PANIC              0x20ULL
#define DEBUGGER_OPTION_COPROC_INITIATED_PANIC      0x40ULL /* panic initiated by a co-processor */
#define DEBUGGER_OPTION_SKIP_LOCAL_COREDUMP         0x80ULL /* don't try to save local coredumps for this panic */
#define DEBUGGER_OPTION_ATTEMPTCOREDUMPANDREBOOT    0x100ULL /* attempt to save coredump. always reboot */
#define DEBUGGER_INTERNAL_OPTION_THREAD_BACKTRACE   0x200ULL /* backtrace the specified thread in the paniclog (x86 only) */
#define DEBUGGER_OPTION_PRINT_CPU_USAGE_PANICLOG    0x400ULL /* print extra CPU usage data in the panic log */
#define DEBUGGER_OPTION_SKIP_PANICEND_CALLOUTS      0x800ULL /* (bridgeOS) skip the kPEPanicEnd callouts -- don't wait for x86 to finish sending panic data */
#define DEBUGGER_OPTION_SYNC_ON_PANIC_UNSAFE        0x1000ULL /* sync() early in Panic - Can add unbounded delay, may be unsafe for some panic scenarios. Intended for userspace, watchdogs and RTBuddy panics */

#define DEBUGGER_INTERNAL_OPTIONS_MASK              (DEBUGGER_INTERNAL_OPTION_THREAD_BACKTRACE)

#define __STRINGIFY(x) #x
#define LINE_NUMBER(x) __STRINGIFY(x)
#ifdef __FILE_NAME__
#define PANIC_LOCATION __FILE_NAME__ ":" LINE_NUMBER(__LINE__)
#else
#define PANIC_LOCATION __FILE__ ":" LINE_NUMBER(__LINE__)
#define __FILE_NAME__ __FILE__
#endif

/* Macros for XNU platform stalls
 *  The "location" macros specify points where we can stall or panic
 *  The "action" macros specify the action to take at these points.
 *  The default action is to stall. */
#if (DEVELOPMENT || DEBUG)
#define PLATFORM_STALL_XNU_DISABLE                              (0)
#define PLATFORM_STALL_XNU_LOCATION_ARM_INIT                    (0x1ULL << 0)
#define PLATFORM_STALL_XNU_LOCATION_KERNEL_BOOTSTRAP            (0x1ULL << 1)
#define PLATFORM_STALL_XNU_LOCATION_BSD_INIT                    (0x1ULL << 2)
#define PLATFORM_STALL_XNU_ACTION_PANIC                         (0x1ULL << 7)

extern uint64_t xnu_platform_stall_value;

void platform_stall_panic_or_spin(uint32_t req);

#endif

#if XNU_KERNEL_PRIVATE
#define panic(ex, ...)  ({ \
	__asm__("" ::: "memory"); \
	(panic)(ex " @%s:%d", ## __VA_ARGS__, __FILE_NAME__, __LINE__); \
})
#else
#define panic(ex, ...)  ({ \
	__asm__("" ::: "memory"); \
	(panic)(#ex " @%s:%d", ## __VA_ARGS__, __FILE_NAME__, __LINE__); \
})
#endif
#define panic_plain(ex, ...)  (panic)(ex, ## __VA_ARGS__)

struct task;
struct thread;
struct proc;

__abortlike __printflike(4, 5)
void panic_with_options(unsigned int reason, void *ctx,
    uint64_t debugger_options_mask, const char *str, ...);
void Debugger(const char * message);
void populate_model_name(char *);

boolean_t panic_validate_ptr(void *ptr, vm_size_t size, const char *what);

boolean_t panic_get_thread_proc_task(struct thread *thread, struct task **task, struct proc **proc);

#define PANIC_VALIDATE_PTR(expr) \
	panic_validate_ptr(expr, sizeof(*(expr)), #expr)


#if defined(__arm__) || defined(__arm64__)
/* Note that producer_name and buf should never be de-allocated as we reference these during panic */
void register_additional_panic_data_buffer(const char *producer_name, void *buf, int len);
#endif

unsigned panic_active(void);

#endif  /* KERNEL_PRIVATE */

#if XNU_KERNEL_PRIVATE

#if defined (__x86_64__)
struct thread;

__abortlike __printflike(5, 6)
void panic_with_thread_context(unsigned int reason, void *ctx,
    uint64_t debugger_options_mask, struct thread* th, const char *str, ...);
#endif

/* limit the max size to a reasonable length */
#define ADDITIONAL_PANIC_DATA_BUFFER_MAX_LEN 64

struct additional_panic_data_buffer {
	const char *producer_name;
	void *buf;
	int len;
};

extern struct additional_panic_data_buffer *panic_data_buffers;

boolean_t oslog_is_safe(void);
boolean_t debug_mode_active(void);
boolean_t stackshot_active(void);
void panic_stackshot_reset_state(void);

/*
 * @function stack_snapshot_from_kernel
 *
 * @abstract Stackshot function for kernel consumers who have their own buffer.
 *
 * @param pid     the PID to be traced or -1 for the whole system
 * @param buf     a pointer to the buffer where the stackshot should be written
 * @param size    the size of the buffer
 * @param flags   flags to be passed to the stackshot
 * @param delta_since_timestamp start time for delta period
 * @param pagetable_mask if pagetable dumping is set in flags, the mask of page table levels to dump
 * @bytes_traced  a pointer to be filled with the length of the stackshot
 *
 */
kern_return_t
stack_snapshot_from_kernel(int pid, void *buf, uint32_t size, uint64_t flags,
    uint64_t delta_since_timestamp, uint32_t pagetable_mask, unsigned *bytes_traced);

/*
 * Returns whether on device corefiles are enabled based on the build
 * and boot configuration.
 */
boolean_t on_device_corefile_enabled(void);

/*
 * Returns whether panic stackshot to disk is enabled based on the build
 * and boot configuration.
 */
boolean_t panic_stackshot_to_disk_enabled(void);

#if defined(__x86_64__)
extern char debug_buf[];
extern boolean_t coprocessor_paniclog_flush;
extern boolean_t extended_debug_log_enabled;
#endif /* defined(__x86_64__) */

extern char     *debug_buf_base;

#if defined(XNU_TARGET_OS_BRIDGE)
extern uint64_t macos_panic_base;
extern unsigned int macos_panic_size;
#endif /* defined(XNU_TARGET_OS_BRIDGE) */

extern char     kernel_uuid_string[];
extern char     panic_disk_error_description[];
extern size_t   panic_disk_error_description_size;

extern unsigned char    *kernel_uuid;
extern unsigned int     debug_boot_arg;
extern int     verbose_panic_flow_logging;

extern boolean_t kernelcache_uuid_valid;
extern uuid_t kernelcache_uuid;
extern uuid_string_t kernelcache_uuid_string;

extern boolean_t pageablekc_uuid_valid;
extern uuid_t pageablekc_uuid;
extern uuid_string_t pageablekc_uuid_string;

extern boolean_t auxkc_uuid_valid;
extern uuid_t auxkc_uuid;
extern uuid_string_t auxkc_uuid_string;

extern boolean_t        doprnt_hide_pointers;

extern unsigned int     halt_in_debugger; /* pending halt in debugger after boot */
extern unsigned int     current_debugger;
#define NO_CUR_DB       0x0
#define KDP_CUR_DB      0x1

extern unsigned int     active_debugger;
extern unsigned int     kernel_debugger_entry_count;

extern unsigned int     panicDebugging;

extern const char       *debugger_panic_str;

extern char *debug_buf_ptr;
extern unsigned int debug_buf_size;

extern void debug_log_init(void);
extern void debug_putc(char);
extern boolean_t debug_is_current_cpu_in_panic_state(void);

/*
 * Initialize the physical carveout requested with the `phys_carveout_mb`
 * boot-arg.  This should only be called at kernel startup, when physically
 * contiguous pages are plentiful.
 */
extern void phys_carveout_init(void);

/*
 * Check whether a kernel virtual address points within the physical carveout.
 */
extern boolean_t debug_is_in_phys_carveout(vm_map_offset_t va);

/*
 * Check whether the physical carveout should be included in a coredump.
 */
extern boolean_t debug_can_coredump_phys_carveout(void);

extern vm_offset_t phys_carveout;
extern uintptr_t phys_carveout_pa;
extern size_t phys_carveout_size;

extern boolean_t kernel_debugging_restricted(void);

#if defined (__x86_64__)
extern void extended_debug_log_init(void);

int     packA(char *inbuf, uint32_t length, uint32_t buflen);
void    unpackA(char *inbuf, uint32_t length);

#define PANIC_STACKSHOT_BUFSIZE (1024 * 1024)

extern uintptr_t panic_stackshot_buf;
extern size_t panic_stackshot_buf_len;

extern size_t panic_stackshot_len;
#endif /* defined (__x86_64__) */

void    SavePanicInfo(const char *message, void *panic_data, uint64_t panic_options);
void    paniclog_flush(void);
void    panic_display_zalloc(void); /* in zalloc.c */
void    panic_display_kernel_aslr(void);
void    panic_display_hibb(void);
void    panic_display_model_name(void);
void    panic_display_kernel_uuid(void);
void    panic_display_process_name(void);
void    panic_print_symbol_name(vm_address_t search);
#if CONFIG_ECC_LOGGING
void    panic_display_ecc_errors(void);
#endif /* CONFIG_ECC_LOGGING */
void panic_display_compressor_stats(void);

/*
 * @var not_in_kdp
 *
 * @abstract True if we're in normal kernel operation, False if we're in a
 * single-core debugger context.
 */
extern unsigned int not_in_kdp;

#define DEBUGGER_NO_CPU -1

typedef enum {
	DBOP_NONE,
	DBOP_STACKSHOT,
	DBOP_RESET_PGO_COUNTERS,
	DBOP_PANIC,
	DBOP_DEBUGGER,
	DBOP_BREAKPOINT,
} debugger_op;

__printflike(3, 0)
kern_return_t DebuggerTrapWithState(debugger_op db_op, const char *db_message, const char *db_panic_str, va_list *db_panic_args,
    uint64_t db_panic_options, void *db_panic_data_ptr, boolean_t db_proceed_on_sync_failure, unsigned long db_panic_caller);
void handle_debugger_trap(unsigned int exception, unsigned int code, unsigned int subcode, void *state);

void DebuggerWithContext(unsigned int reason, void *ctx, const char *message, uint64_t debugger_options_mask, unsigned long debugger_caller);

const char *sysctl_debug_get_preoslog(size_t *size);
void sysctl_debug_free_preoslog(void);

#if DEBUG || DEVELOPMENT
/* leak pointer scan definitions */

enum{
	kInstanceFlagAddress    = 0x01UL,
	kInstanceFlagReferenced = 0x02UL,
	kInstanceFlags          = 0x03UL
};

#define INSTANCE_GET(x) ((x) & ~kInstanceFlags)
#define INSTANCE_PUT(x) ((x) ^ ~kInstanceFlags)

typedef void (^leak_site_proc)(uint32_t siteCount, uint32_t elem_size, uint32_t btref);

extern kern_return_t
zone_leaks(const char * zoneName, uint32_t nameLen, leak_site_proc proc);

extern void
zone_leaks_scan(uintptr_t * instances, uint32_t count, uint32_t zoneSize, uint32_t * found);

/* panic testing hooks */

#define PANIC_TEST_CASE_DISABLED                    0
#define PANIC_TEST_CASE_RECURPANIC_ENTRY            0x2    // recursive panic at panic entrypoint, before panic data structures are initialized
#define PANIC_TEST_CASE_RECURPANIC_PRELOG           0x4    // recursive panic prior to paniclog being written
#define PANIC_TEST_CASE_RECURPANIC_POSTLOG          0x8    // recursive panic after paniclog has been written
#define PANIC_TEST_CASE_RECURPANIC_POSTCORE         0x10   // recursive panic after corefile has been written
#define PANIC_TEST_CASE_COREFILE_IO_ERR             0x20   // single IO error in the corefile write path
extern unsigned int    panic_test_case;

#define PANIC_TEST_FAILURE_MODE_BADPTR 0x1                 // dereference a bad pointer
#define PANIC_TEST_FAILURE_MODE_SPIN   0x2                 // spin until watchdog kicks in
#define PANIC_TEST_FAILURE_MODE_PANIC  0x4                 // explicit panic
extern unsigned int    panic_test_failure_mode;    // panic failure mode

extern unsigned int    panic_test_action_count;    // test parameter, depends on test case

#endif  /* DEBUG || DEVELOPMENT */

/*
 * A callback that reads or writes data from a given offset into the corefile. It is understood that this
 * callback should only be used from within the context where it is given. It should never be stored and
 * reused later on.
 */
typedef kern_return_t (*IOCoreFileAccessCallback)(void *context, boolean_t write, uint64_t offset, int length, void *buffer);

/*
 * A callback that receives temporary file-system access to the kernel corefile
 *
 * Parameters:
 *  - access:            A function to call for reading/writing the kernel corefile.
 *  - access_context:    The context that should be passed to the 'access' function.
 *  - recipient_context: The recipient-specific context. Can be anything.
 */
typedef kern_return_t (*IOCoreFileAccessRecipient)(IOCoreFileAccessCallback access, void *access_context, void *recipient_context);

/*
 * Provides safe and temporary file-system access to the kernel corefile to the given recipient callback.
 * It does so by opening the kernel corefile, then calling the 'recipient' callback, passing it an IOCoreFileAccessCallback
 * function that it can use to read/write data, then closing the kernel corefile as soon as the recipient returns.
 *
 * Parameters:
 *  - recipient:         A function to call, providing it access to the kernel corefile.
 *  - recipient_context: Recipient-specific context. Can be anything.
 */
extern kern_return_t
IOProvideCoreFileAccess(IOCoreFileAccessRecipient recipient, void *recipient_context);

struct kdp_core_encryption_key_descriptor {
	uint64_t kcekd_format;
	uint16_t kcekd_size;
	void *   kcekd_key;
};

/*
 * Registers a new kernel (and co-processor) coredump encryption key. The key format should be one of the
 * supported "next" key formats in mach_debug_types.h. The recipient context pointer should point to a kdp_core_encryption_key_descriptor
 * structure.
 *
 * Note that the given key pointer should be allocated using `kmem_alloc(kernel_map, <pointer>, <size>, VM_KERN_MEMORY_DIAG)`
 *
 * Note that upon successful completion, this function will adopt the given public key pointer
 * and the caller should NOT release it.
 */
kern_return_t kdp_core_handle_new_encryption_key(IOCoreFileAccessCallback access_data, void *access_context, void *recipient_context);

/*
 * Enum of allowed values for the 'lbr_support' boot-arg
 */
typedef enum {
	LBR_ENABLED_NONE,
	LBR_ENABLED_USERMODE,
	LBR_ENABLED_KERNELMODE,
	LBR_ENABLED_ALLMODES
} lbr_modes_t;

extern lbr_modes_t last_branch_enabled_modes;

/* Exclaves stackshot tests support */
#define STACKSHOT_EXCLAVES_TESTING ((DEVELOPMENT || DEBUG) && CONFIG_EXCLAVES)

#endif  /* XNU_KERNEL_PRIVATE */

__END_DECLS

#endif  /* _KERN_DEBUG_H_ */
