/*
   +----------------------------------------------------------------------+
   | PHP version 4.0                                                      |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2001 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Sterling Hughes <sterling@php.net>                          |
   +----------------------------------------------------------------------+
 */
 
/* $Id: bz2.c,v 1.1.1.3 2001/12/14 22:11:59 zarzycki Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_bz2.h"

#if HAVE_BZ2

/* PHP Includes */
#include "ext/standard/file.h"
#include "ext/standard/info.h"

/* for fileno() */
#include <stdio.h>

/* Bzip2 includes */
#include <bzlib.h>

/* Internal error constants */
#define PHP_BZ_ERRNO   0
#define PHP_BZ_ERRSTR  1
#define PHP_BZ_ERRBOTH 2

/* Blocksize of the decompression buffer */
#define PHP_BZ_DECOMPRESS_SIZE 4096

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
	STANDARD_MODULE_HEADER,
	"bz2",
	bz2_functions,
	PHP_MINIT(bz2),
	NULL,
	NULL,
	NULL,
	PHP_MINFO(bz2),
	NO_VERSION_YET,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_BZ2
ZEND_GET_MODULE(bz2)
#endif

static int  le_bz2;

static void php_bz2_close(zend_rsrc_list_entry *rsrc TSRMLS_DC);
static void php_bz2_error(INTERNAL_FUNCTION_PARAMETERS, int);

PHP_MINIT_FUNCTION(bz2)
{
	/* Register the resource, with destructor (arg 1) and text description (arg 3), the 
	   other arguments are just standard placeholders */
	le_bz2 = zend_register_list_destructors_ex(php_bz2_close, NULL, "BZip2", module_number);

	return SUCCESS;
}

PHP_MINFO_FUNCTION(bz2)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "BZip2 Support", "Enabled");
	php_info_print_table_row(2, "BZip2 Version", (char *) BZ2_bzlibVersion());
	php_info_print_table_end();
}

/* {{{ proto int bzopen(string|int file|fp, string mode)
   Open a new BZip2 stream */
