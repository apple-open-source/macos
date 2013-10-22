/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <mach/i386/vm_param.h>
#include <sys/kern_memorystatus.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#include <sys/mman.h>
#include <pthread.h>
#include <assert.h>
#include <dispatch/private.h>

unsigned long	phys_mem = 0;            /* amount of physical memory in bytes */
unsigned int	phys_pages = 0;          /* number of physical memory pages */
int		sleep_seconds = 1;
boolean_t	quiet_mode_on = FALSE;
boolean_t	simulate_mode_on = FALSE;

void		*range_start_addr = NULL;
void		*range_end_addr = NULL;
void		*range_current_addr = NULL;

int			start_referencing_pages = 0;
int			start_allocing_pages = 0;
pthread_cond_t		reference_pages_condvar = PTHREAD_COND_INITIALIZER;
pthread_mutex_t		reference_pages_mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned int		desired_level = 0, desired_percent = 0;
unsigned int		percent_for_level = 0;
int			tool_mode = 0;

#define	TOOL_MODE_FOR_PERCENT	1
#define TOOL_MODE_FOR_LEVEL	2


char		random_data[] = "ffd8ffe000104a46494600010101002400240000ffe100744578696600004d4d002a000000080004011a0005000000010000003e011b0005000000010000004601280003000000010002000087690004000000010000004e00000000000000240000000100000024000000010002a002000400000001000003c0a003000400000001000001ff00000000ffdb00430002020202020102020202020202030306040303030307050504060807080808070808090a0d0b09090c0a08080b0f0b0c0d0e0e0e0e090b10110f0e110d0e0e0effdb004301020202030303060404060e0908090e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0effc000110801ff03c003012200021101031101ffc4001f0000010501010101010100000000000000000102030405060708090a0bffc400b5100002010303020403050504040000017d01020300041105122131410613516107227114328191a1082342b1c11552d1f02433627282090a161718191a25262728292a3435363738393a434445464748494a535455565758595a636465666768696a737475767778797a838485868788898a92939495969798999aa2a3a4a5a6a7a8a9aab2b3b4b5b6b7b8b9bac2c3c4c5c6c7c8c9cad2d3d4d5d6d7d8d9dae1e2e3e4e5e6e7e8e9eaf1f2f3f4f5f6f7f8f9faffc4001f0100030101010101010101010000000000000102030405060708090a0bffc400b51100020102040403040705040400010277000102031104052131061241510761711322328108144291a1b1c109233352f0156272d10a162434e125f11718191a262728292a35363738393a434445464748494a535455565758595a636465666768696a737475767778797a82838485868788898a92939495969798999aa2a3a4a5a6a7a8a9aab2b3b4b5b6b7b8b9bac2c3c4c5c6c7c8c9cad2d3d4d5d6d7d8d9dae2e3e4e5e6e7e8e9eaf2f3f4f5f6f7f8f9faffda000c03010002110311003f00f9e74fbd37baa2db99e6506391f28371f9519ba67fd9fcabd46cbc1315de8d6776752d7419e049084b152a37283c1dfc8e6bc02db4af18d9df79c9e1bd59a40ae9b65b1761f32953c63ae09c7a1c57656fe24f8896da7c16c9e0bb3748a358d5a4d04b31006324f73c75a00935f7fec9f165ee98b7372e2ddc05795763f2a0f20138ebeb590bac3e70d2b6e1fed1ac6d4ecbc65aa6b973a85c7867528a6998168edec1a38c1c01c2f61c550fec1f16ff00d0bdade4f5ff00447ff0a00eaffb5dbfe7abfe668fed76ff009eaff99ae57fb07c5bff0042f6b7ff00808ffe147f60f8b7fe85ed6fff00011ffc2803aafed76ff9eaff0099a3fb5dbfe7abfe66b95fec1f16ff00d0bdadff00e023ff00851fd83e2dff00a17b5bff00c047ff000a00eabfb5dbfe7abfe668fed76ff9eaff0099ae57fb07c5bff42f6b7ff808ff00e147f60f8b7fe85ed6ff00f011ff00c2803aafed76ff009eaff99a3fb5dbfe7abfe66b95fec1f16ffd0bdadffe023ff851fd83e2dffa17b5bffc047ff0a00eabfb5dbfe7abfe668fed76ff009eaff99ae57fb07c5bff0042f6b7ff00808ffe147f60f8b7fe85ed6fff00011ffc2803aafed76ff9eaff0099a3fb5dbfe7abfe66b95fec1f16ff00d0bdadff00e023ff00851fd83e2dff00a17b5bff00c047ff000a00eabfb5dbfe7abfe668fed76ff9eaff0099ae57fb07c5bff42f6b7ff808ff00e147f60f8b7fe85ed6ff00f011ff00c2803aafed76ff009eaff99a3fb5dbfe7abfe66b95fec1f16ffd0bdadffe023ff851fd83e2dffa17b5bffc047ff0a00eabfb5dbfe7abfe668fed76ff009eaff99ae57fb07c5bff0042f6b7ff00808ffe147f60f8b7fe85ed6fff00011ffc2803aafed76ff9eaff0099a3fb5dbfe7abfe66b95fec1f16ff00d0bdadff00e023ff00851fd83e2dff00a17b5bff00c047ff000a00eabfb5dbfe7abfe668fed76ff9eaff0099ae57fb07c5bff42f6b7ff808ff00e147f60f8b7fe85ed6ff00f011ff00c2803aafed76ff009eaff99a3fb5dbfe7abfe66b95fec1f16ffd0bdadffe023ff851fd83e2dffa17b5bffc047ff0a00eabfb5dbfe7abfe668fed76ff009eaff99ae57fb07c5bff0042f6b7ff00808ffe147f60f8b7fe85ed6fff00011ffc2803aafed76ff9eaff0099a3fb5dbfe7abfe66b95fec1f16ff00d0bdadff00e023ff00851fd83e2dff00a17b5bff00c047ff000a00eabfb5dbfe7abfe668fed76ff9eaff0099ae57fb07c5bff42f6b7ff808ff00e147f60f8b7fe85ed6ff00f011ff00c2803aafed76ff009eaff99a3fb5dbfe7abfe66b95fec1f16ffd0bdadffe023ff851fd83e2dffa17b5bffc047ff0a00eabfb5dbfe7abfe668fed76ff009eaff99ae57fb07c5bff0042f6b7ff00808ffe147f60f8b7fe85ed6fff00011ffc2803aafed76ff9eaff0099a3fb5dbfe7abfe66b95fec1f16ff00d0bdadff00e023ff00851fd83e2dff00a17b5bff00c047ff000a00eabfb5dbfe7abfe668fed76ff9eaff0099ae57fb07c5bff42f6b7ff808ff00e147f60f8b7fe85ed6ff00f011ff00c2803aafed76ff009eaff99a3fb5dbfe7abfe66b95fec1f16ffd0bdadffe023ff851fd83e2dffa17b5bffc047ff0a00eabfb5dbfe7abfe668fed76ff009eaff99ae57fb07c5bff0042f6b7ff00808ffe147f60f8b7fe85ed6fff00011ffc2803aafed76ff9eaff0099a3fb5dbfe7abfe66b95fec1f16ff00d0bdadff00e023ff00851fd83e2dff00a17b5bff00c047ff000a00eabfb5dbfe7abfe668fed76ff9eaff0099ae57fb07c5bff42f6b7ff808ff00e147f60f8b7fe85ed6ff00f011ff00c2803aafed76ff009eaff99a3fb5dbfe7abfe66b95fec1f16ffd0bdadffe023ff851fd83e2dffa17b5bffc047ff0a00eabfb5dbfe7abfe668fed76ff009eaff99ae57fb07c5bff0042f6b7ff00808ffe147f60f8b7fe85ed6fff00011ffc2803aafed76ff9eaff0099a3fb5dbfe7abfe66b95fec1f16ff00d0bdadff00e023ff00851fd83e2dff00a17b5bff00c047ff000a00eabfb5dbfe7abfe668fed76ff9eaff0099ae57fb07c5bff42f6b7ff808ff00e147f60f8b7fe85ed6ff00f011ff00c2803aafed76ff009eaff99a3fb5dbfe7abfe66b95fec1f16ffd0bdadffe023ff851fd83e2dffa17b5bffc047ff0a00eabfb5dbfe7abfe668fed76ff009eaff99ae57fb07c5bff0042f6b7ff00808ffe147f60f8b7fe85ed6fff00011ffc2803aafed76ff9eaff0099a3fb5dbfe7abfe66b95fec1f16ff00d0bdadff00e023ff00851fd83e2dff00a17b5bff00c047ff000a00eabfb5dbfe7abfe668fed76ff9eaff0099ae57fb07c5bff42f6b7ff808ff00e147f60f8b7fe85ed6ff00f011ff00c2803aafed76ff009eaff99a3fb5dbfe7abfe66b95fec1f16ffd0bdadffe023ff851fd83e2dffa17b5bffc047ff0a00eabfb5dbfe7abfe668fed76ff009eaff99ae57fb07c5bff0042f6b7ff00808ffe147f60f8b7fe85ed6fff00011ffc2803aafed76ff9eaff0099a3fb5dbfe7abfe66b95fec1f16ff00d0bdadff00e023ff00851fd83e2dff00a17b5bff00c047ff000a00eabfb5dbfe7abfe668fed76ff9eaff0099ae57fb07c5bff42f6b7ff808ff00e147f60f8b7fe85ed6ff00f011ff00c2803aafed76ff009eaff99a3fb5dbfe7abfe66b95fec1f16ffd0bdadffe023ff851fd83e2dffa17b5bffc047ff0a00eabfb5dbfe7abfe668fed76ff009eaff99ae57fb07c5bff0042f6b7ff00808ffe147f60f8b7fe85ed6fff00011ffc2803aafed76ff9eaff0099a3fb5dbfe7abfe66b95fec1f16ff00d0bdadff00e023ff00851fd83e2dff00a17b5bff00c047ff000a00eabfb5dbfe7abfe668fed76ff9eaff0099ae57fb07c5bff42f6b7ff808ff00e147f60f8b7fe85ed6ff00f011ff00c2803aafed76ff009eaff99a3fb5dbfe7abfe66b95fec1f16ffd0bdadffe023ff851fd83e2dffa17b5bffc047ff0a00eabfb5dbfe7abfe668fed76ff009eaff99ae57fb07c5bff0042f6b7ff00808ffe147f60f8b7fe85ed6fff00011ffc2803aafed76ff9eaff0099a3fb5dbfe7abfe66b95fec1f16ff00d0bdadff00e023ff00851fd83e2dff00a17b5bff00c047ff000a00eabfb5dbfe7abfe668fed76ff9eaff0099ae57fb07c5bff42f6b7ff808ff00e147f60f8b7fe85ed6ff00f011ff00c2803aafed76ff009eaff99a3fb5dbfe7abfe66b95fec1f16ffd0bdadffe023ff851fd83e2dffa17b5bffc047ff0a00eabfb5dbfe7abfe668fed76ff009eaff99ae57fb07c5bff0042f6b7ff00808ffe147f60f8b7fe85ed6fff00011ffc2803aafed76ff9eaff0099a3fb5dbfe7abfe66b95fec1f16ff00d0bdadff00e023ff00851fd83e2dff00a17b5bff00c047ff000a00eabfb5dbfe7abfe668fed76ff9eaff0099ae57fb07c5bff42f6b7ff808ff00e147f60f8b7fe85ed6ff00f011ff00c2803aafed76ff009eaff99a3fb5dbfe7abfe66b95fec1f16ffd0bdadffe023ff851fd83e2dffa17b5bffc047ff0a00eabfb5dbfe7abfe668fed76ff009eaff99ae57fb07c5bff0042f6b7ff00808ffe147f60f8b7fe85ed6fff00011ffc2803aafed76ff9eaff0099a3fb5dbfe7abfe66b95fec1f16ff00d0bdadff00e023ff00851fd83e2dff00a17b5bff00c047ff000a00eabfb5dbfe7abfe66b4f45fed1f1078bb4cd0f4bdf3ea37f7296f6d1ee3f33b1c0cfb7a9f4ae0bfb07c5bff42f6b7ff808ff00e15a9a1c5e3bf0f78c74bd774cd0f5a8751d3ee52e2ddcd9be03a9c8cfb7623d2803d5bc6fe0df11780e0d3ae354bab1bdb1bd678e1bab1b8f36312c6079b112380cac4ae3b9535e7ffdaedff3d5ff00335b3e3af177c4af1f5be9b6da8f8567d36c2c59e486d74fd35e28ccb260c92b003966605b3db71af3cfec1f16ff00d0bdadff00e023ff0085007acf83743d57c6baddd59e9d7062fb3c68d248519c0324ab120c2f3cb38c9ec013dab95b8d427b5bfb8b59a4759a195a3917278652411f98ad9f86fe30f88bf0cb57d56f348f095f5ebdfc2914ab716b200a118918c0ff0068d709a9d878cb55f12ea3aa4fe1cd6127bdba92e2455b47c06772c40e3a64d007d13f09fe1d37c41d135ad7b52d52ef4fd074eba86cff00d1537cd7171310123504e0751927d6be8ed17f65bf0aebde19b4d5ac7c5fe2236b700eddf12060558ab03ee1948fc2be41f84bf123e20fc2d1acd87fc20d77e24f0eeabb1af74cbcb4902974fbb22b0190c3fa0f4afa1748fdad3c51a1691fd9fa57c10b8b4b212bc8b0ac931542c7240c8e067271ef401e9bff000c89a07fd0dbafff00dfb4a3fe191340ff00a1b75fff00bf695c2ffc366f8e7fe88c5cff00df72ff00851ff0d9be39ff00a23173ff007dcbfe1401dd7fc322681ff436ebff00f7ed28ff008644d03fe86dd7ff00efda570bff000d9be39ffa23173ff7dcbfe147fc366f8e7fe88c5cff00df72ff008500775ff0c89a07fd0dbaff00fdfb4a3fe191340ffa1b75ff00fbf695c2ff00c366f8e7fe88c5cffdf72ff851ff000d9be39ffa23173ff7dcbfe1401dd7fc322681ff00436ebfff007ed2be52f8d1f0eeff00e11fc42b2d325d49f51d3afedccf637046d6215b6b2b0f5071f98af733fb6678eb071f062e73fefcbfe15f2a7c5af1cfc4bf8bff001262d7758f0bea76515bc1f67b1b1b7b490a4099c9edcb13c93f4a00fe891bc2fe1c672cda1e96589c92";

#define	PAGE_OP_ALLOC	0x1
#define PAGE_OP_FREE	0x2

#define USE_WIRED_PAGES_FOR_PERCENT_MODE	FALSE

#define MAX_RANGE_SIZE	64 * 1024 * 1024 * 1024ULL

void print_vm_statistics(void);
void munch_for_level(unsigned int, unsigned int);
void munch_for_percentage(unsigned int, unsigned int, unsigned int);

static void 
usage(void)
{
	fprintf(stderr, "Usage: memory_pressure [options] [<pages>]\n"
			"  Allocate memory and wait forever.\n"
			"  Options include:\n"
			"  -l <level>     	 - allocate memory until a low memory notification is received (warn OR critical)\n"
			"  -p <percent-free>     - allocate memory until percent free is this (or less)\n"
			"  -s <seconds>          - how long to sleep between checking for a set percent level\n"
			"  -w <percent-free>     - don't allocate, just wait until percent free is this then exit\n"
			"  -v <print VM stats>   - print VM statistics every sampling interval\n"
			"  -Q <quiet mode>   	 - reduces the tool's output\n"
			"  -S			 - simulate the system's memory pressure level without applying any real pressure\n"
			"  \n"
	       );
	exit(0);
}

static unsigned int
read_sysctl_int(const char* name) 
{
	unsigned int var;
	size_t var_size;
	int error;

	var_size = sizeof(var);
	error = sysctlbyname(name, &var, &var_size, NULL, 0);
	if( error ) {
		perror(name);
		exit(-1);
	}
	return var;
}

