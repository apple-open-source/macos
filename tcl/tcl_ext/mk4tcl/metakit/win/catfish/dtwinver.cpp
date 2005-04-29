/*
Module : dtwinver.cpp
Purpose: Implementation of a class to 
         perform version detection on OS
Created: PJN / DATE/2 / 11-05-1996
History: None

Copyright (c) 1996 by PJ Naughter.  
All rights reserved.
*/


/////////////////////////////////  Includes  //////////////////////////////////
#ifdef _WINDOWS  
#include <afxwin.h>
#else
#include <afx.h>
#endif
#include "dtwinver.h"
#include <stdarg.h>


/////////////////////////////////  Local function / variables /////////////////



#ifndef _WIN32
  //taken from Win32 sdk winbase.h file
  #define VER_PLATFORM_WIN32s             0
  #define VER_PLATFORM_WIN32_WINDOWS      1
  #define VER_PLATFORM_WIN32_NT           2
#else
  BOOL WhichNTProduct(DWORD dwVersion);
#endif
                                     
                                     
                                     



#if defined(_WINDOWS) && !defined(_WIN32)     //required for universal thunks
  #define HINSTANCE32              DWORD
  #define HFILE32                  DWORD
  #define HWND32                   DWORD

  // #defines for WOWCallProc32() parameter conversion
  #define PARAM_01                 0x00000001

  // #defines for WOWGetCaps32()
  #define WOW_LOADLIBRARY          0x0001
  #define WOW_FREELIBRARY          0x0002
  #define WOW_GETPROCADDRESS       0x0004
  #define WOW_CALLPROC             0x0008
  #define WOW_VDMPTR32             0x0010
  #define WOW_VDMPTR16             0x0020
  #define WOW_HWND32               0x0040

  // Wrappers for functions in Kernel (16 bit)
  HINSTANCE32 WINAPI    WOWLoadLibraryEx32  (LPSTR, HFILE32, DWORD);
  BOOL        WINAPI    WOWFreeLibrary32    (HINSTANCE32);
  FARPROC     WINAPI    WOWGetProcAddress32 (HINSTANCE32, LPCSTR);
  DWORD       WINAPI    WOWGetVDMPointer32  (LPVOID, UINT);
  DWORD       FAR CDECL WOWCallProc32       (FARPROC, DWORD, DWORD, ...);

  DWORD       WINAPI    WOWGetCaps32         (VOID);
  UINT        WINAPI    WOWCreateVDMPointer16(DWORD, DWORD);
  UINT        WINAPI    WOWDeleteVDMPointer16(UINT);
  HWND32      WINAPI    WOWHwndToHwnd32      (HWND);


  //////////////// OSVERSIONINFO taken from Win32 sdk header file
  typedef struct _OSVERSIONINFO
  { 
    DWORD dwOSVersionInfoSize; 
    DWORD dwMajorVersion; 
    DWORD dwMinorVersion; 
    DWORD dwBuildNumber; 
    DWORD dwPlatformId; 
    TCHAR szCSDVersion[ 128 ]; 
  } OSVERSIONINFO, *POSVERSIONINFO, FAR *LPOSVERSIONINFO; 


  // Function pointers to stuff in Kernel (16 bit)
  typedef HINSTANCE32 (WINAPI *lpfnLoadLibraryEx32W) (LPSTR, HFILE32, DWORD);
  typedef BOOL        (WINAPI *lpfnFreeLibrary32W)   (HINSTANCE32);
  typedef FARPROC     (WINAPI *lpfnGetProcAddress32W)(HINSTANCE32, LPCSTR);
  typedef DWORD       (WINAPI *lpfnGetVDMPointer32W) (LPVOID, UINT);
  typedef DWORD       (WINAPI *lpfnCallProc32W)      (FARPROC, DWORD, DWORD);
  typedef WORD        (WINAPI *lpfnWNetGetCaps)      (WORD);

  lpfnLoadLibraryEx32W LoadLibraryEx32W;
  lpfnFreeLibrary32W FreeLibrary32W;
  lpfnGetProcAddress32W GetProcAddress32W;
  lpfnGetVDMPointer32W GetVDMPointer32W;
  lpfnCallProc32W CallProc32W;
               
  //capability bits               
  DWORD dwCapBits;                                       

  BOOL WFWLoaded();
