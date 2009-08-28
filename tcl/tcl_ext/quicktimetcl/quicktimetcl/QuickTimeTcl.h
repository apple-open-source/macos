/*
 * QuickTimeTcl.h --
 *
 *		Header file for the QuickTime Movie and sequence grabber widget for Tcl/Tk.
 *      Part of QuickTimeTcl.
 *
 * Copyright (c) 1998  Bruce O'Neel
 * Copyright (c) 2000-2005  Mats Bengtsson
 *
 * $Id: QuickTimeTcl.h,v 1.16 2008/02/26 13:40:47 matben Exp $
 */

#ifndef INCLUDED_QUICKTIMETCL_H
#define INCLUDED_QUICKTIMETCL_H

#define USE_NON_CONST
#define USE_PANIC_ON_PHOTO_ALLOC_FAILURE
#define WIN32_LEAN_AND_MEAN

#if TARGET_API_MAC_CARBON
#   include "tkMacOSXInt.h"
#   include <Carbon/Carbon.h>
#   include <QuickTime/QuickTime.h>
#   include "osxMacTcl.h"
#else
#   include "tkInt.h"
#endif

/*
 * From tclInt.h version 1.118.2.11
 * Ensure WORDS_BIGENDIAN is defined correcly:
 * Needs to happen here in addition to configure to work with
 * fat compiles on Darwin (i.e. ppc and i386 at the same time).
 */
 
#ifndef WORDS_BIGENDIAN
#	ifdef HAVE_SYS_TYPES_H
#		include <sys/types.h>
#	endif
#	ifdef HAVE_SYS_PARAM_H
#		include <sys/param.h>
#	endif
#   ifdef BYTE_ORDER
#		ifdef BIG_ENDIAN
#			if BYTE_ORDER == BIG_ENDIAN
#				define WORDS_BIGENDIAN
#			endif
#		endif
#		ifdef LITTLE_ENDIAN
#			if BYTE_ORDER == LITTLE_ENDIAN
#				undef WORDS_BIGENDIAN
#			endif
#		endif
#	endif
#endif

/*
 * Make sure that these are in sync with the ones in the resource file.
 */

#define QTTCL_VERSION "3.2"
#define QTTCL_PATCH_LEVEL "3.2.0"

#ifndef StrLength
#define StrLength(s)    (*((unsigned char *) (s)))
#endif
#ifndef StrBody
#define StrBody(s)      ((char *) (s) + 1)
#endif

#ifndef Nint
#define Nint(f)         (floor((f) + 0.5))
#endif
#ifndef Sign
#define Sign(f)         ((f) >= 0.0 ? 1 : -1 )
#endif

extern Tcl_Encoding gQTTclTranslationEncoding;

/*
 * Some typical time scales for newly created tracks.
 */
 
#define kVideoTimeScale     600
#define kSoundTimeScale     44100
#define kTextTimeScale      60
#define kMusicTimeScale     44100
#define kSpriteTimeScale    600
#define kFlashTimeScale     600

enum {
    kDirLeft    = 0L,
    kDirRight   = 1L,
    kDirUp      = 2L,
    kDirDown    = 3L,
    kDirIn      = 4L,
    kDirOut     = 5L
};

/*
 * For redirecting source tracks for video effects.
 */
 
#define kNoSourceName		FOUR_CHAR_CODE('none')
#define kSourceOneName		FOUR_CHAR_CODE('srcA')
#define kSourceTwoName		FOUR_CHAR_CODE('srcB')

#define ksigMoviePlayer		FOUR_CHAR_CODE('TVOD')

#if TARGET_OS_MAC
#	if TARGET_API_MAC_CARBON
#		define MyIsToplevelInFront( tkwin )     \
            ( FrontWindow() == GetWindowFromPort( TkMacOSXGetDrawablePort(Tk_WindowId(tkwin)) ) )
#	else
#		define MyIsToplevelInFront( tkwin )     \
            ( ((CGrafPtr) FrontWindow()) == (((TkWindow *) (tkwin))->privatePtr->toplevel->portPtr) )
#	endif
#	define MyIsClipRegionInvalid( tkwin )   \
        ( ((TkWindow *) (tkwin))->privatePtr->flags & TK_CLIP_INVALID )      
#endif  // TARGET_OS_MAC

int         MoviePlayerObjCmd( ClientData clientData, Tcl_Interp *interp, 
                    int argc, Tcl_Obj *CONST objv[] );
int		    SeqGrabberObjCmd( ClientData clientData, Tcl_Interp *interp, 
				    int objc, Tcl_Obj *CONST objv[] );
