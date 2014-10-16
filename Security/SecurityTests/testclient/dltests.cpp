#include "dltests.h"
#include "csptests.h"
#include "attributes.h"
#include <security_cdsa_client/multidldb.h>
#include <vector>
#include <security_cdsa_client/securestorage.h> // For CSPDL.
#include <security_cdsa_client/genkey.h>
#include <security_utilities/trackingallocator.h>

using namespace CssmClient;

// Configuration.
#define HEX_DIGITS_PER_LINE  20
#define INDENT_SIZE 2


const CSSM_GUID* gSelectedFileGuid = &gGuidAppleFileDL;



static void testDLCreate(const Guid &dlGuid);
static void testDLDelete(const Guid &dlGuid, bool throwOnError);
static void testGen(const Guid &cspDlGuid);
static void testDLCrypt(const Guid &cspDlGuid);
static void testMultiDLDb(const Guid &dlGuid);
static void dumpRelation(uint32 indent, Db &db, uint32 relationID, const char *relationName, bool printSchema);
static void dumpRecord(uint32 indent, const DbAttributes &record, const CssmData &data, const DbUniqueRecord &uniqueId);

#define CSSM_DB_RELATION(RELATIONID) RecordAttrInfo ## RELATIONID

#define CSSM_DB_DEFINE_RELATION_BEGIN(RELATIONID) \
static const CSSM_DB_ATTRIBUTE_INFO AttrInfo ## RELATIONID[] =

