/******************************************************************************
** 
**  $Id: cryptoki.h,v 1.2 2003/02/13 20:06:37 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Main cryptoki header (include in all source files)
** 
******************************************************************************/

#ifndef __CRYPTOKI_H__
#define __CRYPTOKI_H__

/******************************************************************************
** Include all "standard" RSA PKCS #11 headers
******************************************************************************/
#ifndef WIN32
#include "cryptoki_unix.h"
#else
#include "cryptoki_win32.h"
#endif


/******************************************************************************
** Regular headers
******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>

#ifndef __APPLE__
#include <musclecard.h>
#else
#include <PCSC/wintypes.h>
#include <PCSC/musclecard.h>
#endif

#include <sys/types.h>


/******************************************************************************
** Logging (OSX needs -no-cpp-precomp for VA_ARGS to work)
******************************************************************************/
#ifndef NO_LOG
#define P11_LOG_START(x)        log_Start(x)
#define P11_LOG_END(x)          log_End(x,rv)
#define P11_ERR(x)              log_Err(x,__FILE__,__LINE__)
/* #define P11_LOG(x,...)          log_Log(x,##__VA_ARGS__) */
#else
#define P11_LOG_START(x)
#define P11_LOG_END(x)
#define P11_ERR(x)
/* #define P11_LOG(x,...) */
#endif

#define LOG_LOW     0
#define LOG_MED     5
#define LOG_HIGH    9


/******************************************************************************
** Error checking macros
******************************************************************************/
#define PCSC_ERROR_NOLOG(x)    ((x) != SCARD_S_SUCCESS)
#define CKR_ERROR_NOLOG(x)     ((x) != CKR_OK)
#define MSC_ERROR_NOLOG(x)     ((x) != MSC_SUCCESS)

#ifndef NO_LOG
#define PCSC_ERROR(x)       ((error_LogCmd((x),SCARD_S_SUCCESS,(CK_CHAR*)__FILE__,__LINE__,pcsc_stringify_error)) != SCARD_S_SUCCESS)
#define CKR_ERROR(x)        ((error_LogCmd((x),CKR_OK,(CK_CHAR*)__FILE__,__LINE__,error_Stringify)) != CKR_OK)
#define MSC_ERROR(x)        ((error_LogCmd((x),MSC_SUCCESS,(CK_CHAR*)__FILE__,__LINE__,msc_error)) != MSC_SUCCESS)
#else
#define PCSC_ERROR(x)       PCSC_ERROR_NOLOG(x)
#define CKR_ERROR(x)        CKR_ERROR_NOLOG(x)
#define MSC_ERROR(x)        MSC_ERROR_NOLOG(x)
#endif

#define INVALID_SLOT        ((!st.slots) || (!slotID) || (slotID > st.slot_count))
#define INVALID_SESSION     (!(hSession && (((P11_Session *)hSession)->check == (P11_Session *)hSession)))
#define INVALID_OBJECT      (!(hObject && (((P11_Object *)hObject)->check == (P11_Object *)hObject)))
#define READ_ONLY_SESSION   0 /* Fixme: implement this */
#define USER_MODE           (hSession && ((((P11_Session *)hSession)->session.state == CKS_RO_USER_FUNCTIONS) || (((P11_Session *)hSession)->session.state == CKS_RW_USER_FUNCTIONS)))


/******************************************************************************
** Utility macros
******************************************************************************/
#ifndef max
#define max(a,b)            (((a)>(b))?(a):(b))
#endif
#ifndef min
#define min(a,b)            (((a)<(b))?(a):(b))
#endif

#ifdef SUPERDEBUG
#define malloc(a)           debug_Malloc((a), __LINE__, __FILE__)
#define calloc(a,b)         debug_Calloc((a)*(b), __LINE__, __FILE__)
#define free(a)             debug_Free((a), __LINE__, __FILE__)
#endif

#define P11_MAX_ULONG ((CK_ULONG)(~0))

/* Preference settings */
#define P11_SLOT_WATCH_THREAD_FULL          0
#define P11_SLOT_WATCH_THREAD_PARTIAL       1
#define P11_OBJ_SORT_NEWEST_FIRST           0
#define P11_OBJ_SORT_NEWEST_LAST            1
#define P11_DEFAULT_ATTRIB_OBJ_SIZE       512
#define P11_DEFAULT_PRK_ATTRIB_OBJ_SIZE   912
#define P11_DEFAULT_CERT_ATTRIB_OBJ_SIZE  712
#define P11_DEFAULT_LOG_FILENAME          "PKCS11.log"


