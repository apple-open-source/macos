/******************************************************************************\

    clientlx.c

    Copyright © 1997, Matrox Graphics Inc.

    All Rights Reserved.

\******************************************************************************/
#include "xf86_OSproc.h"
#include "xf86Pci.h"
#include "xf86_ansic.h"
#include "client.h"
#include "mga.h"

CLIENTTABLE ClientFunctions = {
    (ULONG (DECL *)(LPVOID,ULONG,LPVOID))    ClientReadConfigSpaceByte,
    (ULONG (DECL *)(LPVOID,ULONG,LPVOID))    ClientReadConfigSpaceDword,
    (ULONG (DECL *)(LPVOID,ULONG,UCHAR))     ClientWriteConfigSpaceByte,
    (ULONG (DECL *)(LPVOID,ULONG,ULONG))     ClientWriteConfigSpaceDword,
    (ULONG (DECL *)(LPVOID,ULONG,ULONG))     ClientOpenRegisterBase,
    (ULONG (DECL *)(LPVOID))                 ClientCloseRegisterBase,
    (ULONG (DECL *)(LPVOID,ULONG,LPVOID))    ClientReadRegisterByte,
    (ULONG (DECL *)(LPVOID,ULONG,LPVOID))    ClientReadRegisterDword,
    (ULONG (DECL *)(LPVOID,ULONG,UCHAR))     ClientWriteRegisterByte,
    (ULONG (DECL *)(LPVOID,ULONG,ULONG))     ClientWriteRegisterDword,
    (ULONG (DECL *)(LPVOID,ULONG,ULONG))     ClientOpenMemoryBase,
    (ULONG (DECL *)(LPVOID))                 ClientCloseMemoryBase,
    (ULONG (DECL *)(LPVOID,ULONG,LPVOID))    ClientReadMemoryByte,
    (ULONG (DECL *)(LPVOID,ULONG,LPVOID))    ClientReadMemoryDword,
    (ULONG (DECL *)(LPVOID,ULONG,UCHAR))     ClientWriteMemoryByte,
    (ULONG (DECL *)(LPVOID,ULONG,ULONG))     ClientWriteMemoryDword,
    (ULONG (DECL *)(LPVOID,ULONG,ULONG))     ClientOpenSystemDevice,
    (ULONG (DECL *)(LPVOID))                 ClientCloseSystemDevice,
    (ULONG (DECL *)(LPVOID,ULONG,LPVOID))    ClientReadSystemDeviceByte,
    (ULONG (DECL *)(LPVOID,ULONG,LPVOID))    ClientReadSystemDeviceDword,
    (ULONG (DECL *)(LPVOID,ULONG,UCHAR))     ClientWriteSystemDeviceByte,
    (ULONG (DECL *)(LPVOID,ULONG,ULONG))     ClientWriteSystemDeviceDword,
    (ULONG (DECL *)(LPVOID,ULONG))           ClientWait,
    (ULONG (DECL *)(LPVOID,LPVOID,LPVOID))   ClientGetBiosInfo,
    (ULONG (DECL *)(LPVOID,LPVOID,ULONG))    ClientReadDDCEdid
};

/******************************************************************************\

    Function      : ClientReadConfigSpaceByte

    Description   : Read a Byte from the configuration space.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specifib board.
                    ULONG ulOffset | Offset of the Byte to be read.
                    UCHAR *pucByte | pointer to a byte that will receive
                    the data

    Return Val : ULONG

\******************************************************************************/
ULONG ClientReadConfigSpaceByte(LPBOARDHANDLE pBoard, ULONG ulOffset,
                                UCHAR *pucByte)
{
    LPCLIENTDATA pClientStruct = MGAGetClientPointer(pBoard);
    MGAPtr pMga = (MGAPtr)pClientStruct->pMga;

    ASSERT_HANDLER(pBoard);

    *pucByte = pciReadByte(pMga->PciTag,ulOffset);

    return 0;
}


