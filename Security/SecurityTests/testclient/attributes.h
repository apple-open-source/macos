#include <security_cdsa_client/dlclient.h>

class Attributes
{
public:
	// Meta Attributes.
	CSSM_DB_ATTR_DECL(RelationID);
	CSSM_DB_ATTR_DECL(RelationName);
	CSSM_DB_ATTR_DECL(AttributeID);
	CSSM_DB_ATTR_DECL(AttributeNameFormat);
	CSSM_DB_ATTR_DECL(AttributeName);
	CSSM_DB_ATTR_DECL(AttributeNameID);
	CSSM_DB_ATTR_DECL(AttributeFormat);

	// Keychain Attributes
	CSSM_DB_ATTR_DECL(Protected);
	CSSM_DB_ATTR_DECL(Class);
    CSSM_DB_ATTR_DECL(CreationDate);
    CSSM_DB_ATTR_DECL(ModDate);
    CSSM_DB_ATTR_DECL(Description);
    CSSM_DB_ATTR_DECL(Comment);
    CSSM_DB_ATTR_DECL(Creator);
    CSSM_DB_ATTR_DECL(Type);
    CSSM_DB_ATTR_DECL(ScrCode);
    CSSM_DB_ATTR_DECL(Label);
    CSSM_DB_ATTR_DECL(Invisible);
    CSSM_DB_ATTR_DECL(Negative);
    CSSM_DB_ATTR_DECL(Custom);
    // 	for Generic Password items:
    CSSM_DB_ATTR_DECL(Account);
    CSSM_DB_ATTR_DECL(Service);
    CSSM_DB_ATTR_DECL(Generic);
    // 	for Internet Password items:
    CSSM_DB_ATTR_DECL(SecDomain);
    CSSM_DB_ATTR_DECL(Server);
    CSSM_DB_ATTR_DECL(AuthType);
    CSSM_DB_ATTR_DECL(Port);
    CSSM_DB_ATTR_DECL(Path);
    // 	for AppleShare Password items:
    CSSM_DB_ATTR_DECL(Volume);
    CSSM_DB_ATTR_DECL(Addr);
    CSSM_DB_ATTR_DECL(Signature);
    // 	for AppleShare and Interent Password items:
    CSSM_DB_ATTR_DECL(ProtocolType);

	// For keys
	CSSM_DB_ATTR_DECL(KeyClass);
	CSSM_DB_ATTR_DECL(PrintName);
	CSSM_DB_ATTR_DECL(Alias);
	CSSM_DB_ATTR_DECL(Permanent);
	CSSM_DB_ATTR_DECL(Private);
	CSSM_DB_ATTR_DECL(Modifiable);
	//CSSM_DB_ATTR_DECL(Label);
	CSSM_DB_ATTR_DECL(ApplicationTag);
	CSSM_DB_ATTR_DECL(KeyCreator);
	CSSM_DB_ATTR_DECL(KeyType);
	CSSM_DB_ATTR_DECL(KeySizeInBits);
	CSSM_DB_ATTR_DECL(EffectiveKeySize);
	CSSM_DB_ATTR_DECL(StartDate);
	CSSM_DB_ATTR_DECL(EndDate);
	CSSM_DB_ATTR_DECL(Sensitive);
	CSSM_DB_ATTR_DECL(AlwaysSensitive);
	CSSM_DB_ATTR_DECL(Extractable);
	CSSM_DB_ATTR_DECL(NeverExtractable);
	CSSM_DB_ATTR_DECL(Encrypt);
	CSSM_DB_ATTR_DECL(Decrypt);
	CSSM_DB_ATTR_DECL(Derive);
	CSSM_DB_ATTR_DECL(Sign);
	CSSM_DB_ATTR_DECL(Verify);
	CSSM_DB_ATTR_DECL(SignRecover);
	CSSM_DB_ATTR_DECL(VerifyRecover);
	CSSM_DB_ATTR_DECL(Wrap);
	CSSM_DB_ATTR_DECL(UnWrap);
private:
	static const CSSM_OID noOID;
};