int 		QuickTimeStat( ClientData clientData, Tcl_Interp *interp, 
                    int objc, Tcl_Obj *CONST objv[] ); 
#if TARGET_OS_MAC
int         BroadcastObjCmd( ClientData clientData, Tcl_Interp *interp,
                    int objc, Tcl_Obj * CONST objv[] );
void        BroadcastMacEvent( void );
#endif
void        QTTclGetMacFontAttributes( Tcl_Interp *interp, Tk_Window tkwin,
                    Tk_Font tkFont, short *faceNum, short *macSize, Style *style);
int  	    SeqGrabberMacEvent( EventRecord *eventPtr );


int			SafeStrcmp( const char *cs, const char *ct );
char * 		SafeStrcpy( char *s, const char *ct );
Handle      MySafeNewHandle( long size, int clear );
OSErr       MySafeNewGWorld( GWorldPtr *offscreenGWorldHand, short depth, 
					const Rect *bounds, CTabHandle cTable, GDHandle device, 
					GWorldFlags flags );
void        ConvertFloatToBigEndian( float *theFloat );
void        ConvertBigEndianFloatToNative( float *theFloat );
void        MyDebugStr( ConstStr255Param debuggerMsg );
int  		ConvertTkPhotoToPicture( Tcl_Interp *interp, Tk_PhotoHandle tkPhoto, 
					PicHandle *thePic );
int  		ConvertPictureToTkPhoto( Tcl_Interp *interp, PicHandle thePic, 
					int width, int height, char *tkImage );
int  		MakeTkPhotoFromPixMap( Tcl_Interp *interp, PixMapHandle pixels,	char *tkImage );
char *		MakeInternalImageName( void );

void		QTTclDebugPrintf( Tcl_Interp *interp, int level, char *fmt, ... );

OSStatus   	CheckAndSetErrorResult( Tcl_Interp *interp, OSStatus noErr );
Tcl_Obj 	*GetErrorObj ( OSStatus err );

#if TARGET_API_MAC_CARBON
int 		MacControlUICmd( ClientData clientData, Tcl_Interp *interp,
                    int objc, Tcl_Obj *CONST objv[]);
#endif

#if TARGET_OS_MAC
Tcl_Encoding	GetMacSystemEncoding( void );
#endif

/*
 * Some routines to fix compatibility between the mac and QT on Win32 which uses
 * some mac specific stuff.
 */

int	    QTTclNewDataRefFromUTF8Obj(Tcl_Interp *interp, Tcl_Obj *fileNameObj,
		Handle *outDataRef, OSType *outDataRefType);
int	    QTTclNewUTF8ObjFromDataRef(Tcl_Interp *interp, Handle inDataRef, 
		OSType inDataRefType, Tcl_Obj **fileNameObjPtr);
    
OSErr		QTTclNativePathNameToFSSpec( Tcl_Interp *interp,
					const char *filename, FSSpec *fss );
int         QTTclFSSpecToNativePathName( Tcl_Interp *interp, char *pathname, 
                    FSSpec *fss );
void        QTTclMacWinBounds( TkWindow *winPtr, Rect *bounds );
GWorldPtr   QTTclMacGetDrawablePort( Drawable drawable );
RgnHandle	QTTclMacVisableClipRgn( TkWindow *winPtr );

#ifdef _WIN32
int			TkSetMacColor( unsigned long pixel, RGBColor *macColor);
#endif

/*
 * MovieQTVRUtils:
 */

int         PanoramaGetInfoNode( Tcl_Interp *interp, Movie movie, 
					QTVRInstance qtvrInst, UInt32 nodeID, Tcl_Obj **resObj );
int         GetNodeCount( QTVRInstance qtvrInst );
Boolean     SetPanoramaByDegrees( QTVRInstance qtvrInst, long direction, 
					float amount );
void        ZoomInOrOutPanorama( QTVRInstance qtvrInst, long direction, 
					float fov );
int         ProcessHotspotSubCmd( Tcl_Interp *interp, Movie movie,
	                QTVRInstance qtvrInst, int argc, Tcl_Obj *CONST objv[] );

// stolen from tkFont.c
#define GetFontAttributes(tkfont)       \
		((CONST TkFontAttributes *) &((TkFont *) (tkfont))->fa)

/*
 * Which QuickTime version is the lowest acceptable?
 */
 
#define MIN_QUICKTIME_VERSION 	0x0500

#define MOVIE_PLAYER_RUNS   0x0001
#define SEQ_GRABBER_RUNS    0x0002
 
#define STR255LEN 255


#endif      // INCLUDED_QUICKTIMETCL_H
