/*
   +----------------------------------------------------------------------+
   | PHP version 4.0                                                      |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997, 1998, 1999, 2000 The PHP Group                   |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Sterling Hughes <Sterling.Hughes@pentap.net>                |
   +----------------------------------------------------------------------+
 */
 
/* $Id: bz2.c,v 1.1.1.1 2001/01/25 04:59:06 wsanchez Exp $ */

 
#include "php.h"
#include "php_bz2.h"

#if HAVE_BZ2

/* PHP Includes */
#include "ext/standard/file.h"
#include "ext/standard/info.h"

/* Std includes */
#include <stdio.h>

/* Bzip2 includes */
#include <bzlib.h>

function_entry bz2_functions[] = {
	PHP_FE(bzopen,       NULL)
	PHP_FE(bzread,       NULL)
	PHP_FE(bzwrite,      NULL)
	PHP_FE(bzflush,      NULL)
	PHP_FE(bzclose,      NULL)
	PHP_FE(bzerrno,      NULL)
	PHP_FE(bzerrstr,     NULL)
	PHP_FE(bzerror,      NULL)
	PHP_FE(bzcompress,   NULL)
	PHP_FE(bzdecompress, NULL)
	{NULL, NULL, NULL}
};

zend_module_entry bz2_module_entry = {
	"bz2",
	bz2_functions,
	PHP_MINIT(bz2),
	NULL,
	NULL,	
	NULL,
	PHP_MINFO(bz2),
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_BZ2
ZEND_GET_MODULE(bz2)
#endif

static void   _php_bz2_close_handle(zend_rsrc_list_entry *rsrc);
static void   _php_bz2_error_machine(INTERNAL_FUNCTION_PARAMETERS, int);

static int le_bz2;

PHP_MINIT_FUNCTION(bz2)
{
	le_bz2 = zend_register_list_destructors_ex(_php_bz2_close_handle, NULL, "bzip2", module_number);

	return SUCCESS;
}

PHP_MINFO_FUNCTION(bz2)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "BZip2 support", "enabled");
	php_info_print_table_row(2, "BZip2 Version", (char *)BZ2_bzlibVersion());
	php_info_print_table_end();

}

/* {{{ proto int bzopen(string|int file|fp, string mode)
   Open a new BZip2 stream */
