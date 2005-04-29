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
#include <sys/smb_apple.h>
#include <sys/smb_iconv.h>
#include "iconv_converter_if.h"

PRIVSYM struct kobjop_desc iconv_converter_open_desc = {
	0, (kobjop_t) 0
};

PRIVSYM struct kobjop_desc iconv_converter_close_desc = {
	0, (kobjop_t) 0
};

PRIVSYM struct kobjop_desc iconv_converter_conv_desc = {
	0, (kobjop_t) 0
};

PRIVSYM struct kobjop_desc iconv_converter_init_desc = {
	0, (kobjop_t) iconv_converter_initstub
};

PRIVSYM struct kobjop_desc iconv_converter_done_desc = {
	0, (kobjop_t) iconv_converter_donestub
};

PRIVSYM struct kobjop_desc iconv_converter_name_desc = {
	0, (kobjop_t) 0
};

