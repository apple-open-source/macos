//
//  zlog.c
//  zlog
//
//  Created by Rasha Eqbal on 1/4/18.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach_debug/mach_debug.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreSymbolication/CoreSymbolication.h>
#include "SymbolicationHelper.h"

extern kern_return_t
mach_zone_get_btlog_records(host_priv_t             host,
                            mach_zone_name_t        name,
                            zone_btrecord_array_t   *recsp,
                            mach_msg_type_number_t  *recsCntp);
extern kern_return_t
mach_zone_get_zlog_zones(host_priv_t             host,
                         mach_zone_name_array_t  *namesp,
                         mach_msg_type_number_t  *namesCntp);

static int compare_zone_btrecords(const void *left, const void *right);
static void usage(FILE *stream, char **argv);
static void print_zone_info(const char *name);
static void get_zone_btrecords(const char *name, int topN);
static void list_zones_with_zlog_enabled(void);

static void usage(FILE *stream, char **argv)
{
	fprintf (stream, "usage: %s [-t] [-z name [-n num | -l]] [-h]\n", argv[0]);
	fprintf (stream, "    -t            : list all the zones that have logging enabled\n");
	fprintf (stream, "    -z <name>     : show all allocation backtraces for zone <name>\n");
	fprintf (stream, "    -n <num>      : show top <num> backtraces with the most active references in zone <name>\n");
	fprintf (stream, "    -l            : show the backtrace most likely contributing to a leak in zone <name>\n");
	fprintf (stream, "                    (prints the backtrace with the most active references)\n");
	fprintf (stream, "    -h            : print this help text\n");
	exit(stream != stdout);
}

static int compare_zone_btrecords(const void *left, const void *right)
{
	zone_btrecord_t *btl = (zone_btrecord_t *)left;
	zone_btrecord_t *btr = (zone_btrecord_t *)right;

	return (btr->ref_count - btl->ref_count);
}

static void print_zone_info(const char *name)
{
	mach_zone_name_t zname;
	mach_zone_info_t zone_info;
	kern_return_t kr;

	strcpy(zname.mzn_name, name);
	kr = mach_zone_info_for_zone(mach_host_self(), zname, &zone_info);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "error: call to mach_zone_info_for_zone() failed: %s\n", mach_error_string(kr));
		exit(1);
	}
	printf("zone name            : %s\n", name);
	printf("element size (bytes) : %lld\n", zone_info.mzi_elem_size);
	printf("in-use size  (bytes) : %lld\n", zone_info.mzi_count * zone_info.mzi_elem_size);
	printf("total size   (bytes) : %lld\n", zone_info.mzi_cur_size);
	printf("\n");
}

static void get_zone_btrecords(const char *name, int topN)
{
	kern_return_t kr;
	int i, j, index;
	mach_zone_name_t zname;
	unsigned int recs_count = 0;
	zone_btrecord_t *recs, *recs_addr = NULL;
	CSSymbolicatorRef kernelSym;
	CFMutableDictionaryRef binaryImages;

	/* Create kernel symbolicator */
	kernelSym = CSSymbolicatorCreateWithMachKernel();
	if (CSIsNull(kernelSym)) {
		fprintf(stderr, "error: CSSymbolicatorCreateWithMachKernel() returned NULL\n");
		exit(1);
	}
	/* Create dictionary to collect binary image info for offline symbolication */
    binaryImages = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	/* Query the kernel for backtrace records */
	strcpy(zname.mzn_name, name);
	kr = mach_zone_get_btlog_records(mach_host_self(), zname, &recs_addr, &recs_count);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "error: call to mach_zone_get_btlog_records() failed: %s\n", mach_error_string(kr));
		exit(1);
	}

	if (recs_count == 0) {
		goto finish;
	}

	recs = recs_addr;
	if (topN == 1) {
		/* Print the backtrace with the highest no. of refs */
		index = 0;
		for (i = 0; i < recs_count; i++) {
			if (recs[i].ref_count > recs[index].ref_count) {
				index = i;
			}
		}
		recs = recs_addr + index;
	} else if (topN == 0) {
		/* Print all backtraces */
		topN = recs_count;
	} else {
		/* Sort the records by no. of refs, and print the top <topN> */
		qsort(recs, recs_count, sizeof *recs, compare_zone_btrecords);
	}

	printf("printing top %d (out of %d) allocation backtrace(s) for zone %s\n", topN, recs_count, zname.mzn_name);

	for (i = 0; i < topN; i++) {
		printf("\nactive refs: %d   operation type: %s\n", recs[i].ref_count,
			   (recs[i].operation_type == ZOP_ALLOC)? "ALLOC": (recs[i].operation_type == ZOP_FREE)? "FREE": "UNKNOWN");

		for (j = 0; j < MAX_ZTRACE_DEPTH; j++) {
			mach_vm_address_t addr = (mach_vm_address_t)recs[i].bt[j];
			if (!addr) {
				break;
			}
			PrintSymbolicatedAddress(kernelSym, addr, binaryImages);
		}
	}

	/* Print relevant info for offline symbolication */
	PrintBinaryImagesInfo(binaryImages);
	CFRelease(binaryImages);

