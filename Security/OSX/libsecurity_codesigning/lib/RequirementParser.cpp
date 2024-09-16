/* $ANTLR 2.7.7 (20121221): "requirements.grammar" -> "RequirementParser.cpp"$ */
#include "RequirementParser.hpp"
#include <antlr/NoViableAltException.hpp>
#include <antlr/SemanticException.hpp>
#include <antlr/ASTFactory.hpp>

#include "requirement.h"
#include "reqmaker.h"
#include "csutilities.h"
#include <libDER/libDER.h>
#include <libDER/asn1Types.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/hashing.h>
#include <security_cdsa_utilities/cssmdata.h>	// OID coding
#include <Security/SecCertificate.h>
using namespace CodeSigning;
typedef Requirement::Maker Maker;

extern "C" {

/* Decode a choice of UTCTime or GeneralizedTime to a CFAbsoluteTime. Return
an absoluteTime if the date was valid and properly decoded.  Return
NULL_TIME otherwise. */
CFAbsoluteTime SecAbsoluteTimeFromDateContent(DERTag tag, const uint8_t *bytes,
	size_t length);

}


ANTLR_BEGIN_NAMESPACE(Security_CodeSigning)

	//
	// Collect error messages.
	// Note that the immediate caller takes the absence of collected error messages
	// to indicate compilation success.
	//
	void RequirementParser::reportError(const antlr::RecognitionException &ex)
	{
		errors += ex.toString() + "\n";
	}
	
	void RequirementParser::reportError(const std::string &s)
	{
		errors += s + "\n";
	}

	
	//
	// Parser helper functions
	//
	string RequirementParser::hexString(const string &s)
	{
		if (s.size() % 2)
			throw antlr::SemanticException("odd number of digits");
		const char *p = s.data();
		string result;
		for (unsigned n = 0; n < s.length(); n += 2) {
			char c;
			sscanf(p+n, "%2hhx", &c);
			result.push_back(c);
		}
		return result;
	}

	void RequirementParser::hashString(const string &s, SHA1::Digest hash)
	{
		if (s.size() != 2 * SHA1::digestLength)
			throw antlr::SemanticException("invalid hash length");
		memcpy(hash, hexString(s).data(), SHA1::digestLength);
	}
	
	static const char *matchPrefix(const string &key, const char *prefix)
	{
		size_t pLength = strlen(prefix);
		if (!key.compare(0, pLength, prefix, 0, pLength))
			return key.c_str() + pLength;
		else
			return NULL;
	}
	
	void RequirementParser::certMatchOperation(Maker &maker, int32_t slot, string key)
	{
		if (const char *oids = matchPrefix(key, "timestamp.")) {
			maker.put(opCertFieldDate);
			maker.put(slot);
			CssmAutoData oid(Allocator::standard()); oid.fromOid(oids);
			maker.putData(oid.data(), oid.length());
		} else if (matchPrefix(key, "subject.")) {
			maker.put(opCertField);
			maker.put(slot);
			maker.put(key);
		} else if (const char *oids = matchPrefix(key, "field.")) {
			maker.put(opCertGeneric);
			maker.put(slot);
			CssmAutoData oid(Allocator::standard()); oid.fromOid(oids);
			maker.putData(oid.data(), oid.length());
		} else if (const char *oids = matchPrefix(key, "extension.")) {
			maker.put(opCertGeneric);
			maker.put(slot);
			CssmAutoData oid(Allocator::standard()); oid.fromOid(oids);
			maker.putData(oid.data(), oid.length());
		} else if (const char *oids = matchPrefix(key, "policy.")) {
			maker.put(opCertPolicy);
			maker.put(slot);
			CssmAutoData oid(Allocator::standard()); oid.fromOid(oids);
			maker.putData(oid.data(), oid.length());
		} else {
			throw antlr::SemanticException(key + ": unrecognized certificate field");
		}
	}

RequirementParser::RequirementParser(antlr::TokenBuffer& tokenBuf, int k)
: antlr::LLkParser(tokenBuf,k)
{
}

RequirementParser::RequirementParser(antlr::TokenBuffer& tokenBuf)
: antlr::LLkParser(tokenBuf,2)
{
}