/******************************************************************************\

    Function      : ClientReadConfigSpaceDword

    Description   : Read a Dword from the configuration space.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulOffset  | Offset of the Byte to be read.
                    ULONG *pulDword | Dword to receive the data

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientReadConfigSpaceDword(LPBOARDHANDLE pBoard, ULONG ulOffset,
                                 ULONG *pulDword)
{
    LPCLIENTDATA pClientStruct = MGAGetClientPointer(pBoard);
    MGAPtr pMga = (MGAPtr)pClientStruct->pMga;

    ASSERT_HANDLER(pBoard);

    *pulDword = pciReadLong(pMga->PciTag,ulOffset);

    return 0;
}


/******************************************************************************\

    Function      : ClientWriteConfigSpaceByte

    Description   : Write a Byte from the configuration space.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulOffset | Offset of the Byte to be read.
                    UCHAR ucByte   | Byte to receive the data

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientWriteConfigSpaceByte(LPBOARDHANDLE pBoard, ULONG ulOffset,
                                 UCHAR ucByte)
{
    LPCLIENTDATA pClientStruct = MGAGetClientPointer(pBoard);
    MGAPtr pMga = (MGAPtr)pClientStruct->pMga;

    ASSERT_HANDLER(pBoard);

    pciWriteByte(pMga->PciTag,ulOffset, ucByte);

    return 0;
}


/******************************************************************************\

    Function      : ClientWriteConfigSpaceDword

    Description   : Write a Dword from the configuration space.

    I/O Desc.     : LPBOARDHANDLE pBoard     | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulOffset | Offset of the Byte to be read.
                    ULONG ulDword  | Dword containing the data to  be written

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientWriteConfigSpaceDword(LPBOARDHANDLE pBoard, ULONG ulOffset,
                                  ULONG ulDword)
{
    LPCLIENTDATA pClientStruct = MGAGetClientPointer(pBoard);
    MGAPtr pMga = (MGAPtr)pClientStruct->pMga;

    ASSERT_HANDLER(pBoard);

    pciWriteLong(pMga->PciTag,ulOffset, ulDword);

    return 0;
}


/******************************************************************************\

    Function      : ClientOpenRegisterBase

    Description   : Map the register base for future call to ClientReadRegisterX
                    and ClientWriteRegisterX.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulAddress | Physical address of the Register aperture
                    ULONG ulSize    | Size in Byte of the Register Aperture

    Return Val : ULONG

\******************************************************************************/
ULONG ClientOpenRegisterBase(LPBOARDHANDLE pBoard, ULONG ulAddress, ULONG ulSize)
{
    LPCLIENTDATA pClientStruct = MGAGetClientPointer(pBoard);
    MGAPtr pMga = (MGAPtr)pClientStruct->pMga;

    ASSERT_HANDLER(pBoard);

    pClientStruct->ulRegisterBase = (ULONG) pMga->IOBase;

    return 0;
}


/******************************************************************************\

    Function      : ClientCloseRegisterBase

    Description   : Unmap the register base address and free resources needed
                    to address it.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.

    Return Val : ULONG

\******************************************************************************/
ULONG ClientCloseRegisterBase(LPBOARDHANDLE pBoard)
{
    ASSERT_HANDLER(pBoard);
    return 0;
}


/******************************************************************************\

    Function      : ClientReadRegisterByte

    Description   : Read a byte from the Register space.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure.
                    containing all the information about a specific board.
                    ULONG ulOffset | Offset of the Byte to be read.
                    UCHAR *pucByte | pointer to the byte that will receive
                    the data.

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientReadRegisterByte(LPBOARDHANDLE pBoard, ULONG ulOffset,
                             UCHAR *pucByte)
{
    LPCLIENTDATA pClientStruct = MGAGetClientPointer(pBoard);

    ASSERT_HANDLER(pBoard);

    *pucByte = *((UCHAR *)(pClientStruct->ulRegisterBase + ulOffset));

    return 0;
}


/******************************************************************************\

    Function      : ClientReadRegisterDword

    Description   : Read a Dword from the Register space.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulOffset  | Offset of the Byte to be read.
                    ULONG *pulDword | pointer to the dword that will receive
                    the data.

    Return Val : ULONG

\******************************************************************************/
ULONG ClientReadRegisterDword(LPBOARDHANDLE pBoard, ULONG ulOffset,
                              ULONG *pulDword)
{
    LPCLIENTDATA pClientStruct = MGAGetClientPointer(pBoard);

    ASSERT_HANDLER(pBoard);

    *pulDword = *((ULONG *)(pClientStruct->ulRegisterBase + ulOffset));

    return 0;
}


