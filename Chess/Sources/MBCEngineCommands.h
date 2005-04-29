/*
	File:		MBCEngineCommands.h
	Contains:	Encode commands sent by chess engine.
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCEngineCommands.h,v $
		Revision 1.5  2003/07/14 23:22:50  neerache
		Move to much smarter speech recognition model
		
		Revision 1.4  2003/05/27 03:13:57  neerache
		Rework game loading/saving code
		
		Revision 1.3  2003/05/24 20:25:25  neerache
		Eliminate compact moves for most purposes
		
		Revision 1.2  2003/03/28 01:29:53  neeri
		Support hints, last move
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
*/

#ifdef __cplusplus
#import "MBCBoard.h"

extern "C" {
#else
typedef unsigned MBCCompactMove;
#endif

extern MBCCompactMove MBCEncodeMove(const char * move, int ponder);
extern MBCCompactMove MBCEncodeDrop(const char * drop, int ponder);
extern MBCCompactMove MBCEncodeIllegal();
extern MBCCompactMove MBCEncodeLegal();
extern MBCCompactMove MBCEncodePong();
extern MBCCompactMove MBCEncodeStartGame();
extern MBCCompactMove MBCEncodeWhiteWins();
extern MBCCompactMove MBCEncodeBlackWins();
extern MBCCompactMove MBCEncodeDraw();
extern MBCCompactMove MBCEncodeTakeback();

extern void MBCIgnoredText(const char * text);
extern int MBCReadInput(char * buf, int max_size);

extern MBCCompactMove yylex();

#ifdef __cplusplus
}
#endif
