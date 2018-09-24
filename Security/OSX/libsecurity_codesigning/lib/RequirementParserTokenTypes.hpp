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
		LITERAL_notarized = 22,
		LITERAL_anchor = 23,
		LITERAL_apple = 24,
		LITERAL_generic = 25,
		LITERAL_certificate = 26,
		LITERAL_cert = 27,
		LITERAL_trusted = 28,
		LITERAL_info = 29,
		LITERAL_entitlement = 30,
		LITERAL_exists = 31,
		EQL = 32,
		EQQL = 33,
		STAR = 34,
		SUBS = 35,
		LESS = 36,
		GT = 37,
		LE = 38,
		GE = 39,
		LBRACK = 40,
		RBRACK = 41,
		NEG = 42,
		LITERAL_leaf = 43,
		LITERAL_root = 44,
		HASHCONSTANT = 45,
		HEXCONSTANT = 46,
		DOTKEY = 47,
		STRING = 48,
		PATHNAME = 49,
		INTEGER = 50,
		SEMI = 51,
		IDENT = 52,
		HEX = 53,
		COMMA = 54,
		WS = 55,
		SHELLCOMMENT = 56,
		C_COMMENT = 57,
		CPP_COMMENT = 58,
		NULL_TREE_LOOKAHEAD = 3
	};
#ifdef __cplusplus
};
#endif
ANTLR_END_NAMESPACE
#endif /*INC_RequirementParserTokenTypes_hpp_*/
