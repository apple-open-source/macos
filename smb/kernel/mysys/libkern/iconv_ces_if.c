/*
 * This file is produced automatically.
 * Do not modify anything in here by hand.
 *
 * Created from source file
 *   iconv_ces_if.m
 * with
 *   ../../sys5/kern/makeobjops.pl
 *
 * See the source file for legal information
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/sysctl.h>
#ifdef APPLE
#include <sys/smb_apple.h>
#endif
#include <sys/iconv.h>
#include "iconv_ces_if.h"

struct kobjop_desc iconv_ces_open_desc = {
	0, (kobjop_t) 0
};

struct kobjop_desc iconv_ces_close_desc = {
	0, (kobjop_t) 0
};

struct kobjop_desc iconv_ces_reset_desc = {
	0, (kobjop_t) iconv_ces_noreset
};

struct kobjop_desc iconv_ces_names_desc = {
	0, (kobjop_t) 0
};

struct kobjop_desc iconv_ces_nbits_desc = {
	0, (kobjop_t) 0
};

struct kobjop_desc iconv_ces_nbytes_desc = {
	0, (kobjop_t) 0
};

struct kobjop_desc iconv_ces_fromucs_desc = {
	0, (kobjop_t) 0
};

struct kobjop_desc iconv_ces_toucs_desc = {
	0, (kobjop_t) 0
};

struct kobjop_desc iconv_ces_init_desc = {
	0, (kobjop_t) iconv_ces_initstub
};

struct kobjop_desc iconv_ces_done_desc = {
	0, (kobjop_t) iconv_ces_donestub
};