finish:
	if ((recs_addr != NULL) && (recs_count != 0)) {
		kr = vm_deallocate(mach_task_self(), (vm_address_t) recs_addr, (vm_size_t) (recs_count * sizeof *recs));
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "call to vm_deallocate() failed: %s\n", mach_error_string(kr));
			exit(1);
		}
	}
	CSRelease(kernelSym);
}

static void list_zones_with_zlog_enabled(void)
{
	kern_return_t kr;
	mach_zone_name_t *name = NULL;
	unsigned int name_count = 0, i;

	/* Get names for zones that have zone logging enabled */
	kr = mach_zone_get_zlog_zones(mach_host_self(), &name, &name_count);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "error: call to mach_zone_get_zlog_zones() failed: %s\n", mach_error_string(kr));
		exit(1);
	}

	if (name_count == 0) {
		printf("zlog not enabled for any zones.\n");
	} else {
		printf("zlog enabled for zones...\n");
	}

	for (i = 0; i < name_count; i++) {
		print_zone_info(name[i].mzn_name);
	}

	if ((name != NULL) && (name_count != 0)) {
		kr = vm_deallocate(mach_task_self(), (vm_address_t) name, (vm_size_t) (name_count * sizeof *name));
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "call to vm_deallocate() failed: %s\n", mach_error_string(kr));
			exit(1);
		}
	}
}

#define VERSION_STRING	"zlog output version: 1"

static void
get_osversion(char *buffer, size_t buffer_len)
{
	int mib[] = {CTL_KERN, KERN_OSVERSION};
	int ret;
	ret = sysctl(mib, sizeof(mib) / sizeof(int), buffer, &buffer_len, NULL, 0);
	if (ret != 0) {
		strlcpy(buffer, "Unknown", buffer_len);
		return;
	}
}

int main(int argc, char *argv[])
{
	int c, topN = 0;
	const char *zone_name = NULL;

	/* Identifier string for SpeedTracer parsing */
	printf("%s\n\n", VERSION_STRING);
	char version_buffer[32] = {0};
	get_osversion(version_buffer, sizeof(version_buffer));
	printf("Collected on build: %s\n\n", version_buffer);

	if (argc == 1) {
		/* default when no arguments are specified */
		list_zones_with_zlog_enabled();
		printf("Run 'zlog -h' for usage info.\n");
		return 0;
	}

	while ((c = getopt(argc, argv, "tz:n:lh")) != -1) {
		switch(c) {
			case 't':
				list_zones_with_zlog_enabled();
				break;
			case 'z':
				zone_name = optarg;
				break;
			case 'n':
				topN = atoi(optarg);
				break;
			case 'l':
				topN = 1;
				break;
			case 'h':
				usage(stdout, argv);
				break;
			case '?':
			default:
				usage(stderr, argv);
				break;
		}
	}

	if (optind < argc) {
		usage(stderr, argv);
	}

	if (zone_name) {
		print_zone_info(zone_name);
		get_zone_btrecords(zone_name, topN);
	} else {
		/* -n or -l was specified without -z */
		if (topN != 0) {
			usage(stderr, argv);
		}
	}

	return 0;
}
