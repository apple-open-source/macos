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
/* $Id: iisfunc.cpp,v 1.1.1.2 2001/07/19 00:19:15 zarzycki Exp $ */
/*
	iisfunc.cpp : Defines the entry point for the DLL application.
*/
#define UNICODE
#define INITGUID

#include <windows.h>
#include <stdio.h>
#define _ATL_DLL_IMPL
#include <ks.h>
#include <atlbase.h>  // ATL support 
#include <iadmw.h>    // COM Interface header 
#include "iisfunc.h"

extern "C" IISFUNC_API int fnIisGetServerByPath(char * ServerPath)
{
	HRESULT hRes = 0, hResSub = 0;
	DWORD buffer = 0; 
	DWORD dwBuffer = 65535;
	METADATA_HANDLE MyHandle; 
	METADATA_RECORD MyRecord; 
	PBYTE pbBuffer = new BYTE[dwBuffer];
	WCHAR SubKeyName[METADATA_MAX_NAME_LEN]; 
	WCHAR KeyName[METADATA_MAX_NAME_LEN]; 
	WCHAR TestKey[METADATA_MAX_NAME_LEN]; 
	CComPtr <IMSAdminBase> pIMeta; 
	DWORD indx = 0;

	CoInitialize(NULL);
	hRes = CoCreateInstance(CLSID_MSAdminBase, NULL, CLSCTX_ALL, IID_IMSAdminBase, (void **) &pIMeta);  

	if (FAILED(hRes))     
	return hRes;  

	hRes = pIMeta->OpenKey(METADATA_MASTER_ROOT_HANDLE, L"/LM/W3SVC", METADATA_PERMISSION_READ, 20, &MyHandle); 
	while (SUCCEEDED(hRes)){  
		hRes = pIMeta->EnumKeys(MyHandle, L"/", SubKeyName, indx);  
		if (SUCCEEDED(hRes)) {
			MyRecord.dwMDIdentifier = MD_VR_PATH;
			MyRecord.dwMDAttributes = METADATA_INHERIT;
			MyRecord.dwMDUserType = IIS_MD_UT_SERVER;
			MyRecord.dwMDDataType = ALL_METADATA;
			MyRecord.dwMDDataLen = dwBuffer;
			MyRecord.pbMDData = pbBuffer;
			swprintf(KeyName, L"/%s/ROOT", SubKeyName);
			hResSub = pIMeta->GetData(MyHandle, KeyName, &MyRecord, &buffer);  
			if (SUCCEEDED(hResSub)) { 
				memset(KeyName , 0, sizeof(KeyName));
				swprintf(KeyName, L"%s", MyRecord.pbMDData);
				memset(TestKey , 0, sizeof(TestKey));
				swprintf(TestKey, L"%S", ServerPath);
				if (wcscmp(_wcsupr(KeyName),_wcsupr(TestKey))==0)
					return wcstol(SubKeyName,NULL,10);
			}  
		}
		indx++; 
	}
	pIMeta->CloseKey(MyHandle); 
	return 0;
}