#define CSSM_DB_DEFINE_RELATION_END(RELATIONID) \
; \
static const CSSM_DB_RECORD_ATTRIBUTE_INFO CSSM_DB_RELATION(RELATIONID) = \
{ \
    RELATIONID, \
    sizeof(AttrInfo ## RELATIONID) / sizeof(CSSM_DB_ATTRIBUTE_INFO), \
    const_cast<CSSM_DB_ATTRIBUTE_INFO_PTR>(AttrInfo ## RELATIONID) \
}

// GENERIC PASSWORDS
CSSM_DB_DEFINE_RELATION_BEGIN(CSSM_DL_DB_RECORD_GENERIC_PASSWORD)
{
	CSSM_DB_ATTR(Attributes::Class), 
	CSSM_DB_ATTR(Attributes::CreationDate), 
	CSSM_DB_ATTR(Attributes::ModDate), 
	CSSM_DB_ATTR(Attributes::Description),
	CSSM_DB_ATTR(Attributes::Comment),
	CSSM_DB_ATTR(Attributes::Creator),
	CSSM_DB_ATTR(Attributes::Type),
	CSSM_DB_ATTR(Attributes::ScrCode),
	CSSM_DB_ATTR(Attributes::Label),
	CSSM_DB_ATTR(Attributes::Invisible),
	CSSM_DB_ATTR(Attributes::Negative),
	CSSM_DB_ATTR(Attributes::Custom),
	//CSSM_DB_ATTR(Attributes::Protected),
	CSSM_DB_ATTR(Attributes::Account),
	CSSM_DB_ATTR(Attributes::Service),
	CSSM_DB_ATTR(Attributes::Generic)
}
CSSM_DB_DEFINE_RELATION_END(CSSM_DL_DB_RECORD_GENERIC_PASSWORD);


// APPLESHARE PASSWORDS
CSSM_DB_DEFINE_RELATION_BEGIN(CSSM_DL_DB_RECORD_APPLESHARE_PASSWORD)
{
	CSSM_DB_ATTR(Attributes::Class), 
	CSSM_DB_ATTR(Attributes::CreationDate), 
	CSSM_DB_ATTR(Attributes::ModDate), 
	CSSM_DB_ATTR(Attributes::Description),
	CSSM_DB_ATTR(Attributes::Comment),
	CSSM_DB_ATTR(Attributes::Creator),
	CSSM_DB_ATTR(Attributes::Type),
	CSSM_DB_ATTR(Attributes::ScrCode),
	CSSM_DB_ATTR(Attributes::Label),
	CSSM_DB_ATTR(Attributes::Invisible),
	CSSM_DB_ATTR(Attributes::Negative),
	CSSM_DB_ATTR(Attributes::Custom),
	//CSSM_DB_ATTR(Attributes::Protected),
	CSSM_DB_ATTR(Attributes::Volume),
	CSSM_DB_ATTR(Attributes::Addr),
	CSSM_DB_ATTR(Attributes::Signature),
	CSSM_DB_ATTR(Attributes::ProtocolType)
}
CSSM_DB_DEFINE_RELATION_END(CSSM_DL_DB_RECORD_APPLESHARE_PASSWORD);

// INTERNET PASSWORDS
CSSM_DB_DEFINE_RELATION_BEGIN(CSSM_DL_DB_RECORD_INTERNET_PASSWORD)
{ 
	CSSM_DB_ATTR(Attributes::Class), 
	CSSM_DB_ATTR(Attributes::CreationDate), 
	CSSM_DB_ATTR(Attributes::ModDate), 
	CSSM_DB_ATTR(Attributes::Description),
	CSSM_DB_ATTR(Attributes::Comment),
	CSSM_DB_ATTR(Attributes::Creator),
	CSSM_DB_ATTR(Attributes::Type),
	CSSM_DB_ATTR(Attributes::ScrCode),
	CSSM_DB_ATTR(Attributes::Label),
	CSSM_DB_ATTR(Attributes::Invisible),
	CSSM_DB_ATTR(Attributes::Negative),
	CSSM_DB_ATTR(Attributes::Custom),
	//CSSM_DB_ATTR(Attributes::Protected),
	CSSM_DB_ATTR(Attributes::Account),
	CSSM_DB_ATTR(Attributes::SecDomain),
	CSSM_DB_ATTR(Attributes::Server),
	CSSM_DB_ATTR(Attributes::AuthType),
	CSSM_DB_ATTR(Attributes::Port),
	CSSM_DB_ATTR(Attributes::Path),
	CSSM_DB_ATTR(Attributes::ProtocolType)
}
CSSM_DB_DEFINE_RELATION_END(CSSM_DL_DB_RECORD_INTERNET_PASSWORD);

// INTERNET PASSWORDS
CSSM_DB_DEFINE_RELATION_BEGIN(CSSM_DL_DB_RECORD_SYMMETRIC_KEY)
{ 
	CSSM_DB_ATTR(Attributes::KeyClass),
	CSSM_DB_ATTR(Attributes::PrintName),
	CSSM_DB_ATTR(Attributes::Alias),
	CSSM_DB_ATTR(Attributes::Permanent),
	CSSM_DB_ATTR(Attributes::Private),
	CSSM_DB_ATTR(Attributes::Modifiable),
	CSSM_DB_ATTR(Attributes::Label),
	CSSM_DB_ATTR(Attributes::ApplicationTag),
	CSSM_DB_ATTR(Attributes::KeyCreator),
	CSSM_DB_ATTR(Attributes::KeyType),
	CSSM_DB_ATTR(Attributes::KeySizeInBits),
	CSSM_DB_ATTR(Attributes::EffectiveKeySize),
	CSSM_DB_ATTR(Attributes::StartDate),
	CSSM_DB_ATTR(Attributes::EndDate),
	CSSM_DB_ATTR(Attributes::Sensitive),
	CSSM_DB_ATTR(Attributes::AlwaysSensitive),
	CSSM_DB_ATTR(Attributes::Extractable),
	CSSM_DB_ATTR(Attributes::NeverExtractable),
	CSSM_DB_ATTR(Attributes::Encrypt),
	CSSM_DB_ATTR(Attributes::Decrypt),
	CSSM_DB_ATTR(Attributes::Derive),
	CSSM_DB_ATTR(Attributes::Sign),
	CSSM_DB_ATTR(Attributes::Verify),
	CSSM_DB_ATTR(Attributes::SignRecover),
	CSSM_DB_ATTR(Attributes::VerifyRecover),
	CSSM_DB_ATTR(Attributes::Wrap),
	CSSM_DB_ATTR(Attributes::UnWrap)
}
CSSM_DB_DEFINE_RELATION_END(CSSM_DL_DB_RECORD_SYMMETRIC_KEY);


static const CSSM_DB_RECORD_ATTRIBUTE_INFO KCAttrs[] =
{
	CSSM_DB_RELATION(CSSM_DL_DB_RECORD_GENERIC_PASSWORD),
	CSSM_DB_RELATION(CSSM_DL_DB_RECORD_APPLESHARE_PASSWORD),
	CSSM_DB_RELATION(CSSM_DL_DB_RECORD_INTERNET_PASSWORD)
	//CSSM_DB_RELATION(CSSM_DL_DB_RECORD_SYMMETRIC_KEY)
};

static const CSSM_DB_RECORD_INDEX_INFO recordIndex = 
{
    CSSM_DB_RECORDTYPE_APP_DEFINED_START, // CSSM_DB_RECORDTYPE
    0,	//%%% for now
    NULL	//%%% for now
};
static const    CSSM_DB_RECORD_INDEX_INFO recordIndexes[] = {recordIndex, recordIndex, recordIndex};

// parse info (to improve later)
static const        CSSM_DB_PARSING_MODULE_INFO parseInfo =
{
    CSSM_DB_RECORDTYPE_APP_DEFINED_START,
    {
        {0,0,0,{0}},
        {0,0},
        0,
        0
    }
};
static const    CSSM_DB_PARSING_MODULE_INFO parseInfos[] = {parseInfo, parseInfo, parseInfo};

static const CSSM_DBINFO	KCDBInfo =
{
    sizeof(KCAttrs) / sizeof(CSSM_DB_RECORD_ATTRIBUTE_INFO),
    const_cast<CSSM_DB_PARSING_MODULE_INFO_PTR>(parseInfos),
    const_cast<CSSM_DB_RECORD_ATTRIBUTE_INFO_PTR>(KCAttrs),
    const_cast<CSSM_DB_RECORD_INDEX_INFO_PTR>(recordIndexes),
    CSSM_TRUE,
    NULL,
    NULL
};



void dltests(bool autoCommit)
{
	testDLDelete(gGuidAppleFileDL, false);
	testDLCreate(gGuidAppleFileDL);
	testMultiDLDb(gGuidAppleFileDL);

	testDLDelete(gGuidAppleCSPDL, false);
	testDLCreate(gGuidAppleCSPDL);
	testGen(gGuidAppleCSPDL);
	testDLCrypt(gGuidAppleCSPDL);
	testMultiDLDb(gGuidAppleCSPDL);
	//testDLDelete(gGuidAppleCSPDL, true);
}

static void testDLCreate(const Guid &dlGuid)
{
	DL appledl(dlGuid);
	Db testDb(appledl, DBNAME1);
	testDb->dbInfo(&KCDBInfo);
	testDb->create();
}

static void testDLDelete(const Guid &dlGuid, bool throwOnError)
{
	DL appledl(dlGuid);
	Db testDb(appledl, DBNAME1);
	try
	{
		testDb->deleteDb();
	}
	catch(CssmError e)
	{
		if (throwOnError || e.osStatus() != CSSMERR_DL_DATASTORE_DOESNOT_EXIST)
			throw;
	}
}

static void testGen(const Guid &cspDlGuid)
{
	printf("\n* performing CSP/DL keygen test...\n");
	CSPDL cspdl(cspDlGuid);
	Db db(cspdl, DBNAME1);

    printf("Generating permanent key\n");
	GenerateKey genKey(cspdl, CSSM_ALGID_DES, 64);
	genKey.database(db);
	CssmPolyData label("First Key!");
	Key key = genKey(KeySpec(CSSM_KEYUSE_ANY,
							 CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_SENSITIVE,
							 label));
    printf("done\n");
}

static void testDLCrypt(const Guid &cspDlGuid)
{
    printf("\n* performing encrypt/decrypt test...\n");
	CSPDL cspdl(cspDlGuid);
	Db db(cspdl, DBNAME1);

    printf("Finding key\n");
	DbCursor cursor(db);
	cursor->recordType(CSSM_DL_DB_RECORD_SYMMETRIC_KEY);
	DbUniqueRecord keyId;
	CssmDataContainer keyData;
	if (!cursor->next(NULL, &keyData, keyId))
		CssmError::throwMe(CSSMERR_DL_ENDOFDATA);

	Key key(cspdl, *reinterpret_cast<CSSM_KEY *>(keyData.Data));

    printf("done\n");

	// Gnerate IV
    printf("Generating iv\n");
	//CssmData iv = Random(csp, CSSM_ALGID_SHARandom)(8);
	CssmPolyData iv("12345678");

	CssmPolyData in("Om mani padme hum");
	printf("input=");
	dump(in);

	// Encrypt
    printf("Encrypting\n");

	Encrypt encrypt(cspdl, CSSM_ALGID_DES);
	encrypt.mode(CSSM_ALGMODE_CBCPadIV8);
	encrypt.padding(CSSM_PADDING_PKCS1);
	encrypt.initVector(iv);
	encrypt.key(key);
	CssmData cipher;
	CssmData remcipher;
	encrypt.encrypt(&in, 1, &cipher, 1);
	encrypt.final(remcipher);
	printf("ciphertext=");
	dump(cipher);
	printf("remainder=");
	dump(remcipher);

	// Decrypt
    printf("Decrypting\n");

	Decrypt decrypt(cspdl, CSSM_ALGID_DES);
	decrypt.key(key);
	decrypt.mode(CSSM_ALGMODE_CBCPadIV8);
	decrypt.padding(CSSM_PADDING_PKCS1);
	decrypt.initVector(iv);
	CssmData plain;
	CssmData remplain;
	CssmData inp[] = { cipher, remcipher };
	decrypt.decrypt(inp, 2, &plain, 1);
	decrypt.final(remplain);
	printf("plaintext=");
	dump(plain);
	printf("remainder=");
	dump(remplain);

    printf("end encrypt/decrypt test\n");
}

static void print(sint32 value)
{
	printf("%ld", value);
}

static void print(double value)
{
	printf("%g", value);
}

static void print(uint32 value)
{
	uint8 *bytes = reinterpret_cast<uint8 *>(&value);
	bool ascii = true;
	for (uint32 ix = 0; ix < sizeof(uint32); ++ix)
		if (bytes[ix] < 0x20 || bytes[ix] > 0x7f)
		{
			ascii = false;
			break;
		}

	if (ascii)
	{
		putchar('\'');
		for (uint32 ix = 0; ix < sizeof(uint32); ++ix)
			putchar(bytes[ix]);

		printf("' (0x%08lx)", value);
	}
	else
		printf("0x%08lx", value);
}

static void printAsString(uint32 indent, const CSSM_DATA &value)
{
	printf("%.*s", static_cast<int>(value.Length), value.Data);
}

static void print(uint32 indent, const char *value)
{
	printf("%s", value);
}

static void printIndent(uint32 indent)
{
	//if (indent == 0)
	//	return;

	putchar('\n');
	for (uint32 ix = 0; ix < indent; ++ix)
		putchar(' ');
}

static void printRange(uint32 length, const uint8 *data)
{
	for (uint32 ix = 0; ix < HEX_DIGITS_PER_LINE; ++ix)
	{
		if (ix && ix % 4 == 0)
			putchar(' ');

		if (ix < length)
			printf("%02x", static_cast<unsigned int>(data[ix]));
		else
			printf("  ");
	}

	printf("  ");
	for (uint32 ix = 0; ix < length; ++ix)
	{
		if (data[ix] < 0x20 || data[ix] > 0x7f)
			putchar('.');
		else
			putchar(data[ix]);
	}
}

static void print(uint32 indent, const CSSM_DATA &value)
{
	if (value.Length == 0)
		return;

	if (value.Length > HEX_DIGITS_PER_LINE)
	{
		uint32 ix;
		for (ix = 0; ix < value.Length - HEX_DIGITS_PER_LINE; ix += HEX_DIGITS_PER_LINE)
		{
			printIndent(indent);
			printRange(HEX_DIGITS_PER_LINE, &value.Data[ix]);
		}
		printIndent(indent);
		printRange(value.Length - ix, &value.Data[ix]);
		printIndent(indent - INDENT_SIZE);
	}
	else
		printRange(value.Length, value.Data);
}

static void printOID(uint32 indent, const CSSM_OID &value)
{
	print(indent, value);
}

static const char *format(CSSM_DB_ATTRIBUTE_FORMAT format)
{
	switch(format)
	{
	case CSSM_DB_ATTRIBUTE_FORMAT_STRING: return "string";
	case CSSM_DB_ATTRIBUTE_FORMAT_SINT32: return "sint32";
	case CSSM_DB_ATTRIBUTE_FORMAT_UINT32: return "uint32";
	case CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM: return "big_num";
	case CSSM_DB_ATTRIBUTE_FORMAT_REAL: return "real";
	case CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE: return "time_date";
	case CSSM_DB_ATTRIBUTE_FORMAT_BLOB: return "blob";
	case CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32: return "multi_uint32";
	case CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX: return "complex";
	default: abort();
	}
}

static void print(uint32 indent, const CssmDbAttributeData &attr)
{
	bool multiValues = false;
	if (attr.size() == 0)
	{
		printf("<array/>");
		return;
	}

	if (attr.size() != 1)
	{
		printIndent(indent);
		printf("<array>");
		indent += INDENT_SIZE;
		multiValues = true;
	}

	for (uint32 ix = 0; ix < attr.size(); ++ix)
	{
		if (multiValues)
			printIndent(indent);

		printf("<%s>", format(attr.format()));
		switch (attr.format())
		{
		case CSSM_DB_ATTRIBUTE_FORMAT_STRING:
			printAsString(indent + INDENT_SIZE, attr.at(ix));
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_UINT32:
			print(attr.at<uint32>(ix));
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_SINT32:
			print(attr.at<sint32>(ix));
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_REAL:
			print(attr.at<double>(ix));
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE:
			printf("%*s", 15, attr.at<const char *>(ix));
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32:
		case CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM:
		case CSSM_DB_ATTRIBUTE_FORMAT_BLOB:
		case CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX:
		default:
			print(indent + INDENT_SIZE, attr.at<const CssmData &>(ix));
			break;
		}
		printf("</%s>", format(attr.format()));
	}

	if (multiValues)
	{
		indent -= INDENT_SIZE;
		printIndent(indent);
		printf("</array>");
	}
}

static void print(uint32 indent, const CssmDbAttributeInfo &info)
{
		switch (info.nameFormat())
		{
		case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
		{
			printf("<string>");
			print(indent + INDENT_SIZE, info.Label.AttributeName);
			printf("</string>");
			break;
		}
		case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
		{
			printf("<integer>");
			print(info.Label.AttributeID);
			printf("</integer>");
			break;
		}
		case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
		{
			printf("<oid>");
			printOID(indent + INDENT_SIZE, info.Label.AttributeOID);
			printf("</oid>");
			break;
		}
		default:
			throw Error(CSSMERR_DL_DATABASE_CORRUPT);
		}
}

void dumpDb(char *dbName, bool printSchema)
{
	DL appledl(*gSelectedFileGuid);
	Db db(appledl, dbName);
	DbCursor relations(db);
	relations->recordType(CSSM_DL_DB_SCHEMA_INFO);
	DbAttributes schemaRecord(db, 2);
	schemaRecord.add(Attributes::RelationID);
	schemaRecord.add(Attributes::RelationName);
	CssmDataContainer data;
	DbUniqueRecord uniqueId(db);

	uint32 indent = 0;
	printf("<database>");
	indent += INDENT_SIZE;
	printIndent(indent);
	printf("<name>%s</name>", dbName);
	while (relations->next(&schemaRecord, &data, uniqueId))
	{
		uint32 relationID = schemaRecord.at(0);
		if (!printSchema && CSSM_DB_RECORDTYPE_SCHEMA_START <= relationID
			&& relationID < CSSM_DB_RECORDTYPE_SCHEMA_END)
			continue;

		printIndent(indent);
		printf("<relation>");
		string relationName = schemaRecord.at(1);
		dumpRelation(indent + INDENT_SIZE, db, relationID, relationName.c_str(), printSchema);
		printIndent(indent);
		printf("</relation>");
	}

	indent -= INDENT_SIZE;
	printIndent(indent);
	printf("</database>\n");
}

static void dumpRelation(uint32 indent, Db &db, uint32 relationID, const char *relationName, bool printSchema)
{
	TrackingAllocator anAllocator(Allocator::standard());

	printIndent(indent);
	printf("<name>");
	print(indent + INDENT_SIZE, relationName);
	printf("</name>");
	printIndent(indent);
	printf("<id>");
	print(relationID);
	printf("</id>");

	// Create a cursor on the SCHEMA_ATTRIBUTES table for records with RelationID == relationID
	DbCursor attributes(db);
	attributes->recordType(CSSM_DL_DB_SCHEMA_ATTRIBUTES);
	attributes->add(CSSM_DB_EQUAL, Attributes::RelationID, relationID);

	// Set up a record for retriving the SCHEMA_ATTRIBUTES
	DbAttributes schemaRecord(db, 5);
	schemaRecord.add(Attributes::AttributeNameFormat);
	schemaRecord.add(Attributes::AttributeFormat);
	schemaRecord.add(Attributes::AttributeName);
	schemaRecord.add(Attributes::AttributeID);
	schemaRecord.add(Attributes::AttributeNameID);

	DbAttributes record(db);
	CssmDataContainer data;
	DbUniqueRecord uniqueId(db);

	if (printSchema)
	{
		printIndent(indent);
		printf("<schema>");
		indent += INDENT_SIZE;
	}

	while (attributes->next(&schemaRecord, &data, uniqueId))
	{
		CssmDbAttributeInfo &anInfo = record.add().info();
		anInfo.AttributeNameFormat = schemaRecord.at(0);
		anInfo.AttributeFormat = schemaRecord.at(1);
		switch (anInfo.AttributeNameFormat)
		{
		case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
		{
			CssmDbAttributeData &anAttributeName = schemaRecord.at(2);
			
			string name = static_cast<string>(anAttributeName);
			anInfo.Label.AttributeName = reinterpret_cast<char *>(anAllocator.malloc(name.size() + 1));
			strcpy(anInfo.Label.AttributeName, name.c_str());

			// XXX Need to copy the memory.  For now avoid it being freed.
			anAttributeName.Value[0].Data = NULL;
			anAttributeName.Value[0].Length = 0;
			break;
		}
		case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
		{
			CssmDbAttributeData &anAttributeID = schemaRecord.at(3);
			anInfo.Label.AttributeID = anAttributeID;
			break;
		}
		case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
		{
			CssmDbAttributeData &anAttributeOID = schemaRecord.at(4);
			anInfo.Label.AttributeOID = anAttributeOID;

			// XXX Need to copy the memory.  For now avoid it being freed.
			anAttributeOID.Value[0].Data = NULL;
			anAttributeOID.Value[0].Length = 0;
			break;
		}
		default:
			throw Error(CSSMERR_DL_DATABASE_CORRUPT);
		}

		if (printSchema)
		{
			printIndent(indent);
			print(indent, anInfo);
			printf("<format>%s</format>", format(anInfo.format()));
		}
	}

	if (printSchema)
	{
		indent -= INDENT_SIZE;
		printIndent(indent);
		printf("</schema>");
	}

	DbCursor records(db);
	records->recordType(relationID);
	printIndent(indent);
	printf("<records>");
	indent += INDENT_SIZE;
	while (records->next(&record, &data, uniqueId))
		dumpRecord(indent, record, data, uniqueId);

	indent -= INDENT_SIZE;
	printIndent(indent);
	printf("</records>");
}

static void
dumpRecord(uint32 indent, const DbAttributes &record, const CssmData &data, const DbUniqueRecord &uniqueId)
{
	const CSSM_DB_UNIQUE_RECORD *recId = static_cast<const DbUniqueRecord &>(uniqueId);
	uint32 recCount = recId->RecordIdentifier.Length;
	const uint32 *recArray = reinterpret_cast<const uint32 *>(recId->RecordIdentifier.Data);
	printIndent(indent);
	printf("<recid>");
	for (uint32 ix = 0; ix < recCount / 4; ++ix)
	{
		if (ix != 0)
			putchar(' ');
		printf("0x%08lx", recArray[ix]);
	}
	printf("</recid>");

	// Print the attributes
	printIndent(indent);
	if (record.size() == 0)
	{
		printf("<attributes/>");
	}
	else
	{
		printf("<attributes>");
		indent += INDENT_SIZE;
		for (uint32 ix = 0; ix < record.size(); ix++)
		{
			const CssmDbAttributeData &anAttr = record.at(ix);
			if (anAttr.size()) // Skip zero valued attributes.
			{
				printIndent(indent);
				print(indent + INDENT_SIZE, anAttr.info());
				print(indent + INDENT_SIZE, anAttr);
			}
		}

		indent -= INDENT_SIZE;
		printIndent(indent);
		printf("</attributes>");
	}

	// Print the data
	printIndent(indent);
	if (data.length())
	{
		printf("<data>");
		print(indent + INDENT_SIZE, data);
		printf("</data>");
	}
	else
		printf("<data/>");
}

static void testMultiDLDb(const Guid &dlGuid)
{
	// Setup a list of DLDbIdentifier object to hand off the MultiDLDb.
	vector<DLDbIdentifier> list;
	list.push_back(DLDbIdentifier(CssmSubserviceUid(dlGuid), "multidb1.db", NULL));
	list.push_back(DLDbIdentifier(CssmSubserviceUid(dlGuid), "multidb2.db", NULL));

	// Create MultiDLDb instance.
	MultiDLDb multiDLDb(list, false);

	// Get a handle for the first and second Db.
	Db db1(multiDLDb->database(list[0]));
	Db db2(multiDLDb->database(list[1]));

	// Until this point no CSSM API's have been called!

	// Delete both databases if they exist. 
	try
	{ db1->deleteDb(); }
	catch(CssmError e)
	{ if (e.osStatus() != CSSMERR_DL_DATASTORE_DOESNOT_EXIST) throw; }

	try
	{ db2->deleteDb(); }
	catch(CssmError e)
	{ if (e.osStatus() != CSSMERR_DL_DATASTORE_DOESNOT_EXIST) throw; }

	// XXX Note to self if you set the schema but do not call create()
	// explicitly maybe the db should only be created if it did not yet exist...

	// Set the schema of both databases so they get created on activate.
	db1->dbInfo(&KCDBInfo);
	db2->dbInfo(&KCDBInfo);

	// Insert a record into each database.
	DbAttributes attrs(db1);
	attrs.add(Attributes::Comment, "This is the first comment").add("This is the second comment", attrs);
	attrs.add(Attributes::Label, "Item1");
	CssmPolyData testdata1("testdata1");
	db1->insert(CSSM_DL_DB_RECORD_GENERIC_PASSWORD, &attrs, &testdata1);

	attrs.clear();
	attrs.add(Attributes::Comment, "This is the second comment");
	attrs.add(Attributes::Label, "Item (in database2).");
	CssmPolyData testdata2("testdata2");
	db2->insert(CSSM_DL_DB_RECORD_GENERIC_PASSWORD, &attrs, &testdata2);

	// Create a cursor on the multiDLDb.
	DbCursor cursor(multiDLDb);
	// Set the type of records we wish to query.
	cursor->recordType(CSSM_DL_DB_RECORD_GENERIC_PASSWORD);
	cursor->add(CSSM_DB_EQUAL, Attributes::Comment, "This is the second comment");

	DbUniqueRecord uniqueId; // Empty uniqueId calling cursor.next will initialize.
	CssmDataContainer data; // XXX Think about data's allocator.

	// Iterate over the records in all the db's in the multiDLDb.
	while (cursor->next(&attrs, &data, uniqueId))
	{
		// Print the record data.
		dumpRecord(0, attrs, data, uniqueId);
	}
}

#if 0
	CssmDb::Impl *CssmDL::Impl::newDb(args) { new CssmDbImpl(args); }

	SecureStorage ss(Guid);
	CssmDb db(ss, DBNAME);
	CssmUniqueId unique;
	db.insert(attr, data, unique);

	Cursor cursor(db);
	CssmKey key;
	cursor.next(key);

	Cssm cssm;
	Module module(cssm);
	CSPDL cspdl(module);


	SecureStorage ss(Guid);
	CssmDb db = ss->db(DBNAME);
	CssmUniqueId unique;
	db->insert(attr, data, unique);

	Cursor cursor(db);
	CssmKey key;
	cursor->next(key);


#endif