#endif //defined(_WINDOWS) && !defined(_WIN32)




#ifdef _DOS
  BYTE WinMajor;
  BYTE WinMinor;
  WORD WinVer;
  BYTE bRunningWindows;
  void GetWinInfo();
#endif






//////////////////////////////////  Macros  ///////////////////////////////////
#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif                                     





////////////////////////////////// Implementation /////////////////////////////

BOOL GetOSVersion(LPOS_VERSION_INFO lpVersionInformation)
{                
  //size field must be filled in prior to call,
  //this is the same behaviour as the real Win32 api
  if (lpVersionInformation->dwOSVersionInfoSize != sizeof(OS_VERSION_INFO))
    return FALSE;            
    
  #ifdef _WIN32  //Nice and simple when on Win32
    OSVERSIONINFO osvi;
    memset(&osvi, 0, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    BOOL bSuccess = GetVersionEx(&osvi);
    lpVersionInformation->dwEmulatedMajorVersion = osvi.dwMajorVersion; 
    lpVersionInformation->dwEmulatedMinorVersion = osvi.dwMinorVersion; 
    lpVersionInformation->dwEmulatedBuildNumber = LOWORD(osvi.dwBuildNumber); //ignore HIWORD
    lpVersionInformation->dwEmulatedPlatformId = osvi.dwPlatformId;
    _tcscpy(lpVersionInformation->szEmulatedCSDVersion, osvi.szCSDVersion);
    
    //Explicitely map the win32 dwPlatformId to our own values
    if (lpVersionInformation->dwEmulatedPlatformId == VER_PLATFORM_WIN32s)
      lpVersionInformation->dwEmulatedPlatformId = PLATFORM_WIN32S;
    else if (lpVersionInformation->dwEmulatedPlatformId == VER_PLATFORM_WIN32_WINDOWS)
      lpVersionInformation->dwEmulatedPlatformId = PLATFORM_WINDOWS;
    if (lpVersionInformation->dwEmulatedPlatformId == VER_PLATFORM_WIN32_NT)
      lpVersionInformation->dwEmulatedPlatformId = PLATFORM_NT_WORKSTATION;
    
    //Win32s is not an OS in its own right 
    if (lpVersionInformation->dwEmulatedPlatformId == PLATFORM_WIN32S)
    {                                     
      //Could not find a method of determining what Win 16 version from within win32,
      //so just assume Windows 3.10
      lpVersionInformation->dwUnderlyingMajorVersion = 3; 
      lpVersionInformation->dwUnderlyingMinorVersion = 10; 
      lpVersionInformation->dwUnderlyingBuildNumber = 0;
      lpVersionInformation->dwUnderlyingPlatformId = PLATFORM_WINDOWS31;
      _tcscpy(lpVersionInformation->szUnderlyingCSDVersion, _T("Microsoft Windows"));
    }
    else
    { 
      //determine which version on NT we're running                      
      DWORD dwVersion=0;    
      if (WhichNTProduct(dwVersion))
      {
        lpVersionInformation->dwUnderlyingPlatformId = dwVersion; 
        lpVersionInformation->dwEmulatedPlatformId = dwVersion;
      } 
      lpVersionInformation->dwUnderlyingMajorVersion = osvi.dwMajorVersion; 
      lpVersionInformation->dwUnderlyingMinorVersion = osvi.dwMinorVersion; 
      lpVersionInformation->dwUnderlyingBuildNumber = LOWORD(osvi.dwBuildNumber); //ignore HIWORD
      lpVersionInformation->dwUnderlyingPlatformId = osvi.dwPlatformId;
      _tcscpy(lpVersionInformation->szUnderlyingCSDVersion, osvi.szCSDVersion);
    }
  #else //We must be runing on an emulated or real version of Win16 or Dos
    #ifdef _WINDOWS //Running on some version of Windows                   
      DWORD dwVersion = GetVersion();
      // GetVersion does not differentiate between Windows 3.1 and Windows 3.11
      
      lpVersionInformation->dwEmulatedMajorVersion = LOBYTE(LOWORD(dwVersion)); 
      lpVersionInformation->dwEmulatedMinorVersion = HIBYTE(LOWORD(dwVersion));
      lpVersionInformation->dwEmulatedBuildNumber = 0; //no build number with Win3.1x
      lpVersionInformation->dwEmulatedPlatformId = PLATFORM_WINDOWS31;
      _fstrcpy(lpVersionInformation->szEmulatedCSDVersion, "Microsoft Windows");
      
      
      //GetVersion returns 3.1 even on WFW, need to poke further
      //to find the real difference                      
      if (WFWLoaded())
        lpVersionInformation->dwEmulatedPlatformId = PLATFORM_WINDOWSFW;

      //Call to get the underlying OS here through 16 -> 32 bit Generic Thunk
      BOOL bFoundUnderlyingOS = FALSE;

      // Initialize capability bits for supplied functions
      dwCapBits = WOW_VDMPTR16 | WOW_HWND32;
    
      // Get Kernel handle (we're already running in 16 bit mode so its
      // gotta be loaded, right?!
      HMODULE hKernel=GetModuleHandle("KERNEL");
    
      // Dynamically link to the functions we want, setting the capability
      // bits as needed
      LoadLibraryEx32W = ((lpfnLoadLibraryEx32W) (GetProcAddress(hKernel, "LoadLibraryEx32W")));
      if (LoadLibraryEx32W)
        dwCapBits |= WOW_LOADLIBRARY;
           
      FreeLibrary32W = ((lpfnFreeLibrary32W) (GetProcAddress(hKernel, "FreeLibrary32W")));
      if (FreeLibrary32W)
        dwCapBits |= WOW_FREELIBRARY;
           
      GetProcAddress32W = ((lpfnGetProcAddress32W) (GetProcAddress(hKernel, "GetProcAddress32W")));
      if (GetProcAddress32W)
        dwCapBits |= WOW_GETPROCADDRESS;
           
      GetVDMPointer32W = ((lpfnGetVDMPointer32W) (GetProcAddress(hKernel, "GetVDMPointer32W")));
      if (GetVDMPointer32W)
        dwCapBits |= WOW_VDMPTR32;
           
      CallProc32W = ((lpfnCallProc32W) (GetProcAddress(hKernel, "CallProc32W")));     
      if (CallProc32W)
        dwCapBits |= WOW_CALLPROC;

      //Call thro' the thunk to the Win32 function "GetVersionEx"
      HINSTANCE32 hInstKrnl32 = WOWLoadLibraryEx32("KERNEL32.DLL", NULL, NULL);
      //Because we are building the call to the function at runtime, We don't 
      //forget to include the A at the end to call the ASCII version of GetVersionEx
      FARPROC lpfnWin32GetVersionEx = WOWGetProcAddress32(hInstKrnl32, "GetVersionExA");
      if (lpfnWin32GetVersionEx)
      {              
        OSVERSIONINFO osvi;                      
        memset(&osvi, 0, sizeof(OSVERSIONINFO));
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        DWORD dwSuccess = WOWCallProc32(lpfnWin32GetVersionEx, PARAM_01, 1, &osvi);
        if (dwSuccess)
        {
          lpVersionInformation->dwUnderlyingMajorVersion = osvi.dwMajorVersion; 
          lpVersionInformation->dwUnderlyingMinorVersion = osvi.dwMinorVersion; 
          lpVersionInformation->dwUnderlyingBuildNumber = LOWORD(osvi.dwBuildNumber); //ignore HIWORD
          lpVersionInformation->dwUnderlyingPlatformId = osvi.dwPlatformId;
          _fstrcpy(lpVersionInformation->szUnderlyingCSDVersion, osvi.szCSDVersion);
          
          //Explicitely map the win32 dwPlatformId to our own values
          if (lpVersionInformation->dwEmulatedPlatformId == VER_PLATFORM_WIN32s)
            lpVersionInformation->dwEmulatedPlatformId = PLATFORM_WIN32S;
          else if (lpVersionInformation->dwEmulatedPlatformId == VER_PLATFORM_WIN32_WINDOWS)
            lpVersionInformation->dwEmulatedPlatformId = PLATFORM_WINDOWS;
          if (lpVersionInformation->dwEmulatedPlatformId == VER_PLATFORM_WIN32_NT)
            lpVersionInformation->dwEmulatedPlatformId = PLATFORM_NT_WORKSTATION;

          bFoundUnderlyingOS = TRUE;
        }
      }
      WOWFreeLibrary32(hInstKrnl32);

      if (!bFoundUnderlyingOS)
      {
        //must be running on a real version of 16 bit Windows whose underlying OS is DOS
        lpVersionInformation->dwUnderlyingMajorVersion = HIBYTE(HIWORD(dwVersion)); 
        lpVersionInformation->dwUnderlyingMinorVersion = LOBYTE(HIWORD(dwVersion)); 
        lpVersionInformation->dwUnderlyingBuildNumber = 0; 
        lpVersionInformation->dwUnderlyingPlatformId = PLATFORM_DOS;
        _fstrcpy(lpVersionInformation->szUnderlyingCSDVersion, "MS-DOS");
      }
    #else //Must be some version of real or emulated Dos
      
      //Retreive the current version of emulated dos
      BYTE DosMinor;
      BYTE DosMajor;
      _asm
      {
        mov ax, 3306h
        int 21h
        mov byte ptr [DosMajor], bl
        mov byte ptr [DosMinor], bh
      }
      lpVersionInformation->dwEmulatedPlatformId = PLATFORM_DOS;
      lpVersionInformation->dwEmulatedMajorVersion = (DWORD) DosMajor; 
      lpVersionInformation->dwEmulatedMinorVersion = (DWORD) DosMinor;                
      lpVersionInformation->dwEmulatedBuildNumber = 0; //no build number with Dos

      //We can detect if NT is running as it reports Dos v5.5
      if ((lpVersionInformation->dwEmulatedMajorVersion == 5) &&
          (lpVersionInformation->dwEmulatedMinorVersion == 50))    //NT reports Dos v5.5
      {
        _fstrcpy(lpVersionInformation->szUnderlyingCSDVersion, "Microsoft Windows NT");    
        //could not find method of determing version of NT from Dos,
        //so assume 3.50
        lpVersionInformation->dwUnderlyingMajorVersion = 3; 
        lpVersionInformation->dwUnderlyingMinorVersion = 50; 
        lpVersionInformation->dwUnderlyingBuildNumber = 0;  //cannot get access to build number from Dos
        lpVersionInformation->dwUnderlyingPlatformId = PLATFORM_NT_WORKSTATION;
      }            
      else if (lpVersionInformation->dwEmulatedMajorVersion >= 7) //Win95 reports Dos v7.00
      {
        _fstrcpy(lpVersionInformation->szUnderlyingCSDVersion, "Microsoft Windows 95");
        lpVersionInformation->dwUnderlyingMajorVersion = 4; 
        lpVersionInformation->dwUnderlyingMinorVersion = 0; 
        lpVersionInformation->dwUnderlyingBuildNumber = 0;  //cannot get access to build number from Dos
        lpVersionInformation->dwUnderlyingPlatformId = PLATFORM_WINDOWS;              
      }  
      else
      {
        //need to get the underlying OS here here via the int 2FH interface of Windows
        GetWinInfo();
        if (bRunningWindows)
        {                   
          //Could not find method of differentiating between WFW & Win3.1 under Dos,
          //so assume Win3.1
          lpVersionInformation->dwUnderlyingPlatformId = PLATFORM_WINDOWS31;      
          _fstrcpy(lpVersionInformation->szUnderlyingCSDVersion, "Microsoft Windows");
          lpVersionInformation->dwUnderlyingMajorVersion = WinVer & 0xFF00; 
          lpVersionInformation->dwUnderlyingMinorVersion = WinVer & 0x00FF; 
          lpVersionInformation->dwUnderlyingBuildNumber = 0;  //cannot get access to build number from Dos

        }
        else //must be on a real version of Dos
        {                               
          lpVersionInformation->dwUnderlyingMajorVersion = (DWORD) DosMajor; 
          lpVersionInformation->dwUnderlyingMinorVersion = (DWORD) DosMinor;                
          lpVersionInformation->dwUnderlyingBuildNumber = 0; //no build number with Dos
          lpVersionInformation->dwUnderlyingPlatformId = PLATFORM_DOS;
          _fstrcpy(lpVersionInformation->szUnderlyingCSDVersion, "MS-DOS");
        }
      }  
    #endif  
  #endif

  return TRUE;
}
                                  
                                  
      
      
#ifdef _WIN32      
BOOL WhichNTProduct(DWORD dwVersion)
{
  const int MY_BUFSIZE = 100;
  TCHAR szProductType[MY_BUFSIZE];
  DWORD dwBufLen = MY_BUFSIZE * sizeof(TCHAR);
  HKEY hKey;
 
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                  _T("SYSTEM\\CurrentControlSet\\Control\\ProductOptions"),
                  0,
                  KEY_EXECUTE,
                  &hKey) != ERROR_SUCCESS) 
    return FALSE;
 
  if (RegQueryValueEx(hKey,
                     _T("ProductType"),
                     NULL,
                     NULL,
                     (LPBYTE) szProductType,
                     &dwBufLen) != ERROR_SUCCESS)
    return FALSE;
 
  RegCloseKey(hKey);
        
  BOOL bSuccess = FALSE;

  // check product options, in order of likelihood   strcmp
  if (_tcscmp(_T("WINNT"), szProductType) == 0) 
  {
    dwVersion = PLATFORM_NT_WORKSTATION;
    bSuccess = TRUE;
  }  
  else if (_tcscmp(_T("SERVERNT"), szProductType) == 0) 
  {
    dwVersion = PLATFORM_NT_SERVER;
    bSuccess = TRUE;
  }  
  else if (_tcscmp(_T("LANMANNT"), szProductType) == 0) 
  {
    dwVersion = PLATFORM_NT_ADVANCEDSERVER;
    bSuccess = TRUE;
  }  
 
  return bSuccess;
}
#endif                                  
                                   

