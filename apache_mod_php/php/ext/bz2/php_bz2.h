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

#ifndef PHP_BZ2_H
#define PHP_BZ2_H

#if HAVE_BZ2

extern zend_module_entry bz2_module_entry;
#define phpext_bz2_ptr &bz2_module_entry

#ifdef PHP_WIN32
#define PHP_BZ2_API __declspec(dllexport)
#else
#define PHP_BZ2_API
#endif

PHP_MINIT_FUNCTION(bz2);
PHP_MINFO_FUNCTION(bz2);
PHP_FUNCTION(bzopen);
PHP_FUNCTION(bzread);
PHP_FUNCTION(bzwrite);
PHP_FUNCTION(bzflush);
PHP_FUNCTION(bzclose);
PHP_FUNCTION(bzerrno);
PHP_FUNCTION(bzerrstr);
PHP_FUNCTION(bzerror);
PHP_FUNCTION(bzcompress);
PHP_FUNCTION(bzdecompress);

#else
#define phpext_bz2_ptr NULL
#endif

#endif


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