/******************************************************************************\

    Function      : ClientWriteRegisterByte

    Description   : Write a Byte from the Register space.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulOffset | Offset of the Byte to be read.
                    UCHAR ucByte   | CHAR to receive the data.

    Return Val : ULONG

\******************************************************************************/
ULONG ClientWriteRegisterByte(LPBOARDHANDLE pBoard, ULONG ulOffset,
                              UCHAR ucByte)
{
    LPCLIENTDATA pClientStruct = MGAGetClientPointer(pBoard);

    ASSERT_HANDLER(pBoard);

    *((UCHAR *)(pClientStruct->ulRegisterBase + ulOffset)) = ucByte;

    return 0;
}


/******************************************************************************\

    Function      : ClientWriteRegisterSpaceDword

    Description   : Write a Dword from the Register space.

    I/O Desc.     : LPBOARDHANDLE *| pBoard  | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulOffset | Offset of the Byte to be read.
                    ULONG ulDword  | Dword to receive the data

    Return Val : ULONG

\******************************************************************************/
ULONG ClientWriteRegisterDword(LPBOARDHANDLE pBoard, ULONG ulOffset,
                               ULONG ulDword)
{
    LPCLIENTDATA pClientStruct = MGAGetClientPointer(pBoard);

    ASSERT_HANDLER(pBoard);

    *((ULONG *)(pClientStruct->ulRegisterBase + ulOffset)) = ulDword;

    return 0;
}


/******************************************************************************\

    Function      : ClientOpenMemoryBase

    Description   : Map the Memory base for future call to ClientReadMemoryX
                    and ClientWriteMemoryX.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulAddress | Physical address of the Register aperture
                    ULONG ulSize    | Size in Byte of the Register Aperture
    Return Val : ULONG

\******************************************************************************/
ULONG ClientOpenMemoryBase(LPBOARDHANDLE pBoard, ULONG ulAddress, ULONG ulSize)
{
    LPCLIENTDATA pClientStruct = MGAGetClientPointer(pBoard);
    MGAPtr pMga = (MGAPtr) pClientStruct->pMga;

    ASSERT_HANDLER(pBoard);

    pClientStruct->ulFrameBufferBase = (ULONG) pMga->FbBase;

    return 0;
}


/******************************************************************************\

    Function      : ClientCloseMemoryBase

    Description   : Unmap the Frame Buffer aperture and free resources
                    needed to address it.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientCloseMemoryBase(LPBOARDHANDLE pBoard)
{
    ASSERT_HANDLER(pBoard);
    return 0;
}


/******************************************************************************\

    Function      : ClientReadMemoryByte

    Description   : Read a Byte from the Frame Buffer space.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulOffset | Offset of the Byte to be read.
                    UCHAR *ucByte  | CHAR to receive the data

    Return Val : ULONG
\******************************************************************************/
ULONG ClientReadMemoryByte(LPBOARDHANDLE pBoard, ULONG ulOffset, UCHAR *pucByte)
{
    LPCLIENTDATA pClientStruct = MGAGetClientPointer(pBoard);

    ASSERT_HANDLER(pBoard);

    *pucByte = *((UCHAR *)(pClientStruct->ulFrameBufferBase + ulOffset));

    return 0;
}


/******************************************************************************\

    Function      : ClientReadMemoryDword

    Description   : Read a Dword from the Frame Buffer Space.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulOffset | Offset of the Byte to be read.
                    ULONG *uDword  | Dword to receive the data

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientReadMemoryDword(LPBOARDHANDLE pBoard, ULONG ulOffset,
                            ULONG *pulDword)
{
    LPCLIENTDATA pClientStruct = MGAGetClientPointer(pBoard);

    ASSERT_HANDLER(pBoard);

    *pulDword = *((ULONG *)(pClientStruct->ulFrameBufferBase + ulOffset));

    return 0;
}


/******************************************************************************\

   Function      : ClientWriteMemoryByte

   Description   : Write a Byte from the Frame Buffer space.

   I/O Desc.     : LPBOARDHANDLE pBoard     | Handle to the board structure
                   containing all the information about a specific board.
                   ULONG ulOffset | Offset of the Byte to be read.
                   UCHAR ucByte   | CHAR to receive the data

   Return Val    : ULONG

\******************************************************************************/
ULONG ClientWriteMemoryByte(LPBOARDHANDLE pBoard, ULONG ulOffset, UCHAR ucByte)
{
    LPCLIENTDATA pClientStruct = MGAGetClientPointer(pBoard);

    ASSERT_HANDLER(pBoard);

    *((UCHAR *)(pClientStruct->ulFrameBufferBase + ulOffset)) = ucByte;

    return 0;
}


/******************************************************************************\

    Function      : ClientWriteMemoryDword

    Description   : Write a Dword from the Frame Buffer space.

    I/O desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulOffset | Offset of the Byte to be read.
                    ULONG ulDword  | Dword to receive the data

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientWriteMemoryDword(LPBOARDHANDLE pBoard, ULONG ulOffset,
                            ULONG ulDword)
{
    LPCLIENTDATA pClientStruct = MGAGetClientPointer(pBoard);

    ASSERT_HANDLER(pBoard);

    *((ULONG *)(pClientStruct->ulFrameBufferBase + ulOffset)) = ulDword;

    return 0;
}


/******************************************************************************\

    Function      : ClientOpenSystemDevice

    Description   : Map a System device aperture for future call to
                    ClientReadSystemDeviceX and ClientWriteSystemDeviceX.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulAddress | Physical address of the Register aperture
                    ULONG ulSize    | Size in Byte of the Register Aperture

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientOpenSystemDevice(LPBOARDHANDLE pBoard, ULONG ulAddress,
                                    ULONG ulSize)
{
    return 1;
}


/******************************************************************************\

    Function      : ClientCloseSystemDevice

    Description   : Unmap the System Device aperture address and free
                    resources needed to address it.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientCloseSystemDevice (LPBOARDHANDLE pBoard)
{
    return 1;
}


/******************************************************************************\

    Function      : ClientReadSystemDeviceByte

    Description   : Read a Byte from the device Space.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulOffset | Offset of the Byte to be read.
                    ULONG pucByte  | Byte to read the data

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientReadSystemDeviceByte(LPBOARDHANDLE pBoard, ULONG ulOffset,
                                 UCHAR *pucByte)
{
    return 1;
}


/******************************************************************************\

    Function      : ClientReadSystemDeviceDword

    Description   : Read a Dword from the Frame Buffer Space

    I/O Desc.     : LPBOARDHANDLE pBoard     | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulOffset | Offset of the Byte to be read.
                    ULONG ulDword  | Dword to Read the data

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientReadSystemDeviceDword(LPBOARDHANDLE pBoard, ULONG ulOffset,
                                  ULONG *pulDword)
{
    return 1;
}


/******************************************************************************\

    Function      : ClientWriteSystemByte

    Description   : Write a Byte from the System Device Aperture

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulOffset | Offset of the Byte to be read.
                    UCHAR ucByte   | Byte to receive the data
    Return Val : ULONG

\******************************************************************************/
ULONG ClientWriteSystemDeviceByte(LPBOARDHANDLE pBoard, ULONG ulOffset,
                                  UCHAR ucByte)
{
    return 1;
}