#if defined(_WINDOWS) && !defined(_WIN32)
DWORD WINAPI WOWGetCaps32(VOID)
{
  return dwCapBits;
}     


UINT WINAPI WOWCreateVDMPointer16(DWORD dwLinBase, DWORD dwLimit)
{
  UINT uSelector, uDSSel;

  if (!(dwCapBits & WOW_VDMPTR16))
    return NULL;

  // Grab our DS (for access rights)
  __asm mov uDSSel, ds

  // Allocate a new selector
  uSelector = AllocSelector(uDSSel);
  if (uSelector)
  {
    // Assign its linear base address and limit
    SetSelectorBase(uSelector, dwLinBase);
    SetSelectorLimit(uSelector, dwLimit);
  }
  return uSelector;
}


UINT WINAPI WOWDeleteVDMPointer16(UINT uSelector)
{
  if (!(dwCapBits & WOW_VDMPTR16))
    return NULL;
  return FreeSelector(uSelector);
}


HWND32 WINAPI WOWHwndToHwnd32(HWND hWnd)
{
  if (!(dwCapBits & WOW_HWND32))
    return NULL;
  
  // OR mask the upper 16 bits
  HWND32 hWnd32 = (HWND32) (WORD) (hWnd);
  return (hWnd32 | 0xffff0000);
}


