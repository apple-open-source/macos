/*
 * This file is produced automatically.
 * Do not modify anything in here by hand.
 *
 * Created from source file
 *   iconv_converter_if.m
 * with
 *   ../../sys5/kern/makeobjops.pl
 *
 * See the source file for legal information
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#ifdef APPLE
#include <sys/smb_apple.h>
#endif
#include <sys/iconv.h>
#include "iconv_converter_if.h"

struct kobjop_desc iconv_converter_open_desc = {
	0, (kobjop_t) 0
};

struct kobjop_desc iconv_converter_close_desc = {
	0, (kobjop_t) 0
};

struct kobjop_desc iconv_converter_conv_desc = {
	0, (kobjop_t) 0
};

struct kobjop_desc iconv_converter_init_desc = {
	0, (kobjop_t) iconv_converter_initstub
};

struct kobjop_desc iconv_converter_done_desc = {
	0, (kobjop_t) iconv_converter_donestub
};

struct kobjop_desc iconv_converter_name_desc = {
	0, (kobjop_t) 0
};