extern "C" IISFUNC_API int fnIisGetServerByComment(char * ServerComment)
{
	HRESULT hRes = 0, hResSub = 0;
	DWORD buffer = 0; 
	DWORD dwBuffer = 65535;
	METADATA_HANDLE MyHandle; 
	METADATA_RECORD MyRecord; 
	PBYTE pbBuffer = new BYTE[dwBuffer];
	WCHAR SubKeyName[METADATA_MAX_NAME_LEN]; 
	WCHAR KeyName[METADATA_MAX_NAME_LEN]; 
	WCHAR TestKey[METADATA_MAX_NAME_LEN]; 
	CComPtr <IMSAdminBase> pIMeta; 
	DWORD indx = 0;

	CoInitialize(NULL);
	hRes = CoCreateInstance(CLSID_MSAdminBase, NULL, CLSCTX_ALL, IID_IMSAdminBase, (void **) &pIMeta);  

	if (FAILED(hRes))     
	return hRes;  

	hRes = pIMeta->OpenKey(METADATA_MASTER_ROOT_HANDLE, L"/LM/W3SVC", METADATA_PERMISSION_READ, 20, &MyHandle); 
	while (SUCCEEDED(hRes)){  
		hRes = pIMeta->EnumKeys(MyHandle, L"/", SubKeyName, indx);  
		if (SUCCEEDED(hRes)) {
			MyRecord.dwMDIdentifier = MD_SERVER_COMMENT;
			MyRecord.dwMDAttributes = METADATA_INHERIT;
			MyRecord.dwMDUserType = IIS_MD_UT_SERVER;
			MyRecord.dwMDDataType = ALL_METADATA;
			MyRecord.dwMDDataLen = dwBuffer;
			MyRecord.pbMDData = pbBuffer;
			swprintf(KeyName, L"/%s/ROOT", SubKeyName);
			hResSub = pIMeta->GetData(MyHandle, KeyName, &MyRecord, &buffer);  
			if (SUCCEEDED(hResSub)) { 
				memset(KeyName , 0, sizeof(KeyName));
				swprintf(KeyName, L"%s", MyRecord.pbMDData);
				memset(TestKey , 0, sizeof(TestKey));
				swprintf(TestKey, L"%S", ServerComment);
				if (wcscmp(_wcsupr(KeyName),_wcsupr(TestKey))==0)
					return wcstol(SubKeyName,NULL,10);
			}  
		}
		indx++; 
	}
	pIMeta->CloseKey(MyHandle); 
	return 0;
}

extern "C" IISFUNC_API int fnIisAddServer(char * ServerPath, char * ServerComment, char * ServerIp, char * ServerPort, char * ServerHost, DWORD ServerRights, DWORD StartServer)
{
	HRESULT hRes = 0;
	DWORD dwBuffer = 65535;
	METADATA_HANDLE MyHandle; 
	METADATA_RECORD MyRecord; 
	PBYTE pbBuffer = new BYTE[dwBuffer];
	WCHAR SubKeyName[METADATA_MAX_NAME_LEN]; 
	WCHAR KeyName[METADATA_MAX_NAME_LEN]; 
	WCHAR TestKey[METADATA_MAX_NAME_LEN]; 
	DWORD ServerInstance;
	CComPtr <IMSAdminBase> pIMeta; 
	DWORD indx = 0;

	CoInitialize(NULL);
	hRes = CoCreateInstance(CLSID_MSAdminBase, NULL, CLSCTX_ALL, IID_IMSAdminBase, (void **) &pIMeta);  

	if (FAILED(hRes))     
	return hRes;  

	swprintf(TestKey,L"1");
	hRes = pIMeta->OpenKey(METADATA_MASTER_ROOT_HANDLE, L"/LM/W3SVC", METADATA_PERMISSION_READ, 20, &MyHandle); 
	while (SUCCEEDED(hRes)){  
		hRes = pIMeta->EnumKeys(MyHandle, L"/", SubKeyName, indx);  
		if (SUCCEEDED(hRes))
			if (wcstol(SubKeyName,NULL,10)>wcstol(TestKey,NULL,10)) {
				memset(TestKey , 0, sizeof(TestKey));
				swprintf(TestKey,L"%ld",wcstol(SubKeyName,NULL,10));
			}
		indx++; 
	}
	pIMeta->CloseKey(MyHandle); 

	ServerInstance = wcstol(TestKey,NULL,10)+1;
	hRes = pIMeta->OpenKey(METADATA_MASTER_ROOT_HANDLE, L"/LM/W3SVC", METADATA_PERMISSION_WRITE, 20, &MyHandle); 
	if (SUCCEEDED(hRes)) {
		swprintf(SubKeyName,L"/%ld/ROOT",ServerInstance);
		hRes = pIMeta->AddKey(MyHandle, SubKeyName);  
		if (SUCCEEDED(hRes)) {
			memset(KeyName , 0, sizeof(KeyName));
			swprintf(KeyName, L"/%ld", ServerInstance);
			memset(TestKey , 0, sizeof(TestKey));
			swprintf(TestKey,L"IIsWebServer");
			MyRecord.dwMDIdentifier = MD_KEY_TYPE;
			MyRecord.dwMDAttributes = METADATA_INSERT_PATH;
			MyRecord.dwMDUserType = IIS_MD_UT_SERVER;
			MyRecord.dwMDDataType = STRING_METADATA;
			MyRecord.pbMDData=(PBYTE)&TestKey;
			MyRecord.dwMDDataLen = (wcslen(TestKey) + 1) * sizeof(WCHAR);

			if(!SUCCEEDED(pIMeta->SetData(MyHandle, KeyName, &MyRecord)))
				return -503;

			memset(TestKey , 0, sizeof(TestKey));
			swprintf(TestKey,L"%S",ServerComment);
			MyRecord.dwMDIdentifier = MD_SERVER_COMMENT;
			MyRecord.dwMDAttributes = METADATA_INHERIT;
			MyRecord.dwMDUserType = IIS_MD_UT_SERVER;
			MyRecord.dwMDDataType = STRING_METADATA;
			MyRecord.pbMDData=(PBYTE)&TestKey;
			MyRecord.dwMDDataLen = (wcslen(TestKey) + 1) * sizeof(WCHAR);

			if(!SUCCEEDED(pIMeta->SetData(MyHandle, KeyName, &MyRecord)))
				return -504;

			memset(TestKey , 0, sizeof(TestKey));
			swprintf(TestKey,L"%S:%S:%S",ServerIp,ServerPort,ServerHost);
			MyRecord.dwMDIdentifier = MD_SERVER_BINDINGS;
			MyRecord.dwMDAttributes = METADATA_INHERIT;
			MyRecord.dwMDUserType = IIS_MD_UT_SERVER;
			MyRecord.dwMDDataType = MULTISZ_METADATA;
			MyRecord.pbMDData=(PBYTE)&TestKey;
			MyRecord.dwMDDataLen = (wcslen(TestKey) + 1) * sizeof(WCHAR) + 1 * sizeof(WCHAR);

			if(!SUCCEEDED(pIMeta->SetData(MyHandle, KeyName, &MyRecord)))
				return -505;

			memset(TestKey , 0, sizeof(TestKey));
			swprintf(TestKey,L"%S", ServerPath);
			MyRecord.dwMDIdentifier = MD_VR_PATH;
			MyRecord.dwMDAttributes = METADATA_INHERIT;
			MyRecord.dwMDUserType = IIS_MD_UT_FILE;
			MyRecord.dwMDDataType = STRING_METADATA;
			MyRecord.pbMDData=(PBYTE)&TestKey;
			MyRecord.dwMDDataLen = (wcslen(TestKey) + 1) * sizeof(WCHAR);

			if(!SUCCEEDED(pIMeta->SetData(MyHandle, SubKeyName, &MyRecord)))
				return -506;

			MyRecord.dwMDIdentifier = MD_ACCESS_PERM;
			MyRecord.dwMDAttributes = METADATA_INHERIT;
			MyRecord.dwMDUserType = IIS_MD_UT_FILE;
			MyRecord.dwMDDataType = DWORD_METADATA;
			MyRecord.pbMDData=(PBYTE)&ServerRights;
			MyRecord.dwMDDataLen = sizeof(DWORD);

			if(!SUCCEEDED(pIMeta->SetData(MyHandle, KeyName, &MyRecord)))
				return -507;

			MyRecord.dwMDIdentifier = MD_SERVER_COMMAND;
			MyRecord.dwMDAttributes = METADATA_INHERIT;
			MyRecord.dwMDUserType = IIS_MD_UT_SERVER;
			MyRecord.dwMDDataType = DWORD_METADATA;
			MyRecord.pbMDData=(PBYTE)&StartServer;
			MyRecord.dwMDDataLen = sizeof(DWORD);

			if(!SUCCEEDED(pIMeta->SetData(MyHandle, KeyName, &MyRecord)))
				return -508;
		}
		else
			return -502;
	}
	else
		return -501;
	pIMeta->CloseKey(MyHandle); 

	return ServerInstance;
}

