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
		LITERAL_absent = 32,
		EQL = 33,
		EQQL = 34,
		STAR = 35,
		SUBS = 36,
		LESS = 37,
		GT = 38,
		LE = 39,
		GE = 40,
		LBRACK = 41,
		RBRACK = 42,
		NEG = 43,
		LITERAL_leaf = 44,
		LITERAL_root = 45,
		HASHCONSTANT = 46,
		HEXCONSTANT = 47,
		DOTKEY = 48,
		STRING = 49,
		PATHNAME = 50,
		INTEGER = 51,
		LITERAL_timestamp = 52,
		SEMI = 53,
		IDENT = 54,
		HEX = 55,
		COMMA = 56,
		WS = 57,
		SHELLCOMMENT = 58,
		C_COMMENT = 59,
		CPP_COMMENT = 60,
		NULL_TREE_LOOKAHEAD = 3
	};
#ifdef __cplusplus
};
#endif
ANTLR_END_NAMESPACE
#endif /*INC_RequirementParserTokenTypes_hpp_*/
