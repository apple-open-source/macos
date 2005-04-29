/*
 * CCacheString.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/Headers/CCacheString.h,v 1.7 2003/03/17 20:47:01 lxs Exp $
 */
 
#pragma once

#include "Magic.h"
#include "Internal.h"
#include "Internalize.h"

class CCEString {
	public:
		static cc_int32 Release (
			cc_string_t	inString);
			
	private:
		// Disallowed
		CCEString ();
		CCEString (const CCEString&);
		CCEString& operator = (const CCEString&);
};

class CCIString:
	public CCIMagic <CCIString>,
	public CCIInternal <CCIString, cc_string_d> {

	public:
		CCIString (
			const std::string&		inString);
			
		~CCIString ();
			
		enum {
			class_ID = FOUR_CHAR_CODE ('CCSt'),
			invalidObject = ccErrInvalidString
		};
		
	private:
		std::string			mString;
		
		void		Validate ();

		static const	cc_string_f	sFunctionTable;

		friend class StInternalize <CCIString, cc_string_d>;
		friend class CCIInternal <CCIString, cc_string_d>;

		// Disallowed
		CCIString ();
		CCIString (const CCIString&);
		CCIString& operator = (const CCIString&);
};

typedef StInternalize <CCIString, cc_string_d>	StString;

