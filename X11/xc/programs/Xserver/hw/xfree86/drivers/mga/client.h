#ifndef _CLIENT
#define _CLIENT

#include "binding.h"

#if defined(__cplusplus)
    extern "C" {
#endif

typedef void* LPBIOSREGS;
#define ASSERT_HANDLER(pBoard)

typedef struct TAGCLIENTDATA
{
    ULONG          ulFrameBufferBase;
    ULONG          ulRegisterBase;
    pointer        pMga;
} CLIENTDATA, *LPCLIENTDATA;

extern ULONG DECL ClientReadConfigSpaceByte(LPBOARDHANDLE, ULONG, LPUCHAR);
extern ULONG DECL ClientReadConfigSpaceDword(LPBOARDHANDLE , ULONG, LPULONG );
extern ULONG DECL ClientWriteConfigSpaceByte (LPBOARDHANDLE , ULONG, UCHAR);
extern ULONG DECL ClientWriteConfigSpaceDword(LPBOARDHANDLE , ULONG, ULONG);
extern ULONG DECL ClientOpenRegisterBase(LPBOARDHANDLE , ULONG, ULONG);
extern ULONG DECL ClientCloseRegisterBase (LPBOARDHANDLE );
extern ULONG DECL ClientReadRegisterByte(LPBOARDHANDLE , ULONG, LPUCHAR );
extern ULONG DECL ClientReadRegisterDword(LPBOARDHANDLE , ULONG, LPULONG );
extern ULONG DECL ClientWriteRegisterByte (LPBOARDHANDLE , ULONG, UCHAR);
extern ULONG DECL ClientWriteRegisterDword(LPBOARDHANDLE , ULONG, ULONG);
extern ULONG DECL ClientOpenMemoryBase(LPBOARDHANDLE , ULONG, ULONG);
extern ULONG DECL ClientCloseMemoryBase(LPBOARDHANDLE );
extern ULONG DECL ClientReadMemoryByte(LPBOARDHANDLE , ULONG, LPUCHAR );
extern ULONG DECL ClientReadMemoryDword (LPBOARDHANDLE , ULONG, LPULONG );
extern ULONG DECL ClientWriteMemoryByte(LPBOARDHANDLE , ULONG, UCHAR);
extern ULONG DECL ClientWriteMemoryDword (LPBOARDHANDLE , ULONG, ULONG);
extern ULONG DECL ClientOpenSystemDevice (LPBOARDHANDLE , ULONG, ULONG);
extern ULONG DECL ClientCloseSystemDevice (LPBOARDHANDLE );
extern ULONG DECL ClientReadSystemDeviceByte(LPBOARDHANDLE , ULONG, LPUCHAR );
extern ULONG DECL ClientReadSystemDeviceDword(LPBOARDHANDLE , ULONG, LPULONG );
extern ULONG DECL ClientWriteSystemDeviceByte(LPBOARDHANDLE , ULONG, UCHAR);
extern ULONG DECL ClientWriteSystemDeviceDword (LPBOARDHANDLE , ULONG, ULONG);
extern ULONG DECL ClientWait (LPBOARDHANDLE , ULONG);
extern ULONG DECL ClientLocateFirstTwisterOfQuad(ULONG);
extern ULONG DECL ClientSearchDevNode(ULONG, UCHAR, UCHAR);
extern ULONG DECL ClientGetBiosInfo(LPBOARDHANDLE, LPUCHAR, LPULONG );
extern ULONG DECL ClientDebug (LPBOARDHANDLE , ULONG, LPUCHAR, ULONG, LPVOID);
extern ULONG DECL ClientCallBiosInt10(LPBOARDHANDLE, LPBIOSREGS);
extern ULONG DECL ClientReadDDCEdid(LPBOARDHANDLE, LPUCHAR, ULONG);
extern ULONG DECL ClientCustomCall(LPBOARDHANDLE, ULONG, LPVOID, LPVOID);
extern ULONG DECL ClientApplyEpromPatch(LPBOARDHANDLE);
extern ULONG DECL ClientDetectHostInterface(LPBOARDHANDLE, LPULONG);
extern ULONG DECL ClientHSLPatchFunction(LPBOARDHANDLE, ULONG, ULONG, LPUCHAR);
extern ULONG DECL InitClientFunctions(LPBOARDHANDLE, ULONG);
extern ULONG DECL ClientInitTimeBase(LPBOARDHANDLE);
extern ULONG DECL ClientOpenDMABase(LPBOARDHANDLE, ULONG, ULONG);
extern ULONG DECL ClientReadDMAByte(LPBOARDHANDLE, ULONG, LPUCHAR);
extern ULONG DECL ClientReadBIOS(LPBOARDHANDLE, ULONG, ULONG, ULONG, ULONG, LPUCHAR);
extern ULONG DECL ClientWriteBIOS(LPBOARDHANDLE, ULONG, ULONG, ULONG, ULONG);

#if defined(__cplusplus)
    }
#endif

#define ESC_CUSTOM_SET_FUNCTION_PTR     0x80000200
#define ESC_CUSTOM_GET_FUNCTION_PTR     0x80000201
#define ESC_CUSTOM_PATCH_HSL            0x80000202

#endif