RequirementParser::RequirementParser(antlr::TokenStream& lexer, int k)
: antlr::LLkParser(lexer,k)
{
}

RequirementParser::RequirementParser(antlr::TokenStream& lexer)
: antlr::LLkParser(lexer,2)
{
}

RequirementParser::RequirementParser(const antlr::ParserSharedInputState& state)
: antlr::LLkParser(state,2)
{
}

BlobCore * RequirementParser::autosense() {
	BlobCore *result = NULL;
	
	try {      // for error handling
		switch ( LA(1)) {
		case LPAREN:
		case NOT:
		case LITERAL_always:
		case LITERAL_true:
		case LITERAL_never:
		case LITERAL_false:
		case LITERAL_identifier:
		case LITERAL_cdhash:
		case LITERAL_platform:
		case LITERAL_notarized:
		case LITERAL_legacy:
		case LITERAL_anchor:
		case LITERAL_certificate:
		case LITERAL_cert:
		case LITERAL_info:
		case LITERAL_entitlement:
		{
			result=requirement();
			break;
		}
		case LITERAL_guest:
		case LITERAL_host:
		case LITERAL_designated:
		case LITERAL_library:
		case LITERAL_plugin:
		case INTEGER:
		{
			result=requirementSet();
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_0);
	}
	return result;
}

Requirement * RequirementParser::requirement() {
	Requirement *result = NULL;
	
	try {      // for error handling
		result=requirementElement();
		match(antlr::Token::EOF_TYPE);
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_0);
	}
	return result;
}

Requirements * RequirementParser::requirementSet() {
	Requirements *result = NULL;
	Requirements::Maker maker;
	
	try {      // for error handling
		{ // ( ... )+
		int _cnt4=0;
		for (;;) {
			if ((_tokenSet_1.member(LA(1)))) {
				uint32_t t; Requirement *req;
				t=requirementType();
				match(ARROW);
				req=requirementElement();
				maker.add(t, req);
			}
			else {
				if ( _cnt4>=1 ) { goto _loop4; } else {throw antlr::NoViableAltException(LT(1), getFilename());}
			}
			
			_cnt4++;
		}
		_loop4:;
		}  // ( ... )+
		result = errors.empty() ? maker() : NULL;
		match(antlr::Token::EOF_TYPE);
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_0);
	}
	return result;
}

uint32_t  RequirementParser::requirementType() {
	uint32_t type = kSecInvalidRequirementType;
	
	try {      // for error handling
		switch ( LA(1)) {
		case LITERAL_guest:
		{
			match(LITERAL_guest);
			type = kSecGuestRequirementType;
			break;
		}
		case LITERAL_host:
		{
			match(LITERAL_host);
			type = kSecHostRequirementType;
			break;
		}
		case LITERAL_designated:
		{
			match(LITERAL_designated);
			type = kSecDesignatedRequirementType;
			break;
		}
		case LITERAL_library:
		{
			match(LITERAL_library);
			type = kSecLibraryRequirementType;
			break;
		}
		case LITERAL_plugin:
		{
			match(LITERAL_plugin);
			type = kSecPluginRequirementType;
			break;
		}
		case INTEGER:
		{
			type=integer();
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_2);
	}
	return type;
}

Requirement * RequirementParser::requirementElement() {
	Requirement *result = NULL;
	Requirement::Maker maker;
	
	try {      // for error handling
		expr(maker);
		result = maker();
		{ // ( ... )*
		for (;;) {
			if ((LA(1) == SEMI)) {
				fluff();
			}
			else {
				goto _loop9;
			}
			
		}
		_loop9:;
		} // ( ... )*
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_3);
	}
	return result;
}

int32_t  RequirementParser::integer() {
	int32_t result;
	antlr::RefToken  s = antlr::nullToken;
	
	try {      // for error handling
		s = LT(1);
		match(INTEGER);
		result = int32_t(atol(s->getText().c_str()));
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_4);
	}
	return result;
}

