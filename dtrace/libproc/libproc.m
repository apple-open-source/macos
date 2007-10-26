/* vim: set noet ts=4 sw=4: */
//
//  libproc.m
//  dtrace
//
//  Created by James McIlree on 10/9/06.
//  Copyright 2006 Apple Computer, Inc. All rights reserved.
//

#import <Symbolication/Symbolication.h>
#import <Symbolication/SymbolicationPrivate.h>

#import <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#import <sys/sysctl.h>

// /System//Library/Frameworks/System.framework/Versions/B/PrivateHeaders/sys/proc_info.h
#include <System/sys/proc_info.h>

// This must be done *after* any references to Foundation.h!
#define uint_t  __Solaris_uint_t

#import "libproc.h"
#import "libproc_apple.h"

#import <spawn.h>

#include "dtrace_dyld_types.h"
#include "dtrace_dyldServer.h"
#include "notifyServer.h"

#include <crt_externs.h>

extern int _dtrace_mangled;

/*
 * This is a helper category, used by symbolOwnerForName. It needs to do
 * partial matches against symbol owner names, and does not want to fault
 * in every symbol owner to do so. We add a category to do things as lazily
 * as possible. NOTE!!! This contains deep inner secrets about the workings
 * of Symbolication.framework, and can easily break.
 */

@interface VMUSymbolicator (ViolationOfGoodSenseAndEncapsulation)

// Copied from VMUSymbolicator (Internal)
- (VMUSymbolOwner*)faultLazySymbolOwnerAtIndex:(NSInteger)index;

@end

@interface VMUSymbolicator (DTrace)

- (NSArray*)symbolOwnersForPrefix:(NSString*)prefix;

@end

@implementation VMUSymbolicator (DTrace)

- (NSArray*)symbolOwnersForPrefix:(NSString*)prefix
{    
	NSMutableArray* owners = [NSMutableArray array];
	
	@synchronized(self) {
		NSInteger i, count = [_symbolOwners count];
		for (i=0; i<count; i++) {
			VMUSymbolOwner* owner = [_symbolOwners objectAtIndex:i];
			
			// Only fault in owners that match. 
			if ([owner isLazy]) {
				VMULazySymbolOwner* lazyOwner = (VMULazySymbolOwner*)owner;
				if ([[lazyOwner name] hasPrefix:prefix]) {
					owner = [self faultLazySymbolOwnerAtIndex:i];
				} else
				continue; // Must not proceed if this is lazy and not faulted.
			}
			
			if ([[owner name] hasPrefix:prefix]) {
				[owners addObject:owner];
			}
		}
	}
	
	return owners;
}

@end

/*
 * This is a helper method, it does extended lookups following roughly these rules
 *
 * 1. An exact match (i.e. a full pathname): "/usr/lib/libc.so.1"
 * 2. An exact basename match: "libc.so.1"
 * 3. An initial basename match up to a '.' suffix: "libc.so" or "libc"
 * 4. The literal string "a.out" is an alias for the executable mapping
 *
 * You must have an autorelease pool when calling this method.
 */
VMUSymbolOwner* symbolOwnerForName(VMUSymbolicator* symbolicator, NSString* name) {        
        // Check for a.out specifically
        if ([name isEqualToString:@"a.out"]) {
                NSArray* owners = [symbolicator symbolOwnersWithFlags:VMUSymbolOwnerIsAOut];
                NSCAssert([owners count] == 1, @"Should only be one a.out per symbolicator");
                return [owners objectAtIndex:0];
        }
        
        // Skip the path based lookup for now.

        // Try exact name match
	NSArray* owners = [symbolicator symbolOwnersForName:name];
	if ([owners count] > 0) {
		// Underspecified names (more than one match) are legal, just take the first
		return [owners objectAtIndex:0];
        }
        
        // Strip off extensions. We know there are no direct matches now.
	for (VMUSymbolOwner* candidate in [symbolicator symbolOwnersForPrefix:name]) {
		NSString* candidate_name = [candidate name];
		NSRange range;
		do {
			range = [candidate_name rangeOfString:@"." options:NSBackwardsSearch];
			if (range.location != NSNotFound) {
				candidate_name = [candidate_name substringToIndex:range.location];
				if ([candidate_name isEqualToString:name]) {
					return candidate;
				}
			}
		} while (range.location != NSNotFound);		
	}
        
        return nil;
}

#define APPLE_PCREATE_BAD_SYMBOLICATOR 0x0F000001

struct ps_prochandle *
Pcreate(const char *file,	/* executable file name */
        char *const *argv,	/* argument vector */
        int *perr,		/* pointer to error return code */
        char *path,		/* if non-null, holds exec path name on return */
        size_t len)		/* size of the path buffer */
{		
	struct ps_prochandle* proc = NULL;
	
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	
	@try {
		int pid;
		posix_spawnattr_t attr;
		task_t task;

		*perr = posix_spawnattr_init(&attr);
		if (0 != *perr) goto destroy_attr;
		
		*perr = posix_spawnattr_setflags(&attr, POSIX_SPAWN_START_SUSPENDED);
		if (0 != *perr) goto destroy_attr;
		
		setenv("DYLD_INSERT_LIBRARIES", "/usr/lib/dtrace/libdtrace_dyld.dylib", 1);
		
		*perr = posix_spawnp(&pid, file, NULL, &attr, argv, *_NSGetEnviron());

		unsetenv("DYLD_INSERT_LIBRARIES"); /* children must not have this present in their env */

destroy_attr:
		posix_spawnattr_destroy(&attr);
		
		if (0 == *perr) {
			*perr = task_for_pid(mach_task_self(), pid, &task);
			if (*perr == KERN_SUCCESS) {
				VMUSymbolicator* symbolicator = [VMUSymbolicator symbolicatorForTask:task];
				
				if (symbolicator) {
					proc = calloc(sizeof(struct ps_prochandle), 1);
					
					proc->task = task;
					
					proc->symbolicator = [symbolicator retain];
					proc->prev_symbolicator = nil;
									
					proc->prmap_dictionary = [[NSMutableDictionary alloc] init];
					
					proc->status.pr_flags = 0;
					proc->status.pr_pid = pid;
					proc->status.pr_dmodel = [[symbolicator architecture] is64Bit] ? PR_MODEL_LP64 : PR_MODEL_ILP32;
				} else {
					*perr = APPLE_PCREATE_BAD_SYMBOLICATOR;
				}
			} else
				*perr = -(*perr); // Make room for mach errors
		}
	} @catch (NSException* e) {
		// Silently drop any exceptions generated. So far it seems that they are happening when
		// we attempt to symbolicate a process that is exiting.
		
		// Free any allocated resources
		if (proc) {
			Prelease(proc, PRELEASE_CLEAR);
			proc = NULL;
		}
	}
	
	[pool drain];
	
	return proc;
}

/*
 * Return a printable string corresponding to a Pcreate() error return.
 */
const char *
Pcreate_error(int error)
{
	const char *str;
	
	switch (error) {
		case C_FORK:
			str = "cannot fork";
			break;
		case C_PERM:
			str = "file is set-id or unreadable [Note: the '-c' option requires a full pathname to the file]\n";
			break;
		case C_NOEXEC:
			str = "cannot execute file";
			break;
		case C_INTR:
			str = "operation interrupted";
			break;
		case C_LP64:
			str = "program is _LP64, self is not";
			break;
		case C_STRANGE:
			str = "unanticipated system error";
			break;
		case C_NOENT:
			str = "cannot find executable file";
			break;
		case APPLE_PCREATE_BAD_SYMBOLICATOR:
			str = "Could not create symbolicator for task";
			break;
		default:
			if (error < 0)
				str = mach_error_string(-error);
			else
				str = "unknown error";
			break;
	}
    
    return (str);
 }


/*
 * Grab an existing process.
 * Return an opaque pointer to its process control structure.
 *
 * pid:		UNIX process ID.
 * flags:
 *	PGRAB_RETAIN	Retain tracing flags (default clears all tracing flags).
 *	PGRAB_FORCE	Grab regardless of whether process is already traced.
 *	PGRAB_RDONLY	Open the address space file O_RDONLY instead of O_RDWR,
 *                      and do not open the process control file.
 *	PGRAB_NOSTOP	Open the process but do not force it to stop.
 * perr:	pointer to error return code.
 */
/*
 * APPLE NOTE:
 *
 * We don't seem to have an equivalent to the tracing
 * flag(s), so we're also going to punt on them for now.
 */

#define APPLE_PGRAB_BAD_SYMBOLICATOR  0x0F0F0F0E
#define APPLE_PGRAB_UNSUPPORTED_FLAGS 0x0F0F0F0F

struct ps_prochandle *Pgrab(pid_t pid, int flags, int *perr) {
        struct ps_prochandle* proc = NULL;
                
        if (flags & PGRAB_RDONLY || (0 == flags)) {
                NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
                
                @try {
                        task_t task;
                        
                        *perr = task_for_pid(mach_task_self(), pid, &task);
                        if (*perr == KERN_SUCCESS) {
				if (0 == (flags & PGRAB_RDONLY))
					(void)task_suspend(task);
                                VMUSymbolicator* symbolicator = [VMUSymbolicator symbolicatorForTask:task];
                                
                                if (symbolicator) {
                                        proc = calloc(sizeof(struct ps_prochandle), 1);
                                        
                                        proc->task = task;
                                        
                                        proc->symbolicator = [symbolicator retain];
										proc->prev_symbolicator = nil;
                                       
                                        proc->prmap_dictionary = [[NSMutableDictionary alloc] init];
                                        
                                        proc->status.pr_flags = 0;
                                        proc->status.pr_pid = pid;
                                        proc->status.pr_dmodel = [[symbolicator architecture] is64Bit] ? PR_MODEL_LP64 : PR_MODEL_ILP32;
                                } else {
                                        *perr = APPLE_PGRAB_BAD_SYMBOLICATOR;
                                }
						}
                } @catch (NSException* e) {
                        // Silently drop any exceptions generated. So far it seems that they are happening when
                        // we attempt to symbolicate a process that is exiting.
                        
                        // Free any allocated resources
                        if (proc) {
                                Prelease(proc, PRELEASE_CLEAR);
                                proc = NULL;
                        }
                }
                
                [pool drain];
        } else {
                *perr = APPLE_PGRAB_UNSUPPORTED_FLAGS;
        }
        
        return proc;
}

const char *Pgrab_error(int err) {
    const char* str;
    
    switch (err) {
        case APPLE_PGRAB_BAD_SYMBOLICATOR:
            str = "Pgrab could not create symbolicator for pid";
        case APPLE_PGRAB_UNSUPPORTED_FLAGS:
            str = "Pgrab was called with unsupported flags";
        default:
				str = mach_error_string(err);
    }
    
    return str;
}

