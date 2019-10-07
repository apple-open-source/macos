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
		LITERAL_legacy = 23,
		LITERAL_anchor = 24,
		LITERAL_apple = 25,
		LITERAL_generic = 26,
		LITERAL_certificate = 27,
		LITERAL_cert = 28,
		LITERAL_trusted = 29,
		LITERAL_info = 30,
		LITERAL_entitlement = 31,
		LITERAL_exists = 32,
		LITERAL_absent = 33,
		EQL = 34,
		EQQL = 35,
		STAR = 36,
		SUBS = 37,
		LESS = 38,
		GT = 39,
		LE = 40,
		GE = 41,
		LBRACK = 42,
		RBRACK = 43,
		NEG = 44,
		LITERAL_leaf = 45,
		LITERAL_root = 46,
		HASHCONSTANT = 47,
		HEXCONSTANT = 48,
		DOTKEY = 49,
		STRING = 50,
		PATHNAME = 51,
		INTEGER = 52,
		LITERAL_timestamp = 53,
		SEMI = 54,
		IDENT = 55,
		HEX = 56,
		COMMA = 57,
		WS = 58,
		SHELLCOMMENT = 59,
		C_COMMENT = 60,
		CPP_COMMENT = 61,
		NULL_TREE_LOOKAHEAD = 3
	};
#ifdef __cplusplus
};
#endif
ANTLR_END_NAMESPACE
#endif /*INC_RequirementParserTokenTypes_hpp_*/
