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
   | Authors: Frank M. Kromann <fmk@swwwing.com>                          |
   +----------------------------------------------------------------------+
*/
/* $Id: setup.c,v 1.1.1.1 2001/01/25 04:59:22 wsanchez Exp $ */

#ifdef COMPILE_DL_IISFUNC
#define HAVE_IISFUNC 1
#endif

#include "php.h"
#include "php_globals.h"
#include "ext/standard/info.h"
#include "setup.h"

#if HAVE_IISFUNC

function_entry iisfunc_functions[] = {
	PHP_FE(iis_getserverbypath,		NULL)
	PHP_FE(iis_getserverbycomment,	NULL)
	PHP_FE(iis_addserver,			NULL)
	PHP_FE(iis_removeserver,		NULL)
	PHP_FE(iis_setdirsecurity,		NULL)
	PHP_FE(iis_getdirsecurity,		NULL)
	PHP_FE(iis_setserverright,		NULL)
	PHP_FE(iis_getserverright,		NULL)
	PHP_FE(iis_startserver,			NULL)
	PHP_FE(iis_stopserver,			NULL)
	PHP_FE(iis_setscriptmap,		NULL)
	PHP_FE(iis_getscriptmap,		NULL)
	PHP_FE(iis_setappsettings,		NULL)
	PHP_FE(iis_stopservice,			NULL)
	PHP_FE(iis_startservice,		NULL)
	PHP_FE(iis_getservicestate,		NULL)
	{NULL, NULL, NULL}
};

zend_module_entry iisfunc_module_entry = {
	"iisfunc", 
	iisfunc_functions, 
	PHP_MINIT(iisfunc), 
	PHP_MSHUTDOWN(iisfunc), 
	PHP_RINIT(iisfunc), 
	PHP_RSHUTDOWN(iisfunc), 
	PHP_MINFO(iisfunc), 
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_IISFUNC
ZEND_GET_MODULE(iisfunc)
#endif

PHP_MINIT_FUNCTION(iisfunc)
{
    REGISTER_LONG_CONSTANT("IIS_READ", 0x1, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("IIS_WRITE", 0x2, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("IIS_EXECUTE", 0x4, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("IIS_SCRIPT", 0x200, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("IIS_ANONYMOUS", 0x1, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("IIS_BASIC", 0x2, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("IIS_NTLM", 0x4, CONST_CS | CONST_PERSISTENT);
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(iisfunc)
{
	return SUCCESS;
}

PHP_RINIT_FUNCTION(iisfunc)
{
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(iisfunc)
{
	return SUCCESS;
}

PHP_MINFO_FUNCTION(iisfunc)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "IIS Admin Functions support", "enabled");

	php_info_print_table_end();
}

/* {{{ proto int iis_GetServerByPath(string Path)
   Return the instance number associated with the Path*/
PHP_FUNCTION(iis_getserverbypath)
{
	pval *ServerPath;
	int argc, rc;

	// Get and check parameters
	argc = ARG_COUNT(ht);

	if (argc != 1 || getParameters(ht, argc, &ServerPath) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_string(ServerPath);

	rc = fnIisGetServerByPath(ServerPath->value.str.val);
	if (rc) {
		RETVAL_LONG(rc);
	}
	else {
		RETVAL_LONG(0);
	}
}
/* }}} */

/* {{{ proto int iis_GetServerByComment(string Comment)
   Return the instance number associated with the Comment*/
PHP_FUNCTION(iis_getserverbycomment)
{
	pval *ServerComment;
	int argc, rc;

	// Get and check parameters
	argc = ARG_COUNT(ht);

	if (argc != 1 || getParameters(ht, argc, &ServerComment) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_string(ServerComment);

	rc = fnIisGetServerByComment(ServerComment->value.str.val);
	if (rc) {
		RETVAL_LONG(rc);
	}
	else {
		RETVAL_LONG(0);
	}
}
/* }}} */

/* {{{ proto int iis_AddServer(string Path, string Comment, string ServerIP, int ServerPort, string HostName, int ServerRights, int StartServer)
   Creates a new virtual web server*/
PHP_FUNCTION(iis_addserver)
{
	pval *ServerPath, *ServerComment, *ServerIp, *ServerPort, 
		 *ServerHost, *ServerRights, *StartServer;
	int argc, rc;

	argc = ARG_COUNT(ht);

	if (argc != 7 || getParameters(ht, argc, &ServerPath, &ServerComment, &ServerIp, &ServerPort, &ServerHost, &ServerRights, &StartServer) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_string(ServerPath);
	convert_to_string(ServerComment);
	convert_to_string(ServerIp);
	convert_to_string(ServerPort);
	convert_to_string(ServerHost);
	convert_to_long(ServerRights);
	convert_to_long(StartServer);

	rc = fnIisAddServer(ServerPath->value.str.val, 
						ServerComment->value.str.val,
						ServerIp->value.str.val,
						ServerPort->value.str.val,
						ServerHost->value.str.val,
						ServerRights->value.lval,
						StartServer->value.lval);
	if (rc) {
		RETVAL_LONG(rc);
	}
	else {
		RETVAL_LONG(0);
	}
}
/* }}} */

/* {{{ proto int iis_RemoveServer(int ServerInstance)
   Removes the virtual web server indicated by ServerInstance*/
PHP_FUNCTION(iis_removeserver)
{
	pval *ServerInstance;
	int argc, rc;

	argc = ARG_COUNT(ht);

	if (argc != 1 || getParameters(ht, argc, &ServerInstance) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(ServerInstance);

	rc = fnIisRemoveServer(ServerInstance->value.lval);
	if (rc) {
		RETVAL_LONG(rc);
	}
	else {
		RETVAL_LONG(0);
	}
}
/* }}} */

/* {{{ proto int iis_SetDirectorySecurity(int ServerInstance, string VirtualPath, int DirectoryFlags)
   Sets Directory Security*/
PHP_FUNCTION(iis_setdirsecurity)
{
	pval *ServerInstance, *VirtualPath, *DirFlags;
	int argc, rc;

	argc = ARG_COUNT(ht);

	if (argc != 3 || getParameters(ht, argc, &ServerInstance, &VirtualPath, &DirFlags) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(ServerInstance);
	convert_to_string(VirtualPath);
	convert_to_long(DirFlags);

	rc = fnIisSetDirSecurity(ServerInstance->value.lval, VirtualPath->value.str.val, DirFlags->value.lval);
	if (rc) {
		RETVAL_LONG(rc);
	}
	else {
		RETVAL_LONG(0);
	}
}
/* }}} */

/* {{{ proto int iis_DirectorySecurity(int ServerInstance, string VirtualPath)
   Gets Directory Security*/
PHP_FUNCTION(iis_getdirsecurity)
{
	pval *ServerInstance, *VirtualPath;
	int argc, rc;
	DWORD DirFlags = 0;

	argc = ARG_COUNT(ht);

	if (argc != 2 || getParameters(ht, argc, &ServerInstance, &VirtualPath) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(ServerInstance);
	convert_to_string(VirtualPath);

	rc = fnIisGetDirSecurity(ServerInstance->value.lval, VirtualPath->value.str.val, &DirFlags);
	if (rc) {
		RETVAL_LONG(DirFlags);
	}
	else {
		RETVAL_LONG(0);
	}
}
/* }}} */

/* {{{ proto int iis_SetServerRights(int ServerInstance, string VirtualPath, int ServerFlags)
   Sets server rights*/
PHP_FUNCTION(iis_setserverright)
{
	pval *ServerInstance, *VirtualPath, *ServerFlags;
	int argc, rc;

	argc = ARG_COUNT(ht);

	if (argc != 3 || getParameters(ht, argc, &ServerInstance, &VirtualPath, &ServerFlags) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(ServerInstance);
	convert_to_string(VirtualPath);
	convert_to_long(ServerFlags);

	rc = fnIisSetServerRight(ServerInstance->value.lval, VirtualPath->value.str.val, ServerFlags->value.lval);
	if (rc) {
		RETVAL_LONG(rc);
	}
	else {
		RETVAL_LONG(0);
	}
}
/* }}} */

/* {{{ proto int iis_GetServerRights(int ServerInstance, string VirtualPath)
   Gets Directory Security*/
PHP_FUNCTION(iis_getserverright)
{
	pval *ServerInstance, *VirtualPath;
	int argc, rc;
	DWORD DirFlags = 0;

	argc = ARG_COUNT(ht);

	if (argc != 2 || getParameters(ht, argc, &ServerInstance, &VirtualPath) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(ServerInstance);
	convert_to_string(VirtualPath);

	rc = fnIisGetServerRight(ServerInstance->value.lval, VirtualPath->value.str.val, &DirFlags);
	if (rc) {
		RETVAL_LONG(DirFlags);
	}
	else {
		RETVAL_LONG(0);
	}
}
/* }}} */

/* {{{ proto int iis_SetScriptMap(int ServerInstance, string VirtualPath, string EnginePath, int AllowScripting)
   Sets script mapping on a virtual directory*/
PHP_FUNCTION(iis_setscriptmap)
{
	pval *ServerInstance, *VirtualPath, *ScriptExtention, *EnginePath, *Scripting;
	int argc, rc;
	char ScriptingValue[256];

	argc = ARG_COUNT(ht);

	if (argc != 5 || getParameters(ht, argc, &ServerInstance, &VirtualPath, &ScriptExtention, &EnginePath, &Scripting) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(ServerInstance);
	convert_to_string(VirtualPath);
	convert_to_string(ScriptExtention);
	convert_to_string(EnginePath);
	convert_to_long(Scripting);

	sprintf(ScriptingValue, "%s,%s,%li", ScriptExtention->value.str.val, EnginePath->value.str.val, Scripting->value.lval);

	rc = fnIisSetScriptMap(ServerInstance->value.lval, VirtualPath->value.str.val, ScriptingValue);
	if (rc) {
		RETVAL_LONG(rc);
	}
	else {
		RETVAL_LONG(0);
	}
}
/* }}} */

/* {{{ proto int iis_GetScriptMap(int ServerInstance, string VirtualPath)
   Gets script mapping on a virtual directory for a specific extention*/
PHP_FUNCTION(iis_getscriptmap)
{
	pval *ServerInstance, *VirtualPath, *ScriptExtention;
	int argc, rc;
	DWORD DirFlags = 0;

	char *strSetting = emalloc(512);
	memset(strSetting, 0, 512);

	argc = ARG_COUNT(ht);

	if (argc != 3 || getParameters(ht, argc, &ServerInstance, &VirtualPath, &ScriptExtention) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(ServerInstance);
	convert_to_string(VirtualPath);
	convert_to_string(ScriptExtention);

	rc = fnIisGetScriptMap(ServerInstance->value.lval, VirtualPath->value.str.val, ScriptExtention->value.str.val, strSetting);
	if (rc == 1 && strlen(strSetting)) {
		RETVAL_STRING(strSetting, 1);
	}
	else {
		RETVAL_STRING("", 1);
	}
	efree(strSetting);
}
/* }}} */

/* {{{ proto int iis_SetAppSetings(int ServerInstance, string VirtualPath, string Name)
   Creates application scope for a virtual directory*/
PHP_FUNCTION(iis_setappsettings)
{
	pval *ServerInstance, *VirtualPath, *Name;
	int argc, rc;

	argc = ARG_COUNT(ht);

	if (argc != 3 || getParameters(ht, argc, &ServerInstance, &VirtualPath, &Name) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(ServerInstance);
	convert_to_string(VirtualPath);
	convert_to_string(Name);

	rc = fnIisSetAppSettings(ServerInstance->value.lval, VirtualPath->value.str.val, Name->value.str.val);
	if (rc) {
		RETVAL_LONG(rc);
	}
	else {
		RETVAL_LONG(0);
	}
}
/* }}} */

/* {{{ proto int iis_StartServer(int ServerInstance)
   Starts the virtual web server*/
PHP_FUNCTION(iis_startserver)
{
	pval *ServerInstance;
	int argc, rc;

	// Get and check parameters
	argc = ARG_COUNT(ht);

	if (argc != 1 || getParameters(ht, argc, &ServerInstance) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(ServerInstance);

	rc = fnIisSetServerStatus(ServerInstance->value.lval, 1);
	if (rc) {
		RETVAL_LONG(rc);
	}
	else {
		RETVAL_LONG(0);
	}
}
/* }}} */

/* {{{ proto int iis_StopServer(int ServerInstance)
   Stops the virtual web server*/
PHP_FUNCTION(iis_stopserver)
{
	pval *ServerInstance;
	int argc, rc;

	// Get and check parameters
	argc = ARG_COUNT(ht);

	if (argc != 1 || getParameters(ht, argc, &ServerInstance) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(ServerInstance);

	rc = fnIisSetServerStatus(ServerInstance->value.lval, 0);
	if (rc) {
		RETVAL_LONG(rc);
	}
	else {
		RETVAL_LONG(0);
	}
}
/* }}} */

/* {{{ proto int iis_StopService(string ServiceId)
   Stops the service defined by ServiceId*/
PHP_FUNCTION(iis_stopservice)
{
	pval *ServiceId;
	int argc, rc;

	// Get and check parameters
	argc = ARG_COUNT(ht);

	if (argc != 1 || getParameters(ht, argc, &ServiceId) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_string(ServiceId);

	rc = fnStopService(ServiceId->value.str.val);
	if (rc) {
		RETVAL_LONG(rc);
	}
	else {
		RETVAL_LONG(0);
	}
}
/* }}} */

/* {{{ proto int iis_StartService(string ServiceId)
   Starts the service defined by ServiceId*/
PHP_FUNCTION(iis_startservice)
{
	pval *ServiceId;
	int argc, rc;

	// Get and check parameters
	argc = ARG_COUNT(ht);

	if (argc != 1 || getParameters(ht, argc, &ServiceId) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_string(ServiceId);

	rc = fnStartService(ServiceId->value.str.val);
	if (rc) {
		RETVAL_LONG(rc);
	}
	else {
		RETVAL_LONG(0);
	}
}
/* }}} */

/* {{{ proto int iis_GetServiceState(string ServiceId)
   Returns teh state for the service defined by ServiceId*/
PHP_FUNCTION(iis_getservicestate)
{
	pval *ServiceId;
	int argc, rc;

	// Get and check parameters
	argc = ARG_COUNT(ht);

	if (argc != 1 || getParameters(ht, argc, &ServiceId) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_string(ServiceId);

	rc = fnGetServiceState(ServiceId->value.str.val);
	if (rc) {
		RETVAL_LONG(rc);
	}
	else {
		RETVAL_LONG(0);
	}
}
/* }}} */

#endif