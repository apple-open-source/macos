#include "attributes.h"

//#include <Keychain.h>
//#include <SecurityCore/KeychainPriv.h>

#include <CoreServices/../Frameworks/OSServices.framework/Headers/KeychainCore.h>
// --------------------------------------------------------------------
//			Attribute initializations
//--------------------------------------------------------------------

// Meta Attributes
CSSM_DB_NAME_ATTR(Attributes::RelationID, 0, "RelationID", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::RelationName, 1, "RelationName", 0, NULL, STRING);
CSSM_DB_NAME_ATTR(Attributes::AttributeID, 1, "AttributeID", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::AttributeNameFormat, 2, "AttributeNameFormat", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::AttributeName, 3, "AttributeName", 0, NULL, STRING);
CSSM_DB_NAME_ATTR(Attributes::AttributeNameID, 4, "AttributeNameID", 0, NULL, BLOB);
CSSM_DB_NAME_ATTR(Attributes::AttributeFormat, 5, "AttributeFormat", 0, NULL, UINT32);

// Keychain Attributes.
//CSSM_DB_INTEGER_ATTR(Attributes::Protected, kProtectedDataKCItemAttr, "Protected", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Class, kClassKCItemAttr, "Class", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::CreationDate, kCreationDateKCItemAttr, "CreationDate", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::ModDate, kModDateKCItemAttr, "ModDate", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Description, kDescriptionKCItemAttr, "Description", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Comment, kCommentKCItemAttr, "Comment", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Creator, kCreatorKCItemAttr, "Creator", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Type, kTypeKCItemAttr, "Type", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::ScrCode, kScriptCodeKCItemAttr, "ScriptCode", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Label, kLabelKCItemAttr, "Label", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Invisible, kInvisibleKCItemAttr, "Invisible", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Negative, kNegativeKCItemAttr, "Negative", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Custom, kCustomIconKCItemAttr, "CustomIcon", 0, NULL, BLOB);
// 	for Generic Password items:
CSSM_DB_INTEGER_ATTR(Attributes::Account, kAccountKCItemAttr, "Account", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Service, kServiceKCItemAttr, "Service", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Generic, kGenericKCItemAttr, "Generic", 0, NULL, BLOB);
// 	for Internet Password items:
CSSM_DB_INTEGER_ATTR(Attributes::SecDomain, kSecurityDomainKCItemAttr, "SecurityDomain", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Server, kServerKCItemAttr, "Server", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::AuthType, kAuthTypeKCItemAttr, "AuthType", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Port, kPortKCItemAttr, "Port", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Path, kPathKCItemAttr, "Path", 0, NULL, BLOB);
// 	for AppleShare Password items:
CSSM_DB_INTEGER_ATTR(Attributes::Volume, kVolumeKCItemAttr, "Volume", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Addr, kAddressKCItemAttr, "Address", 0, NULL, BLOB);
CSSM_DB_INTEGER_ATTR(Attributes::Signature, kSignatureKCItemAttr, "Signature", 0, NULL, BLOB);
// 	for AppleShare and Interent Password items:
CSSM_DB_INTEGER_ATTR(Attributes::ProtocolType, kProtocolKCItemAttr, "Protocol", 0, NULL, BLOB);

// Key Attributes
CSSM_DB_NAME_ATTR(Attributes::KeyClass, 0, "KeyClass", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::PrintName, 1, "PrintName", 0, NULL, BLOB);
CSSM_DB_NAME_ATTR(Attributes::Alias, 2, "Alias", 0, NULL, BLOB);
CSSM_DB_NAME_ATTR(Attributes::Permanent, 3, "Permanent", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::Private, 4, "Private", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::Modifiable, 5, "Modifiable", 0, NULL, UINT32);
//CSSM_DB_NAME_ATTR(Attributes::Label, 6, "Label", 0, NULL, BLOB);
CSSM_DB_NAME_ATTR(Attributes::ApplicationTag, 7, "ApplicationTag", 0, NULL, BLOB);
CSSM_DB_NAME_ATTR(Attributes::KeyCreator, 8, "KeyCreator", 0, NULL, BLOB);
CSSM_DB_NAME_ATTR(Attributes::KeyType, 9, "KeyType", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::KeySizeInBits, 10, "KeySizeInBits", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::EffectiveKeySize, 11, "EffectiveKeySize", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::StartDate, 12, "StartDate", 0, NULL, TIME_DATE);
CSSM_DB_NAME_ATTR(Attributes::EndDate, 13, "EndDate", 0, NULL, TIME_DATE);
CSSM_DB_NAME_ATTR(Attributes::Sensitive, 14, "Sensitive", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::AlwaysSensitive, 15, "AlwaysSensitive", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::Extractable, 16, "Extractable", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::NeverExtractable, 17, "NeverExtractable", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::Encrypt, 18, "Encrypt", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::Decrypt, 19, "Decrypt", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::Derive, 20, "Derive", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::Sign, 21, "Sign", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::Verify, 22, "Verify", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::SignRecover, 23, "SignRecover", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::VerifyRecover, 24, "VerifyRecover", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::Wrap, 25, "Wrap", 0, NULL, UINT32);
CSSM_DB_NAME_ATTR(Attributes::UnWrap, 26, "UnWrap", 0, NULL, UINT32);