HINSTANCE32 WINAPI WOWLoadLibraryEx32(LPSTR lpszFile, HFILE32 hFile,
                                               DWORD dwFlags)
{
  if (!(dwCapBits & WOW_LOADLIBRARY))
    return NULL;

  return LoadLibraryEx32W(lpszFile, hFile, dwFlags);
}


BOOL WINAPI WOWFreeLibrary32(HINSTANCE32 hInst32)
{
  if (!(dwCapBits & WOW_FREELIBRARY))
    return NULL;

  return FreeLibrary32W(hInst32);
}


FARPROC WINAPI WOWGetProcAddress32(HINSTANCE32 hInst32,
                                            LPCSTR lpszProc)
{
  if (!(dwCapBits & WOW_GETPROCADDRESS))
    return NULL;
  return GetProcAddress32W(hInst32, lpszProc);
}


DWORD WINAPI WOWGetVDMPointer32(LPVOID lpAddress, UINT fMode)
{
  if (!(dwCapBits & WOW_VDMPTR32))
    return NULL;
  return GetVDMPointer32W(lpAddress, fMode);
}


DWORD WOWCallProc32(FARPROC lpfnFunction, DWORD dwAddressConvert, DWORD dwParams, ...)
{
  va_list vaList;
  DWORD   dwCount;
  DWORD   dwTemp;

  if (!(dwCapBits & WOW_CALLPROC))
    return NULL;

  // Variable list start
  va_start(vaList,dwParams);

  for(dwCount=0; dwCount < dwParams; dwCount++)
  {
    // Pull each variable off of the stack
    dwTemp=(DWORD)va_arg(vaList,DWORD);

    // Push the DWORD
    __asm push word ptr [dwTemp+2];
    __asm push word ptr [dwTemp];
  }
  // Variable list end
  va_end(vaList);

  // Call Win32.  The pushed variable list precedes the parameters.
  // Appropriate parameters will be popped by this function (based
  // on the value in dwParams)
  return CallProc32W(lpfnFunction, dwAddressConvert, dwParams);
}