extern "C" IISFUNC_API int fnIisRemoveServer(DWORD ServerInstance)
{
	HRESULT hRes = 0;
	DWORD dwBuffer = 65535;
	METADATA_HANDLE MyHandle; 
	PBYTE pbBuffer = new BYTE[dwBuffer];
	WCHAR SubKeyName[METADATA_MAX_NAME_LEN]; 
	CComPtr <IMSAdminBase> pIMeta; 

	CoInitialize(NULL);
	hRes = CoCreateInstance(CLSID_MSAdminBase, NULL, CLSCTX_ALL, IID_IMSAdminBase, (void **) &pIMeta);  

	if (FAILED(hRes))     
	return hRes;  

	hRes = pIMeta->OpenKey(METADATA_MASTER_ROOT_HANDLE, L"/LM/W3SVC", METADATA_PERMISSION_WRITE, 20, &MyHandle); 
	if (SUCCEEDED(hRes)) {
		memset(SubKeyName , 0, sizeof(SubKeyName));
		swprintf(SubKeyName, L"/%ld", ServerInstance);
		hRes = pIMeta->DeleteChildKeys(MyHandle, SubKeyName);
		if (!SUCCEEDED(hRes)) 
			return -602;

		hRes = pIMeta->DeleteAllData(MyHandle, SubKeyName, ALL_METADATA, ALL_METADATA);  
		if (!SUCCEEDED(hRes)) 
			return -603;

		hRes = pIMeta->DeleteKey(MyHandle, SubKeyName);
		if (!SUCCEEDED(hRes)) 
			return -604;
	}
	else
		return -601;
	pIMeta->CloseKey(MyHandle); 

	return 1;
}

extern "C" IISFUNC_API int fnIisSetDirSecurity(DWORD ServerInstance, char * VirtualPath, DWORD DirFlags)
{
	HRESULT hRes = 0;
	METADATA_HANDLE MyHandle; 
	METADATA_RECORD MyRecord; 
	WCHAR SubKeyName[METADATA_MAX_NAME_LEN]; 
	CComPtr <IMSAdminBase> pIMeta; 

	CoInitialize(NULL);
	hRes = CoCreateInstance(CLSID_MSAdminBase, NULL, CLSCTX_ALL, IID_IMSAdminBase, (void **) &pIMeta);  

	if (FAILED(hRes))     
	return hRes;  

	hRes = pIMeta->OpenKey(METADATA_MASTER_ROOT_HANDLE, L"/LM/W3SVC", METADATA_PERMISSION_WRITE, 20, &MyHandle); 
	if (SUCCEEDED(hRes)) {
		memset(SubKeyName , 0, sizeof(SubKeyName));
		swprintf(SubKeyName, L"/%ld/ROOT%S", ServerInstance, VirtualPath);
		MyRecord.dwMDIdentifier = MD_AUTHORIZATION;
		MyRecord.dwMDAttributes = METADATA_INHERIT;
		MyRecord.dwMDUserType = IIS_MD_UT_FILE;
		MyRecord.dwMDDataType = DWORD_METADATA;
		MyRecord.pbMDData=(PBYTE)&DirFlags;
		MyRecord.dwMDDataLen = sizeof(DWORD);

		if(!SUCCEEDED(pIMeta->SetData(MyHandle, SubKeyName, &MyRecord)))
			return -509;
	}
	else
		return -501;
	pIMeta->CloseKey(MyHandle); 

	return 1;
}

extern "C" IISFUNC_API int fnIisGetDirSecurity(DWORD ServerInstance, char * VirtualPath, DWORD * DirFlags)
{
	HRESULT hRes = 0;
	DWORD buffer = 0; 
	METADATA_HANDLE MyHandle; 
	METADATA_RECORD MyRecord; 
	WCHAR SubKeyName[METADATA_MAX_NAME_LEN]; 
	CComPtr <IMSAdminBase> pIMeta; 

	CoInitialize(NULL);
	hRes = CoCreateInstance(CLSID_MSAdminBase, NULL, CLSCTX_ALL, IID_IMSAdminBase, (void **) &pIMeta);  

	if (FAILED(hRes))     
	return hRes;  

	hRes = pIMeta->OpenKey(METADATA_MASTER_ROOT_HANDLE, L"/LM/W3SVC", METADATA_PERMISSION_READ, 20, &MyHandle); 
	if (SUCCEEDED(hRes)) {
		memset(SubKeyName , 0, sizeof(SubKeyName));
		swprintf(SubKeyName, L"/%ld/ROOT%S", ServerInstance, VirtualPath);
		MyRecord.dwMDIdentifier = MD_AUTHORIZATION;
		MyRecord.dwMDAttributes = METADATA_INHERIT;
		MyRecord.dwMDUserType = IIS_MD_UT_FILE;
		MyRecord.dwMDDataType = DWORD_METADATA;
		MyRecord.pbMDData=(PBYTE)DirFlags;
		MyRecord.dwMDDataLen = sizeof(DWORD);

		if(!SUCCEEDED(pIMeta->GetData(MyHandle, SubKeyName, &MyRecord, &buffer)))
			return -509;
	}
	else
		return -501;
	pIMeta->CloseKey(MyHandle); 

	return 1;
}

