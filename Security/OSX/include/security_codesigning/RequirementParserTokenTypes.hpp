#ifndef INC_RequirementParserTokenTypes_hpp_
#define INC_RequirementParserTokenTypes_hpp_

ANTLR_BEGIN_NAMESPACE(Security_CodeSigning)
/* $ANTLR 2.7.7 (20121221): "requirements.grammar" -> "RequirementParserTokenTypes.hpp"$ */

#ifndef CUSTOM_API
# define CUSTOM_API
#endif

#ifdef __cplusplus
struct CUSTOM_API RequirementParserTokenTypes {
#endif
	enum {
		EOF_ = 1,
		ARROW = 4,
		LITERAL_guest = 5,
		LITERAL_host = 6,
		LITERAL_designated = 7,
		LITERAL_library = 8,
		LITERAL_plugin = 9,
		LITERAL_or = 10,
		LITERAL_and = 11,
		LPAREN = 12,
		RPAREN = 13,
		NOT = 14,
		LITERAL_always = 15,
		LITERAL_true = 16,
		LITERAL_never = 17,
		LITERAL_false = 18,
		LITERAL_identifier = 19,
		LITERAL_cdhash = 20,
		LITERAL_platform = 21,
		LITERAL_anchor = 22,
		LITERAL_apple = 23,
		LITERAL_generic = 24,
		LITERAL_certificate = 25,
		LITERAL_cert = 26,
		LITERAL_trusted = 27,
		LITERAL_info = 28,
		LITERAL_entitlement = 29,
		LITERAL_exists = 30,
		EQL = 31,
		EQQL = 32,
		STAR = 33,
		SUBS = 34,
		LESS = 35,
		GT = 36,
		LE = 37,
		GE = 38,
		LBRACK = 39,
		RBRACK = 40,
		NEG = 41,
		LITERAL_leaf = 42,
		LITERAL_root = 43,
		HASHCONSTANT = 44,
		HEXCONSTANT = 45,
		DOTKEY = 46,
		STRING = 47,
		PATHNAME = 48,
		INTEGER = 49,
		SEMI = 50,
		IDENT = 51,
		HEX = 52,
		COMMA = 53,
		WS = 54,
		SHELLCOMMENT = 55,
		C_COMMENT = 56,
		CPP_COMMENT = 57,
		NULL_TREE_LOOKAHEAD = 3
	};
#ifdef __cplusplus
};
#endif
ANTLR_END_NAMESPACE
#endif /*INC_RequirementParserTokenTypes_hpp_*/