void RequirementParser::expr(
	Maker &maker
) {
	Maker::Label label(maker);
	
	try {      // for error handling
		term(maker);
		{ // ( ... )*
		for (;;) {
			if ((LA(1) == LITERAL_or)) {
				match(LITERAL_or);
				maker.insert<ExprOp>(label) = opOr;
				term(maker);
			}
			else {
				goto _loop12;
			}
			
		}
		_loop12:;
		} // ( ... )*
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_5);
	}
}

void RequirementParser::fluff() {
	
	try {      // for error handling
		match(SEMI);
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_6);
	}
}

void RequirementParser::term(
	Maker &maker
) {
	Maker::Label label(maker);
	
	try {      // for error handling
		primary(maker);
		{ // ( ... )*
		for (;;) {
			if ((LA(1) == LITERAL_and)) {
				match(LITERAL_and);
				maker.insert<ExprOp>(label) = opAnd;
				primary(maker);
			}
			else {
				goto _loop15;
			}
			
		}
		_loop15:;
		} // ( ... )*
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_7);
	}
}

void RequirementParser::primary(
	Maker &maker
) {
	
	try {      // for error handling
		switch ( LA(1)) {
		case NOT:
		{
			match(NOT);
			maker.put(opNot);
			primary(maker);
			break;
		}
		case LITERAL_always:
		case LITERAL_true:
		{
			{
			switch ( LA(1)) {
			case LITERAL_always:
			{
				match(LITERAL_always);
				break;
			}
			case LITERAL_true:
			{
				match(LITERAL_true);
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(LT(1), getFilename());
			}
			}
			}
			maker.put(opTrue);
			break;
		}
		case LITERAL_never:
		case LITERAL_false:
		{
			{
			switch ( LA(1)) {
			case LITERAL_never:
			{
				match(LITERAL_never);
				break;
			}
			case LITERAL_false:
			{
				match(LITERAL_false);
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(LT(1), getFilename());
			}
			}
			}
			maker.put(opFalse);
			break;
		}
		case LITERAL_anchor:
		case LITERAL_certificate:
		case LITERAL_cert:
		{
			certspec(maker);
			break;
		}
		case LITERAL_info:
		{
			infospec(maker);
			break;
		}
		case LITERAL_entitlement:
		{
			entitlementspec(maker);
			break;
		}
		case LITERAL_identifier:
		{
			match(LITERAL_identifier);
			string code;
			eql();
			code=identifierString();
			maker.ident(code);
			break;
		}
		case LITERAL_cdhash:
		{
			match(LITERAL_cdhash);
			SHA1::Digest digest;
			eql();
			hash(digest);
			maker.cdhash(digest);
			break;
		}
		case LITERAL_platform:
		{
			match(LITERAL_platform);
			int32_t ident;
			eql();
			ident=integer();
			maker.platform(ident);
			break;
		}
		case LITERAL_notarized:
		{
			match(LITERAL_notarized);
			maker.put(opNotarized);
			break;
		}
		case LITERAL_legacy:
		{
			match(LITERAL_legacy);
			maker.put(opLegacyDevID);
			break;
		}
		default:
			if ((LA(1) == LPAREN) && (_tokenSet_8.member(LA(2)))) {
				match(LPAREN);
				expr(maker);
				match(RPAREN);
			}
			else if ((LA(1) == LPAREN) && (LA(2) == DOTKEY || LA(2) == STRING)) {
				match(LPAREN);
				string name;
				name=identifierString();
				match(RPAREN);
				maker.put(opNamedCode); maker.put(name);
			}
		else {
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_9);
	}
}

void RequirementParser::certspec(
	Maker &maker
) {
	
	try {      // for error handling
		if ((LA(1) == LITERAL_anchor) && (LA(2) == LITERAL_apple)) {
			match(LITERAL_anchor);
			match(LITERAL_apple);
			appleanchor(maker);
		}
		else if ((LA(1) == LITERAL_anchor) && (LA(2) == LITERAL_generic)) {
			match(LITERAL_anchor);
			match(LITERAL_generic);
			match(LITERAL_apple);
			maker.put(opAppleGenericAnchor);
		}
		else if ((LA(1) == LITERAL_anchor || LA(1) == LITERAL_certificate || LA(1) == LITERAL_cert) && (LA(2) == LITERAL_trusted)) {
			{
			switch ( LA(1)) {
			case LITERAL_certificate:
			{
				match(LITERAL_certificate);
				break;
			}
			case LITERAL_cert:
			{
				match(LITERAL_cert);
				break;
			}
			case LITERAL_anchor:
			{
				match(LITERAL_anchor);
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(LT(1), getFilename());
			}
			}
			}
			match(LITERAL_trusted);
			maker.trustedAnchor();
		}
		else if ((LA(1) == LITERAL_certificate || LA(1) == LITERAL_cert) && (_tokenSet_10.member(LA(2)))) {
			{
			switch ( LA(1)) {
			case LITERAL_certificate:
			{
				match(LITERAL_certificate);
				break;
			}
			case LITERAL_cert:
			{
				match(LITERAL_cert);
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(LT(1), getFilename());
			}
			}
			}
			int32_t slot;
			slot=certSlot();
			{
			switch ( LA(1)) {
			case EQL:
			case EQQL:
			case LBRACK:
			case HASHCONSTANT:
			case DOTKEY:
			case STRING:
			case PATHNAME:
			{
				certslotspec(maker, slot);
				break;
			}
			case LITERAL_trusted:
			{
				match(LITERAL_trusted);
				maker.trustedAnchor(slot);
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(LT(1), getFilename());
			}
			}
			}
		}
		else if ((LA(1) == LITERAL_anchor) && (_tokenSet_11.member(LA(2)))) {
			match(LITERAL_anchor);
			certslotspec(maker, Requirement::anchorCert);
		}
		else {
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_9);
	}
}

void RequirementParser::infospec(
	Maker &maker
) {
	string key;
	
	try {      // for error handling
		match(LITERAL_info);
		key=bracketKey();
		maker.put(opInfoKeyField); maker.put(key);
		match_suffix(maker);
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_9);
	}
}

void RequirementParser::entitlementspec(
	Maker &maker
) {
	string key;
	
	try {      // for error handling
		match(LITERAL_entitlement);
		key=bracketKey();
		maker.put(opEntitlementField); maker.put(key);
		match_suffix(maker);
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_9);
	}
}

void RequirementParser::eql() {
	
	try {      // for error handling
		switch ( LA(1)) {
		case EQL:
		{
			match(EQL);
			break;
		}
		case EQQL:
		{
			match(EQQL);
			break;
		}
		case HASHCONSTANT:
		case DOTKEY:
		case STRING:
		case PATHNAME:
		case INTEGER:
		{
			empty();
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_12);
	}
}

string  RequirementParser::identifierString() {
	string result;
	antlr::RefToken  dk = antlr::nullToken;
	antlr::RefToken  s = antlr::nullToken;
	
	try {      // for error handling
		switch ( LA(1)) {
		case DOTKEY:
		{
			dk = LT(1);
			match(DOTKEY);
			result = dk->getText();
			break;
		}
		case STRING:
		{
			s = LT(1);
			match(STRING);
			result = s->getText();
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_9);
	}
	return result;
}

void RequirementParser::hash(
	SHA1::Digest digest
) {
	antlr::RefToken  hash = antlr::nullToken;
	
	try {      // for error handling
		hash = LT(1);
		match(HASHCONSTANT);
		hashString(hash->getText(), digest);
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_9);
	}
}

void RequirementParser::appleanchor(
	Maker &maker
) {
	
	try {      // for error handling
		switch ( LA(1)) {
		case antlr::Token::EOF_TYPE:
		case LITERAL_guest:
		case LITERAL_host:
		case LITERAL_designated:
		case LITERAL_library:
		case LITERAL_plugin:
		case LITERAL_or:
		case LITERAL_and:
		case RPAREN:
		case INTEGER:
		case SEMI:
		{
			empty();
			maker.put(opAppleAnchor);
			break;
		}
		case LITERAL_generic:
		{
			match(LITERAL_generic);
			maker.put(opAppleGenericAnchor);
			break;
		}
		case DOTKEY:
		case STRING:
		{
			string name;
			name=identifierString();
			maker.put(opNamedAnchor); maker.put(name);
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_9);
	}
}

int32_t  RequirementParser::certSlot() {
	int32_t slot = 0;
	
	try {      // for error handling
		switch ( LA(1)) {
		case INTEGER:
		{
			slot=integer();
			break;
		}
		case NEG:
		{
			match(NEG);
			slot=integer();
			slot = -slot;
			break;
		}
		case LITERAL_leaf:
		{
			match(LITERAL_leaf);
			slot = Requirement::leafCert;
			break;
		}
		case LITERAL_root:
		{
			match(LITERAL_root);
			slot = Requirement::anchorCert;
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_13);
	}
	return slot;
}

void RequirementParser::certslotspec(
	Maker &maker, int32_t slot
) {
	string key;
	
	try {      // for error handling
		switch ( LA(1)) {
		case EQL:
		case EQQL:
		case HASHCONSTANT:
		case DOTKEY:
		case STRING:
		case PATHNAME:
		{
			eql();
			SHA1::Digest digest;
			certificateDigest(digest);
			maker.anchor(slot, digest);
			break;
		}
		case LBRACK:
		{
			key=bracketKey();
			certMatchOperation(maker, slot, key);
			match_suffix(maker);
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_9);
	}
}

void RequirementParser::empty() {
	
	try {      // for error handling
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_14);
	}
}

void RequirementParser::certificateDigest(
	SHA1::Digest digest
) {
	
	try {      // for error handling
		switch ( LA(1)) {
		case HASHCONSTANT:
		{
			hash(digest);
			break;
		}
		case DOTKEY:
		case STRING:
		case PATHNAME:
		{
			string path;
			path=pathstring();
			if (CFRef<CFDataRef> certData = cfLoadFile(path))
							hashOfCertificate(CFDataGetBytePtr(certData), CFDataGetLength(certData), digest);
						  else
							throw antlr::SemanticException(path + ": not found");
						
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_9);
	}
}

string  RequirementParser::bracketKey() {
	string key;
	
	try {      // for error handling
		match(LBRACK);
		key=stringvalue();
		match(RBRACK);
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_15);
	}
	return key;
}

void RequirementParser::match_suffix(
	Maker &maker
) {
	
	try {      // for error handling
		switch ( LA(1)) {
		case antlr::Token::EOF_TYPE:
		case LITERAL_guest:
		case LITERAL_host:
		case LITERAL_designated:
		case LITERAL_library:
		case LITERAL_plugin:
		case LITERAL_or:
		case LITERAL_and:
		case RPAREN:
		case LITERAL_exists:
		case INTEGER:
		case SEMI:
		{
			empty();
			{
			switch ( LA(1)) {
			case LITERAL_exists:
			{
				match(LITERAL_exists);
				break;
			}
			case antlr::Token::EOF_TYPE:
			case LITERAL_guest:
			case LITERAL_host:
			case LITERAL_designated:
			case LITERAL_library:
			case LITERAL_plugin:
			case LITERAL_or:
			case LITERAL_and:
			case RPAREN:
			case INTEGER:
			case SEMI:
			{
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(LT(1), getFilename());
			}
			}
			}
			maker.put(matchExists);
			break;
		}
		case LITERAL_absent:
		{
			match(LITERAL_absent);
			maker.put(matchAbsent);
			break;
		}
		case SUBS:
		{
			match(SUBS);
			string value;
			value=datavalue();
			maker.put(matchContains); maker.put(value);
			break;
		}
		default:
			if ((LA(1) == EQL || LA(1) == EQQL) && (_tokenSet_16.member(LA(2)))) {
				{
				switch ( LA(1)) {
				case EQL:
				{
					match(EQL);
					break;
				}
				case EQQL:
				{
					match(EQQL);
					break;
				}
				default:
				{
					throw antlr::NoViableAltException(LT(1), getFilename());
				}
				}
				}
				MatchOperation mop = matchEqual; string value;
				{
				switch ( LA(1)) {
				case STAR:
				{
					match(STAR);
					mop = matchEndsWith;
					break;
				}
				case HEXCONSTANT:
				case DOTKEY:
				case STRING:
				{
					break;
				}
				default:
				{
					throw antlr::NoViableAltException(LT(1), getFilename());
				}
				}
				}
				value=datavalue();
				{
				switch ( LA(1)) {
				case STAR:
				{
					match(STAR);
					mop = (mop == matchEndsWith) ? matchContains : matchBeginsWith;
					break;
				}
				case antlr::Token::EOF_TYPE:
				case LITERAL_guest:
				case LITERAL_host:
				case LITERAL_designated:
				case LITERAL_library:
				case LITERAL_plugin:
				case LITERAL_or:
				case LITERAL_and:
				case RPAREN:
				case INTEGER:
				case SEMI:
				{
					break;
				}
				default:
				{
					throw antlr::NoViableAltException(LT(1), getFilename());
				}
				}
				}
				maker.put(mop); maker.put(value);
			}
			else if ((LA(1) == EQL || LA(1) == EQQL) && (LA(2) == LITERAL_timestamp)) {
				{
				switch ( LA(1)) {
				case EQL:
				{
					match(EQL);
					break;
				}
				case EQQL:
				{
					match(EQQL);
					break;
				}
				default:
				{
					throw antlr::NoViableAltException(LT(1), getFilename());
				}
				}
				}
				MatchOperation mop = matchOn; int64_t value;
				value=timestamp();
				maker.put(mop); maker.put(value);
			}
			else if ((LA(1) == LESS) && ((LA(2) >= HEXCONSTANT && LA(2) <= STRING))) {
				match(LESS);
				string value;
				value=datavalue();
				maker.put(matchLessThan); maker.put(value);
			}
			else if ((LA(1) == GT) && ((LA(2) >= HEXCONSTANT && LA(2) <= STRING))) {
				match(GT);
				string value;
				value=datavalue();
				maker.put(matchGreaterThan); maker.put(value);
			}
			else if ((LA(1) == LE) && ((LA(2) >= HEXCONSTANT && LA(2) <= STRING))) {
				match(LE);
				string value;
				value=datavalue();
				maker.put(matchLessEqual); maker.put(value);
			}
			else if ((LA(1) == GE) && ((LA(2) >= HEXCONSTANT && LA(2) <= STRING))) {
				match(GE);
				string value;
				value=datavalue();
				maker.put(matchGreaterEqual); maker.put(value);
			}
			else if ((LA(1) == LESS) && (LA(2) == LITERAL_timestamp)) {
				match(LESS);
				int64_t value;
				value=timestamp();
				maker.put(matchBefore); maker.put(value);
			}
			else if ((LA(1) == GT) && (LA(2) == LITERAL_timestamp)) {
				match(GT);
				int64_t value;
				value=timestamp();
				maker.put(matchAfter); maker.put(value);
			}
			else if ((LA(1) == LE) && (LA(2) == LITERAL_timestamp)) {
				match(LE);
				int64_t value;
				value=timestamp();
				maker.put(matchOnOrBefore); maker.put(value);
			}
			else if ((LA(1) == GE) && (LA(2) == LITERAL_timestamp)) {
				match(GE);
				int64_t value;
				value=timestamp();
				maker.put(matchOnOrAfter); maker.put(value);
			}
		else {
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_9);
	}
}

string  RequirementParser::datavalue() {
	string result;
	antlr::RefToken  hex = antlr::nullToken;
	
	try {      // for error handling
		switch ( LA(1)) {
		case DOTKEY:
		case STRING:
		{
			result=stringvalue();
			break;
		}
		case HEXCONSTANT:
		{
			hex = LT(1);
			match(HEXCONSTANT);
			result = hexString(hex->getText());
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_17);
	}
	return result;
}

int64_t  RequirementParser::timestamp() {
	int64_t result;
	antlr::RefToken  s = antlr::nullToken;
	
	try {      // for error handling
		match(LITERAL_timestamp);
		s = LT(1);
		match(STRING);
		result = (int64_t)SecAbsoluteTimeFromDateContent(ASN1_GENERALIZED_TIME, (uint8_t const *)s->getText().c_str(), s->getText().length());
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_9);
	}
	return result;
}

string  RequirementParser::stringvalue() {
	string result;
	antlr::RefToken  dk = antlr::nullToken;
	antlr::RefToken  s = antlr::nullToken;
	
	try {      // for error handling
		switch ( LA(1)) {
		case DOTKEY:
		{
			dk = LT(1);
			match(DOTKEY);
			result = dk->getText();
			break;
		}
		case STRING:
		{
			s = LT(1);
			match(STRING);
			result = s->getText();
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_18);
	}
	return result;
}

string  RequirementParser::pathstring() {
	string result;
	antlr::RefToken  dk = antlr::nullToken;
	antlr::RefToken  s = antlr::nullToken;
	antlr::RefToken  pn = antlr::nullToken;
	
	try {      // for error handling
		switch ( LA(1)) {
		case DOTKEY:
		{
			dk = LT(1);
			match(DOTKEY);
			result = dk->getText();
			break;
		}
		case STRING:
		{
			s = LT(1);
			match(STRING);
			result = s->getText();
			break;
		}
		case PATHNAME:
		{
			pn = LT(1);
			match(PATHNAME);
			result = pn->getText();
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		reportError(ex);
		recover(ex,_tokenSet_9);
	}
	return result;
}

void RequirementParser::initializeASTFactory( antlr::ASTFactory& )
{
}
const char* RequirementParser::tokenNames[] = {
	"<0>",
	"EOF",
	"<2>",
	"NULL_TREE_LOOKAHEAD",
	"ARROW",
	"\"guest\"",
	"\"host\"",
	"\"designated\"",
	"\"library\"",
	"\"plugin\"",
	"\"or\"",
	"\"and\"",
	"LPAREN",
	"RPAREN",
	"NOT",
	"\"always\"",
	"\"true\"",
	"\"never\"",
	"\"false\"",
	"\"identifier\"",
	"\"cdhash\"",
	"\"platform\"",
	"\"notarized\"",
	"\"legacy\"",
	"\"anchor\"",
	"\"apple\"",
	"\"generic\"",
	"\"certificate\"",
	"\"cert\"",
	"\"trusted\"",
	"\"info\"",
	"\"entitlement\"",
	"\"exists\"",
	"\"absent\"",
	"EQL",
	"EQQL",
	"STAR",
	"SUBS",
	"LESS",
	"GT",
	"LE",
	"GE",
	"LBRACK",
	"RBRACK",
	"NEG",
	"\"leaf\"",
	"\"root\"",
	"HASHCONSTANT",
	"HEXCONSTANT",
	"DOTKEY",
	"STRING",
	"PATHNAME",
	"INTEGER",
	"\"timestamp\"",
	"SEMI",
	"IDENT",
	"HEX",
	"COMMA",
	"WS",
	"SHELLCOMMENT",
	"C_COMMENT",
	"CPP_COMMENT",
	0
};

const unsigned long RequirementParser::_tokenSet_0_data_[] = { 2UL, 0UL, 0UL, 0UL };
// EOF
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_0(_tokenSet_0_data_,4);
const unsigned long RequirementParser::_tokenSet_1_data_[] = { 992UL, 1048576UL, 0UL, 0UL };
// "guest" "host" "designated" "library" "plugin" INTEGER
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_1(_tokenSet_1_data_,4);
const unsigned long RequirementParser::_tokenSet_2_data_[] = { 16UL, 0UL, 0UL, 0UL };
// ARROW
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_2(_tokenSet_2_data_,4);
const unsigned long RequirementParser::_tokenSet_3_data_[] = { 994UL, 1048576UL, 0UL, 0UL };
// EOF "guest" "host" "designated" "library" "plugin" INTEGER
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_3(_tokenSet_3_data_,4);
const unsigned long RequirementParser::_tokenSet_4_data_[] = { 536883186UL, 6194188UL, 0UL, 0UL };
// EOF ARROW "guest" "host" "designated" "library" "plugin" "or" "and" 
// RPAREN "trusted" EQL EQQL LBRACK HASHCONSTANT DOTKEY STRING PATHNAME 
// INTEGER SEMI 
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_4(_tokenSet_4_data_,4);
const unsigned long RequirementParser::_tokenSet_5_data_[] = { 9186UL, 5242880UL, 0UL, 0UL };
// EOF "guest" "host" "designated" "library" "plugin" RPAREN INTEGER SEMI 
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_5(_tokenSet_5_data_,4);
const unsigned long RequirementParser::_tokenSet_6_data_[] = { 994UL, 5242880UL, 0UL, 0UL };
// EOF "guest" "host" "designated" "library" "plugin" INTEGER SEMI 
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_6(_tokenSet_6_data_,4);
const unsigned long RequirementParser::_tokenSet_7_data_[] = { 10210UL, 5242880UL, 0UL, 0UL };
// EOF "guest" "host" "designated" "library" "plugin" "or" RPAREN INTEGER 
// SEMI 
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_7(_tokenSet_7_data_,4);
const unsigned long RequirementParser::_tokenSet_8_data_[] = { 3657420800UL, 0UL, 0UL, 0UL };
// LPAREN NOT "always" "true" "never" "false" "identifier" "cdhash" "platform" 
// "notarized" "legacy" "anchor" "certificate" "cert" "info" "entitlement" 
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_8(_tokenSet_8_data_,4);
const unsigned long RequirementParser::_tokenSet_9_data_[] = { 12258UL, 5242880UL, 0UL, 0UL };
// EOF "guest" "host" "designated" "library" "plugin" "or" "and" RPAREN 
// INTEGER SEMI 
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_9(_tokenSet_9_data_,4);
const unsigned long RequirementParser::_tokenSet_10_data_[] = { 0UL, 1077248UL, 0UL, 0UL };
// NEG "leaf" "root" INTEGER 
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_10(_tokenSet_10_data_,4);
const unsigned long RequirementParser::_tokenSet_11_data_[] = { 0UL, 951308UL, 0UL, 0UL };
// EQL EQQL LBRACK HASHCONSTANT DOTKEY STRING PATHNAME 
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_11(_tokenSet_11_data_,4);
const unsigned long RequirementParser::_tokenSet_12_data_[] = { 0UL, 1998848UL, 0UL, 0UL };
// HASHCONSTANT DOTKEY STRING PATHNAME INTEGER 
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_12(_tokenSet_12_data_,4);
const unsigned long RequirementParser::_tokenSet_13_data_[] = { 536870912UL, 951308UL, 0UL, 0UL };
// "trusted" EQL EQQL LBRACK HASHCONSTANT DOTKEY STRING PATHNAME 
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_13(_tokenSet_13_data_,4);
const unsigned long RequirementParser::_tokenSet_14_data_[] = { 12258UL, 6193153UL, 0UL, 0UL };
// EOF "guest" "host" "designated" "library" "plugin" "or" "and" RPAREN 
// "exists" HASHCONSTANT DOTKEY STRING PATHNAME INTEGER SEMI 
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_14(_tokenSet_14_data_,4);
const unsigned long RequirementParser::_tokenSet_15_data_[] = { 12258UL, 5243887UL, 0UL, 0UL };
// EOF "guest" "host" "designated" "library" "plugin" "or" "and" RPAREN 
// "exists" "absent" EQL EQQL SUBS LESS GT LE GE INTEGER SEMI 
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_15(_tokenSet_15_data_,4);
const unsigned long RequirementParser::_tokenSet_16_data_[] = { 0UL, 458768UL, 0UL, 0UL };
// STAR HEXCONSTANT DOTKEY STRING 
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_16(_tokenSet_16_data_,4);
const unsigned long RequirementParser::_tokenSet_17_data_[] = { 12258UL, 5242896UL, 0UL, 0UL };
// EOF "guest" "host" "designated" "library" "plugin" "or" "and" RPAREN 
// STAR INTEGER SEMI 
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_17(_tokenSet_17_data_,4);
const unsigned long RequirementParser::_tokenSet_18_data_[] = { 12258UL, 5244944UL, 0UL, 0UL };
// EOF "guest" "host" "designated" "library" "plugin" "or" "and" RPAREN 
// STAR RBRACK INTEGER SEMI 
[[clang::no_destroy]] const antlr::BitSet RequirementParser::_tokenSet_18(_tokenSet_18_data_,4);


ANTLR_END_NAMESPACE