/*
 * Release the process.  Frees the process control structure.
 * flags:
 *	PRELEASE_CLEAR	Clear all tracing flags.
 *	PRELEASE_RETAIN	Retain current tracing flags.
 *	PRELEASE_HANG	Leave the process stopped and abandoned.
 *	PRELEASE_KILL	Terminate the process with SIGKILL.
 */
/*
 * APPLE NOTE:
 *
 * We're ignoring most flags for now. They will eventually need to be honored.
 */
void Prelease(struct ps_prochandle *P, int flags) {
        NSCAssert(P != NULL, @"Should be a valid ps_prochandle");
        
        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
		
		if (0 == flags) {
			if (P->status.pr_flags & PR_KLC)
				(void)kill(P->status.pr_pid, SIGKILL);
		} else if (flags & PRELEASE_KILL) {
			(void)kill(P->status.pr_pid, SIGKILL);
		} else if (flags & PRELEASE_HANG)
			(void)kill(P->status.pr_pid, SIGSTOP);
        
        // Time to free resources.
        
        mach_port_deallocate(mach_task_self(), P->task);
        P->task = MACH_PORT_NULL;
        
		if (P->symbolicator && (P->symbolicator != P->prev_symbolicator))
			[P->symbolicator release];
        P->symbolicator = nil;
			
		if (P->prev_symbolicator)
			[P->prev_symbolicator release];
		P->prev_symbolicator = nil;
			        
        // The dictionary contains NSData objects, which will free their backing store automagically
        [P->prmap_dictionary release];
        P->prmap_dictionary = nil;
        
        [pool drain];
        
        free(P);
}

/*
 * APPLE NOTE:
 *
 * These low-level breakpoint functions are no-ops. We expect dyld to make RPC
 * calls to give us roughly the same functionality.
 *
 */
int Psetbkpt(struct ps_prochandle *P, uintptr_t addr, ulong_t *instr) {
	return 0;
}

int Pdelbkpt(struct ps_prochandle *P, uintptr_t addr, ulong_t instr) {
	return 0;
}

int	Pxecbkpt(struct ps_prochandle *P, ulong_t instr) {
	return 0;
}

/*
 * APPLE NOTE:
 *
 * Psetflags/Punsetflags has three caller values at this time.
 * PR_KLC - proc kill on last close
 * PR_RLC - proc resume/run on last close
 * PR_BPTAD - x86 only, breakpoint adjust eip
 *
 * We are not supporting any of these at this time.
 */

int Psetflags(struct ps_prochandle *P, long flags) {
	P->status.pr_flags |= flags;
    return 0;
}

int Punsetflags(struct ps_prochandle *P, long flags) {
	P->status.pr_flags &= ~flags;
    return 0;
}

int pr_open(struct ps_prochandle *P, const char *foo, int bar, mode_t baz) {
    NSLog(@"libProc.a UNIMPLEMENTED: pr_open()");
    return 0;
}

int pr_close(struct ps_prochandle *P, int foo) {
    NSLog(@"libProc.a UNIMPLEMENTED: pr_close");
    return 0;
}

int pr_ioctl(struct ps_prochandle *P, int foo, int bar, void *baz, size_t blah) {
    NSLog(@"libProc.a UNIMPLEMENTED: pr_ioctl");
    return 0;
}

/*
 * Search the process symbol tables looking for a symbol whose name matches the
 * specified name and whose object and link map optionally match the specified
 * parameters.  On success, the function returns 0 and fills in the GElf_Sym
 * symbol table entry.  On failure, -1 is returned.
 */
/*
 * APPLE NOTE:
 *
 * We're completely blowing off the lmid.
 *
 * It looks like the only GElf_Sym entries used are value & size.
 * Most of the time, prsyminfo_t is null, and when it is used, only
 * prs_lmid is set.
 */
int Pxlookup_by_name(
 	struct ps_prochandle *P,
 	Lmid_t lmid,			/* link map to match, or -1 (PR_LMID_EVERY) for any */
 	const char *oname,		/* load object name */
 	const char *sname,		/* symbol name */
 	GElf_Sym *symp,			/* returned symbol table entry */
 	prsyminfo_t *sip)		/* returned symbol info */
{
        int err = -1;
    
        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
        
        VMUSymbol* symbol = nil;
        
        if (oname != NULL) {
                VMUSymbolOwner* owner = symbolOwnerForName(P->symbolicator, [NSString stringWithUTF8String:oname]);
                NSArray* symbols = NULL;
                if (_dtrace_mangled) {
                        symbols = [owner symbolsForMangledName:[NSString stringWithUTF8String:sname]];
                } else {
                        symbols = [owner symbolsForName:[NSString stringWithUTF8String:sname]];
                }
                if ([symbols count] > 0) 
                        symbol = [symbols objectAtIndex:0]; // Just take the first one...
        } else {
                NSArray* symbols = NULL;
                if (_dtrace_mangled) {
                        symbols = [P->symbolicator symbolsForMangledName:[NSString stringWithUTF8String:sname]];
                } else {
                        symbols = [P->symbolicator symbolsForName:[NSString stringWithUTF8String:sname]];
                }
                if ([symbols count] > 0) 
                        symbol = [symbols objectAtIndex:0]; // Just take the first one...
        }
        
        // Filter out symbols we do not want to instrument
        if ([symbol isDyldStub]) symbol = nil;
        if (![symbol isFunction]) symbol = nil;
        if ([[symbol owner] isCommpage]) symbol = nil; // <rdar://problem/4877551>
    
        if (symbol)
                err = 0;
        
        if (symbol && symp) {
                VMURange addressRange = [symbol addressRange];
                                
                symp->st_name = 0;
                symp->st_info = GELF_ST_INFO((STB_GLOBAL), (STT_FUNC));
                symp->st_other = 0;
                symp->st_shndx = SHN_MACHO;
                symp->st_value = addressRange.location;
                symp->st_size = addressRange.length;
        }

        if (symbol && sip) {
                sip->prs_lmid = LM_ID_BASE;
        }
        
        [pool drain];
        
        return err;
}


/*
 * Search the process symbol tables looking for a symbol whose
 * value to value+size contain the address specified by addr.
 * Return values are:
 *	sym_name_buffer containing the symbol name
 *	GElf_Sym symbol table entry
 *	prsyminfo_t ancillary symbol information
 * Returns 0 on success, -1 on failure.
 */
/*
 * APPLE NOTE:
 *
 * This function is called directly by the plockstat binary and it passes the
 * psyminfo_t argument We only set fields that plockstat actually uses.
 * sip->prs_table and sip->prs_id are not used by plockstat so we don't
 * attempt to set them.
 */
int
Pxlookup_by_addr(
                 struct ps_prochandle *P,
                 uint64_t addr,			/* process address being sought */
                 char *sym_name_buffer,		/* buffer for the symbol name */
                 size_t bufsize,		/* size of sym_name_buffer */
                 GElf_Sym *symbolp,		/* returned symbol table entry */
                 prsyminfo_t *sip)		/* returned symbol info (used only by plockstat) */
{
    int err = -1;

    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    VMUSymbol* symbol = [P->symbolicator symbolForAddress:addr];

    // See comments in Ppltdest()
    // Filter out symbols we do not want to instrument
    // if ([symbol isDyldStub]) symbol = nil;
    // if (![symbol isFunction]) symbol = nil;

    if (symbol) {
        if (_dtrace_mangled) {
            const char *mangledName = [[symbol mangledName] UTF8String];
            if (strlen(mangledName) >= 3 &&
                mangledName[0] == '_' &&
                mangledName[1] == '_' &&
                mangledName[2] == 'Z') {
                // mangled name - use it
                strncpy(sym_name_buffer, mangledName, bufsize);
            } else {
                strncpy(sym_name_buffer, [[symbol name] UTF8String], bufsize);
            }
        } else
            strncpy(sym_name_buffer, [[symbol name] UTF8String], bufsize);
        err = 0;

        if (symbolp) {
            VMURange addressRange = [symbol addressRange];

            symbolp->st_name = 0;
            symbolp->st_info = GELF_ST_INFO((STB_GLOBAL), (STT_FUNC));
            symbolp->st_other = 0;
            symbolp->st_shndx = SHN_MACHO;
            symbolp->st_value = addressRange.location;
            symbolp->st_size = addressRange.length;
        }

        if (sip) {
            VMUSymbolOwner* owner = [symbol owner];
            sip->prs_name = (bufsize == 0 ? NULL : sym_name_buffer);

            if (owner) {
                sip->prs_object = [[owner name] UTF8String];
            } else {
                sip->prs_object = NULL;
            }

            // APPLE: The following fields set by Solaris code are not used by
            // plockstat, hence we don't return them.
            //sip->prs_id = (symp == sym1p) ? i1 : i2;
            //sip->prs_table = (symp == sym1p) ? PR_SYMTAB : PR_DYNSYM;

            // FIXME:!!!
            //sip->prs_lmid = (fptr->file_lo == NULL) ? LM_ID_BASE : fptr->file_lo->rl_lmident;
            sip->prs_lmid = LM_ID_BASE;
        }
    }

    [pool drain];

    return err;
}


int Plookup_by_addr(struct ps_prochandle *P, uint64_t addr, char *buf, size_t size, GElf_Sym *symp) {
  return Pxlookup_by_addr(P, addr, buf, size, symp, NULL);
}

/*
 * APPLE NOTE:
 *
 * We're just calling task_resume(). Returns 0 on success, -1 on failure.
 */
int Psetrun(struct ps_prochandle *P,
	    int sig,	/* Ignored in OS X. Nominally supposed to be the signal passed to the target process */
	    int flags	/* Ignored in OS X. PRSTEP|PRSABORT|PRSTOP|PRCSIG|PRCFAULT */)
{
	return (int)task_resume(P->task);
}

ssize_t Pread(struct ps_prochandle *P, void *buf, size_t nbyte, uint64_t address) {
        vm_offset_t mapped_address;
        mach_msg_type_number_t mapped_size;
        ssize_t bytes_read = 0;
        
        kern_return_t err = mach_vm_read(P->task, (mach_vm_address_t)address, (mach_vm_size_t)nbyte, &mapped_address, &mapped_size);
        if (! err) {
                bytes_read = nbyte;
                memcpy(buf, (void*)mapped_address, nbyte);
                vm_deallocate(mach_task_self(), (vm_address_t)mapped_address, (vm_size_t)mapped_size);
        }  
        
        return bytes_read;
}

