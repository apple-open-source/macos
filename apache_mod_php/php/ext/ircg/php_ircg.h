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

#ifndef PHP_IRCG_H
#define PHP_IRCG_H

extern zend_module_entry ircg_module_entry;
#define phpext_ircg_ptr &ircg_module_entry

#ifdef PHP_WIN32
#define PHP_IRCG_API __declspec(dllexport)
#else
#define PHP_IRCG_API
#endif

PHP_FUNCTION(ircg_pconnect);
PHP_FUNCTION(ircg_join);
PHP_FUNCTION(ircg_set_current);
PHP_FUNCTION(ircg_part);
PHP_FUNCTION(ircg_register_current_conn);
PHP_FUNCTION(ircg_msg);
PHP_FUNCTION(ircg_disconnect);

PHP_MINIT_FUNCTION(ircg);
PHP_MSHUTDOWN_FUNCTION(ircg);
PHP_RINIT_FUNCTION(ircg);
PHP_RSHUTDOWN_FUNCTION(ircg);
PHP_MINFO_FUNCTION(ircg);

PHP_FUNCTION(confirm_ircg_compiled);	/* For testing, remove later. */

/* 
  	Declare any global variables you may need between the BEGIN
	and END macros here:     

ZEND_BEGIN_MODULE_GLOBALS(ircg)
	int global_variable;
ZEND_END_MODULE_GLOBALS(ircg)
*/

/* In every function that needs to use variables in php_ircg_globals,
   do call IRCGLS_FETCH(); after declaring other variables used by
   that function, and always refer to them as IRCGG(variable).
   You are encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define IRCGG(v) (ircg_globals->v)
#define IRCGLS_FETCH() php_ircg_globals *ircg_globals = ts_resource(ircg_globals_id)
#else
#define IRCGG(v) (ircg_globals.v)
#define IRCGLS_FETCH()
#endif

#endif	/* PHP_IRCG_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
