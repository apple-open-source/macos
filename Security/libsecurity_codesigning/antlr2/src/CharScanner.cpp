/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.7/lib/cpp/src/CharScanner.cpp#2 $
 */

#include <stdio.h>
#include <string>

#include "antlr/CharScanner.hpp"
#include "antlr/CommonToken.hpp"

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif
ANTLR_C_USING(exit)

CharScanner::CharScanner(InputBuffer& cb, bool case_sensitive )
	: saveConsumedInput(true) //, caseSensitiveLiterals(true)
	, caseSensitive(case_sensitive)
	, literals(CharScannerLiteralsLess(this))
	, inputState(new LexerInputState(cb))
	, commitToPath(false)
	, tabsize(8)
	, traceDepth(0)
{
	setTokenObjectFactory(&CommonToken::factory);
}

CharScanner::CharScanner(InputBuffer* cb, bool case_sensitive )
	: saveConsumedInput(true) //, caseSensitiveLiterals(true)
	, caseSensitive(case_sensitive)
	, literals(CharScannerLiteralsLess(this))
	, inputState(new LexerInputState(cb))
	, commitToPath(false)
	, tabsize(8)
	, traceDepth(0)
{
	setTokenObjectFactory(&CommonToken::factory);
}

CharScanner::CharScanner( const LexerSharedInputState& state, bool case_sensitive )
	: saveConsumedInput(true) //, caseSensitiveLiterals(true)
	, caseSensitive(case_sensitive)
	, literals(CharScannerLiteralsLess(this))
	, inputState(state)
	, commitToPath(false)
	, tabsize(8)
	, traceDepth(0)
{
	setTokenObjectFactory(&CommonToken::factory);
}

/** Report exception errors caught in nextToken() */
void CharScanner::reportError(const RecognitionException& ex)
{
	fprintf(stderr, "%s", (ex.toString() + "\n").c_str());
}

/** Parser error-reporting function can be overridden in subclass */
void CharScanner::reportError(const ANTLR_USE_NAMESPACE(std)string& s)
{
	if ( getFilename()=="" )
		fprintf(stderr, "%s", ("error: " + s + "\n").c_str());
	else
		fprintf(stderr, "%s", (getFilename() + ": error: " + s + "\n").c_str());
}

/** Parser warning-reporting function can be overridden in subclass */
void CharScanner::reportWarning(const ANTLR_USE_NAMESPACE(std)string& s)
{
	if ( getFilename()=="" )
		fprintf(stderr, "%s", ("warning: " + s + "\n").c_str());
	else
		fprintf(stderr, "%s", (getFilename() + ": warning: " + s + "\n").c_str());
}

void CharScanner::traceIndent()
{
	for( int i = 0; i < traceDepth; i++ )
		printf(" ");
}

void CharScanner::traceIn(const char* rname)
{
	traceDepth++;
	traceIndent();
	printf("> lexer %s; c==%d\n", rname, LA(1));
}

void CharScanner::traceOut(const char* rname)
{
	traceIndent();
	printf("< lexer %s; c==%d\n", rname, LA(1));
	traceDepth--;
}

#ifndef NO_STATIC_CONSTS
const int CharScanner::NO_CHAR;
const int CharScanner::EOF_CHAR;
#endif

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif

