/**************************************************************************************

 @doc MGA MGA_STRUCT

 @module MGA Structures | Structure of The MGA Layer
 @normal Copyright © 1997, Matrox Graphics Inc.

 All Rights Reserved.

 @head3 MGA Structures | 
 @index struct | MGA_STRUCT

 @end

***************************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/HALlib/binding.h,v 1.5 2000/12/06 20:39:52 dawes Exp $ */

#ifndef _BINDING
#define _BINDING

#ifndef _INTERNALBINDING

#define BINDING_NOERROR     0x00000000
#define BINDING_ERROR       0x90000000
#define FAR
#define DECL

typedef void            VOID;
typedef void            FAR *LPVOID;
typedef void            FAR *LPBOARDHANDLE;

typedef long            LONG;
typedef unsigned long   ULONG;
typedef unsigned long   FLONG;
typedef unsigned long   FAR *LPULONG;

typedef char            CHAR;
typedef unsigned char   UCHAR;
typedef unsigned char   FAR*LPUCHAR;

typedef struct TAGCLIENTTABLE{
    ULONG (DECL *ClientReadConfigSpaceByte       )(LPVOID , ULONG, LPVOID );        
    ULONG (DECL *ClientReadConfigSpaceDword      )(LPVOID , ULONG, LPVOID );        
    ULONG (DECL *ClientWriteConfigSpaceByte      )(LPVOID , ULONG, UCHAR);          
    ULONG (DECL *ClientWriteConfigSpaceDword     )(LPVOID , ULONG, ULONG);          
    ULONG (DECL *ClientOpenRegisterBase          )(LPVOID , ULONG, ULONG);          
    ULONG (DECL *ClientCloseRegisterBase         )(LPVOID );                        
    ULONG (DECL *ClientReadRegisterByte          )(LPVOID , ULONG, LPVOID );        
    ULONG (DECL *ClientReadRegisterDword         )(LPVOID , ULONG, LPVOID );        
    ULONG (DECL *ClientWriteRegisterByte         )(LPVOID , ULONG, UCHAR);          
    ULONG (DECL *ClientWriteRegisterDword        )(LPVOID , ULONG, ULONG);          
    ULONG (DECL *ClientOpenMemoryBase            )(LPVOID , ULONG, ULONG);          
    ULONG (DECL *ClientCloseMemoryBase           )(LPVOID );                        
    ULONG (DECL *ClientReadMemoryByte            )(LPVOID , ULONG, LPVOID );        
    ULONG (DECL *ClientReadMemoryDword           )(LPVOID , ULONG, LPVOID );        
    ULONG (DECL *ClientWriteMemoryByte           )(LPVOID , ULONG, UCHAR);          
    ULONG (DECL *ClientWriteMemoryDword          )(LPVOID , ULONG, ULONG);          
    ULONG (DECL *ClientOpenSystemDevice          )(LPVOID , ULONG, ULONG);          
    ULONG (DECL *ClientCloseSystemDevice         )(LPVOID );                        
    ULONG (DECL *ClientReadSystemDeviceByte      )(LPVOID , ULONG, LPVOID );        
    ULONG (DECL *ClientReadSystemDeviceDword     )(LPVOID , ULONG, LPVOID );        
    ULONG (DECL *ClientWriteSystemDeviceByte     )(LPVOID , ULONG, UCHAR);          
    ULONG (DECL *ClientWriteSystemDeviceDword    )(LPVOID , ULONG, ULONG);          
    ULONG (DECL *ClientWait                      )(LPVOID , ULONG);                 
    ULONG (DECL *ClientGetBiosInfo               ) (LPVOID, LPVOID, LPVOID);        
    ULONG (DECL *ClientReadDDCEdid               ) (LPVOID, LPVOID, ULONG);         
#ifdef DEBUG
    ULONG (DECL *ClientDebug                     ) (LPVOID, ULONG, LPVOID, ULONG, LPVOID);
#endif
} CLIENTTABLE, FAR *LPCLIENTTABLE;

#endif /* _INTERNALBINDING */

/***************************************************************************************************
 MGAHWINFO STRUCTURE
 ***************************************************************************************************
 @struct MGAHWINFO | Public MGA Board Information
 @field  OUT ULONG   | ulCapsFirstOutput  | Capabilities of firts output
 @flag                                     Bit 0       | Analog output supported on primary CRTC
 @flag                                     Bit 1       | Digital output supported on primary CRTC
 @flag                                     Bit 2       | TV output supported on primary CRTC
 @flag                                     Bit 3       | Analog output supported on second CRTC
 @flag                                     Bit 4       | Digital output supported on second CRTC
 @flag                                     Bit 5       | TV output supported on second CRTC
 @flag                                     Bit 6       | VGA output supported
 @field  OUT ULONG   | ulCapsSecondOutput | Capabilities of second output
 @flag                                     Bit 0       | Analog output supported on primary CRTC
 @flag                                     Bit 1       | Digital output supported on primary CRTC
 @flag                                     Bit 2       | TV output supported on primary CRTC
 @flag                                     Bit 3       | Analog output supported on second CRTC
 @flag                                     Bit 4       | Digital output supported on second CRTC
 @flag                                     Bit 5       | TV output supported on second CRTC
 @flag                                     Bit 6       | VGA output supported
 @field  OUT ULONG   | ulVideoMemory      | Total number of video memory in bytes
 @end                            
 **************************************************************************************************/
typedef struct TAGMGAHWINFO {
    ULONG   ulCapsFirstOutput;      
    ULONG   ulCapsSecondOutput;      
    ULONG   ulVideoMemory;      
} MGAHWINFO, FAR *LPMGAHWINFO;

/***************************************************************************************************
 MGAMODEINFO STRUCTURE
 ***************************************************************************************************
 @struct MGAMODEINFO | Mode Information
 @field  IN     FLONG   | flOutput         | Where we want to apply this parameters
 @flag                                     Bit 0       | Use second CRTC
 @flag                                     Bit 1       | Use primary analog output
 @flag                                     Bit 2       | Use secondary analog output
 @flag                                     Bit 3       | Use primary digital output
 @flag                                     Bit 4       | Use secondary digital output
 @flag                                     Bit 5       | Force a particular frame buffer pitch
 @flag                                     Bit 6       | Force a particular display origin
 @flag                                     Bit 7-31    | Reserved
 @field  IN     ULONG   | ulDispWidth      | Display Width in pixels
 @field  IN     ULONG   | ulDispHeight     | Display Height in pixels
 @field  IN     ULONG   | ulDeskWidth      | Desktop Width in pixels
 @field  IN     ULONG   | ulDeskHeight     | Desktop Height in pixels
 @field  IN OUT ULONG   | ulFBPitch        | Frame Buffer Pitch in pixel
 @field  IN     ULONG   | ulBpp            | Bits Per Pixels and input format
 @flag                                    lower 16 bit   |  Bits per pixel
                                                             <nl><tab> 8, 15, 16, 24 or 32 
 @flag                                    upper 16 bit   |  Input format
                                                         <nl><tab> 0  RGB
                                                         <nl><tab> 1  RGBA
                                                         <nl><tab> 2  YcbCr 4:2:0 3 Planes
                                                         <nl><tab> 3  YcbCr 4:2:0 4 Planes
                                                         <nl><tab> 4  YcbCr 4:2:2-UYVY
                                                         <nl><tab> 5  YcbCr 4:2:2-YUY2
                                                         <nl><tab> 6-10 none interleave mode
                                                         <nl><tab> 6  NI RGBA
                                                         <nl><tab> 7  NI YcbCr 4:2:0 3 Planes
                                                         <nl><tab> 8  NI YcbCr 4:2:0 4 Planes
                                                         <nl><tab> 9  NI YcbCr 4:2:2-UYVY
                                                         <nl><tab> 10 NI YcbCr 4:2:2-YUY2
 @field IN     ULONG    | ulZoom            | Zoom factor
 @flag                                     1x (1), 2x (2) or 4x (4)|
 @field IN OUT FLONG    | flSignalMode      | Signal Mode
 @flag                                     Bit 0     | Interlace (0 : Non-interlace / 1: Interlace)
 @flag                                     Bit 1     | Overscan  (0 : No Overscan / 1 : Overscan)
 @flag                                     Bit 2     | Horizontal Sync Polarity (0 : Negative / 1 : Positive)
 @flag                                     Bit 3     | Vertical Sync Polarity (0 : Negative / 1 : Positive)
 @flag                                     Bit 4-7   | Standard TV output
                                                       <nl><tab>000 - PAL B G H
                                                       <nl><tab>001 - NTSC M
                                                       <nl><tab>010..111 - Reserved
 @flag                                     Bit 8-31  | Reserved for future use
 @field IN OUT ULONG | ulRefreshRate     | Vertical Refresh Rate in Hz
 @field IN OUT ULONG | ulHorizRate       | Horizontal Refresh Rate in KHz
 @field IN OUT ULONG | ulPixClock        | Pixel Clock in kHz
 @field IN OUT ULONG | ulHFPorch         | Horizontal front porch in pixels
 @field IN OUT ULONG | ulHSync           | Horizontal Sync in pixels
 @field IN OUT ULONG | ulHBPorch         | Horizontal back porch in pixels
 @field IN OUT ULONG | ulVFPorch         | Vertical front porch in lines
 @field IN OUT ULONG | ulVSync           | Vertical Sync in lines
 @field IN OUT ULONG | ulVBPorch         | Vertical back Porch in lines
 @field IN OUT ULONG | ulDisplayOrg      | Origin of the display Offset(pixels)
 @field IN OUT ULONG | ulDstOrg          | Origin of the drawing Offset in the frame (pixels)
 @field IN OUT ULONG | ulPanXGran        | Panning in X granularity in pixel
 @field IN OUT ULONG | ulPanYGran        | Panning in Y granularity in pixel
 @field IN OUT ULONG | ulTVStandard      | TV Standard
 @field IN OUT ULONG | ulCableType       | Cable Type
 @end                            
 **************************************************************************************************/
