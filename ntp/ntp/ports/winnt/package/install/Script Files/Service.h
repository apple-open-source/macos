////////////////////////////////////////////////////////////////////////////////
//
//  IIIIIII SSSSSS
//    II    SS                          InstallShield (R)
//    II    SSSSSS      (c) 1996-1997, InstallShield Software Corporation
//    II        SS      (c) 1990-1996, InstallShield Corporation
//  IIIIIII SSSSSS                     All rights reserved.
//
//
//    File Name: Service.h
//
//  Description: Header file for NT Services template.
//
//     Comments: Contains prototypes and definitions, and so on.
//
//      Updated:  02 December, 1997
//
////////////////////////////////////////////////////////////////////////////////

typedef SERVICE_STATUS
begin
    LONG dwServiceType;
    LONG dwCurrentState;
    LONG dwControlsAccepted;
    LONG dwWin32ExitCode;
    LONG dwServiceSpecificExitCode;
    LONG dwCheckPoint;
    LONG dwWaitHint;
end;

typedef SERVICE_ARGS
begin
    POINTER pszArg1;
    POINTER pszArg2;
    POINTER pszArg3;
    POINTER pszArg4;
    POINTER pszArg5;
    POINTER pszArg6;
    POINTER pszArg7;
    POINTER pszArg8;
    POINTER pszArg9;
    POINTER pszArg10;
end;

#define SERVICE_STOPPED                0x00000001
#define SERVICE_START_PENDING          0x00000002
#define SERVICE_STOP_PENDING           0x00000003
#define SERVICE_RUNNING                0x00000004
#define SERVICE_CONTINUE_PENDING       0x00000005
#define SERVICE_PAUSE_PENDING          0x00000006
#define SERVICE_PAUSED                 0x00000007

// Constants for windows APIs

//ControlService
#define SERVICE_CONTROL_STOP           0x00000001
#define SERVICE_CONTROL_PAUSE          0x00000002
#define SERVICE_CONTROL_CONTINUE       0x00000003
#define SERVICE_CONTROL_INTERROGATE    0x00000004
#define SERVICE_CONTROL_SHUTDOWN       0x00000005

//OpenSCManager
#define SC_MANAGER_ALL_ACCESS          0x000F003F        //WINSVC.H
#define SC_MANAGER_CONNECT             0x00000001
#define SC_MANAGER_CREATE_SERVICE      0x00000002
#define SC_MANAGER_ENUMERATE_SERVICE   0x00000004
#define SC_MANAGER_LOCK                0x00000008
#define SC_MANAGER_QUERY_LOCK_STATUS   0x00000010
#define SC_MANAGER_MODIFY_BOOT_CONFIG  0x00000020
#define GENERIC_READ                   0x80000000
#define GENERIC_WRITE                  0x40000000
#define GENERIC_EXECUTE                0x20000000

//CreateService
//desired access
#define DELETE                         0x00010000
#define READ_CONTROL                   0x00020000
#define WRITE_DAC                      0x00040000
#define WRITE_OWNER                    0x00080000
#define SYNCHRONIZE                    0x00100000
#define STANDARD_RIGHTS_REQUIRED       0x000F0000

#define SERVICE_ALL_ACCESS             0x000F01FF
#define SERVICE_QUERY_CONFIG           0x00000001
#define SERVICE_CHANGE_CONFIG          0x00000002
#define SERVICE_QUERY_STATUS           0x00000004
#define SERVICE_ENUMERATE_DEPENDENTS   0x00000008
#define SERVICE_START                  0x00000010
#define SERVICE_STOP                   0x00000020
#define SERVICE_PAUSE_CONTINUE         0x00000040
#define SERVICE_INTERROGATE            0x00000080
#define SERVICE_USER_DEFINED_CONTROL   0x00000100

//service type
#define SERVICE_WIN32_OWN_PROCESS      0x00000010        //WINNT.H
#define SERVICE_WIN32_SHARE_PROCESS    0x00000020
#define SERVICE_KERNEL_DRIVER          0x00000001
#define SERVICE_FILE_SYSTEM_DRIVER     0x00000002
#define SERVICE_ADAPTER                0x00000004
#define SERVICE_RECOGNIZER_DRIVER      0x00000008
#define SERVICE_INTERACTIVE_PROCESS    0x00000100
//start type
#define SERVICE_BOOT_START             0x00000000 //for driver services
#define SERVICE_SYSTEM_START           0x00000001 //for driver services
#define SERVICE_AUTO_START             0x00000002
#define SERVICE_DEMAND_START           0x00000003
#define SERVICE_DISABLED               0x00000004
//error control
#define SERVICE_ERROR_IGNORE           0x00000000
#define SERVICE_ERROR_NORMAL           0x00000001
#define SERVICE_ERROR_SEVERE           0x00000002
#define SERVICE_ERROR_CRITICAL         0x00000003