PHP_FUNCTION(bzopen)
{
	zval    **file,   /* The file to open */
	        **mode;   /* The mode to open the stream with */
	BZFILE   *bz;     /* The compressed file stream */
	FILE     *fp;     /* If filepointer is given, its placed in here */
	char	*path;
	
	if (ZEND_NUM_ARGS() != 2 ||
	    zend_get_parameters_ex(2, &file, &mode) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string_ex(mode);
	
	/* If it's not a resource its a string containing the filename to open */
	if (Z_TYPE_PP(file) != IS_RESOURCE) {
		convert_to_string_ex(file);

#ifdef VIRTUAL_DIR
	virtual_filepath(Z_STRVAL_PP(file), &path TSRMLS_CC);
#else
	path = Z_STRVAL_PP(file);
#endif  

		bz = BZ2_bzopen(path, Z_STRVAL_PP(mode));
	} 
	/* If it is a resource, than its a 'FILE *' resource */
	else {
		ZEND_FETCH_RESOURCE(fp, FILE *, file, -1, "File Handle", php_file_le_fopen());
		bz = BZ2_bzdopen(fileno(fp), Z_STRVAL_PP(mode));
	}

	if (bz == NULL) {
		php_error(E_WARNING, "bzopen(): Unable to open file");
		RETURN_FALSE;
	}
	
	ZEND_REGISTER_RESOURCE(return_value, bz, le_bz2);
}
/* }}} */

/* {{{ proto string bzread(int bz[, int len])
   Read len bytes from the BZip2 stream given by bz */
PHP_FUNCTION(bzread)
{
	zval      **bzp,                       /* BZip2 Resource Pointer */
	          **zlen;                      /* The (optional) length to read */
	BZFILE     *bz;                        /* BZip2 File pointer */
	void       *buf;                       /* Buffer to read data into */
	int         len  = 1024,               /* Length to read, passed to the BZ2_bzread function */
			    argc = ZEND_NUM_ARGS();    /* Argument count */

	if (argc < 1 || argc > 2 ||
	    zend_get_parameters_ex(argc, &bzp, &zlen) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(bz, BZFILE *, bzp, -1, "BZip2 File Handle", le_bz2);

	/* Optional second argument, the length to read, if not given, it 
	   defaults to 1024 bytes (why not? :) */
	if (argc > 1) {
		convert_to_long_ex(zlen);
		len = Z_LVAL_PP(zlen);
	}

	/* Allocate the buffer and read data into it */
	buf = emalloc(len + 1);
	BZ2_bzread(bz, buf, len);

	RETVAL_STRINGL(buf, len, 1);
	
	/* We copied the buffer, so now we can free it */
	efree(buf);
}
/* }}} */

/* {{{ proto int bzwrite(int bz, string data[, int len])
   Write data to the BZip2 stream given by bz */
PHP_FUNCTION(bzwrite)
{
	zval        **bzp,                        /* Bzip2 Resource Pointer */
	            **data,                       /* The data to write */
				**zlen;                       /* The (optional) length of the data to write */
	BZFILE       *bz;                         /* BZip2 File pointer */
	int           error,                      /* Error container */
	              len,                        /* Length to read, passed to the BZ2_bzwrite function */
				  argc = ZEND_NUM_ARGS();     /* Argument count */
	
	if (argc < 2 || argc > 3 ||
	    zend_get_parameters_ex(argc, &bzp, &data, &zlen) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string_ex(data);
	
	ZEND_FETCH_RESOURCE(bz, BZFILE *, bzp, -1, "BZip2 File Handle", le_bz2);
	
	/* If the length of the data is given, use that, otherwise, just use the
	   data's string length */
	if (argc > 2) {
		convert_to_long_ex(zlen);
		len = Z_LVAL_PP(zlen);
	}
	else { 
		len = Z_STRLEN_PP(data); 
	}
	
	/* Write the data and return the error */
	error = BZ2_bzwrite(bz, (void *) Z_STRVAL_PP(data), len);
	RETURN_LONG(error);
}
/* }}} */

/* {{{ proto int bzflush(int bz)
   Flush a BZip2 stream */
PHP_FUNCTION(bzflush)
{
	zval      **bzp;    /* BZip2 Resource Pointer */
	BZFILE     *bz;     /* BZip2 File pointer */
	int         error;  /* Error container */

	if (ZEND_NUM_ARGS() != 1 ||
	    zend_get_parameters_ex(1, &bzp) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	ZEND_FETCH_RESOURCE(bz, BZFILE *, bzp, -1, "BZip2 File Handle", le_bz2);
	
	error = BZ2_bzflush(bz);
	RETURN_LONG(error);
}
/* }}} */

/* {{{ proto int bzclose(int bz)
   Close a BZip2 stream */
PHP_FUNCTION(bzclose)
{
	zval     **bzp=NULL;  /* BZip2 Resource Pointer */
	BZFILE    *bz;        /* BZip2 File pointer */
	
	if (ZEND_NUM_ARGS() != 1 ||
	    zend_get_parameters_ex(1, &bzp) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	ZEND_FETCH_RESOURCE(bz, BZFILE *, bzp, -1, "BZip2 File Handle", le_bz2);
	zend_list_delete(Z_RESVAL_PP(bzp));
}
/* }}} */

/* {{{ proto int bzerrno(int bz)
   Return the error number */
PHP_FUNCTION(bzerrno)
{
	php_bz2_error(INTERNAL_FUNCTION_PARAM_PASSTHRU, PHP_BZ_ERRNO);
}
/* }}} */

/* {{{ proto string bzerrstr(int bz)
   Return the error string */
PHP_FUNCTION(bzerrstr)
{
	php_bz2_error(INTERNAL_FUNCTION_PARAM_PASSTHRU, PHP_BZ_ERRSTR);
}
/* }}} */

/* {{{ proto array bzerror(int bz)
   Return the error number and error string in an associative array */
PHP_FUNCTION(bzerror)
{
	php_bz2_error(INTERNAL_FUNCTION_PARAM_PASSTHRU, PHP_BZ_ERRBOTH);
}
/* }}} */

/* {{{ proto string bzcompress(string source[, int blockSize100k[, int workFactor]])
   Compress a string into BZip2 encoded data */
PHP_FUNCTION(bzcompress)
{
	zval            **source,                            /* Source data to compress */
	                **zblock_size,                       /* Optional block size to use */
					**zwork_factor;                      /* Optional work factor to use */
	char             *dest = NULL;                       /* Destination to place the compressed data into */
	int               error,                             /* Error Container */
	                  iter        = 1,                   /* Iteration count for the do {} while loop */
					  block_size  = 4,                   /* Block size for compression algorithm */
					  work_factor = 0,                   /* Work factor for compression algorithm */
					  argc        = ZEND_NUM_ARGS();     /* Argument count */
	unsigned int      size,                              /* The size to "realloc" if the initial buffer wasn't big enough */
	                  source_len,                        /* Length of the source data */
					  dest_len;                          /* Length of the destination buffer */ 
	
	if (argc < 1 || argc > 3 || 
	    zend_get_parameters_ex(argc, &source, &zblock_size, &zwork_factor) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string_ex(source);
	
	/* Assign them to easy to use variables, dest_len is initially the length of the data
	   + .01 x length of data + 600 which is the largest size the results of the compression 
	   could possibly be, at least that's what the libbz2 docs say (thanks to jeremy@nirvani.net 
	   for pointing this out).  */
	source_len = Z_STRLEN_PP(source);
	dest_len   = Z_STRLEN_PP(source) + (0.01 * Z_STRLEN_PP(source)) + 600;
	
	/* Allocate the destination buffer */
	dest = emalloc(dest_len + 1);
	
	/* Handle the optional arguments */
	if (argc > 1) {
		convert_to_long_ex(zblock_size);
		block_size = Z_LVAL_PP(zblock_size);
	}
	
	if (argc > 2) {
		convert_to_long_ex(zwork_factor);
		work_factor = Z_LVAL_PP(zwork_factor);
	}

	error = BZ2_bzBuffToBuffCompress(dest, &size, Z_STRVAL_PP(source), source_len, block_size, 0, work_factor);
	if (error != BZ_OK) {
		RETVAL_LONG(error);
	} else {
		/* Copy the buffer, we have perhaps allocate alot more than we need,
		   so we want to copy the correct amount and then free the in-exactly 
		   allocated buffer */
		RETVAL_STRINGL(dest, size, 1);
	}
	
	/* Free the buffer */
	efree(dest);
}
/* }}} */

/* {{{ proto string bzdecompress(string source[, int small])
   Decompress BZip2 compressed data */
PHP_FUNCTION(bzdecompress)
{
	zval    **source,                                     /* Source data to decompress */
	        **zsmall;                                     /* (Optional) user specified small */
	char     *dest   = emalloc(PHP_BZ_DECOMPRESS_SIZE);   /* Destination buffer, initially allocated */
	int       error,                                      /* Error container */
	          iter = 1,                                   /* Iteration count for the compression loop */
			  size,                                       /* Current size to realloc the dest buffer to */
			  dest_len = PHP_BZ_DECOMPRESS_SIZE,          /* Size of the destination length */
			  small    = 0,                               /* The actual small */
			  argc     = ZEND_NUM_ARGS();                 /* Argument count */
	
	if (argc < 1 || argc > 2 ||
	    zend_get_parameters_ex(argc, &source, &zsmall) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string_ex(source);
	
	/* optional small argument handling */
	if (argc > 1) {
		convert_to_long_ex(zsmall);
		small = Z_LVAL_PP(zsmall);
	}
	
	/* (de)Compression Loop */	
	do {
		/* Handle the (re)allocation of the buffer */
		size = dest_len * iter;
		if (iter > 1) {
			dest = erealloc(dest, size);
		}
		iter++;
		
		/* Perform the decompression */
		error = BZ2_bzBuffToBuffDecompress(dest, &size, Z_STRVAL_PP(source), Z_STRLEN_PP(source), small, 0);
	} while (error == BZ_OUTBUFF_FULL);
	
	if (error != BZ_OK) {
		RETVAL_LONG(error);
	} else {
		/* we might have allocated a little to much, so copy the exact size and free the 
		   in-exactly allocated buffer */
		RETVAL_STRINGL(dest, size, 1);
	}
	
	efree(dest);
}
/* }}} */

/* {{{ php_bz2_close() 
   Closes a BZip2 file pointer */
static void php_bz2_close(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	BZFILE *bz = (BZFILE *) rsrc->ptr;  /* The BZip2 File pointer */

	BZ2_bzclose(bz);
}
/* }}} */

/* {{{ php_bz2_error()
   The central error handling interface, does the work for bzerrno, bzerrstr and bzerror */
static void php_bz2_error(INTERNAL_FUNCTION_PARAMETERS, int opt)
{ 
	zval        **bzp;     /* BZip2 Resource Pointer */
	BZFILE       *bz;      /* BZip2 File pointer */
	const char   *errstr;  /* Error string */
	int           errnum;  /* Error number */
	
	if (ZEND_NUM_ARGS() != 1 ||
	    zend_get_parameters_ex(1, &bzp) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	ZEND_FETCH_RESOURCE(bz, BZFILE *, bzp, -1, "BZip2 File pointer", le_bz2);
	
	/* Fetch the error information */
	errstr = BZ2_bzerror(bz, &errnum);
	
	/* Determine what to return */
	switch (opt) {
	case PHP_BZ_ERRNO:
		RETURN_LONG(errnum);
		break;
	case PHP_BZ_ERRSTR:
		RETURN_STRING((char*)errstr, 1);
		break;
	case PHP_BZ_ERRBOTH:
		array_init(return_value);
		
		add_assoc_long  (return_value, "errno",  errnum);
		add_assoc_string(return_value, "errstr", (char*)errstr, 1);
		
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
 * vim600: sw=4 ts=4 tw=78 fdm=marker
 * vim<600: sw=4 ts=4 tw=78
 */