/******************************************************************************
** Library information
******************************************************************************/
#define PKCS11_MAJOR            0x02
#define PKCS11_MINOR            0x0b
#define PKCS11_LIB_MAJOR        0x01
#define PKCS11_LIB_MINOR        0x00
#define PKCS11_MFR_ID           "SCHLUMBERGER"
#define PKCS11_DESC             "SLB PKCS #11 module"
#define PKCS11_MAX_PIN_TRIES    8
#define PKCS11_SO_USER_PIN      0
#define PKCS11_USER_PIN         1


/******************************************************************************
** P11 typedefs
******************************************************************************/

typedef void* P11_Mutex;

/* PKCS #11 mechanism info list */
typedef struct _P11_MechInfo
{
    CK_MECHANISM_TYPE type;         /* Mechanism type   */
    CK_MECHANISM_INFO info;         /* Mechanism info   */

    struct _P11_MechInfo *prev;
    struct _P11_MechInfo *next;
} P11_MechInfo;

/* PKCS #11 object attribute */
typedef struct _P11_Attrib
{
    CK_ATTRIBUTE attrib;            /* Object attribute data        */
    CK_BBOOL token;                 /* Store attribute on token?    */

    struct _P11_Attrib *prev;
    struct _P11_Attrib *next;
} P11_Attrib;

/* A PKCS #11 object (session or on-card) */
typedef struct _P11_Object
{
    CK_SESSION_HANDLE session;      /* Session that owns this object. Not used with token objects   */
    P11_Attrib *attrib;             /* List of attributes                                           */
    MSCObjectInfo *msc_obj;         /* On-token object info.  Not used with session objects         */
    MSCKeyInfo *msc_key;            /* On-token key info.  Not used with session objects            */
    CK_ULONG sensitive;             /* True/false if this object is PIN protected                   */

    struct _P11_Object *prev;
    struct _P11_Object *next;
    struct _P11_Object *check;      /* Should contain the memory address of this structure  */
} P11_Object;

/* Cached PIN */
typedef struct _P11_Pin
{
    CK_BYTE pin[256];               /* Fixme: don't hardcode, use MAX_Musclecard_PIN)   */
    CK_ULONG pin_size;
} P11_Pin;

/* A card reader.  */
typedef struct
{
    CK_ULONG pin_state;             /* NONE (0), USER (1), SO (2)        */
    CK_SLOT_INFO slot_info;         /* CK slot structure                 */
    CK_TOKEN_INFO token_info;       /* CK token structure                */
    P11_Object *objects;            /* List of objects                   */
    P11_MechInfo *mechanisms;       /* List of mechanisms                */
    P11_Pin pins[2];                /* Array of cached PIN's             */
    MSCStatusInfo status_info;      /* Status of token                   */
    MSCTokenConnection conn;        /* Connection to token               */
} P11_Slot;

/* A session with one slot.  */
typedef struct _P11_Session
{
    CK_SESSION_INFO session;        /* CK session info                   */
    CK_VOID_PTR application;        /* Passed to notify callback         */
    CK_NOTIFY notify;               /* Notify callback                   */

    P11_Object *search_object;      /* Current object (used with C_FindObjects) */
    CK_ATTRIBUTE *search_attrib;    /* Current search attributes                */
    CK_ULONG search_attrib_count;   /* Current search attribute count           */

    CK_MECHANISM sign_mech;         /* Active signing mechanism */
    CK_OBJECT_HANDLE sign_key;      /* Active signing key       */

    struct _P11_Session *prev;
    struct _P11_Session *next;
    struct _P11_Session *check;     /* Should contain the memory address of this structure  */
} P11_Session;

/* Preferences. */
typedef struct _P11_Preferences
{
    CK_ULONG multi_app;
    CK_ULONG threaded;
    CK_ULONG log_level;
    CK_ULONG obj_sort_order;
    CK_ULONG slot_watch_scheme;
    CK_ULONG cache_pin;
    CK_ULONG version_major;
    CK_ULONG version_minor;
    CK_ULONG max_pin_tries;
    CK_ULONG so_user_pin_num;
    CK_ULONG user_pin_num;
    CK_ULONG cert_attrib_size;
    CK_ULONG pubkey_attrib_size;
    CK_ULONG prvkey_attrib_size;
    CK_ULONG data_attrib_size;
    CK_ULONG disable_security;
    CK_CHAR log_filename[256];
} P11_Preferences;

/* Master PKCS #11 module state information */
typedef struct
{
    CK_ULONG initialized;           /* Has Cryptoki been intialized                              */
    P11_Preferences prefs;          /* Preferences                                               */
    P11_Slot *slots;                /* Array of all slots                                        */
    CK_ULONG slot_count;            /* Number of slots in array                                  */
    P11_Session *sessions;          /* List of all sessions with all slots                       */
    char *slot_status;              /* Says if the token state changed for a slot                */
    P11_Mutex log_lock;             /* Log mutex                                                 */
    P11_Mutex async_lock;           /* Asychronous mutex                                         */
    CK_ULONG native_locks;
    CK_ULONG create_threads;
} P11_State;