//errors
#define ERROR_ACCESS_DENIED                 000000005
#define ERROR_DATABASE_DOES_NOT_EXIST       000001065
#define ERROR_INVALID_PARAMETER             000000087
#define ERROR_CIRCULAR_DEPENDENCY           000001059
#define ERROR_DUP_NAME                      000000052
#define ERROR_INVALID_HANDLE                000000006
#define ERROR_INVALID_NAME                  000000123
#define ERROR_INVALID_SERVICE_ACCOUNT       000001057
#define ERROR_SERVICE_EXISTS                000001073
#define ERROR_PATH_NOT_FOUND                000000003
#define ERROR_SERVICE_ALREADY_RUNNING       000001056
#define ERROR_SERVICE_DATABASE_LOCKED       000001055
#define ERROR_SERVICE_DEPENDENCY_DELETED    000001075
#define ERROR_SERVICE_DEPENDENCY_FAIL       000001068
#define ERROR_SERVICE_DISABLED              000001058
#define ERROR_SERVICE_LOGON_FAILED          000001069
#define ERROR_SERVICE_MARKED_FOR_DELETE     000001072
#define ERROR_SERVICE_NO_THREAD             000001054
#define ERROR_SERVICE_REQUEST_TIMEOUT       000001053
#define ERROR_SERVICE_DOES_NOT_EXIST        000001060
#define ERROR_DEPENDENT_SERVICES_RUNNING    000001051
#define ERROR_INVALID_SERVICE_CONTROL       000001052
#define ERROR_SERVICE_CANNOT_ACCEPT_CTRL    000001061
#define ERROR_SERVICE_NOT_ACTIVE            000001062

// User defined errors
#define IS_ERROR_SERVICE_FAILED_TO_STOP     000010001
#define IS_ERROR_STORE_UNINSTALL_INFO       000010002
#define IS_ERROR_UPDATEUNINSTSTRING         000010003
#define IS_ERROR_INIT_VARS                  000010004

// User defined functions
prototype ISOpenServiceManager();
prototype ISOpenService(HWND);
prototype ISInitializeVars(STRING);
prototype ISQueryServices();
prototype ISQueryService(STRING);
prototype ISInstallServices();
prototype ISInstallService(STRING);
prototype ISStoreUninstallInfo();
prototype ISUpdateUninstallString();
prototype ISHandleServiceError(LONG);
prototype ISAddFolderIcon ( STRING, STRING, STRING, STRING, STRING, NUMBER, STRING, NUMBER );
prototype ISReplacePathSymbols ( STRING, BYREF STRING, STRING, STRING );
prototype ISSubstPathForSymbol ( STRING, BYREF STRING, STRING, STRING );
prototype ISConvertStringToConstant(STRING, BYREF LONG);
prototype ISStartService(STRING, HWND);

// Windows APIs
prototype HWND Advapi32.OpenSCManagerA(POINTER, POINTER, LONG);
prototype HWND Advapi32.CreateServiceA(HWND, POINTER, POINTER, LONG, LONG, LONG, LONG,
    POINTER, POINTER, POINTER, POINTER, POINTER, POINTER);
prototype BOOL Advapi32.ChangeServiceConfigA(HWND, LONG, LONG, LONG, POINTER, POINTER,
    POINTER, POINTER, POINTER, POINTER, POINTER);
prototype HWND Advapi32.DeleteServiceA(HWND);

prototype HWND Advapi32.OpenServiceA(HWND, POINTER, LONG);
prototype LONG ADVAPI32.CloseServiceHandle( HWND );
prototype LONG KERNEL.GetLastError();
prototype BOOL Advapi32.ControlService (HWND, LONG, POINTER);
prototype BOOL Advapi32.QueryServiceStatus(HWND, POINTER);
prototype BOOL Advapi32.StartServiceA(HWND, LONG, POINTER);

#define IS_INI_WINSYSDIR                "<WINSYSDIR>"
#define IS_INI_WINDIR                   "<WINDIR>"
#define IS_INI_PROGRAMFILES             "<PROGRAMFILES>"
#define IS_INI_TARGETDIR                "<TARGETDIR>"
#define IS_INI_COMMONFILES              "<COMMONFILES>"
#define IS_ERRNO_UNKNOWNSYMBOL           -2

// Global variables
BOOL    bServiceExists;
POINTER pszServiceName;
POINTER pszDisplayName;
LONG    nDesiredAccess;
LONG    nServiceType;
LONG    nStartType;
LONG    nErrorControl;
POINTER pszBinaryPathName;
POINTER pszLoadOrderGroup;
POINTER pszTagID;
POINTER pszDependencies;
POINTER pszServiceStartName;
POINTER pszPassword;

 STRING  szDisplayName;
 STRING  szBinaryPathName;   // service's binary
 STRING  szLoadOrderGroup;
 STRING  szTagID;
 STRING  szDependencies;
 STRING  szServiceStartName;
 STRING  szPassword;
 STRING  szINIFileName;
 STRING  szServiceName;
 STRING  svKey;
 STRING  svServiceComponent;














