/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.7/lib/cpp/src/LLkParser.cpp#2 $
 */

#include "antlr/LLkParser.hpp"
#include <stdio.h>

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

ANTLR_USING_NAMESPACE(std)

/**An LL(k) parser.
 *
 * @see antlr.Token
 * @see antlr.TokenBuffer
 * @see antlr.LL1Parser
 */

//	LLkParser(int k_);

LLkParser::LLkParser(const ParserSharedInputState& state, int k_)
: Parser(state), k(k_)
{
}

LLkParser::LLkParser(TokenBuffer& tokenBuf, int k_)
: Parser(tokenBuf), k(k_)
{
}

LLkParser::LLkParser(TokenStream& lexer, int k_)
: Parser(new TokenBuffer(lexer)), k(k_)
{
}

void LLkParser::trace(const char* ee, const char* rname)
{
	traceIndent();

	printf("%s", ((string)ee + rname + ((inputState->guessing>0)?"; [guessing]":"; ")).c_str());

	for (int i = 1; i <= k; i++)
	{
		if (i != 1) {
			printf(", ");
		}
		printf("LA(%d)==", i);

		string temp;

		try {
			temp = LT(i)->getText().c_str();
		}
		catch( ANTLRException& ae )
		{
			temp = "[error: ";
			temp += ae.toString();
			temp += ']';
		}
		printf("%s", temp.c_str());
	}

	printf("\n");;
}

void LLkParser::traceIn(const char* rname)
{
	traceDepth++;
	trace("> ",rname);
}

void LLkParser::traceOut(const char* rname)
{
	trace("< ",rname);
	traceDepth--;
}

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif
