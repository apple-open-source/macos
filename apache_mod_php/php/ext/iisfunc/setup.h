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
   | Authors: Frank M. Kromann <fmk@swwwing.com>                          |
   +----------------------------------------------------------------------+
*/
/* $Id: setup.h,v 1.1.1.2 2001/07/19 00:19:15 zarzycki Exp $ */

#ifndef PHP_IIS_SETUP_H
#define PHP_IIS_SETUP_H


#if HAVE_IISFUNC
#ifdef PHP_WIN32
#define PHP_IISFUNC_API __declspec(dllexport)
#else
#define PHP_IISFUNC_API
#endif

#ifdef __ZTS
#include "TSRM.h"
#endif

extern zend_module_entry iisfunc_module_entry;
#define iisfunc_module_ptr &iisfunc_module_entry

extern PHP_MINIT_FUNCTION(iisfunc);
extern PHP_MSHUTDOWN_FUNCTION(iisfunc);
extern PHP_RINIT_FUNCTION(iisfunc);
extern PHP_RSHUTDOWN_FUNCTION(iisfunc);
PHP_MINFO_FUNCTION(iisfunc);

PHP_FUNCTION(iis_getserverbypath);
PHP_FUNCTION(iis_getserverbycomment);
PHP_FUNCTION(iis_addserver);
PHP_FUNCTION(iis_removeserver);
PHP_FUNCTION(iis_setdirsecurity);
PHP_FUNCTION(iis_getdirsecurity);
PHP_FUNCTION(iis_setserverright);
PHP_FUNCTION(iis_getserverright);
PHP_FUNCTION(iis_startserver);
PHP_FUNCTION(iis_stopserver);
PHP_FUNCTION(iis_setscriptmap);
PHP_FUNCTION(iis_getscriptmap);
PHP_FUNCTION(iis_setappsettings);
PHP_FUNCTION(iis_stopservice);
PHP_FUNCTION(iis_startservice);
PHP_FUNCTION(iis_getservicestate);

int fnIisGetServerByPath(char * ServerPath);
int fnIisGetServerByComment(char * ServerComment);
int fnIisAddServer(char * ServerPath, char * ServerComment, char * ServerIp, char * ServerPort, char * ServerHost, DWORD ServerRights, DWORD StartServer);
int fnIisRemoveServer(DWORD ServerInstance);
int fnIisSetDirSecurity(DWORD ServerInstance, char * VirtualPath, DWORD DirFlags);
int fnIisGetDirSecurity(DWORD ServerInstance, char * VirtualPath, DWORD * DirFlags);
int fnIisSetServerRight(DWORD ServerInstance, char * VirtualPath, DWORD ServerRights);
int fnIisGetServerRight(DWORD ServerInstance, char * VirtualPath, DWORD * ServerRights);
int fnIisSetServerStatus(DWORD ServerInstance, DWORD StartServer);
int fnIisSetScriptMap(DWORD ServerInstance, char * VirtualPath, char * ScriptMap);
int fnIisGetScriptMap(DWORD ServerInstance, char * VirtualPath, char * SeciptExtention, char * ReturnValue);
int fnIisSetAppSettings(DWORD ServerInstance, char * VirtualPath, char * AppName);
int fnStopService(LPSTR ServiceId);
int fnStartService(LPTSTR ServiceId);
int fnGetServiceState(LPTSTR ServiceId);

#else

#define iisfunc_module_ptr NULL

#endif /* HAVE_IISFUNC */

#define phpext_iisfunc_ptr iisfunc_module_ptr

#endif