int Pobject_iter(struct ps_prochandle *P, proc_map_f *func, void *cd) {
        int err = 0;
        
        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
        
        for (VMUSymbolOwner* owner in [P->symbolicator symbolOwners]) {
                if ([owner isCommpage]) continue; // <rdar://problem/4877551>
	
                prmap_t map;
                const char* name = [[owner name] UTF8String];
                
				if (P->prev_symbolicator) {
					int found = 0;
					for (VMUSymbolOwner* prev_owner in [P->prev_symbolicator symbolOwners]) {
						const char* prev_name = [[prev_owner name] UTF8String];
						
						if (0 == strcmp(prev_name, name)) {
							found = 1;
							break;
						}
					}
					if (found)
						continue;
				}
				
                map.pr_vaddr = [[[owner regions] objectAtIndex:0] addressRange].location;
                map.pr_mflags = MA_READ;
                
                if ((err = func(cd, &map, name)) != 0)
                        break;
        }
        
        [pool drain];
        
        return err;
}

const prmap_t *Paddr_to_map(struct ps_prochandle *P, uint64_t addr) {
	const prmap_t* map = NULL;
                
        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
        
        VMUSymbolOwner* owner = [P->symbolicator symbolOwnerForAddress:addr];
	
        // <rdar://problem/4877551>
        if (owner && ![owner isCommpage]) {
                // Now we try a lookup
                NSData* data = [P->prmap_dictionary objectForKey:owner];
                
                if (!data) {
                        prmap_t temp;
                        
                        temp.pr_vaddr = [[[owner regions] objectAtIndex:0] addressRange].location;                        
                        temp.pr_mflags = MA_READ; // Anything we get from a symbolicator is readable
                        
                        data = [NSData dataWithBytes:&temp length:sizeof(temp)];
                        
                        [P->prmap_dictionary setObject:data forKey:owner];
                }
		
                map = [data bytes];
        }
	
	[pool drain];
        
        return map;	
}

const prmap_t *Pname_to_map(struct ps_prochandle *P, const char *name) {
    return (Plmid_to_map(P, PR_LMID_EVERY, name));
}

/*
 * Given a shared object name, return the map_info_t for it.  If no matching
 * object is found, return NULL.  Normally, the link maps contain the full
 * object pathname, e.g. /usr/lib/libc.so.1.  We allow the object name to
 * take one of the following forms:
 *
 * 1. An exact match (i.e. a full pathname): "/usr/lib/libc.so.1"
 * 2. An exact basename match: "libc.so.1"
 * 3. An initial basename match up to a '.' suffix: "libc.so" or "libc"
 * 4. The literal string "a.out" is an alias for the executable mapping
 *
 * The third case is a convenience for callers and may not be necessary.
 *
 * As the exact same object name may be loaded on different link maps (see
 * dlmopen(3DL)), we also allow the caller to resolve the object name by
 * specifying a particular link map id.  If lmid is PR_LMID_EVERY, the
 * first matching name will be returned, regardless of the link map id.
 */
/*
 * APPLE NOTE:
 * 
 * It appears there are only 3 uses of this currently. A check for ld.so (dtrace fails against static exe's),
 * and a test for a.out'ness. dtrace looks up the map for a.out, and the map for the module, and makes sure
 * they share the same v_addr.
 *
 * To avoid faulting in all symbol owners to create an array of prmap_t, we keep a dictionary of NSData's,
 * keyed by symbol owner. We key by symbol owner because prmaps can also be looked up by address.
 */

const prmap_t *Plmid_to_map(struct ps_prochandle *P, Lmid_t ignored, const char *cname) {
        const prmap_t* map = NULL;
        
        // Need to handle some special case defines
        if (cname == PR_OBJ_LDSO) 
                cname = "dyld";
        else if (cname == PR_OBJ_EXEC)
                cname = "a.out";
        
        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
        
        NSString* name = [NSString stringWithUTF8String:cname];

        VMUSymbolOwner* owner = symbolOwnerForName(P->symbolicator, name);
  
        // <rdar://problem/4877551>
        if (owner && ![owner isCommpage]) {
                // Now we try a lookup
                NSData* data = [P->prmap_dictionary objectForKey:owner];
                
                if (!data) {
                        prmap_t temp;
                        
                        temp.pr_vaddr = [[[owner regions] objectAtIndex:0] addressRange].location;                        
                        temp.pr_mflags = MA_READ; // Anything we get from a symbolicator is readable
                        
                        data = [NSData dataWithBytes:&temp length:sizeof(temp)];
                        
                        [P->prmap_dictionary setObject:data forKey:owner];
                }
                 
                map = [data bytes];
        }
        
        [pool drain];
        
        return map;
}

/*
 * Given a virtual address, return the name of the underlying
 * mapped object (file), as provided by the dynamic linker.
 * Return NULL on failure (no underlying shared library).
 */
char *Pobjname(struct ps_prochandle *P, uint64_t addr, char *buffer, size_t bufsize) {
        char *err = NULL;
        
        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

        VMUSymbolOwner* owner = [P->symbolicator symbolOwnerForAddress:addr];
        
        if (owner) {
                strncpy(buffer, [[owner name] UTF8String], bufsize);
                err = buffer;
        }
        
        [pool drain];
        
        // ALWAYS! make certain buffer is null terminated
        buffer[bufsize-1] = 0;
        
        return err;
}

/*
 * Given a virtual address, return the link map id of the underlying mapped
 * object (file), as provided by the dynamic linker.  Return -1 on failure.
 */
/*
 * APPLE NOTE:
 *
 * We are treating everything as being in the base map, so no work is needed.
 */
int Plmid(struct ps_prochandle *P, uint64_t addr, Lmid_t *lmidp) {
        *lmidp = LM_ID_BASE;
        return 0;
}