static int
get_percent_free(unsigned int* level) 
{
	int error;

	error = memorystatus_get_level((user_addr_t) level);

	if( error ) {
		perror("memorystatus_get_level failed:");
		exit(-1);
	}
	return error;
}

void
print_vm_statistics(void)
{
	unsigned int count = HOST_VM_INFO64_COUNT;
	kern_return_t ret = 0;
	vm_statistics64_data_t vm_stat;;

	if (quiet_mode_on == TRUE) {
		return;
	}

	if ((ret = host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stat, &count) != KERN_SUCCESS)) {
		fprintf(stderr, "Failed to get statistics. Error %d\n", ret);
	} else {
		printf("\nStats: \n");
		printf("Pages free: %llu \n", (uint64_t) (vm_stat.free_count - vm_stat.speculative_count));
		printf("Pages purgeable: %llu \n",  (uint64_t) (vm_stat.purgeable_count));
		printf("Pages purged: %llu \n",(uint64_t) (vm_stat.purges));

		printf("\nSwap I/O:\n");
		printf("Swapins: %llu \n", (uint64_t) (vm_stat.swapins));
		printf("Swapouts: %llu \n", (uint64_t) (vm_stat.swapouts));

		printf("\nPage Q counts:\n");
		printf("Pages active: %llu \n", (uint64_t) (vm_stat.active_count));
		printf("Pages inactive: %llu \n", (uint64_t) (vm_stat.inactive_count));
		printf("Pages speculative: %llu \n", (uint64_t) (vm_stat.speculative_count));
		printf("Pages throttled: %llu \n", (uint64_t) (vm_stat.throttled_count));
		printf("Pages wired down: %llu \n", (uint64_t) (vm_stat.wire_count));
		
		printf("\nCompressor Stats:\n");
		printf("Pages used by compressor: %llu \n", (uint64_t) (vm_stat.compressor_page_count));
		printf("Pages decompressed: %llu \n", (uint64_t) (vm_stat.decompressions));
		printf("Pages compressed: %llu \n", (uint64_t) (vm_stat.compressions));

		printf("\nFile I/O:\n");
		printf("Pageins: %llu \n", (uint64_t) (vm_stat.pageins));
		printf("Pageouts: %llu \n", (uint64_t) (vm_stat.pageouts));

#if 0
		printf("\"Translation faults\": %llu \n", (uint64_t) (vm_stat.faults));
		printf("Pages copy-on-write: %llu \n", (uint64_t) (vm_stat.cow_faults));
		printf("Pages zero filled: %llu \n", (uint64_t) (vm_stat.zero_fill_count));
		printf("Pages reactivated: %llu \n", (uint64_t) (vm_stat.reactivations));
#endif
		printf("\n");
	}
}