PHP_FUNCTION(bzopen)
{
	zval **File,
	     **Mode;
	BZFILE *bz;
	
	if (ZEND_NUM_ARGS() != 2 ||
	    zend_get_parameters_ex(2, &File, &Mode) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string_ex(Mode);
	
	if (Z_TYPE_PP(File) != IS_RESOURCE) {
		convert_to_string_ex(File);
		
		bz = BZ2_bzopen(Z_STRVAL_PP(File), Z_STRVAL_PP(Mode));
	} else {
		FILE *fp;
		ZEND_FETCH_RESOURCE(fp, FILE *, File, -1, "File-Handle", php_file_le_fopen());
		
		bz = BZ2_bzdopen(fileno(fp), Z_STRVAL_PP(Mode));
	}
	
	ZEND_REGISTER_RESOURCE(return_value, bz, le_bz2);
}
/* }}} */

/* {{{ proto string bzread(int bz[, int len])
   Read len bytes from the BZip2 stream given by bz */
PHP_FUNCTION(bzread)
{
	zval **Bz,
	     **Len;
	BZFILE *bz;
	void *buf = NULL;
	int ret,
	    len  = 1024,
		argc = ZEND_NUM_ARGS();
	
	if (argc < 1 || argc > 2 ||
	    zend_get_parameters_ex(argc, &Bz, &Len) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	ZEND_FETCH_RESOURCE(bz, BZFILE *, Bz, -1, "BZFile", le_bz2);
	
	if (argc > 1) {
		convert_to_long_ex(Len);
		len = Z_LVAL_PP(Len);
	}
	
	buf = emalloc(len + 1);
	BZ2_bzread(bz, buf, len);
	
	RETURN_STRING((char *)buf, 1);
}
/* }}} */

/* {{{ proto int bzwrite(int bz, string data[, int len])
   Write data to the BZip2 stream given by bz */
PHP_FUNCTION(bzwrite)
{
	zval **Bz,
	     **Data,
		 **Len;
	BZFILE *bz;
	int ret,
	    len,
		argc = ZEND_NUM_ARGS();
	
	if (argc < 2 || argc > 3 ||
	    zend_get_parameters_ex(argc, &Bz, &Data, &Len) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string_ex(Data);

	ZEND_FETCH_RESOURCE(bz, BZFILE *, Bz, -1, "BZFile", le_bz2);

	if (argc > 2) {
		convert_to_long_ex(Len);
		len = Z_LVAL_PP(Len);
	} else {
		len = Z_STRLEN_PP(Data);
	}
	
	ret = BZ2_bzwrite(bz, (void *)Z_STRVAL_PP(Data), len);
	RETURN_LONG(ret);
}
/* }}} */

/* {{{ proto int bzflush(int bz)
   Flush a BZip2 stream */
PHP_FUNCTION(bzflush)
{
	zval **Bz;
	BZFILE *bz;
	int ret;
	
	if (ZEND_NUM_ARGS() != 1 ||
	    zend_get_parameters_ex(1, &Bz) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	ZEND_FETCH_RESOURCE(bz, BZFILE *, Bz, -1, "BZFile", le_bz2);

	ret = BZ2_bzflush(bz);
	RETURN_LONG(ret);
}
/* }}} */

/* {{{ proto int bzclose(int bz)
   Close a BZip2 stream */
PHP_FUNCTION(bzclose)
{
	zval **Bz;
	BZFILE *bz;
	
	if (ZEND_NUM_ARGS() != 1 ||
	    zend_get_parameters_ex(1, &Bz) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	ZEND_FETCH_RESOURCE(bz, BZFILE *, Bz, -1, "BZFile", le_bz2);
	zend_list_delete(Z_LVAL_PP(Bz));
}
/* }}} */

/* {{{ proto int bzerrno(int bz)
   Return the error number */
PHP_FUNCTION(bzerrno)
{
	_php_bz2_error_machine(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto string bzerrstr(int bz)
   Return the error string */
PHP_FUNCTION(bzerrstr)
{
	_php_bz2_error_machine(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ proto array bzerror(int bz)
   Return the error number and error string in an associative array */
PHP_FUNCTION(bzerror)
{
	_php_bz2_error_machine(INTERNAL_FUNCTION_PARAM_PASSTHRU, 2);
}
/* }}} */

/* {{{ proto string bzcompress(string source[, int blockSize100k[, int workFactor]])
   Compress a string into BZip2 encoded data */
PHP_FUNCTION(bzcompress)
{
	zval **Source,
	     **BlockSize100k,
		 **WorkFactor;
	char *dest = NULL;
	int ret,
		iter = 1, 
	    blockSize100k = 4, 
		workFactor = 0, 
		argc = ZEND_NUM_ARGS();
	unsigned int size, 
	             destLen, 
				 sourceLen;
	
	if (argc < 1 || argc > 3 ||
	    zend_get_parameters_ex(argc, &Source, &BlockSize100k, &WorkFactor) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string_ex(Source);
	sourceLen = destLen = Z_STRLEN_PP(Source);

	dest = emalloc(destLen+1);

	if (argc > 1) {
		convert_to_long_ex(BlockSize100k);
		blockSize100k = Z_LVAL_PP(BlockSize100k);
	}
	
	if (argc > 2) {
		convert_to_long_ex(WorkFactor);
		workFactor = Z_LVAL_PP(WorkFactor);
	}

	do {
		size = destLen * iter;
		if (iter > 1)
			dest = erealloc(dest, size);
		
		ret = BZ2_bzBuffToBuffCompress(dest, &size, Z_STRVAL_PP(Source), sourceLen, blockSize100k, 0, workFactor);
		iter++;
	} while (ret == BZ_OUTBUFF_FULL);
	
	if (ret != BZ_OK) {
		RETURN_LONG(ret);
	} else {
		RETURN_STRINGL(dest, destLen, 1);
	}
}
/* }}} */

#define _PHP_BZ_DECOMPRESS_SIZE 4096

/* {{{ proto string bzdecompress(string source[, int small])
   Decompress BZip2 compressed data */
PHP_FUNCTION(bzdecompress)
{
	zval **Source,
	     **Small;
	char *dest = emalloc(_PHP_BZ_DECOMPRESS_SIZE),
	     *source = NULL;
	int ret,
	    iter = 1,
		size,
	    destLen = _PHP_BZ_DECOMPRESS_SIZE,
	    small = 0,
		argc  = ZEND_NUM_ARGS();
	
	if (argc < 1 || argc > 2 ||
	    zend_get_parameters_ex(argc, &Source, &Small) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string_ex(Source);
	source = estrndup(Z_STRVAL_PP(Source), Z_STRLEN_PP(Source));
	
	if (argc > 1) {
		convert_to_long_ex(Small);
		small = Z_LVAL_PP(Small);
	}


	do {
		size = destLen * iter;
		if (iter > 1)
			dest = erealloc(dest, size);

		ret = BZ2_bzBuffToBuffDecompress(dest, &size, source, Z_STRLEN_PP(Source), small, 0);
		iter++;
	} while (ret == BZ_OUTBUFF_FULL);

	if (ret != BZ_OK) {
		RETURN_LONG(ret);
	} else {
		RETURN_STRINGL(dest, size, 0);
	}
}
/* }}} */

/* {{{ _php_bz2_close_handle() */
static void _php_bz2_close_handle(zend_rsrc_list_entry *rsrc)
{
	BZFILE *bz = (BZFILE *)rsrc->ptr;
	BZ2_bzclose(bz);
}
/* }}} */

/* {{{ _php_bz2_error_machine() */
static void _php_bz2_error_machine(INTERNAL_FUNCTION_PARAMETERS, int opt)
{
	zval **u_bz;
	BZFILE *bz;
	int errnum;
	const char *errstr;
	
	if (ZEND_NUM_ARGS() != 1 ||
	    zend_get_parameters_ex(1, &u_bz) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	ZEND_FETCH_RESOURCE(bz, BZFILE *, u_bz, -1, "BZFile", le_bz2);
	
	errstr = BZ2_bzerror(bz, &errnum);
	
	switch (opt) {
		case 0:
			RETURN_LONG(errnum);
			break;
		case 1:
			RETURN_STRING((char *)errstr, 1);
			break;
		case 2:
			if (array_init(return_value) == FAILURE) {
				php_error(E_WARNING, "Cannot initialize return value from bzerror()");
				RETURN_NULL();
			}
			
			add_assoc_long(return_value, "errno", errnum);
			add_assoc_string(return_value, "errstr", (char *)errstr, 1);
			break;
	}
}
/* }}} */

#endif


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