// A helper category to crack "-[Class method]" into "Class" "-method"
@interface VMUSymbol (dtrace)
- (NSString*)dtraceClassName;
- (NSString*)dtraceMethodName;
@end

@implementation VMUSymbol (dtrace)

//
// FIX ME!!
//
// These methods are horribly inefficient.
- (NSString*)dtraceClassName
{
        NSString* temp = (_name) ? _name : _mangledName;
        NSAssert([self isObjcMethod], @"Only valid for objc methods");
        NSAssert(([temp hasPrefix:@"-["] || [temp hasPrefix:@"+["]) && [temp hasSuffix:@"]"], @"Not objc method name!");
        NSRange r = [temp rangeOfString:@" "];
        return [temp substringWithRange:NSMakeRange(2, r.location - 2)];
}

- (NSString*)dtraceMethodName
{
        NSString* temp = (_name) ? _name : _mangledName;
        NSAssert([self isObjcMethod], @"Only valid for objc methods");
        NSAssert(([temp hasPrefix:@"-["] || [temp hasPrefix:@"+["]) && [temp hasSuffix:@"]"], @"Not objc method name!");    
        NSRange r = [temp rangeOfString:@" "];
        r.location++;
        return [NSString stringWithFormat:@"%c%@", (char)[temp characterAtIndex:0], [temp substringWithRange:NSMakeRange(r.location, ([temp length] - 1) - r.location)]];
}

@end

/*
 * This is an Apple only proc method. It is used by the objc provider,
 * to iterate all classes and methods.
 */

int Pobjc_method_iter(struct ps_prochandle *P, proc_objc_f *func, void *cd) {
        int err = 0;
        
        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

        for (VMUSymbolOwner* owner in [P->symbolicator symbolOwners]) {
                for (VMUSymbol* symbol in [owner symbols]) {
                        if ([symbol isObjcMethod]) {
                                GElf_Sym gelf_sym;
                                VMURange addressRange = [symbol addressRange];
                                
                                gelf_sym.st_name = 0;
                                gelf_sym.st_info = GELF_ST_INFO((STB_GLOBAL), (STT_FUNC));
                                gelf_sym.st_other = 0;
                                gelf_sym.st_shndx = SHN_MACHO;
                                gelf_sym.st_value = addressRange.location;
                                gelf_sym.st_size = addressRange.length;
                                
                                NSString* class_name = [symbol dtraceClassName];
                                NSString* method_name = [symbol dtraceMethodName];
                                
                                if ((err = func(cd, &gelf_sym, [class_name UTF8String], [method_name UTF8String])) != 0)
                                        break;
                                
                        }
                }
                
                /* We need to propagate errors from the inner loop */
                if (err != 0)
                        break;
        }
        
        [pool drain];
        
        return err;
}

/*
 * APPLE NOTE: 
 *
 * object_name == VMUSymbolOwner
 * which == PR_SYMTAB || PR_DYNSYM 
 * mask == BIND_ANY | TYPE_FUNC (Binding type and func vs data?)
 * cd = caller data, pass through
 *
 * If which is not PR_SYMTAB, return success without doing any work
 * We're ignoring the binding type, but honoring TYPE_FUNC
 *
 * Note that we do not actually iterate in address order!
 */

int Psymbol_iter_by_addr(struct ps_prochandle *P, const char *object_name, int which, int mask, proc_sym_f *func, void *cd) {
	int err = 0;

	if (which != PR_SYMTAB)
		return err;

	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

	NSString* name = [NSString stringWithUTF8String:object_name];

	VMUSymbolOwner* owner = symbolOwnerForName(P->symbolicator, name);

	if (owner && ![owner isCommpage]) { // <rdar://problem/4877551>
		for (VMUSymbol* symbol in [owner symbols]) {

			if ([symbol isDyldStub]) // We never instrument dyld stubs
				continue;

			if ((mask & TYPE_FUNC) && ![symbol isFunction])
				continue;

			GElf_Sym gelf_sym;
			VMURange addressRange = [symbol addressRange];

			gelf_sym.st_name = 0;
			gelf_sym.st_info = GELF_ST_INFO((STB_GLOBAL), (STT_FUNC));
			gelf_sym.st_other = 0;
			gelf_sym.st_shndx = SHN_MACHO;
			gelf_sym.st_value = addressRange.location;
			gelf_sym.st_size = addressRange.length;

			const char *mangledName = [[symbol mangledName] UTF8String];
			if (_dtrace_mangled &&
			    strlen(mangledName) >= 3 &&
			    mangledName[0] == '_' &&
			    mangledName[1] == '_' &&
			    mangledName[2] == 'Z') {
				// mangled name - use it
				err = func(cd, &gelf_sym, mangledName);
			} else {
				err = func(cd, &gelf_sym, [[symbol name] UTF8String]);
			}
			if (err != 0)
				break;
		}
	} else {
		// We must fail if the owner cannot be found
		err = -1;
	}

	[pool drain];

	return err;
}

void Pupdate_maps(struct ps_prochandle *P) {
	Pupdate_syms( P );
}

void Pmothball_syms(struct ps_prochandle *P) {
	if (P->prev_symbolicator)
		[P->prev_symbolicator release];
	
	P->prev_symbolicator = P->symbolicator;
}
	