extern "C" IISFUNC_API int fnIisSetServerRight(DWORD ServerInstance, char * VirtualPath, DWORD ServerRights)
{
	HRESULT hRes = 0;
	METADATA_HANDLE MyHandle; 
	METADATA_RECORD MyRecord; 
	WCHAR SubKeyName[METADATA_MAX_NAME_LEN]; 
	CComPtr <IMSAdminBase> pIMeta; 

	CoInitialize(NULL);
	hRes = CoCreateInstance(CLSID_MSAdminBase, NULL, CLSCTX_ALL, IID_IMSAdminBase, (void **) &pIMeta);  

	if (FAILED(hRes))     
	return hRes;  

	hRes = pIMeta->OpenKey(METADATA_MASTER_ROOT_HANDLE, L"/LM/W3SVC", METADATA_PERMISSION_WRITE, 20, &MyHandle); 
	if (SUCCEEDED(hRes)) {
		memset(SubKeyName , 0, sizeof(SubKeyName));
		swprintf(SubKeyName, L"/%ld/ROOT%S", ServerInstance, VirtualPath);
		MyRecord.dwMDIdentifier = MD_ACCESS_PERM;
		MyRecord.dwMDAttributes = METADATA_INHERIT;
		MyRecord.dwMDUserType = IIS_MD_UT_FILE;
		MyRecord.dwMDDataType = DWORD_METADATA;
		MyRecord.pbMDData=(PBYTE)&ServerRights;
		MyRecord.dwMDDataLen = sizeof(DWORD);

		if(!SUCCEEDED(pIMeta->SetData(MyHandle, SubKeyName, &MyRecord)))
			return -507;
	}
	else
		return -501;
	pIMeta->CloseKey(MyHandle); 

	return 1;
}

extern "C" IISFUNC_API int fnIisGetServerRight(DWORD ServerInstance, char * VirtualPath, DWORD * ServerRights)
{
	HRESULT hRes = 0;
	DWORD buffer = 0; 
	METADATA_HANDLE MyHandle; 
	METADATA_RECORD MyRecord; 
	WCHAR SubKeyName[METADATA_MAX_NAME_LEN]; 
	CComPtr <IMSAdminBase> pIMeta; 

	CoInitialize(NULL);
	hRes = CoCreateInstance(CLSID_MSAdminBase, NULL, CLSCTX_ALL, IID_IMSAdminBase, (void **) &pIMeta);  

	if (FAILED(hRes))     
	return hRes;  

	hRes = pIMeta->OpenKey(METADATA_MASTER_ROOT_HANDLE, L"/LM/W3SVC", METADATA_PERMISSION_READ, 20, &MyHandle); 
	if (SUCCEEDED(hRes)) {
		memset(SubKeyName , 0, sizeof(SubKeyName));
		swprintf(SubKeyName, L"/%ld/ROOT%S", ServerInstance, VirtualPath);
		MyRecord.dwMDIdentifier = MD_ACCESS_PERM;
		MyRecord.dwMDAttributes = METADATA_INHERIT;
		MyRecord.dwMDUserType = IIS_MD_UT_FILE;
		MyRecord.dwMDDataType = DWORD_METADATA;
		MyRecord.pbMDData=(PBYTE)ServerRights;
		MyRecord.dwMDDataLen = sizeof(DWORD);

		if(!SUCCEEDED(pIMeta->GetData(MyHandle, SubKeyName, &MyRecord, &buffer)))
			return -507;
	}
	else
		return -501;
	pIMeta->CloseKey(MyHandle); 

	return 1;
}

