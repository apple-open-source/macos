/*
	File:		MBCEngineCommands.l
	Contains:	Encode commands sent by chess engine.
	Version:	1.0
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCEngineCommands.l,v $
		Revision 1.4  2003/05/27 03:13:57  neerache
		Rework game loading/saving code
		
		Revision 1.3  2003/05/24 20:25:25  neerache
		Eliminate compact moves for most purposes
		
		Revision 1.2  2003/03/28 01:29:53  neeri
		Support hints, last move
		
		Revision 1.1  2002/08/22 23:47:06  neeri
		Initial Checkin
		
*/
%{
#include "MBCEngineCommands.h"

#define YY_SKIP_YYWRAP		1
#define YY_NO_UNPUT			1
#define YY_DECL				MBCCompactMove yylex()
#define YY_INPUT(buf,res,max_size) res = MBCReadInput(buf, max_size)
#define yywrap()	1
%}

ROW	([1-8])
COL	([a-h])
PIECE	([prnbqkPRNBQK])
%%
"move "{COL}{ROW}{COL}{ROW}\n			{ return MBCEncodeMove(yytext+yyleng-5, 0); }
"move "{PIECE}@{COL}{ROW}\n				{ return MBCEncodeDrop(yytext+yyleng-5, 0); }
"move "{COL}{ROW}{COL}{ROW}{PIECE}\n	{ return MBCEncodeMove(yytext+yyleng-6, 0); }
"ponder "{COL}{ROW}{COL}{ROW}\n			{ return MBCEncodeMove(yytext+yyleng-5, 1); }
"ponder "{PIECE}@{COL}{ROW}\n			{ return MBCEncodeDrop(yytext+yyleng-5, 1); }
"ponder "{COL}{ROW}{COL}{ROW}{PIECE}\n	{ return MBCEncodeMove(yytext+yyleng-6, 1); }
"Illegal move":.*\n                     { return MBCEncodeIllegal();	}
"Legal move":.*\n                     	{ return MBCEncodeLegal();	}
"pong ".*\n                     		{ return MBCEncodePong();	}
"tellics set 1 Sjeng ".*\n      	    { return MBCEncodeStartGame();	}
1-0.*\n									{ return MBCEncodeWhiteWins();  }
0-1.*\n									{ return MBCEncodeBlackWins();	}
"1/2-1/2".*\n							{ return MBCEncodeDraw();		}
.*\n									{ MBCIgnoredText(yytext);}
%%

// Local Variables:
// mode:C
// End:
