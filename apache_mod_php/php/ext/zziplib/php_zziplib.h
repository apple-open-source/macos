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

#ifndef PHP_ZZIPLIB_H
#define PHP_ZZIPLIB_H

#if HAVE_ZZIPLIB

#include <zziplib.h>

extern zend_module_entry zziplib_module_entry;
#define phpext_zziplib_ptr &zziplib_module_entry

#ifdef PHP_WIN32
#define PHP_ZZIPLIB_API __declspec(dllexport)
#else
#define PHP_ZZIPLIB_API
#endif

PHP_MINIT_FUNCTION(zziplib);
PHP_MINFO_FUNCTION(zziplib);

PHP_FUNCTION(zzip_opendir);
PHP_FUNCTION(zzip_readdir);
PHP_FUNCTION(zzip_closedir);
PHP_FUNCTION(zzip_entry_name);
PHP_FUNCTION(zzip_entry_compressedsize);
PHP_FUNCTION(zzip_entry_filesize);
PHP_FUNCTION(zzip_entry_compressionmethod);
PHP_FUNCTION(zzip_open);
PHP_FUNCTION(zzip_read);
PHP_FUNCTION(zzip_close);

typedef struct {
	ZZIP_FILE *fp;
	ZZIP_DIRENT dirent;
} php_zzip_dirent;

#else
#define phpext_zziplib_ptr NULL
#endif

#endif	/* PHP_ZZIPLIB_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