extern "C" IISFUNC_API int fnIisSetServerStatus(DWORD ServerInstance, DWORD StartServer)
{
	HRESULT hRes = 0;
	METADATA_HANDLE MyHandle; 
	METADATA_RECORD MyRecord; 
	WCHAR SubKeyName[METADATA_MAX_NAME_LEN]; 
	CComPtr <IMSAdminBase> pIMeta; 

	CoInitialize(NULL);
	hRes = CoCreateInstance(CLSID_MSAdminBase, NULL, CLSCTX_ALL, IID_IMSAdminBase, (void **) &pIMeta);  

	if (FAILED(hRes))     
	return hRes;  

	hRes = pIMeta->OpenKey(METADATA_MASTER_ROOT_HANDLE, L"/LM/W3SVC", METADATA_PERMISSION_WRITE, 20, &MyHandle); 
	if (SUCCEEDED(hRes)) {
		memset(SubKeyName , 0, sizeof(SubKeyName));
		swprintf(SubKeyName, L"/%ld", ServerInstance);
		MyRecord.dwMDIdentifier = MD_SERVER_COMMAND;
		MyRecord.dwMDAttributes = METADATA_INHERIT;
		MyRecord.dwMDUserType = IIS_MD_UT_SERVER;
		MyRecord.dwMDDataType = DWORD_METADATA;
		MyRecord.pbMDData=(PBYTE)&StartServer;
		MyRecord.dwMDDataLen = sizeof(DWORD);

		if(!SUCCEEDED(pIMeta->SetData(MyHandle, SubKeyName, &MyRecord)))
			return -508;
	}
	else
		return -501;
	pIMeta->CloseKey(MyHandle); 

	return 1;
}

extern "C" IISFUNC_API int fnIisSetScriptMap(DWORD ServerInstance, char * VirtualPath, char * ScriptMap)
{
	HRESULT hRes = 0;
	DWORD buffer = 0; 
	DWORD dwBuffer = 65535;
	METADATA_HANDLE MyHandle; 
	METADATA_RECORD MyRecord; 
	PBYTE pbBuffer = new BYTE[dwBuffer];
	WCHAR SubKeyName[METADATA_MAX_NAME_LEN]; 
	CComPtr <IMSAdminBase> pIMeta; 
	WCHAR NewData[65535];

	CoInitialize(NULL);
	hRes = CoCreateInstance(CLSID_MSAdminBase, NULL, CLSCTX_ALL, IID_IMSAdminBase, (void **) &pIMeta);  

	if (FAILED(hRes))     
	return hRes;  

	hRes = pIMeta->OpenKey(METADATA_MASTER_ROOT_HANDLE, L"/LM/W3SVC", METADATA_PERMISSION_READ, 20, &MyHandle); 
	if (SUCCEEDED(hRes)) {
		memset(SubKeyName , 0, sizeof(SubKeyName));
		swprintf(SubKeyName, L"/%ld/ROOT%S", ServerInstance, VirtualPath);
		MyRecord.dwMDIdentifier = MD_SCRIPT_MAPS;
		MyRecord.dwMDAttributes = METADATA_INHERIT;
		MyRecord.dwMDUserType = IIS_MD_UT_FILE;
		MyRecord.dwMDDataType = MULTISZ_METADATA;
		MyRecord.pbMDData = pbBuffer;
		MyRecord.dwMDDataLen = dwBuffer;
		hRes = pIMeta->GetData(MyHandle, SubKeyName, &MyRecord, &buffer);
		if (SUCCEEDED(hRes)) {
			pIMeta->ChangePermissions(MyHandle, 1000, METADATA_PERMISSION_WRITE);

			memset(NewData, 0, sizeof(NewData));
			swprintf(NewData, L"%S", ScriptMap);
			memcpy(NewData + (wcslen(NewData) + 1), pbBuffer, MyRecord.dwMDDataLen);

			MyRecord.dwMDIdentifier = MD_SCRIPT_MAPS;
			MyRecord.dwMDAttributes = METADATA_INHERIT;
			MyRecord.dwMDUserType = IIS_MD_UT_FILE;
			MyRecord.dwMDDataType = MULTISZ_METADATA;
			MyRecord.pbMDData = (PBYTE)&NewData;
			MyRecord.dwMDDataLen += (wcslen(NewData) + 1) * sizeof(WCHAR) + sizeof(WCHAR);

			if(!SUCCEEDED(pIMeta->SetData(MyHandle, SubKeyName, &MyRecord)))
				return -509;
		}
	}
	else
		return -501;
	pIMeta->CloseKey(MyHandle); 

	return 1;
}

extern "C" IISFUNC_API int fnIisGetScriptMap(DWORD ServerInstance, char * VirtualPath, char * ScriptExtention, char * ReturnValue)
{
	HRESULT hRes = 0;
	DWORD buffer = 0; 
	DWORD dwBuffer = 65535;
	METADATA_HANDLE MyHandle; 
	METADATA_RECORD MyRecord; 
	PBYTE pbBuffer = new BYTE[dwBuffer];
	WCHAR SubKeyName[METADATA_MAX_NAME_LEN]; 
	CComPtr <IMSAdminBase> pIMeta; 
	PBYTE temp;
	char extBuffer[256];

	CoInitialize(NULL);
	hRes = CoCreateInstance(CLSID_MSAdminBase, NULL, CLSCTX_ALL, IID_IMSAdminBase, (void **) &pIMeta);  

	if (FAILED(hRes))     
	return hRes;  

	hRes = pIMeta->OpenKey(METADATA_MASTER_ROOT_HANDLE, L"/LM/W3SVC", METADATA_PERMISSION_READ, 20, &MyHandle); 
	if (SUCCEEDED(hRes)) {
		memset(SubKeyName , 0, sizeof(SubKeyName));
		swprintf(SubKeyName,L"/%ld/ROOT%S",ServerInstance,VirtualPath);
		MyRecord.dwMDIdentifier = MD_SCRIPT_MAPS;
		MyRecord.dwMDAttributes = METADATA_INHERIT;
		MyRecord.dwMDUserType = IIS_MD_UT_FILE;
		MyRecord.dwMDDataType = MULTISZ_METADATA;
		MyRecord.pbMDData = pbBuffer;
		MyRecord.dwMDDataLen = dwBuffer;

		hRes = pIMeta->GetData(MyHandle, SubKeyName, &MyRecord, &buffer);
		temp = pbBuffer;
		while (*temp) {
			memset(extBuffer, 0, sizeof(extBuffer));
			sprintf(extBuffer, "%ls\n", temp);
			if (!strncmp(extBuffer, ScriptExtention, strlen(ScriptExtention)))
				strcpy(ReturnValue, extBuffer);
			temp += (wcslen((wchar_t *)temp) + 1) * sizeof(WCHAR);
		}
	}
	pIMeta->CloseKey(MyHandle); 

	return 1;
}

