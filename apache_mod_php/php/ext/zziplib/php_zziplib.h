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
   | Authors:                                                             |
   |                                                                      |
   +----------------------------------------------------------------------+
 */

#ifndef PHP_ZZIPLIB_H
#define PHP_ZZIPLIB_H

/* You should tweak config.m4 so this symbol (or some else suitable)
   gets defined.
*/
#if HAVE_ZZIPLIB

extern zend_module_entry zziplib_module_entry;
#define phpext_zziplib_ptr &zziplib_module_entry

#ifdef PHP_WIN32
#define PHP_ZZIPLIB_API __declspec(dllexport)
#else
#define PHP_ZZIPLIB_API
#endif

PHP_MINIT_FUNCTION(zziplib);
PHP_MSHUTDOWN_FUNCTION(zziplib);
PHP_RINIT_FUNCTION(zziplib);
PHP_RSHUTDOWN_FUNCTION(zziplib);
PHP_MINFO_FUNCTION(zziplib);

PHP_FUNCTION(confirm_zziplib_compiled);	/* For testing, remove later. */

/* 
  	Declare any global variables you may need between the BEGIN
	and END macros here:     

ZEND_BEGIN_MODULE_GLOBALS(zziplib)
	int global_variable;
ZEND_END_MODULE_GLOBALS(zziplib)
*/

/* In every function that needs to use variables in php_zziplib_globals,
   do call ZZIPLIBLS_FETCH(); after declaring other variables used by
   that function, and always refer to them as ZZIPLIBG(variable).
   You are encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define ZZIPLIBG(v) (zziplib_globals->v)
#define ZZIPLIBLS_FETCH() php_zziplib_globals *zziplib_globals = ts_resource(zziplib_globals_id)
#else
#define ZZIPLIBG(v) (zziplib_globals.v)
#define ZZIPLIBLS_FETCH()
#endif

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