/* Global state variable : see p11x_state.c */
extern P11_State st;

/******************************************************************************
** Prototypes (extensions in addition to the standard PKCS #11 functions)
******************************************************************************/

/* p11x_async.c */
CK_RV async_StartSlotWatcher();
CK_RV async_StopSlotWatcher();
 void *async_WatchSlots(void *parent_pid);
 void async_SignalHandler(int sig);
MSCULong32 async_TokenEventCallback(MSCTokenInfo *tokenInfo, MSCULong32 len, void *data);

/* p11x_debug.c */
void debug_Init();
void debug_CheckCorrupt(size_t i);
void debug_Check();
void *debug_Malloc(size_t size, int line, char *file);
void debug_Free(void *ptr, int line, char *file);
void *debug_Calloc(size_t size, int line, char *file);

/* p11x_error.c */
CK_RV error_LogCmd(CK_RV err, CK_RV cond, CK_CHAR *file, CK_LONG line, char *(*stringifyFn)(CK_RV));
 char *error_Stringify(CK_RV rv);

/* p11x_log.c */
void log_Start(char *func);
void log_End(char *func, CK_RV rv);
void log_Err(char *msg, char *file, CK_LONG line);
void log_Log(CK_ULONG level, char *format, ...);

/* p11x_object.c */
 void object_FreeAllObjects(CK_SLOT_ID slotID, P11_Object *list);
 void object_FreeObject(CK_SLOT_ID slotID, P11_Object *object);
 void object_FreeAllAttributes(P11_Attrib *list);
CK_RV object_AddObject(CK_SLOT_ID slotID, CK_OBJECT_HANDLE *phObject);
CK_RV object_UpdateKeyInfo(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE *hObject, MSCKeyInfo *pKeyInfo);
CK_RV object_UpdateObjectInfo(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE *hObject, MSCObjectInfo *pObjectInfo);
CK_RV object_FreeTokenObjects();
CK_RV object_AddAttribute(P11_Object *object, CK_ATTRIBUTE_TYPE type, CK_BBOOL token, CK_BYTE *value, CK_ULONG value_len, P11_Attrib **attrib);
CK_RV object_MatchAttrib(CK_ATTRIBUTE *attrib, P11_Object *object);
CK_RV object_TemplateGetAttrib(CK_ATTRIBUTE_TYPE type, CK_ATTRIBUTE *attrib, CK_ULONG attrib_count, CK_ATTRIBUTE **attrib_out);
CK_RV object_RSAGenKeyPair(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE *pPublicKeyTemplate, CK_ULONG ulPublicKeyAttributeCount, CK_ATTRIBUTE *pPrivateKeyTemplate, CK_ULONG ulPrivateKeyAttributeCount, CK_OBJECT_HANDLE *phPublicKey, CK_OBJECT_HANDLE *phPrivateKey);
CK_RV object_GetAttrib(CK_ATTRIBUTE_TYPE type, P11_Object *object, P11_Attrib **attrib);
CK_RV object_SetAttrib(P11_Object *object, CK_ATTRIBUTE *attrib);
void object_LogObjects(CK_SLOT_ID slotID);
void object_LogObject(P11_Object *object);
void object_LogAttribute(CK_ATTRIBUTE *attrib);
CK_RV object_AddBoolAttribute(CK_ATTRIBUTE_TYPE type, CK_BBOOL value, P11_Object *object);
 void object_BinToHex(CK_BYTE *data, CK_ULONG data_len, CK_BYTE *out);
CK_RV object_AddAttributes(P11_Object *object, CK_BYTE *data, CK_ULONG len);
CK_RV object_ReadAttributes(CK_SESSION_HANDLE hSession, CK_BYTE *obj_id, P11_Object *object);
CK_RV object_InferAttributes(CK_SESSION_HANDLE hSession, P11_Object *object);
CK_RV object_InferKeyAttributes(CK_SESSION_HANDLE hSession, P11_Object *object);
CK_RV object_InferObjAttributes(CK_SESSION_HANDLE hSession, P11_Object *object);
CK_RV object_WriteAttributes(CK_SESSION_HANDLE hSession, P11_Object *object);
CK_RV object_InferClassAttributes(CK_SESSION_HANDLE hSession, P11_Object *object);
CK_RV object_GetCertIssuer(CK_BYTE *cert, CK_ULONG cert_size, CK_BYTE *out, CK_ULONG *out_len);
CK_RV object_GetCertSubject(CK_BYTE *cert, CK_ULONG cert_size, CK_BYTE *out, CK_ULONG *out_len);
CK_RV object_GetCertSerial(CK_BYTE *cert, CK_ULONG cert_size, CK_BYTE *out, CK_ULONG *out_len);
CK_RV object_GetCertModulus(CK_BYTE *cert, CK_ULONG cert_size, CK_BYTE *out, CK_ULONG *out_len);
CK_RV object_GetCertPubExponent(CK_BYTE *cert, CK_ULONG cert_size, CK_BYTE *out, CK_ULONG *out_len);
CK_RV object_CreateCertificate(CK_SESSION_HANDLE hSession, P11_Object *object);
CK_RV object_CreatePublicKey(CK_SESSION_HANDLE hSession, P11_Object *object);
CK_RV object_CreatePrivateKey(CK_SESSION_HANDLE hSession, P11_Object *object);
CK_RV object_CreateObject(CK_SESSION_HANDLE hSession, P11_Object *object);
CK_ULONG object_MapPIN(CK_ULONG pinNum);
CK_ULONG object_UserMode(CK_SESSION_HANDLE hSession);

