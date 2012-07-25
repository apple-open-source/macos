/*
	File:		MBCEngineCommands.l
	Contains:	Encode commands sent by chess engine.
	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.

	IMPORTANT: This Apple software is supplied to you by Apple Computer,
	Inc.  ("Apple") in consideration of your agreement to the following
	terms, and your use, installation, modification or redistribution of
	this Apple software constitutes acceptance of these terms.  If you do
	not agree with these terms, please do not use, install, modify or
	redistribute this Apple software.
	
	In consideration of your agreement to abide by the following terms,
	and subject to these terms, Apple grants you a personal, non-exclusive
	license, under Apple's copyrights in this original Apple software (the
	"Apple Software"), to use, reproduce, modify and redistribute the
	Apple Software, with or without modifications, in source and/or binary
	forms; provided that if you redistribute the Apple Software in its
	entirety and without modifications, you must retain this notice and
	the following text and disclaimers in all such redistributions of the
	Apple Software.  Neither the name, trademarks, service marks or logos
	of Apple Computer, Inc. may be used to endorse or promote products
	derived from the Apple Software without specific prior written
	permission from Apple.  Except as expressly stated in this notice, no
	other rights or licenses, express or implied, are granted by Apple
	herein, including but not limited to any patent rights that may be
	infringed by your derivative works or by other works in which the
	Apple Software may be incorporated.
	
	The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
	MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
	THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND
	FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS
	USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
	
	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT,
	INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
	REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE,
	HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING
	NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
%{
#include "MBCEngineCommands.h"

#define YY_SKIP_YYWRAP		1
#define YY_NO_UNPUT			1
#define YY_DECL				MBCCompactMove yylex(yyscan_t yyscanner)
#define YY_INPUT(buf,res,max_size) res = MBCReadInput(buf, max_size)
#define yywrap(x)           1
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

void MBCLexerInit(MBCLexerInstance * scanner)
{
    yylex_init((yyscan_t*)scanner);
}

void MBCLexerDestroy(MBCLexerInstance scanner)
{
    yylex_destroy((yyscan_t)scanner);
}

MBCCompactMove MBCLexerScan(MBCLexerInstance scanner)
{
    return yylex((yyscan_t)scanner);
}
// Local Variables:
// mode:C
// End:
