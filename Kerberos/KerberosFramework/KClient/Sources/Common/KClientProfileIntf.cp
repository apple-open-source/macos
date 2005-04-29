#include <sys/param.h>

#include "KClientProfileIntf.h"

KClientProfileInterface::KClientProfileInterface (
	profile_t			inProfileHandle):
	mProfile (inProfileHandle)
{
}

KClientProfileInterface::KClientProfileInterface () {
}

KClientProfileInterface::~KClientProfileInterface () {
}

#ifdef KClientDeprecated_
		
void
KClientProfileInterface::GetLocalRealm (
			char*		outRealm) const {

	UProfileInputList	relation (
		REALMS_V4_PROF_LIBDEFAULTS_SECTION,
		REALMS_V4_PROF_LOCAL_REALM
	);
	UProfileOutputList	values;

	mProfile.GetValues (relation, values);
	strncpy (outRealm, values [0], REALM_SZ);
	outRealm [REALM_SZ - 1] = 0;
	
	try {
		UProfileInputList	relation (
			"realms",
			outRealm,
			"v4_realm");
			
		mProfile.GetValues (relation, values);

		// If we have a correpsonding v4 realm, that's good, if the realm is a valid v4 realm
		if (values [0] != nil) {
			strncpy (outRealm, values [0], REALM_SZ);
			outRealm [REALM_SZ - 1] = 0;
		}
		
	} catch (UProfileConfigurationError&) {
		// If we get a configuration error, there is no corresponding v4 realm
	} catch (...) {
		throw;
	}
}
	
void
KClientProfileInterface::SetLocalRealm (
	const 	char*		inRealm) {

	UProfile::StProfileChanger		profileChanger (mProfile);

	UProfileInputList	relation (
		REALMS_V4_PROF_LIBDEFAULTS_SECTION,
		REALMS_V4_PROF_LOCAL_REALM
	);
	UProfileOutputList	values;
	
	mProfile.GetValues (relation, values);

	UProfileInputString		name (values [0]);
	UProfileInputString		value (inRealm);

	mProfile.UpdateRelation (relation, name, value);
}	
	
void
KClientProfileInterface::GetRealmOfHost (
	const	char*		inHost,
			char*		outRealm) const {
	char*	realm = krb_realmofhost (const_cast <char*> (inHost));
	if ((realm == NULL) || (realm [0] == '\0'))
		DebugThrow_ (KClientRuntimeError (kcErrInvalidPreferences));
	
	strncpy (outRealm, realm, REALM_SZ);
	outRealm [REALM_SZ - 1] = '\0';
}

void
KClientProfileInterface::AddRealmMap (
	const	char*		inDomain,
	const	char*		inRealm) {

	UProfile::StProfileChanger		profileChanger (mProfile);

	UProfileInputList	relation (
		REALMS_V4_PROF_DOMAIN_SECTION,
		inDomain
	);
	UProfileOutputList	values;
	
	// If it already exists, we change it (this is different from old behavior, which would
	// keep both, but that's just bogus.
	try {
		mProfile.GetValues (relation, values);
		mProfile.UpdateRelation (relation, values [0], inRealm);

	} catch (UProfileConfigurationError& e) {

		if (e.Error () != PROF_NO_RELATION)
			throw;
		mProfile.AddRelation (relation, inRealm);
	}
}	

void
KClientProfileInterface::DeleteRealmMap (
	const	char*		inHost) {

	UProfile::StProfileChanger		profileChanger (mProfile);

	UProfileInputList	relation (
		REALMS_V4_PROF_DOMAIN_SECTION,
		inHost
	);

	mProfile.ClearRelation (relation);
}	

void
KClientProfileInterface::GetNthRealmMap (
			SInt32		inIndex,
			char*		outHost,
			char*		outRealm) const {
	
	if (inIndex < 1)
		DebugThrow_ (std::range_error ("KClientProfileInterface::GetNthRealmMap: inIndex < 1"));
	
	UProfileInputList		section (
		REALMS_V4_PROF_DOMAIN_SECTION);
	UProfileOutputString		name;
	UProfileOutputString		value;
	UInt32				index = 0;
	Boolean				found = false;
	
	UProfileIterator 	iterator  = mProfile. NewIterator (section, PROFILE_ITER_LIST_SECTION);
	
	while (iterator.Next (name, value)) {
		index++;
		if ((SInt32) index == inIndex) {
			strncpy (outRealm, value.Get (), REALM_SZ);
			strncpy (outHost, name.Get (), MAXHOSTNAMELEN);
			outRealm [REALM_SZ - 1] = '\0';
			outHost [MAXHOSTNAMELEN - 1] = '\0';
			found = true;
			break;
		} 
	}
	
	if (!found)
		DebugThrow_ (std::range_error ("KClientProfileInterface::GetNthRealmMap: inIndex too large"));
}

void
KClientProfileInterface::GetNthServer (
			SInt32		inIndex,
	const	char*		inRealm,
			Boolean		inAdmin,
			char*		outHost) const {
			
	if (inIndex < 1)
		DebugThrow_ (std::range_error ("KClientProfileInterface::GetNthServer: inIndex < 1"));
		
	UProfileOutputString	name;
	UProfileOutputString	value;
		
	SInt32			index = 0;
	Boolean			found = false;

	if (!inAdmin) {
		UProfileInputList	kdcRelation (
			REALMS_V4_PROF_REALMS_SECTION,
			inRealm,
			REALMS_V4_PROF_KDC
		);
		
		UProfileIterator	iterator = mProfile.NewIterator (kdcRelation, PROFILE_ITER_RELATIONS_ONLY);
		
		while (iterator.Next (name, value)) {
			index++;
			
			if (index == inIndex) {
				found = true;
				
				// Remove port from output
				UInt32	hostLength;
				char*	colon = strchr (value.Get (), ':');
				if (colon == nil) {
					hostLength = strlen (value.Get ());
				} else {
					hostLength = (UInt32) (colon - value.Get ());
				}

				if (hostLength > MAXHOSTNAMELEN - 1)
					hostLength = MAXHOSTNAMELEN - 1;

				strncpy (outHost, value.Get (), hostLength);
				outHost [hostLength] = '\0';
				
				break;
			}
		}
	} else {
		UProfileInputList	adminRelation (
			REALMS_V4_PROF_REALMS_SECTION,
			inRealm,
			REALMS_V4_PROF_ADMIN_KDC
		);
		
		UProfileIterator	iterator = mProfile.NewIterator (adminRelation, PROFILE_ITER_RELATIONS_ONLY);
		
		while (iterator.Next (name, value)) {
			index++;
			
			if (index == inIndex) {
				found = true;
				
				strncpy (outHost, value.Get (), MAXHOSTNAMELEN);
				outHost [MAXHOSTNAMELEN - 1] = '\0';
				
				break;
			}
		}
	}
	if (!found)
		DebugThrow_ (std::range_error ("KClientProfileInterface::GetNthServer: inIndex too large"));
}
			
void
KClientProfileInterface::AddServerMap (
	const	char*		inHost,
	const	char*		inRealm,
			Boolean		inAdmin) {

	UProfile::StProfileChanger	profileChanger (mProfile);

	UProfileInputList	kdcRelation (
		REALMS_V4_PROF_REALMS_SECTION,
		inRealm,
		REALMS_V4_PROF_KDC
	);
	UProfileOutputList		values;
	UProfileOutputString	name;
	UProfileOutputString	value;
	
	// If it already exists, we do nothing. Existence check must parse hostname:port
	Boolean found = false;
	try {
		UProfileIterator	iterator = mProfile.NewIterator (kdcRelation, PROFILE_ITER_RELATIONS_ONLY);
		
		while (iterator.Next (name, value)) {
	
			UInt32	hostLength;
			char*	colon = strchr (value.Get (), ':');
			if (colon == nil) {
				hostLength = strlen (value.Get ());
			} else {
				hostLength = (UInt32) (colon - value.Get ());
			}

			if (strncmp (inHost, value.Get (), hostLength) == 0) {
				found = true;
				break;
			}
		}
	} catch (UProfileConfigurationError& e) {
		if (e.Error () != PROF_NO_RELATION)
			throw;
	}
	
	if (!found)
		mProfile.AddRelation (kdcRelation, inHost);
			
	UProfileInputList	adminRelation (
		REALMS_V4_PROF_REALMS_SECTION,
		inRealm,
		REALMS_V4_PROF_ADMIN_KDC
	);
		
	if (!inAdmin) {
		// If we are adding as non-admin a server that is already an admin,
		// remove it from admin
		found = false;
		UProfileIterator	iterator = mProfile.NewIterator (adminRelation, PROFILE_ITER_RELATIONS_ONLY);
		while (iterator.Next (name, value)) {
	
			if (strcmp (inHost, value.Get ()) == 0) {
				mProfile.UpdateRelation (adminRelation, value, nil);
				break;
			}
		}
		
		return;
	}


	try {
		found = false;
		UProfileIterator	iterator = mProfile.NewIterator (adminRelation, PROFILE_ITER_RELATIONS_ONLY);
		
		while (iterator.Next (name, value)) {
	
			if (strcmp (inHost, value.Get ()) == 0) {
				found = true;
				break;
			}
		}
	} catch (UProfileConfigurationError& e) {
		if (e.Error () != PROF_NO_RELATION)
			throw;
	}
	
	if (!found)			
		mProfile.AddRelation (adminRelation, inHost);
}	

			
void
KClientProfileInterface::DeleteServerMap (
	const	char*		inRealm,
	const	char*		inHost) {

	UProfile::StProfileChanger	profileChanger (mProfile);

	UProfileInputList	relation (
		REALMS_V4_PROF_REALMS_SECTION,
		inRealm,
		REALMS_V4_PROF_KDC
	);
	UProfileOutputString	name;
	UProfileOutputString	value;
	
	// This is annoying. Since KDCs are specified as hostname:port, I can't just
	// call UpdateRelation (nameServer, inHost, nil). I need to look at each KDC, 
	// parse the hostname
	
	UProfileIterator	iterator = mProfile.NewIterator (relation, PROFILE_ITER_RELATIONS_ONLY);
	
	while (iterator.Next (name, value)) {
		// Find :
		char*	colon = strchr (value.Get (), ':');
		if (colon != nil) {
			// : found, compare hostname
			UInt32	colonIndex = (UInt32) (colon - value.Get ());
			if (strncmp (value.Get (), inHost, colonIndex) == 0) {
				mProfile.UpdateRelation (relation, value.Get (), nil);
				break;
			}
		}
	}

	// remove the corresponding admin entry, if it exists. admin entries don't have port.
	UProfileInputList	nameAdmin (
		REALMS_V4_PROF_REALMS_SECTION,
		inRealm,
		REALMS_V4_PROF_ADMIN_KDC
	);

	try {	
		mProfile.UpdateRelation (nameAdmin, inHost, nil);
	} catch (UProfileConfigurationError& e) {

		if (e.Error () != PROF_NO_RELATION)
			throw;
	}
}	

/* One word: bogus! Whose idea was it to provide API to enumerate servers _and_ realms in
 * one call?!
 */
void
KClientProfileInterface::GetNthServerMap (
			SInt32		inIndex,
			char*		outHost,
			char*		outRealm,
			Boolean&	outAdmin) const {
			
	if (inIndex < 1)
		DebugThrow_ (std::range_error ("KClientProfileInterface::GetNthServerMap: inIndex < 1"));
			
	SInt32			index = 0;
	Boolean			found = false;
	
	UProfileInputList		realmRelation (
		REALMS_V4_PROF_REALMS_SECTION
	);

	UProfileIterator 	realmIterator = mProfile.NewIterator (realmRelation, PROFILE_ITER_LIST_SECTION);
	
	UProfileOutputString		name;
	UProfileOutputString		realmName;
	
	while (realmIterator.Next (realmName, name)) {
		// This inner block creates a new kdcIterator every time through the loop
		{
			UProfileInputList	kdcRelation (
				REALMS_V4_PROF_REALMS_SECTION,
				realmName.Get (),
				REALMS_V4_PROF_KDC
			);
			UProfileIterator	kdcIterator = mProfile.NewIterator (kdcRelation, PROFILE_ITER_RELATIONS_ONLY);
			
			UProfileOutputString		kdcName;
			
			while (kdcIterator.Next (name, kdcName)) {
				
				index++;
				
				if (index == inIndex) {
					strncpy (outRealm, realmName.Get (), REALM_SZ);
					outRealm [REALM_SZ - 1]  = '\0';

					// Remove port from kdcName
					char*	colon = strchr (kdcName.Get (), ':');
					UInt32	hostLength;
					if (colon == nil) {
						hostLength = strlen (kdcName.Get ());
					} else {
						hostLength = (UInt32) (colon - kdcName.Get ());
					}
					
					if (hostLength > MAXHOSTNAMELEN - 1)
						hostLength = MAXHOSTNAMELEN - 1;
						
					strncpy (outHost, kdcName.Get (), hostLength);
					outHost [hostLength] = '\0';
					found = true;
					break;
				}
			}
			
			if (found)
				break;
		}
	}
	
	if (!found)
		DebugThrow_ (std::range_error ("KClientProfileInterface::GetNthServerMap: inIndex too large"));
		
	// If found, look if it's an admin server
	
	UProfileInputList	adminRelation (
		REALMS_V4_PROF_REALMS_SECTION,
		outRealm,
		REALMS_V4_PROF_ADMIN_KDC
	);
	UProfileIterator		adminIterator = mProfile.NewIterator (adminRelation, PROFILE_ITER_RELATIONS_ONLY);
	
	UProfileOutputString	adminName;
	Boolean					admin = false;
	
	while (adminIterator.Next (name, adminName)) {
		if (strcmp (adminName.Get (), outHost) == 0) {
			admin = true;
		}
	}
	
	outAdmin = admin;
}	

UInt16
KClientProfileInterface::GetNthServerPort (
			SInt32		inIndex) const {
			
	if (inIndex < 1)
		DebugThrow_ (std::range_error ("KClientProfileInterface::GetNthServerPort: inIndex < 1"));
			
	SInt32			index = 0;
	
	UProfileInputList		realmRelation (
		REALMS_V4_PROF_REALMS_SECTION
	);

	UProfileIterator 	realmIterator = mProfile.NewIterator (realmRelation, PROFILE_ITER_LIST_SECTION);
	
	UProfileOutputString		name;
	UProfileOutputString		realmName;
	
	while (realmIterator.Next (realmName, name)) {
		// This inner block creates a new kdcIterator every time through the loop
		{
			UProfileInputList	kdcRelation (
				REALMS_V4_PROF_REALMS_SECTION,
				realmName.Get (),
				REALMS_V4_PROF_KDC
			);
			UProfileIterator	kdcIterator = mProfile.NewIterator (kdcRelation, PROFILE_ITER_RELATIONS_ONLY);
			
			UProfileOutputString		kdcName;
			
			while (kdcIterator.Next (name, kdcName)) {
				
				index++;
				
				if (index == inIndex) {
					char*	colon = strchr (kdcName.Get (), ':');
					if (colon == nil) {
						// Is this correct?
						return 0;
					} else {
						return (UInt16) atoi (colon + 1);
					}
				}
			}
		}
	}

	DebugThrow_ (std::range_error ("KClientProfileInterface::GetNthServerPort: inIndex too large"));

	return 0; // silence the warning
}	

void
KClientProfileInterface::SetNthServerPort (
			SInt32		inIndex,
			UInt16		inPort) {

	if (inIndex < 1)
		DebugThrow_ (std::range_error ("KClientProfileInterface::SetNthServerPort: inIndex < 1"));
			
	UProfile::StProfileChanger	profileChanger (mProfile);
	SInt32			index = 0;
	Boolean			found = false;
	
	UProfileInputList		realmRelation (
		REALMS_V4_PROF_REALMS_SECTION
	);

	UProfileIterator 	realmIterator = mProfile.NewIterator (realmRelation, PROFILE_ITER_LIST_SECTION);
	
	UProfileOutputString		name;
	UProfileOutputString		realmName;
	
	while (realmIterator.Next (realmName, name)) {
		// This inner block creates a new kdcIterator every time through the loop
		{
			UProfileInputList	kdcRelation (
				REALMS_V4_PROF_REALMS_SECTION,
				realmName.Get (),
				REALMS_V4_PROF_KDC
			);
			UProfileIterator	kdcIterator = mProfile.NewIterator (kdcRelation, PROFILE_ITER_RELATIONS_ONLY);
			
			UProfileOutputString		kdcName;
			
			while (kdcIterator.Next (name, kdcName)) {
				
				index++;
				
				if (index == inIndex) {
				
					char		hostnameWithPort [MAXHOSTNAMELEN + 1 /* : */ + 5 /* port */];
					char*		colon = strchr (kdcName.Get (), ':');
					UInt32		hostLength;
					
					if (colon == nil) {
						hostLength = strlen (kdcName.Get ());
					} else {
						hostLength = (UInt32) (colon - kdcName.Get ());
					}
					
					if (hostLength > MAXHOSTNAMELEN - 1)
						hostLength = MAXHOSTNAMELEN - 1;
						
					strncpy (hostnameWithPort, kdcName.Get (), hostLength);
					hostnameWithPort [hostLength] = '\0';
					
					if (inPort != 0) {
						sprintf (hostnameWithPort + hostLength, ":%u", inPort);
					}
					
					mProfile.UpdateRelation (kdcRelation, kdcName, hostnameWithPort);
					found = true;
					break;
				}
			}
			
			if (found)
				break;
		}
	}
	
	if (!found)
		DebugThrow_ (std::range_error ("KClientProfileInterface::SetNthServerPort: inIndex too large"));
}	

#endif // KClientDeprecated_
