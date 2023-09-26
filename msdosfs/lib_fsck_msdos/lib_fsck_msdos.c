//
//  lib_fsck_msdos.c
//  fsck_msdos
//
//  Created by Kujan Lauz on 25/08/2022.
//

#include "lib_fsck_msdos.h"

/** Struct containing the preferenes on running fsck_msdos */
typedef struct {
	int alwaysno;       /* assume "no" for all questions */
	int alwaysyes;      /* assume "yes" for all questions */
	int preen;          /* set when preening */
	int quick;          /* set to quickly check if volume is dirty */
	int quiet;          /* set to supress most messages */
	int rdonly;         /* device is opened read only (supersedes above) */
	size_t maxmem;      /* If non-zero, limit major allocations to this many bytes */
	const char *dev;    /* Device name */
	int fd;             /* File descriptor */
} fsck_state_t;

fsck_state_t fsck_state;
lib_fsck_ctx_t fsck_ctx;

void fsck_set_context_properties(fsck_msdos_print_funct_t print,
								 fsck_msdos_ask_func_t ask,
								 fsck_client_ctx_t client)
{
	fsck_ctx.print = print;
	fsck_ctx.ask = ask;
	fsck_ctx.client_ctx = client;
	fsck_state.fd = -1;
}

void fsck_set_alwaysyes(bool alwaysyes)
{
	fsck_state.alwaysyes = alwaysyes;
}

bool fsck_alwaysyes(void)
{
	return fsck_state.alwaysyes;
}

void fsck_set_alwaysno(bool alwaysno)
{
	fsck_state.alwaysno = alwaysno;
}

bool fsck_alwaysno(void)
{
	return fsck_state.alwaysno;
}

void fsck_set_preen(bool preen)
{
	fsck_state.preen = preen;
}

bool fsck_preen(void)
{
	return fsck_state.preen;
}

void fsck_set_quick(bool quick)
{
	fsck_state.quick = quick;
}
bool fsck_quick(void)
{
	return fsck_state.quick;
}

void fsck_set_quiet(bool quiet)
{
	fsck_state.quiet = quiet;
}
bool fsck_quiet(void)
{
	return fsck_state.quiet;
}

void fsck_set_rdonly(bool rdonly)
{
	fsck_state.rdonly = rdonly;
}
bool fsck_rdonly(void)
{
	return fsck_state.rdonly;
}

void fsck_set_maxmem(size_t maxmem)
{
	fsck_state.maxmem = maxmem;
}

size_t fsck_maxmem(void)
{
	return fsck_state.maxmem;
}

void fsck_set_dev(const char* dev)
{
	fsck_state.dev = dev;
}

const char* fsck_dev(void)
{
	return fsck_state.dev;
}

void fsck_set_fd(int fd)
{
	fsck_state.fd = fd;
}
int fsck_fd(void)
{
	return fsck_state.fd;
}