typedef struct TAGMAGMODEINFO {
    FLONG   flOutput;           /* Where we want to apply this parameters                   */
    ULONG   ulDispWidth;        /* Display Width in pixels                                  */
    ULONG   ulDispHeight;       /* Display Height in pixels                                 */
    ULONG   ulDeskWidth;        /* Desktop Width in pixels                                  */
    ULONG   ulDeskHeight;       /* Desktop Height in pixels                                 */
    ULONG   ulFBPitch;          /* Frame Buffer Pitch in pixel                              */
    ULONG   ulBpp;              /* Bits Per Pixels / input format                           */
    ULONG   ulZoom;             /* Zoom factor                                              */
    FLONG   flSignalMode;       /* Signal Mode                                              */
    ULONG   ulRefreshRate;      /* Vertical Refresh Rate in Hz                              */
    ULONG   ulHorizRate;        /* Horizontal Refresh Rate in KHz                           */
    ULONG   ulPixClock;         /* Pixel Clock in kHz                                       */
    ULONG   ulHFPorch;          /* Horizontal front porch in pixels                         */
    ULONG   ulHSync;            /* Horizontal Sync in pixels                                */
    ULONG   ulHBPorch;          /* Horizontal back porch in pixels                          */
    ULONG   ulVFPorch;          /* Vertical front porch in lines                            */
    ULONG   ulVSync;            /* Vertical Sync in lines                                   */
    ULONG   ulVBPorch;          /* Vertical back Porch in lines                             */
    ULONG   ulDisplayOrg;       /* Origin of the display Offset(bytes)                      */
    ULONG   ulDstOrg;           /* Origin of the drawing Offset in the frame (bytes)        */
    ULONG   ulPanXGran;         /* Panning in X granularity in pixel                        */
    ULONG   ulPanYGran;         /* Panning in Y granularity in pixel                        */
    ULONG   ulTVStandard;       /* TV Standard                                              */
    ULONG   ulCableType;        /* Cable Type                                               */
} MGAMODEINFO, FAR *LPMGAMODEINFO;

#define MGAHWINFOCAPS_CRTC1_ANALOG          1L      
#define MGAHWINFOCAPS_CRTC1_DIGITAL         (1L << 1)     
#define MGAHWINFOCAPS_CRTC1_TV              (1L << 2)     
#define MGAHWINFOCAPS_CRTC2_ANALOG          (1L << 3)     
#define MGAHWINFOCAPS_CRTC2_DIGITAL         (1L << 4)     
#define MGAHWINFOCAPS_CRTC2_TV              (1L << 5)     
#define MGAHWINFOCAPS_OUTPUT_VGA            (1L << 6)
#define MGAHWINFOCAPS_CRTC2                 (MGAHWINFOCAPS_CRTC2_ANALOG | MGAHWINFOCAPS_CRTC2_DIGITAL | MGAHWINFOCAPS_CRTC2_TV)
#define MGAHWINFOCAPS_OUTPUT_ANALOG         (MGAHWINFOCAPS_CRTC1_ANALOG | MGAHWINFOCAPS_CRTC2_ANALOG)
#define MGAHWINFOCAPS_OUTPUT_DIGITAL        (MGAHWINFOCAPS_CRTC1_DIGITAL | MGAHWINFOCAPS_CRTC2_DIGITAL)
#define MGAHWINFOCAPS_OUTPUT_TV             (MGAHWINFOCAPS_CRTC1_TV | MGAHWINFOCAPS_CRTC2_TV)

#define MGAMODEINFO_SECOND_CRTC        1L
#define MGAMODEINFO_ANALOG1            (1L << 1) 
#define MGAMODEINFO_ANALOG2            (1L << 2) 
#define MGAMODEINFO_DIGITAL1           (1L << 3) 
#define MGAMODEINFO_DIGITAL2           (1L << 4) 
#define MGAMODEINFO_FORCE_PITCH        (1L << 5) 
#define MGAMODEINFO_FORCE_DISPLAYORG   (1L << 6) 
#define MGAMODEINFO_TV                 (1L << 7)
#define MGAMODEINFO_TESTONLY           0x80000000

/* Cable Type */
#define TV_YC_COMPOSITE                 0
#define TV_SCART_RGB                    1
#define TV_SCART_COMPOSITE              2
#define TV_SCART_TYPE2                  3

/* TV Standard */

#define TV_PAL                          0
#define TV_NTSC                         1

#if defined(__cplusplus)
    extern "C" {
#endif

ULONG MGACloseLibrary(LPBOARDHANDLE pBoard);
ULONG MGAValidateMode(LPBOARDHANDLE pBoard, LPMGAMODEINFO pMgaModeInfo);
ULONG MGAValidateVideoParameters(LPBOARDHANDLE pBoard, LPMGAMODEINFO pMgaModeInfo);
ULONG MGASetMode(LPBOARDHANDLE pBoard, LPMGAMODEINFO pMgaModeInfo);
ULONG MGASetTVMode(LPBOARDHANDLE pBoard, LPMGAMODEINFO pMgaModeInfo);
ULONG MGASetVgaMode(LPBOARDHANDLE pBoard);
ULONG MGASaveVgaState(LPBOARDHANDLE pBoard);
ULONG MGARestoreVgaState(LPBOARDHANDLE pBoard);
ULONG MGAInitHardware(LPBOARDHANDLE pBoard);
ULONG MGAGetVideoParameters(LPBOARDHANDLE pBoard, LPMGAMODEINFO pMgaModeInfo, ULONG ulRefresh);
ULONG MGAGetHardwareInfo(LPBOARDHANDLE pBoard, LPMGAHWINFO pMgaHwInfo);
LPVOID MGAGetClientPointer(LPBOARDHANDLE pBoard);
ULONG MGAOpenLibrary(LPBOARDHANDLE pBoard, LPVOID lpClient, ULONG ulClientSize);
ULONG MGAGetBOARDHANDLESize(void);
ULONG MGASetTVStandard(LPBOARDHANDLE pBoard, ULONG ulTVStandard);
ULONG MGASetTVCableType(LPBOARDHANDLE pBoard, ULONG ulCableType);

#if defined(__cplusplus)
    }
#endif

#endif /* _BINDING */