void Pupdate_syms(struct ps_prochandle *P) {	
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	
	if (P->symbolicator && (P->symbolicator != P->prev_symbolicator))
		[P->symbolicator release];
	
	P->symbolicator = nil;
	
	// The dictionary contains NSData objects, which will free their backing store automagically
	[P->prmap_dictionary release];
	P->prmap_dictionary = nil;
	
	@try {
#if !defined(REVERT_PR_5048245)
		VMUSymbolicator* symbolicator = [VMUSymbolicator symbolicatorForTask:(P->task)];
#else
#warning REVERT_PR_5048245
		VMUSymbolicator* symbolicator = [VMUSymbolicator symbolicatorForPid:(P->status.pr_pid)];
#endif

		if (symbolicator) {
			P->symbolicator = [symbolicator retain];
			P->prmap_dictionary = [[NSMutableDictionary alloc] init];
		}
	} @catch (NSException* e) {
		Prelease(P, PRELEASE_CLEAR);
	}
	
	[pool drain];
}

/*
 * Given an address, Ppltdest() determines if this is part of a PLT, and if
 * so returns a pointer to the symbol name that will be used for resolution.
 * If the specified address is not part of a PLT, the function returns NULL.
 */
/*
 * APPLE NOTE:
 *
 * I believe a PLT is equivalent to our dyld stubs. This method is difficult
 * to implement properly, there isn't a good place to free a string allocated
 * here. Currently, the only use of this method does not actually use the
 * returned string, it simply checks !NULL.
 *
 * We're cheating in one other way. Plookup_by_addr() is used in dt_pid.c to
 * match functions to instrument in the pid provider. For that use, we want to
 * screen out (!functions && dyldStubs). However, the stack backtrace also uses
 * Plookup_by_addr(), and should be able to find dyldStubs. 
 * 
 * Because dt_pid.c calls this method just before lookups, we're going to overload
 * it to also screen out !functions.
 */
const char *Ppltdest(struct ps_prochandle *P, uint64_t addr) {
        const char* err = NULL;
        
        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
        
        VMUSymbol* symbol = [P->symbolicator symbolForAddress:addr];
        
	// Do not allow dyld stubs, !functions, or commpage entries (<rdar://problem/4877551>)
        if (symbol && ([symbol isDyldStub] || (![symbol isFunction]) || [[symbol owner] isCommpage]))
                err = "Ppltdest is not implemented";
        
        [pool drain];
        
        return err;
}

//
// Pactivityserver() implements a mach server that fields reports of dyld image activity
// in the client spawned by Pcreate() represented by the ps_prochandle P.
//
union MaxMsgSize {
	union __RequestUnion__dt_dyld_dtrace_dyld_subsystem req;
	union __ReplyUnion__dt_dyld_dtrace_dyld_subsystem rep; 
};

#define MAX_BOOTSTRAP_NAME_CHARS 32 /* _DTRACE_DYLD_BOOTSTRAP_NAME with pid digits tacked on. */

rd_agent_t *Prd_agent(struct ps_prochandle *P) {
	kern_return_t kr;
	mach_port_t dt_dyld_port, notify_service, oldNotifyPort, pset;
	char bsname[MAX_BOOTSTRAP_NAME_CHARS];
	
	(void) snprintf(bsname, sizeof(bsname), "%s_%d", _DTRACE_DYLD_BOOTSTRAP_NAME, P->status.pr_pid);
	
	/* If we've been started already by a previous call here. */
	kr = bootstrap_check_in(bootstrap_port, bsname, &dt_dyld_port);
	if (kr == KERN_SUCCESS) {
		fprintf(stderr, "Revisting bootstrap_check_in() for %s (!)\n", bsname);
		return (rd_agent_t *)P;
	}

	/* First call here. Establish RPC service that dyld can use for rendezvous. */
	dt_dyld_port = (mach_port_t)P; // Encode ps_prochandle value "P" into port name.
	kr = mach_port_allocate_name(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, dt_dyld_port);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "mach_port_allocate(dt_dyld_port): %s\n", mach_error_string(kr));
		return NULL;
	}

	kr = mach_port_insert_right(mach_task_self(), dt_dyld_port, dt_dyld_port, MACH_MSG_TYPE_MAKE_SEND);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "mach_port_insert_right(dt_dyld_port): %s\n", mach_error_string(kr));
		return NULL;
	}

	kr = bootstrap_register(bootstrap_port, bsname, dt_dyld_port);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "bootstrap_register(dt_dyld_port): %s\n", mach_error_string(kr));
		return NULL;
	}

	notify_service = 1 + (mach_port_t)P; // Encode ps_prochandle value "P" into notify port name.
	kr = mach_port_allocate_name(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, notify_service);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "mach_port_allocate(notify_service): %s\n", mach_error_string(kr));
		return NULL;
	}

	oldNotifyPort = MACH_PORT_NULL;
	kr = mach_port_request_notification(mach_task_self(),
					    P->task,
					    MACH_NOTIFY_DEAD_NAME,
					    1,
					    notify_service,
					    MACH_MSG_TYPE_MAKE_SEND_ONCE,
					    &oldNotifyPort);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "mach_port_request_notification(notify_service): %s\n", mach_error_string(kr));
	} else if (MACH_PORT_NULL != oldNotifyPort) {
		(void)mach_port_deallocate(mach_task_self(), oldNotifyPort);
	}
	
	
	kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &pset);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "mach_port_allocate(pset): %s\n", mach_error_string(kr));
		return NULL;
	}

	(void)mach_port_move_member(mach_task_self(), dt_dyld_port, pset);
	(void)mach_port_move_member(mach_task_self(), notify_service, pset);

	P->dtrace_dyld_port = pset;
	return (rd_agent_t *)P; // rd_agent_t is type punned onto ps_proc_handle (see below).
}

void Prd_agent_shutdown(struct ps_prochandle *P)
{
	// Called from main thread. 
	// Want to break out from mach_msg_server() looping on a dt_proc_control thread.
	(void)mach_port_destroy(mach_task_self(), P->dtrace_dyld_port);
}

kern_return_t do_mach_notify_no_senders(
	mach_port_t notify,
	mach_port_name_t name)
{
	return KERN_FAILURE;
}

kern_return_t do_mach_notify_send_once(
	mach_port_t notify)
{
	return KERN_FAILURE;
}

kern_return_t do_mach_notify_port_deleted(
	mach_port_t notify,
	mach_port_name_t name)
{
	return KERN_FAILURE;
}

kern_return_t do_mach_notify_port_destroyed(
	mach_port_t notify,
	mach_port_name_t name)
{
	return KERN_FAILURE;
}

kern_return_t do_mach_notify_dead_name(
	mach_port_t notify,
	mach_port_name_t name)
{
	struct ps_prochandle *P = (struct ps_prochandle *)(notify - 1);
	
	if (P->activity_handler_func && P->activity_handler_arg)
		P->activity_handler_func(P->activity_handler_arg);
	return KERN_SUCCESS;
}

kern_return_t
dt_dyld_report_activity(mach_port_t activity_port, rd_event_msg_t rdm)
{
	struct ps_prochandle *P = (struct ps_prochandle *)activity_port;
	
	P->activity_handler_rdm = rdm;
	if (P->activity_handler_func && P->activity_handler_arg)
		P->activity_handler_func(P->activity_handler_arg);

	return KERN_SUCCESS;
}

static boolean_t dtrace_dyld_demux(mach_msg_header_t *request, mach_msg_header_t *reply) {

	if (MACH_MSGH_BITS_LOCAL(request->msgh_bits) == MACH_MSG_TYPE_PORT_SEND_ONCE) {
		return notify_server(request, reply);
	} else {
		return dtrace_dyld_server(request, reply);
	}
	/* NOTREACHED */
}

void Pactivityserver(struct ps_prochandle *P, Phandler_func_t f, void *arg)
{
	mach_msg_size_t mxmsgsz = sizeof(union MaxMsgSize) + MAX_TRAILER_SIZE;
	kern_return_t kr;

	P->activity_handler_func = f;
	P->activity_handler_arg = arg;

	kr = mach_msg_server(dtrace_dyld_demux, mxmsgsz, P->dtrace_dyld_port, 0);
}

//
// Shims for Solaris run-time loader SPI.
// The (opaque) rd_agent type is type punned onto ps_prochandle.
// "breakpoint" addresses corresponding to rd_events are mapped to the event type (a small enum value),
// that's sufficient to uniquely identify the event.
//

rd_err_e rd_event_getmsg(rd_agent_t *oo7, rd_event_msg_t *rdm) {
	struct ps_prochandle *P = (struct ps_prochandle *)oo7;
	
	*rdm = P->activity_handler_rdm;
	return RD_OK;
}

//
// Procedural replacement called from dt_proc_bpmatch() to simulate "psp->pr_reg[R_PC]"
//
psaddr_t rd_event_mock_addr(struct ps_prochandle *P)
{
	return (psaddr_t)(P->activity_handler_rdm.type);
}

rd_err_e rd_event_enable(rd_agent_t *oo7, int onoff) {
	return RD_OK;
}

rd_err_e rd_event_addr(rd_agent_t *oo7, rd_event_e ev, rd_notify_t *rdn)
{
	rdn->type = RD_NOTIFY_BPT;
	rdn->u.bptaddr = (psaddr_t)ev;
	
	return RD_OK;
}

char *rd_errstr(rd_err_e err) {
	switch (err) {
		case RD_ERR:
			return "RD_ERR";
		case RD_OK:
			return "RD_OK";
		case RD_NOCAPAB:
			return "RD_NOCAPAB";
		case RD_DBERR:
			return "RD_DBERR";
		case RD_NOBASE:
			return "RD_NOBASE";
		case RD_NODYNAM:
			return "RD_NODYNAM";
		case RD_NOMAPS:
			return "RD_NOMAPS";
		default:
			return "RD_UNKNOWN";
	}
	/* NOTREACHED */
}

extern int proc_pidinfo(int pid, int flavor, uint64_t arg,  void *buffer, int buffersize);

int Pstate(struct ps_prochandle *P) {
        int retval = 0;
        struct proc_bsdinfo pbsd;
        
		retval = proc_pidinfo(P->status.pr_pid, PROC_PIDTBSDINFO, (uint64_t)0, &pbsd, sizeof(struct proc_bsdinfo));
        if (retval == -1) {
			return -1;
		} else if (retval == 0) {
			return PS_LOST;
		} else {
			switch(pbsd.pbi_status) {
			case SIDL:
				return PS_IDLE;
			case SRUN:
				return PS_RUN;
			case SSTOP:
				return PS_STOP;
			case SZOMB:
				return PS_UNDEAD;
			case SSLEEP:
			default:
				return -1;
			}
        }
		/* NOTREACHED */
}

const pstatus_t *Pstatus(struct ps_prochandle *P) {
    return &P->status;
}

//
// Unsorted below here.
//

int Pctlfd(struct ps_prochandle *ignore) {
    NSLog(@"libProc.a UNIMPLEMENTED: Pctlfd()");
    return 0;
}
