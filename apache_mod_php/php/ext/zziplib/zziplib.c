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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "php_zziplib.h"

#if HAVE_ZZIPLIB

#include "ext/standard/info.h"
#include <zziplib.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

static int le_zzip_dir;
static int le_zzip_entry;

function_entry zziplib_functions[] = {
	PHP_FE(zzip_opendir,                 NULL)
	PHP_FE(zzip_readdir,                 NULL)
	PHP_FE(zzip_closedir,                NULL)
	PHP_FE(zzip_entry_name,              NULL)
	PHP_FE(zzip_entry_compressedsize,    NULL)
	PHP_FE(zzip_entry_filesize,          NULL)
	PHP_FE(zzip_entry_compressionmethod, NULL)
	PHP_FE(zzip_open,                    NULL)
	PHP_FE(zzip_read,                    NULL)
	PHP_FE(zzip_close,                   NULL)
	{NULL, NULL, NULL}
};

zend_module_entry zziplib_module_entry = {
	"zziplib",
	zziplib_functions,
	PHP_MINIT(zziplib),
	NULL,
	NULL,		
	NULL,
	PHP_MINFO(zziplib),
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_ZZIPLIB
ZEND_GET_MODULE(zziplib)
#endif

static void php_zziplib_free_dir(zend_rsrc_list_entry *rsrc)
{
	ZZIP_DIR *z_dir = (ZZIP_DIR *) rsrc->ptr;
	zzip_closedir(z_dir);
}

static void php_zziplib_free_entry(zend_rsrc_list_entry *rsrc)
{
	php_zzip_dirent *entry = (php_zzip_dirent *) rsrc->ptr;

	if (entry->fp) {
		zzip_close(entry->fp);
	}

	efree(entry);
}

PHP_MINIT_FUNCTION(zziplib)
{
	le_zzip_dir   = zend_register_list_destructors_ex(php_zziplib_free_dir, NULL, "ZZIP Directory", module_number);
	le_zzip_entry = zend_register_list_destructors_ex(php_zziplib_free_entry, NULL, "ZZIP Entry", module_number);

	return(SUCCESS);
}

PHP_MINFO_FUNCTION(zziplib)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "zziplib support", "enabled");
	php_info_print_table_end();

}

/* {{{ proto resource zzip_opendir(string filename)
   Open a new zzip archive for reading */
PHP_FUNCTION(zzip_opendir)
{
	zval **filename;
	ZZIP_DIR *archive_p = NULL;

	if (ZEND_NUM_ARGS() != 1 ||
		zend_get_parameters_ex(1, &filename) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	archive_p = zzip_opendir(Z_STRVAL_PP(filename));
	if (archive_p == NULL) {
		php_error(E_WARNING, "Cannot open zip archive %s", Z_STRVAL_PP(filename));
		RETURN_FALSE;
	}

	ZEND_REGISTER_RESOURCE(return_value, archive_p, le_zzip_dir);
}
/* }}} */

/* {{{ proto resource zzip_readdir(resource zzipp)
   Returns the next file in the archive */
PHP_FUNCTION(zzip_readdir)
{
	zval **zzip_dp;
	ZZIP_DIR *archive_p = NULL;
	php_zzip_dirent *entry = NULL;
	int ret;

	if (ZEND_NUM_ARGS() != 1 ||
		zend_get_parameters_ex(1, &zzip_dp) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(archive_p, ZZIP_DIR *, zzip_dp, -1, "ZZIP Directory", le_zzip_dir);

	entry = (php_zzip_dirent *) emalloc(sizeof(php_zzip_dirent));

	ret = zzip_dir_read(archive_p, &entry->dirent);
	if (ret == 0) {
		efree(entry);
		RETURN_FALSE;
	}

	ZEND_REGISTER_RESOURCE(return_value, entry, le_zzip_entry);
}
/* }}} */

/* {{{ proto void zzip_closedir(resource zzipp)
   Close a Zip archive */
PHP_FUNCTION(zzip_closedir)
{
	zval **zzip_dp;
	ZZIP_DIR *archive_p = NULL;

	if (ZEND_NUM_ARGS() != 1 ||
		zend_get_parameters_ex(1, &zzip_dp) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(archive_p, ZZIP_DIR *, zzip_dp, -1, "ZZIP Directory", le_zzip_dir);

	zend_list_delete(Z_LVAL_PP(zzip_dp));
}
/* }}} */

static void php_zzip_get_entry(INTERNAL_FUNCTION_PARAMETERS, int opt)
{
	zval **zzip_ent;
	php_zzip_dirent *entry = NULL;

	if (ZEND_NUM_ARGS() != 1 ||
		zend_get_parameters_ex(1, &zzip_ent) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(entry, php_zzip_dirent *, zzip_ent, -1, "ZZIP Entry", le_zzip_entry);

	switch (opt) {
	case 0:
		RETURN_STRING(entry->dirent.d_name, 1);
		break;
	case 1:
		RETURN_LONG(entry->dirent.d_csize);
		break;
	case 2:
		RETURN_LONG(entry->dirent.st_size);
		break;
	case 3:
		RETURN_STRING((char *) zzip_compr_str(entry->dirent.d_compr), 1);
		break;
	}
}

/* {{{ proto string zzip_entry_name(resource zzip_entry)
   Return the name given a ZZip entry */
PHP_FUNCTION(zzip_entry_name)
{
	php_zzip_get_entry(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto int zzip_entry_compressedsize(resource zzip_entry)
   Return the compressed size of a ZZip entry */
PHP_FUNCTION(zzip_entry_compressedsize)
{
	php_zzip_get_entry(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ proto int zzip_entry_filesize(resource zzip_entry)
   Return the actual filesize of a ZZip entry */
PHP_FUNCTION(zzip_entry_filesize)
{
	php_zzip_get_entry(INTERNAL_FUNCTION_PARAM_PASSTHRU, 2);
}
/* }}} */

/* {{{ proto string zzip_entry_compressionmethod(resource zzip_entry)
   Return a string containing the compression method used on a particular entry */
PHP_FUNCTION(zzip_entry_compressionmethod)
{
	php_zzip_get_entry(INTERNAL_FUNCTION_PARAM_PASSTHRU, 3);
}
/* }}} */

/* {{{ proto bool zzip_open(resource zzip_dp, resource zzip_entry, string mode)
   Open a Zip File, pointed by the resource entry */
PHP_FUNCTION(zzip_open)
{
	zval **zzip_dp, **zzip_ent, **mode;
	ZZIP_DIR *archive_p = NULL;
	php_zzip_dirent *entry = NULL;

	if (ZEND_NUM_ARGS() != 2 ||
		zend_get_parameters_ex(3, &zzip_dp, &zzip_ent, &mode) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(archive_p, ZZIP_DIR *, zzip_dp, -1, "ZZIP Directory", le_zzip_dir);
	ZEND_FETCH_RESOURCE(entry, php_zzip_dirent *, zzip_dp, -1, "ZZIP Entry", le_zzip_entry);

	entry->fp = zzip_file_open(archive_p, entry->dirent.d_name, O_RDONLY|O_BINARY);

	if (entry->fp) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string zzip_read(resource zzip_ent)
   Read X bytes from an opened zzip entry */
PHP_FUNCTION(zzip_read)
{
	zval **zzip_ent, **length;
	php_zzip_dirent *entry = NULL;
	char *buf = NULL;
	int len = 1024,
		argc = ZEND_NUM_ARGS(),
		ret = 0;

	if (argc < 1 || argc > 2 ||
		zend_get_parameters_ex(argc, &zzip_ent, &length) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(entry, php_zzip_dirent *, zzip_ent, -1, "ZZIP Entry", le_zzip_entry);
	if (argc > 1) {
		convert_to_long_ex(length);
		len = Z_LVAL_PP(length);
	}

	buf = emalloc(len + 1);

	ret = zzip_read(entry->fp, buf, len);
	if (ret == 0) {
		RETURN_FALSE;
	}
	
	RETURN_STRINGL(buf, len, 0);
}
/* }}} */

/* {{{ proto void zzip_close(resource zzip_ent)
   Close a zzip entry */
PHP_FUNCTION(zzip_close)
{
	zval **zzip_ent;
	php_zzip_dirent *entry = NULL;

	if (ZEND_NUM_ARGS() != 1 ||
		zend_get_parameters_ex(1, &zzip_ent) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(entry, php_zzip_dirent *, zzip_ent, -1, "ZZIP Entry", le_zzip_entry);

	zend_list_delete(Z_LVAL_PP(zzip_ent));
}
/* }}} */

#endif	/* HAVE_ZZIPLIB */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