static int
reached_or_bypassed_desired_result(void)
{
	if (tool_mode == TOOL_MODE_FOR_LEVEL) {

		unsigned int	current_level = 0;

		current_level = read_sysctl_int("kern.memorystatus_vm_pressure_level");

		if (desired_level > 0 && current_level >= desired_level) {
		       return 1;
		}

		return 0;
	}

	if (tool_mode == TOOL_MODE_FOR_PERCENT) {
		
		unsigned int current_percent = 0;

		get_percent_free(&current_percent);

		if (desired_percent > 0 && current_percent <= desired_percent) {
			return 1;
		}

		return 0;
	}

	return 0;
}

static void
reference_pages(int level)
{
	int	error;
	void	*addr = NULL;
	int	num_pages = 0;

	error = pthread_mutex_lock(&reference_pages_mutex);
	addr = range_start_addr;
again:
     	while(start_referencing_pages == 0) {
		error = pthread_cond_wait(&reference_pages_condvar, &reference_pages_mutex);
	}

	start_allocing_pages = 0;
	pthread_mutex_unlock(&reference_pages_mutex);
	
	num_pages = 0;
	for(; addr < range_current_addr;) {
		
		char p;
		
		if (reached_or_bypassed_desired_result()) {
			//printf("stopped referencing after %d pages\n", num_pages);
			break;
		}

		p = *(char*) addr;
		addr += PAGE_SIZE;
		num_pages++;
				
	}
	
	//if (num_pages) {
	//	printf("Referenced %d\n", num_pages);
	//}
	error = pthread_mutex_lock(&reference_pages_mutex);
	start_referencing_pages = 0;
	start_allocing_pages = 1;

	goto again;	

}

static void
process_pages(int num_pages, int page_op) 
{
	if (num_pages > 0) {
		
		int 	error = 0, i = 0;
		size_t	size = num_pages * PAGE_SIZE;

		if (page_op == PAGE_OP_ALLOC) {

			if (tool_mode == TOOL_MODE_FOR_PERCENT && USE_WIRED_PAGES_FOR_PERCENT_MODE) {
				error = mlock(range_current_addr, size);
				if (error == -1) {
					perror("Failed to lock memory!");
					exit(-1);
				}        

				memset(range_current_addr, 0xFF, size);
				range_current_addr += size;

			} else {

				pthread_mutex_lock(&reference_pages_mutex);
				while (start_allocing_pages == 0) {
					pthread_mutex_unlock(&reference_pages_mutex);
					sleep(1);
					pthread_mutex_lock(&reference_pages_mutex);
				}
				pthread_mutex_unlock(&reference_pages_mutex);

				for (i=0; i < num_pages; i++) {
			
					if (reached_or_bypassed_desired_result()) {
						//printf("stopped faulting after %d pages\n", i);
						break;
					}

					memcpy(range_current_addr, random_data, PAGE_SIZE);
					range_current_addr += PAGE_SIZE;
				}

				pthread_mutex_lock(&reference_pages_mutex);
				start_referencing_pages = 1;
				pthread_cond_signal(&reference_pages_condvar);
				pthread_mutex_unlock(&reference_pages_mutex);
			}
		} else {
			if (tool_mode == TOOL_MODE_FOR_PERCENT && USE_WIRED_PAGES_FOR_PERCENT_MODE) {
				error = munlock(range_current_addr, size);
				if (error == -1) {
					perror("Failed to unlock memory!");
					exit(-1);
				}        
				
				error = madvise(range_current_addr, size, MADV_FREE);
				if (error == -1) {
					perror("Failed to madv_free memory!");
					exit(-1);
				}

				range_current_addr -= size;

			} else {
				pthread_mutex_lock(&reference_pages_mutex);
				while (start_referencing_pages == 1) {
					pthread_mutex_unlock(&reference_pages_mutex);
					sleep(1);
					pthread_mutex_lock(&reference_pages_mutex);
				}
				
				error = madvise(range_current_addr, size, MADV_FREE);
				if (error == -1) {
					perror("Failed to madv_free memory!");
					exit(-1);
				}
				range_current_addr -= size;
				start_referencing_pages = 1;
				pthread_cond_signal(&reference_pages_condvar);
				pthread_mutex_unlock(&reference_pages_mutex);
			}
		}
	}
}

void
munch_for_level(unsigned int sleep_seconds, unsigned int print_vm_stats) 
{

	unsigned int	current_level = 0;
	unsigned int	desired_percent = 0;
	unsigned int	current_percent = 0;
	unsigned int	page_op = PAGE_OP_ALLOC;
	unsigned int	previous_page_op = PAGE_OP_ALLOC;
	unsigned int	pages_to_process = 0;
	unsigned int	stabilized_percentage = 0;
	boolean_t	print_vm_stats_on_page_processing = FALSE;
	boolean_t	ok_to_print_stablity_message = TRUE;

	current_level =  read_sysctl_int("kern.memorystatus_vm_pressure_level");

	if (current_level >= desired_level) {
		return;
	}

	get_percent_free(&current_percent);
			
	if (print_vm_stats) {
		print_vm_stats_on_page_processing = TRUE;
	}

	page_op = PAGE_OP_ALLOC;
	previous_page_op = 0;

	while (1) {

			if (current_percent > percent_for_level) {
				desired_percent = current_percent - percent_for_level;
			} else {
				desired_percent = 1;
			}
			
			pages_to_process = (desired_percent * phys_pages) / 100;

			page_op = PAGE_OP_ALLOC;

			if (previous_page_op != page_op) {
				//printf("%s %d pages.\n", (page_op == PAGE_OP_ALLOC) ? "Allocating" : "Freeing", pages_to_process);
				printf("\nCMD: %s pages to go from level: %d to level: %d", (page_op == PAGE_OP_ALLOC) ? "Allocating" : "Freeing", current_level, desired_level);
				previous_page_op = page_op;
				fflush(stdout);
			} else {
				printf(".");
				fflush(stdout);
			}
			
			if (print_vm_stats_on_page_processing == TRUE) {
				print_vm_statistics();
			}
			process_pages(pages_to_process, page_op);
			ok_to_print_stablity_message = TRUE;

			current_level =  read_sysctl_int("kern.memorystatus_vm_pressure_level");

			if (current_level >= desired_level) {
				
				while(1) {
					current_level =  read_sysctl_int("kern.memorystatus_vm_pressure_level");
					if (current_level < desired_level) {
						break;
					}
					
					if (current_level > desired_level) {
						page_op = PAGE_OP_FREE;
						
						get_percent_free(&current_percent);

						if (stabilized_percentage > current_percent) {
							pages_to_process = ((stabilized_percentage - current_percent) * phys_pages) / 100;
			
							if (previous_page_op != page_op) {
								printf("\nCMD: %s pages to go from %d to %d level", (page_op == PAGE_OP_ALLOC) ? "Allocating" : "Freeing", current_level, desired_level);
								previous_page_op = page_op;
								fflush(stdout);
							} else {
								printf(".");
								fflush(stdout);
							}

							if (print_vm_stats_on_page_processing == TRUE) {
								print_vm_statistics();
							}
							process_pages(pages_to_process, page_op);
							ok_to_print_stablity_message = TRUE;
						}
					}

					while (current_level == desired_level) {
						get_percent_free(&current_percent);
						if (ok_to_print_stablity_message == TRUE) {
							print_vm_statistics();
							printf("\nStabilizing at Percent: %d Level: %d", current_percent, current_level);
							fflush(stdout);
							ok_to_print_stablity_message = FALSE;
							previous_page_op = 0;
						} else {
							printf(".");
							fflush(stdout);
						}

						stabilized_percentage = current_percent;
						sleep(sleep_seconds);
						current_level =  read_sysctl_int("kern.memorystatus_vm_pressure_level");
					}
				}
			}

			get_percent_free(&current_percent);
			//printf("Percent: %d Level: %d\n", current_percent, current_level);
			sleep(1);
		
			if (print_vm_stats) {
				print_vm_stats_on_page_processing = TRUE;
			}

	} /* while */
}

void 
munch_for_percentage(unsigned int sleep_seconds, unsigned int wait_percent_free, unsigned int print_vm_stats) 
{

	int		total_pages_allocated = 0;
	unsigned int	current_percent = 0;
	boolean_t	page_op = PAGE_OP_FREE;
	unsigned int	pages_to_process = 0;
	boolean_t	print_vm_stats_on_page_processing = FALSE;
	boolean_t	previous_page_op = 0;
	boolean_t	ok_to_print_stablity_message = TRUE;

	/* Allocate until memory level is hit. */
	
	get_percent_free(&current_percent);

	/* 
	 * "wait" mode doesn't alloc, it just waits and exits.  This is used
	 * while waiting for *other* processes to allocate memory.
	 */
	if (wait_percent_free) {
		while (current_percent > wait_percent_free) {
			sleep(sleep_seconds);
			get_percent_free (&current_percent);
		}
		return;
	}

	page_op = PAGE_OP_ALLOC;
	previous_page_op = 0;

	while (1) {

		if (current_percent > desired_percent) {
			pages_to_process = ((current_percent - desired_percent) * phys_pages) / 100;
			page_op = PAGE_OP_ALLOC;
		} else {
			pages_to_process = ((desired_percent - current_percent) * phys_pages) / 100;
			page_op = PAGE_OP_FREE;
		}

		if (pages_to_process > 0) {

			if (page_op != previous_page_op) {
				//printf("\n%s %d pages to go from %d%% to %d%% pages free\n", (page_op == PAGE_OP_ALLOC) ? "Allocating" : "Freeing", pages_to_process, current_percent, desired_percent);
				printf("\nCMD: %s pages to go from %d%% to %d%% percent free", (page_op == PAGE_OP_ALLOC) ? "Allocating" : "Freeing", current_percent, desired_percent);
				fflush(stdout);
				previous_page_op = page_op;
			} else {
				printf(".");
				fflush(stdout);
			}

			if (page_op == PAGE_OP_ALLOC) {
				total_pages_allocated += pages_to_process;
				process_pages(pages_to_process, page_op);
				ok_to_print_stablity_message = TRUE;
			} else {
				
				if (total_pages_allocated >= pages_to_process) {
					total_pages_allocated -= pages_to_process;
					process_pages(pages_to_process, page_op);
					ok_to_print_stablity_message = TRUE;
				} else {
					get_percent_free(&current_percent);
					if (ok_to_print_stablity_message == TRUE) {
						printf("\nDesired Percent: %d, Current Percent: %d. No pages to free so waiting", desired_percent, current_percent);
						fflush(stdout);
						ok_to_print_stablity_message = FALSE;
					}
				}
			}
			
			//printf("kernel memorystatus: %d%% free, allocated %d pages total. Requested: %d\n", current_percent, total_pages_allocated, desired_percent);
			if (print_vm_stats) {
				print_vm_stats_on_page_processing = TRUE;
			}
		} else {
			if (ok_to_print_stablity_message == TRUE) {
				print_vm_statistics();
				printf("\nStable at percent free: %d", current_percent);
				fflush(stdout);
				ok_to_print_stablity_message = FALSE;
			} else {
				printf(".");
				fflush(stdout);
			}
			print_vm_stats_on_page_processing = FALSE;
		}

		if (print_vm_stats_on_page_processing) {
			
			print_vm_statistics();

			if (print_vm_stats_on_page_processing == TRUE) {
				print_vm_stats_on_page_processing = FALSE;
			}
		}
		
		sleep(sleep_seconds);

		get_percent_free(&current_percent);
	} /* while */
}