/******************************************************************************\

    Function      : ClientWriteSystemDword

    Description   : Write a Dword from the System Device Aperture.

    I/O Desc.     : LPBOARDHANDLE pBoard     | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulOffset | Offset of the Byte to be read.
                    ULONG uDword   | Dword to receive the data

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientWriteSystemDeviceDword(LPBOARDHANDLE pBoard, ULONG ulOffset,
                                   ULONG ulDword)
{
    return 1;
}


/******************************************************************************\

    Function      : ClientWait

    Description   : Wait for ulDelayus micro-seconds.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    ULONG ulDelayus | Delay in uSec

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientWait (LPBOARDHANDLE pBoard, ULONG ulDelayus)
{
    int i;
    ULONG ulTmp;

    ASSERT_HANDLER(pBoard);

    for(i = 0; i < ulDelayus * 3; i++)
    {
        ClientReadRegisterDword(pBoard,0x1e14,&ulTmp);
    }

    return 0;
}


/******************************************************************************\

    Function      : ClientGetBiosInfo

    Description   : This function will be call if no PINS can be found
                    in physical EEPROM.

    I/O Desc.     :  LPBOARDHANDLE pBoard | Handle to the board structure
                     containing all the information about a specific board.
                     LPUCHAR    | Buffer where we copy bios pins.
                     ULONG      | Bios version

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientGetBiosInfo(LPBOARDHANDLE pBoard, LPUCHAR pucPins,LPULONG pulBIOSVersion)
{
    Bool bNotFound = TRUE;
    UCHAR ucBIOS[32768];
    UCHAR ucTmpByte;
    UCHAR ucCheckSum;
    UCHAR ucPinsIndex;
    UCHAR ucPinsSize;
    ULONG ulTmpDword;
    ULONG ulPinsOffset = 0;
    ULONG ulPCIINFOffset;

    ASSERT_HANDLER(pBoard);

    xf86ReadBIOS(0xc0000,0,ucBIOS,32768);

    if(ucBIOS[0] == 0x55)
    {
        if(ucBIOS[1] == 0xaa)
        {
            while((ulPinsOffset < 0x10000) && bNotFound)
            {
                ulTmpDword = *(ULONG *)(ucBIOS + ulPinsOffset);
                ucPinsSize = (UCHAR) (ulTmpDword >> 16);
                if(((ulTmpDword & 0x0000ffff) == 0x0000412e)
                    && ucPinsSize <= 128)
                {
                    ucCheckSum = 0;
                    for(ucPinsIndex = 0;ucPinsIndex < ucPinsSize; ucPinsIndex++)
                    {
                        pucPins[ucPinsIndex] = ucBIOS[ulPinsOffset +
                                               ucPinsIndex];
                        ucCheckSum += pucPins[ucPinsIndex];
                    }
                    if(ucCheckSum == 0)
                    {
                        bNotFound = FALSE;
                    }
                }
                ulPinsOffset++;
            }

            if(bNotFound)
            {
                return 1;
            }

            ulPCIINFOffset = *(ULONG *)(ucBIOS + 0x18);
            ulPCIINFOffset &= 0x0000ffff;
            ulTmpDword = *(ULONG *)(ucBIOS + ulPCIINFOffset);

            if(ulTmpDword == 0x52494350)  /* "PCIR" */
            {
                ulPCIINFOffset += 0x12;
                ulTmpDword = *(ULONG *)(ucBIOS + ulPCIINFOffset);
                *pulBIOSVersion = ((ULONG) ((ulTmpDword & 0xf0) >> 4) << 16) |
                                  ((ulTmpDword &0xf) << 12) | ((ulTmpDword >> 8)
                                  & 0xff);
            }
            else
            {
                return 1;
            }
        }
        else
        {
           return 1;
        }
    }
    else
    {
        return 1;
    }

    if(!*pulBIOSVersion)
    {
        ucTmpByte = ucBIOS[5];
        *pulBIOSVersion = ((ULONG) (ucTmpByte >> 4) << 16) | ((ULONG)
                          (ucTmpByte & 0x0f) << 12);
    }

    return 0;
}

/******************************************************************************\

    Function      : ClientCallBiosInt10

    Description   : Call the BIOS Int10h with specified parameters.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    LPBIOSREGS pBiosRegs | Pointor to the Bios register
                    structure.

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientCallBiosInt10(LPBOARDHANDLE pBoard, LPBIOSREGS pBiosRegs)
{
   ASSERT_HANDLER(pBoard);

   return 1;
}


/******************************************************************************\

    Function      : ClientReadDDCEdid

    Description   : Not implemented.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    LPBIOSREGS pBiosRegs | Pointor to the Bios register
                    structure.

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientReadDDCEdid(LPBOARDHANDLE pBoard, LPUCHAR pEdid,
                               ULONG ulMonitorIndex)
{
   ASSERT_HANDLER(pBoard);

   return 1;
}


/******************************************************************************\

    Function      : ClientCustomCall

    Description   : Not implemented.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    LPBIOSREGS pBiosRegs | Pointor to the Bios register
                    structure.

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientCustomCall(LPBOARDHANDLE pBoard, ULONG ulServiceNumber,
                       LPVOID pInData, LPVOID pOutData)
{
   ASSERT_HANDLER(pBoard);

   return 1;
}


/******************************************************************************\

    Function      : ClientApplyEpromPatch

    Description   : Not implemented.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    LPBIOSREGS pBiosRegs | Pointor to the Bios register
                    structure.

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientApplyEpromPatch(LPBOARDHANDLE pBoard)
{
   ASSERT_HANDLER(pBoard);

   return 1;
}


/******************************************************************************\

    Function      : ClientDetectHostInterface

    Description   : Not implemented.

    I/O Desc.     : LPBOARDHANDLE pBoard | Handle to the board structure
                    containing all the information about a specific board.
                    LPBIOSREGS pBiosRegs | Pointor to the Bios register
                    structure.

    Return Val    : ULONG

\******************************************************************************/
ULONG ClientDetectHostInterface(LPBOARDHANDLE pBoard, LPULONG pulData)
{
   ASSERT_HANDLER(pBoard);

   return 1;
}