extern "C" IISFUNC_API int fnIisSetAppSettings(DWORD ServerInstance, char * VirtualPath, char * AppName)
{
	HRESULT hRes = 0;
	METADATA_HANDLE MyHandle; 
	METADATA_RECORD MyRecord; 
	WCHAR SubKeyName[METADATA_MAX_NAME_LEN]; 
	WCHAR TestKey[METADATA_MAX_NAME_LEN]; 
	CComPtr <IMSAdminBase> pIMeta;
	DWORD isolated = 0;

	CoInitialize(NULL);
	hRes = CoCreateInstance(CLSID_MSAdminBase, NULL, CLSCTX_ALL, IID_IMSAdminBase, (void **) &pIMeta);  

	if (FAILED(hRes))     
	return hRes;  
	hRes = pIMeta->OpenKey(METADATA_MASTER_ROOT_HANDLE, L"/LM/W3SVC", METADATA_PERMISSION_WRITE, 20, &MyHandle); 
	if (SUCCEEDED(hRes)) {
		memset(SubKeyName , 0, sizeof(SubKeyName));
		swprintf(SubKeyName, L"/%ld/ROOT%S", ServerInstance, VirtualPath);
		memset(TestKey , 0, sizeof(TestKey));
		swprintf(TestKey, L"%S", AppName);
		MyRecord.dwMDIdentifier = MD_APP_FRIENDLY_NAME;
		MyRecord.dwMDAttributes = METADATA_INHERIT;
		MyRecord.dwMDUserType = IIS_MD_UT_WAM;
		MyRecord.dwMDDataType = STRING_METADATA;
		MyRecord.pbMDData = (PBYTE)&TestKey;
		MyRecord.dwMDDataLen = (wcslen(TestKey) + 1) * sizeof(WCHAR);

		hRes = pIMeta->SetData(MyHandle, SubKeyName, &MyRecord);

		memset(TestKey , 0, sizeof(TestKey));
		swprintf(TestKey, L"/LM/W3SVC/%ld/ROOT%S", ServerInstance, VirtualPath);
		MyRecord.dwMDIdentifier = MD_APP_ROOT;
		MyRecord.dwMDAttributes = METADATA_INHERIT;
		MyRecord.dwMDUserType = IIS_MD_UT_WAM;
		MyRecord.dwMDDataType = STRING_METADATA;
		MyRecord.pbMDData = (PBYTE)&TestKey;
		MyRecord.dwMDDataLen = (wcslen(TestKey) + 1) * sizeof(WCHAR);

		hRes = pIMeta->SetData(MyHandle, SubKeyName, &MyRecord);

		memset(TestKey , 0, sizeof(TestKey));
		swprintf(TestKey, L"%S", "{465E38DF-66E8-11D3-847C-006097B892CD}");
		MyRecord.dwMDIdentifier = MD_APP_WAM_CLSID;
		MyRecord.dwMDAttributes = METADATA_INHERIT;
		MyRecord.dwMDUserType = IIS_MD_UT_WAM;
		MyRecord.dwMDDataType = STRING_METADATA;
		MyRecord.pbMDData = (PBYTE)&TestKey;
		MyRecord.dwMDDataLen = (wcslen(TestKey) + 1) * sizeof(WCHAR);

		hRes = pIMeta->SetData(MyHandle, SubKeyName, &MyRecord);

		MyRecord.dwMDIdentifier = MD_APP_ISOLATED;
		MyRecord.dwMDAttributes = METADATA_INHERIT;
		MyRecord.dwMDUserType = IIS_MD_UT_WAM;
		MyRecord.dwMDDataType = DWORD_METADATA;
		MyRecord.pbMDData = (PBYTE)&isolated;
		MyRecord.dwMDDataLen = sizeof(DWORD);

		hRes = pIMeta->SetData(MyHandle, SubKeyName, &MyRecord);

	}
	pIMeta->CloseKey(MyHandle); 

	return 1;
}

extern "C" IISFUNC_API int fnStopService(LPCTSTR ServiceId)
{
	SC_HANDLE hnd,hsvc;
	SERVICE_STATUS ss;

	hnd = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (hnd) {
		hsvc = OpenService(hnd,ServiceId,SERVICE_STOP);
		if (hsvc)
			if (ControlService(hsvc,SERVICE_CONTROL_STOP,&ss))
				return 1;
			else
				return -103;
		else
			return -102;
	}
	else
		return -101;
}

extern "C" IISFUNC_API int fnStartService(LPCTSTR ServiceId)
{
	SC_HANDLE hnd,hsvc;

	hnd = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (hnd) {
		hsvc = OpenService(hnd,ServiceId,SERVICE_START);
		if (hsvc)
			if (StartService(hsvc,0, NULL))
				return 1;
			else
				return -104;
		else
			return -102;
	}
	else
		return -101;
}

extern "C" IISFUNC_API int fnGetServiceState(LPCTSTR ServiceId)
{
	SC_HANDLE hnd,hsvc;
	SERVICE_STATUS ss;

	hnd = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (hnd) {
		hsvc = OpenService(hnd,ServiceId,SERVICE_QUERY_STATUS);
		if (hsvc)
			if (QueryServiceStatus(hsvc,&ss))
				return ss.dwCurrentState;
			else
				return -105;
		else
			return -102;
	}
	else
		return -101;
}