/* p11x_prefs.c */
void util_ParsePreference(char *buf, CK_ULONG buf_size);
CK_RV util_ReadPreferences();

/* p11x_state.c */
CK_RV state_Init();
CK_RV state_Free();

/* p11x_slot.c */
   CK_RV slot_BeginTransaction(CK_ULONG slotID);
   CK_RV slot_EndTransaction(CK_ULONG slotID, CK_ULONG action);
CK_BBOOL slot_CheckRWSOsession(CK_ULONG slotID);
   CK_RV slot_EstablishConnection(CK_ULONG slotID);
   CK_RV slot_ReleaseConnection(CK_ULONG slotID);
   CK_RV slot_UpdateSlot(CK_ULONG slotID);
   CK_RV slot_VerifyPIN(CK_SLOT_ID slotID, CK_USER_TYPE userType, CK_UTF8CHAR_PTR pPin, CK_ULONG ulPinLen);
CK_ULONG slot_MinRSAKeySize(MSCULong32 cap);
CK_ULONG slot_MaxRSAKeySize(MSCULong32 cap);
   CK_RV slot_UpdateMechanisms(CK_ULONG slotID);
CK_ULONG slot_MechanismCount(P11_MechInfo *mech);
    void slot_FreeAllMechanisms(P11_MechInfo *list);
   CK_RV slot_AddMechanism(P11_Slot *slot, CK_MECHANISM_TYPE type, P11_MechInfo **mech_info);
   CK_RV slot_UpdateToken(CK_ULONG slotID);
   CK_RV slot_UpdateSlotList();
   CK_RV slot_FreeAllSlots();
   CK_RV slot_DisconnectSlot(CK_ULONG slotID, CK_ULONG action);
   CK_RV slot_PublicMode(CK_ULONG slotID);
   CK_RV slot_UserMode(CK_ULONG slotID);
   CK_RV slot_TokenPresent(CK_ULONG slotID);
   CK_RV slot_TokenChanged();
   CK_RV slot_AsyncUpdateSlot();
    void slot_BlankTokenInfo(CK_TOKEN_INFO *token_info);
   CK_RV slot_ReverifyPins();

/* p11x_session.c */
CK_RV session_AddSession(CK_SESSION_HANDLE *phSession);
CK_RV session_FreeSession(CK_SESSION_HANDLE hSession);

/* p11x_thread_xxx.c */
CK_RV thread_Initialize();
CK_RV thread_InitFunctions(CK_CREATEMUTEX fn_init,
                           CK_DESTROYMUTEX fn_destroy,
                           CK_LOCKMUTEX fn_lock,
                           CK_UNLOCKMUTEX fn_unlock);
CK_RV thread_Finalize();
CK_RV thread_MutexInit(P11_Mutex *mutex);
CK_RV thread_MutexDestroy(P11_Mutex mutex);
CK_RV thread_MutexLock(P11_Mutex mutex);
CK_RV thread_MutexUnlock(P11_Mutex mutex);

/* p11x_util.c */
    void util_byterev(CK_BYTE *data, CK_ULONG len);
CK_ULONG util_strpadlen(CK_CHAR *string, CK_ULONG max_len);
   CK_RV util_PadStrSet(CK_CHAR *string, CK_CHAR *value, CK_ULONG size);
   CK_RV util_StripPKCS1(CK_BYTE *data, CK_ULONG len, CK_BYTE *output, CK_ULONG *out_len);
#ifndef __USE_GNU
#ifndef WIN32
   size_t strnlen(__const char *__string, size_t __maxlen);
#else
   size_t strnlen(const char *__string, size_t __maxlen);
#endif
#endif /* __USE_GNU */
CK_BBOOL util_IsLittleEndian();

/* p11x_msc.c */
#include "p11x_msc.h"


/******************************************************************************
** dmalloc debugging
******************************************************************************/
#ifdef DMALLOC
#include "dmalloc.h"
#endif


#endif /* __CRYPTOKI_H__ */
