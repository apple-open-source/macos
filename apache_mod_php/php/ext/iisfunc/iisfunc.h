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
/* $Id: iisfunc.h,v 1.1.1.2 2001/07/19 00:19:15 zarzycki Exp $ */

#include <iiscnfg.h>  // MD_ & IIS_MD_ #defines 

#ifdef __cplusplus
extern "C" 
{
#endif

#ifdef IISFUNC_EXPORTS
#define IISFUNC_API __declspec(dllexport)
#else
#define IISFUNC_API __declspec(dllimport)
#endif

extern "C" IISFUNC_API int fnIisGetServerByPath(char * ServerPath);
extern "C" IISFUNC_API int fnIisGetServerByComment(char * ServerComment);

extern "C" IISFUNC_API int fnIisAddServer(char * ServerPath, char * ServerComment, char * ServerIp, char * ServerPort, char * ServerHost, DWORD ServerRights, DWORD StartServer);
extern "C" IISFUNC_API int fnIisSetServerRight(DWORD ServerInstance, char * VirtualPath, DWORD ServerRights);
extern "C" IISFUNC_API int fnIisSetDirSecurity(DWORD ServerInstance, char * VirtualPath, DWORD DirFlags);

extern "C" IISFUNC_API int fnStopService(LPCTSTR ServiceId);
extern "C" IISFUNC_API int fnStartService(LPCTSTR ServiceId);
extern "C" IISFUNC_API int fnGetServiceState(LPCTSTR ServiceId);

#ifdef __cplusplus 
}
#endif