int 
main(int argc, char * const argv[])
{
	int opt;
	unsigned int wait_percent_free = 0;
	unsigned int current_percent = 0;
	unsigned int print_vm_stats = 0;
	char	     level[10];

	while ((opt = getopt(argc, argv, "hl:p:s:w:vQS")) != -1) {
		switch (opt) {
			case 'h':
				usage();
				break;
			case 'l':
				strlcpy(level, optarg, 9);

				if (strncasecmp(level, "normal", 6) == 0) {
					desired_level = DISPATCH_MEMORYSTATUS_PRESSURE_NORMAL;
					percent_for_level = 90;
				} else if (strncasecmp(level, "warn", 4) == 0) {
					desired_level = DISPATCH_MEMORYSTATUS_PRESSURE_WARN;
					percent_for_level = 60;

				} else if (strncasecmp(level, "critical", 8) == 0) {
					desired_level = DISPATCH_MEMORYSTATUS_PRESSURE_CRITICAL;
					percent_for_level = 30;

				} else {
					printf("Incorrect level. Allowed \"normal\" or \"warn\" or \"critical\". Specified: %s\n", level);
					exit(0);
				}
				break;
			case 'p':
				desired_percent = atoi(optarg);
				break;
			case 's':
				sleep_seconds = atoi(optarg);
				break;
			case 'w':
				wait_percent_free = atoi(optarg);
				break;
			case 'v':
				print_vm_stats = 1;
				break;			
			case 'Q':
				quiet_mode_on = TRUE;
				break;
			case 'S':
				simulate_mode_on = TRUE;
				break;
			default:
				usage();
		}
	}

	if (simulate_mode_on == TRUE && desired_level == 0) {
		printf("Expected level with -l along with \"simulated\" mode.\n");
		return 0;
	}

	phys_mem   = read_sysctl_int("hw.physmem");
	phys_pages = (unsigned int) (phys_mem / PAGE_SIZE);

	printf("The system has %ld (%d pages with a page size of %d).\n", phys_mem, phys_pages, PAGE_SIZE);

	print_vm_statistics();
	
	get_percent_free(&current_percent);
	printf("System-wide memory free percentage: %d%%\n", current_percent);

	if (desired_percent == 0 && wait_percent_free == 0 && desired_level == 0) {
		return 0;
	}
	
	if (simulate_mode_on == TRUE) {

		/*	
			We use the sysctl "kern.memorypressure_manual_trigger" for this mode. Here's a blurb:
			
			Supported behaviors when using the manual trigger tests.

			#define TEST_LOW_MEMORY_TRIGGER_ONE		1	most suitable app is notified
			#define TEST_LOW_MEMORY_TRIGGER_ALL		2	all apps are notified
			#define TEST_PURGEABLE_TRIGGER_ONE		3
			#define TEST_PURGEABLE_TRIGGER_ALL		4
			#define TEST_LOW_MEMORY_PURGEABLE_TRIGGER_ONE	5
			#define TEST_LOW_MEMORY_PURGEABLE_TRIGGER_ALL	6

			So, for example, to simulate one app getting a poke when the "pressure" reaches critical levels: "sudo sysctl -w kern.memorypressure_manual_trigger = level"
			where level is calculated as: ((TEST_LOW_MEMORY_TRIGGER_ONE << 16) | NOTE_MEMORYSTATUS_PRESSURE_CRITICAL), which will be "65540".

			For this tool, currently, we only support the "TEST_LOW_MEMORY_PURGEABLE_TRIGGER_ALL" options.
		*/

#define TEST_LOW_MEMORY_PURGEABLE_TRIGGER_ALL	6
	
		unsigned int var = 0;
		size_t var_size = 0;
		int error = 0;

		var_size = sizeof(var);

		var = ((TEST_LOW_MEMORY_PURGEABLE_TRIGGER_ALL << 16) | desired_level);

		error = sysctlbyname("kern.memorypressure_manual_trigger", NULL, 0, &var, var_size);
	
		if(error) {
			perror("sysctl: kern.memorypressure_manual_trigger failed ");
			exit(-1);
		}
	
		printf("Waiting %d seconds before resetting system state\n", sleep_seconds);

		sleep(sleep_seconds);

		var_size = sizeof(var);

		var = ((TEST_LOW_MEMORY_PURGEABLE_TRIGGER_ALL << 16) | DISPATCH_MEMORYSTATUS_PRESSURE_NORMAL);

		error = sysctlbyname("kern.memorypressure_manual_trigger", NULL, 0, &var, var_size);
	
		if(error) {
			perror("sysctl: kern.memorypressure_manual_trigger failed ");
			exit(-1);
		}

		printf("Reset system state\n");

	} else {
		range_start_addr = mmap(NULL, MAX_RANGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
		
		if (range_start_addr == MAP_FAILED) {
			perror("mmap failed");
		} else {

			int		error = 0;
			pthread_t	thread = NULL;

			error = pthread_create(&thread, NULL, (void*) reference_pages, NULL);

			range_current_addr = range_start_addr;
			range_end_addr = range_start_addr + MAX_RANGE_SIZE;
			start_allocing_pages = 1;

			if (desired_level) {
				tool_mode = TOOL_MODE_FOR_LEVEL;
				munch_for_level(sleep_seconds, print_vm_stats);
			} else {
				tool_mode = TOOL_MODE_FOR_PERCENT;
				munch_for_percentage(sleep_seconds, wait_percent_free, print_vm_stats);
			}
		}
	}

	return 0;
}