BOOL WFWLoaded()
{
  const WORD WNNC_NET_MultiNet         = 0x8000;
  const WORD WNNC_SUBNET_WinWorkgroups = 0x0004;
  const WORD WNNC_NET_TYPE             = 0x0002;
  BOOL rVal;
   
  HINSTANCE hUserInst = LoadLibrary("USER.EXE");
  lpfnWNetGetCaps lpWNetGetCaps = (lpfnWNetGetCaps) GetProcAddress(hUserInst, "WNetGetCaps");
  if (lpWNetGetCaps != NULL)
  {
    // Get the network type
    WORD wNetType = lpWNetGetCaps(WNNC_NET_TYPE);
    if (wNetType & WNNC_NET_MultiNet)
    {
      // a multinet driver is installed
      if (LOBYTE(wNetType) & WNNC_SUBNET_WinWorkgroups) // It is WFW
        rVal = TRUE;
      else // It is not WFW
        rVal = FALSE;
    }
    else
     rVal = FALSE;
  }
  else
    rVal = FALSE;
   
  // Clean up the module instance
  if (hUserInst)
    FreeLibrary(hUserInst);
    
  return rVal;  
}

#endif //defined(_WINDOWS) && !defined(_WIN32)



             
#ifdef _DOS             
void GetWinInfo()
{ 
  //use some inline assembly to determine if Windows if
  //running and what version is active
  _asm
  {
  ; check for Windows 3.1
    mov     ax,160ah                ; WIN31CHECK
    int     2fh                     ; check if running under Win 3.1.
    or      ax,ax
    jz      RunningUnderWin31       ; can check if running in standard
                                    ; or enhanced mode
   
  ; check for Windows 3.0 enhanced mode
    mov     ax,1600h                ; WIN386CHECK
    int     2fh
    test    al,7fh
    jnz     RunningUnderWin30Enh    ; enhanced mode
   
  ; check for 3.0 WINOLDAP
    mov     ax,4680h                ; IS_WINOLDAP_ACTIVE
    int     2fh
    or      ax,ax                   ; running under 3.0 derivative?
    jnz     NotRunningUnderWin
   
  ; rule out MS-DOS 5.0 task switcher
    mov     ax,4b02h                ; detect switcher
    push    bx
    push    es
    push    di
    xor     bx,bx
    mov     di,bx
    mov     es,bx
    int     2fh
    pop     di
    pop     es
    pop     bx
    or      ax,ax
    jz      NotRunningUnderWin      ; MS-DOS 5.0 task switcher found
   
  ; check for standard mode Windows 3.0
    mov     ax,1605h                ; PMODE_START
    int     2fh
    cmp     cx,-1
    jz      RunningUnderWin30Std
   
  ; check for real mode Windows 3.0
    mov     ax,1606h                ; PMODE_STOP
    int     2fh                     ; in case someone is counting
    ; Real mode Windows 3.0 is running
    mov     byte ptr [bRunningWindows], 1
    mov     word ptr [WinVer], 0300h
    jmp     ExitLabel
   
  RunningUnderWin30Std:
    ; Standard mode Windows 3.0 is running
    mov     byte ptr [bRunningWindows], 1
    mov     word ptr [WinVer], 0300h
    jmp     ExitLabel
   
  RunningUnderWin31:
    ; At this point: CX == 3 means Windows 3.1 enhanced mode
    ;                CX == 2 means Windows 3.1 standard mode
    mov     byte ptr [bRunningWindows], 1
    mov     word ptr [WinVer], 0310h    
    jmp     ExitLabel
   
  RunningUnderWin30Enh:
    ; Enhanced mode Windows 3.0 is running
    mov     byte ptr [bRunningWindows], 1    
    mov     word ptr [WinVer], 0300h    
    jmp     ExitLabel
   
  NotRunningUnderWin:                    
    mov     byte ptr [bRunningWindows], 0
    
  ExitLabel:
  } 
} 

#endif //_DOS 
