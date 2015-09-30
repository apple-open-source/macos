//
//  agentclient.h
//  securityd
//
//  Created by cschmidt on 11/24/14.
//

#ifndef securityd_agentclient_h
#define securityd_agentclient_h

namespace SecurityAgent {
	enum Reason {
		noReason = 0,					// no reason (not used, used as a NULL)
		unknownReason,					// something else (catch-all internal error)

		// reasons for asking for a new passphrase
		newDatabase = 11,				// need passphrase for a new database
		changePassphrase,				// changing passphrase for existing database

		// reasons for retrying an unlock query
		invalidPassphrase = 21,			// passphrase was wrong

		// reasons for retrying a new passphrase query
		passphraseIsNull = 31,			// empty passphrase
		passphraseTooSimple,			// passphrase is not complex enough
		passphraseRepeated,				// passphrase was used before (must use new one)
		passphraseUnacceptable,			// passphrase unacceptable for some other reason
		oldPassphraseWrong,				// the old passphrase given is wrong

		// reasons for retrying an authorization query
		userNotInGroup = 41,			// authenticated user not in needed group
		unacceptableUser,				// authenticated user unacceptable for some other reason

		// reasons for canceling a staged query
		tooManyTries = 61,				// too many failed attempts to get it right
		noLongerNeeded,					// the queried item is no longer needed
		keychainAddFailed,				// the requested itemed couldn't be added to the keychain
		generalErrorCancel,				// something went wrong so we have to give up now
		resettingPassword,              // The user has indicated that they wish to reset their password

		worldChanged = 101
	};

	typedef enum {
		tool = 'TOOL',
		bundle = 'BNDL',
		unknown = 'UNKN'
	} RequestorType;
}
#endif
