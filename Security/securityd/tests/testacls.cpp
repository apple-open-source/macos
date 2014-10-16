/*
 * Copyright (c) 2000-2001,2003-2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


//
// testacls - ACL-related test cases.
// 
#include "testclient.h"
#include "testutils.h"
#include <Security/osxsigner.h>

using namespace CodeSigning;


//
// ACL get/set tests
//
void acls()
{
    printf("* Basic ACL tests\n");
	CssmAllocator &alloc = CssmAllocator::standard();
	ClientSession ss(alloc, alloc);
	
	// create key with initial ACL
	StringData initialAclPassphrase("very secret");
	AclEntryPrototype initialAcl;
	initialAcl.TypedSubject = TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_PASSWORD,
		new(alloc) ListElement(initialAclPassphrase));
	AclEntryInput initialAclInput(initialAcl);
	AclTester tester(ss, &initialAclInput);
	
	// get the owner and verify
	AclOwnerPrototype owner;
	ss.getKeyOwner(tester.keyRef, owner);
	assert(owner.subject().type() == CSSM_ACL_SUBJECT_TYPE_PASSWORD);
	assert(owner.subject().length() == 1);
	
	// get the acl entry and verify
	{
		uint32 count;
		AclEntryInfo *acls;
		ss.getKeyAcl(tester.keyRef, NULL/*tag*/, count, acls);
		assert(count == 1);
		const AclEntryInfo &acl1 = acls[0];
		const TypedList &subject1 = acl1.proto().subject();
		assert(subject1.type() == CSSM_ACL_SUBJECT_TYPE_PASSWORD);
		assert(subject1.length() == 1);
	}
	
	// try to use the key and see...
	tester.testWrap(&nullCred, "ACCEPTING NULL CREDENTIAL");
	AutoCredentials cred(alloc);
	cred += TypedList(alloc, CSSM_SAMPLE_TYPE_PASSWORD,
		new(alloc) ListElement(StringData("wrongo")));
	tester.testWrap(&cred, "ACCEPTING WRONG PASSWORD CREDENTIAL");
	cred += TypedList(alloc, CSSM_SAMPLE_TYPE_PASSWORD,
		new(alloc) ListElement(StringData("very secret")));
	tester.testWrap(&cred);
    
    // now *replace* the ACL entry with a new one...
    {
        detail("Changing ACL");
        uint32 count;
        AclEntryInfo *infos;
        ss.getKeyAcl(tester.keyRef, NULL, count, infos);
        assert(count == 1);	// one entry
        
        AclEntryPrototype newAcl;
        TypedList subject = TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_THRESHOLD,
            new(alloc) ListElement(2), new(alloc) ListElement(3));
        subject += new(alloc) ListElement(TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_PASSWORD,
                new(alloc) ListElement(alloc, "check me!")));
        subject += new(alloc) ListElement(TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_PASSWORD,
                new(alloc) ListElement(alloc, "once again!")));
        subject += new(alloc) ListElement(TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_PASSWORD,
                new(alloc) ListElement(alloc, "hug me!")));
        newAcl.TypedSubject = subject;
        AclEntryInput input(newAcl);
        AclEdit edit(infos[0].handle(), input);
        
        try {
            AutoCredentials nullCred(alloc);
            ss.changeKeyAcl(tester.keyRef, nullCred, edit);
            error("ALLOWED ACL EDIT WITHOUT CREDENTIALS");
        } catch (CssmCommonError &err) {
            detail(err, "Acl Edit rejected properly");
        }
        ss.changeKeyAcl(tester.keyRef, cred, edit);
        detail("ACL changed OK");
    }
    
    // ... and see how the new one reacts
    tester.testWrap(&nullCred, "ACCEPTING NULL CREDENTIALS NOW");
    tester.testWrap(&cred, "ACCEPTING OLD CREDENTIALS FOR NEW ACL");
    {
        AutoCredentials cred(alloc);
        cred += TypedList(alloc, CSSM_SAMPLE_TYPE_PASSWORD,
            new(alloc) ListElement(alloc, "check me!"));
        tester.testWrap(&cred, "ACCEPTING LEAF SAMPLE WITHOUT THRESHOLD FRAMEWORK");
    }
    
    // Threshold subjects
    {
        detail("Testing threshold ACLs");
        AutoCredentials cred(alloc);
        TypedList &threshold = cred += TypedList(alloc, CSSM_SAMPLE_TYPE_THRESHOLD,
            new(alloc) ListElement(TypedList(alloc, CSSM_SAMPLE_TYPE_PASSWORD,
                new(alloc) ListElement(alloc, "wrongo!")))
        );
        tester.testWrap(&cred, "ACCEPTING ALL WRONG SAMPLES IN THRESHOLD");
        threshold += new(alloc) ListElement(TypedList(alloc, CSSM_SAMPLE_TYPE_PASSWORD,
                new(alloc) ListElement(alloc, "hug me!")));
        tester.testWrap(&cred, "ACCEPTING TOO FEW THRESHOLD SAMPLES");
        threshold += new(alloc) ListElement(TypedList(alloc, CSSM_SAMPLE_TYPE_PASSWORD,
                new(alloc) ListElement(alloc, "check me!")));
        tester.testWrap(&cred);
        // stuff the ballot box
        threshold += new(alloc) ListElement(TypedList(alloc, CSSM_SAMPLE_TYPE_PASSWORD,
                new(alloc) ListElement(alloc, "and this!")));
        threshold += new(alloc) ListElement(TypedList(alloc, CSSM_SAMPLE_TYPE_PASSWORD,
                new(alloc) ListElement(alloc, "and that!")));
        threshold += new(alloc) ListElement(TypedList(alloc, CSSM_SAMPLE_TYPE_PASSWORD,
                new(alloc) ListElement(alloc, "and more!")));
#ifdef STRICT_THRESHOLD_SUBJECTS
        tester.testWrap(&cred, "ACCEPTING OVER-STUFFED THRESHOLD");
#else
		tester.testWrap(&cred);
#endif //STRICT_THRESHOLD_SUBJECTS
    }
	
	// comment ACLs and tags
	{
        detail("Adding Comment entry");
		
        AclEntryPrototype newAcl;
        TypedList subject = TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_COMMENT,
			new(alloc) ListElement(TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_THRESHOLD,
				new(alloc) ListElement(alloc, "Robby Ray!"))),
			new(alloc) ListElement(666));
        newAcl.TypedSubject = subject;
		strcpy(newAcl.EntryTag, "vamos");
        AclEntryInput input(newAcl);
        AclEdit edit(input);
        ss.changeKeyAcl(tester.keyRef, cred, edit);
        detail("Entry added");
		
        uint32 count;
        AclEntryInfo *infos;
        ss.getKeyAcl(tester.keyRef, "vamos", count, infos);
        assert(count == 1);	// one entry (with this tag)
		const AclEntryInfo &acl = infos[0];
		const TypedList &read = acl.proto().subject();
		assert(read.type() == CSSM_ACL_SUBJECT_TYPE_COMMENT);
		assert(read.length() == 3);
		assert(read[2] == 666);
		CssmList &sublist = read[1];
		assert(sublist[0] == CSSM_ACL_SUBJECT_TYPE_THRESHOLD);
		assert(string(sublist[1]) == "Robby Ray!");
		
		detail("Comment entry retrieved okay");
	}
}


//
// ACL authorization tests
//
void authAcls()
{
    printf("* ACL authorizations test\n");
	CssmAllocator &alloc = CssmAllocator::standard();
	ClientSession ss(alloc, alloc);
	
	// create key with initial ACL
    CSSM_ACL_AUTHORIZATION_TAG wrapTag = CSSM_ACL_AUTHORIZATION_EXPORT_CLEAR;
    CSSM_ACL_AUTHORIZATION_TAG encryptTag = CSSM_ACL_AUTHORIZATION_ENCRYPT;
	StringData initialAclPassphrase("very secret");
    StringData the2ndAclPassword("most secret");
	AclEntryPrototype initialAcl;
	initialAcl.TypedSubject = TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_PASSWORD,
		new(alloc) ListElement(initialAclPassphrase));
    initialAcl.authorization().NumberOfAuthTags = 1;
    initialAcl.authorization().AuthTags = &wrapTag;
	AclEntryInput initialAclInput(initialAcl);
	AclTester tester(ss, &initialAclInput);
	
	// get the owner and verify
	AclOwnerPrototype owner;
	ss.getKeyOwner(tester.keyRef, owner);
	assert(owner.subject().type() == CSSM_ACL_SUBJECT_TYPE_PASSWORD);
	assert(owner.subject().length() == 1);
	
	// get the acl entry and verify
	{
		uint32 count;
		AclEntryInfo *acls;
		ss.getKeyAcl(tester.keyRef, NULL/*tag*/, count, acls);
		assert(count == 1);
		const AclEntryInfo &acl1 = acls[0];
		const TypedList &subject1 = acl1.proto().subject();
		assert(subject1.type() == CSSM_ACL_SUBJECT_TYPE_PASSWORD);
		assert(subject1.length() == 1);
        const AuthorizationGroup &auths = acl1.proto().authorization();
        assert(auths.count() == 1);
        assert(auths[0] == CSSM_ACL_AUTHORIZATION_EXPORT_CLEAR);
	}
	
	// try to use the key and see...
	tester.testWrap(&nullCred, "ACCEPTING NULL CREDENTIAL");
	AutoCredentials cred(alloc);
	cred += TypedList(alloc, CSSM_SAMPLE_TYPE_PASSWORD,
		new(alloc) ListElement(StringData("wrongo")));
	tester.testWrap(&cred, "ACCEPTING WRONG PASSWORD CREDENTIAL");
	cred += TypedList(alloc, CSSM_SAMPLE_TYPE_PASSWORD,
		new(alloc) ListElement(initialAclPassphrase));
	tester.testWrap(&cred);

    tester.testEncrypt(&nullCred, "ACCEPTING NULL CREDENTIAL FOR UNAUTHORIZED OPERATION");
    tester.testEncrypt(&cred, "ACCEPTING GOOD CREDENTIAL FOR UNAUTHORIZED OPERATION");
    
    // now *add* a new ACL entry for encryption
    {
        detail("Adding new ACL entry");
        
        AclEntryPrototype newAcl;
        newAcl.TypedSubject = TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_PASSWORD,
            new(alloc) ListElement(the2ndAclPassword));
        newAcl.authorization().NumberOfAuthTags = 1;
        newAcl.authorization().AuthTags = &encryptTag;
        AclEntryInput newInput(newAcl);
        AclEdit edit(newInput);
        
        try {
            AutoCredentials nullCred(alloc);
            ss.changeKeyAcl(tester.keyRef, nullCred, edit);
            error("ALLOWED ACL EDIT WITHOUT CREDENTIALS");
        } catch (CssmCommonError &err) {
            detail(err, "Acl Edit rejected properly");
        }
        ss.changeKeyAcl(tester.keyRef, cred, edit);
        detail("ACL changed OK");

        // read it back and check
        {
            uint32 count;
            AclEntryInfo *acls;
            ss.getKeyAcl(tester.keyRef, NULL/*tag*/, count, acls);
            assert(count == 2);
            const AclEntryInfo &acl1 = acls[0];
            const TypedList &subject1 = acl1.proto().subject();
            assert(subject1.type() == CSSM_ACL_SUBJECT_TYPE_PASSWORD);
            assert(subject1.length() == 1);
            const AuthorizationGroup &auths1 = acl1.proto().authorization();
            assert(auths1.count() == 1);
            assert(auths1[0] == CSSM_ACL_AUTHORIZATION_EXPORT_CLEAR);
            const AclEntryInfo &acl2 = acls[1];
            const TypedList &subject2 = acl2.proto().subject();
            assert(subject2.type() == CSSM_ACL_SUBJECT_TYPE_PASSWORD);
            assert(subject2.length() == 1);
            const AuthorizationGroup &auths2 = acl2.proto().authorization();
            assert(auths2.count() == 1);
            assert(auths2[0] == CSSM_ACL_AUTHORIZATION_ENCRYPT);
        }
    }
    
    // ... and see how the new composite ACL behaves
	AutoCredentials cred2(alloc);
	cred2 += TypedList(alloc, CSSM_SAMPLE_TYPE_PASSWORD,
		new(alloc) ListElement(the2ndAclPassword));
    tester.testWrap(&nullCred, "ACCEPTING NULL CREDENTIALS FOR WRAPPING");	
    tester.testEncrypt(&nullCred, "ACCEPTING NULL CREDENTIALS FOR ENCRYPTION");
    tester.testWrap(&cred);	// "very secret" allows wrapping
    tester.testEncrypt(&cred2); // "most secret" allows encrypting
    tester.testWrap(&cred2, "ACCEPTING ENCRYPT CRED FOR WRAPPING");
    tester.testEncrypt(&cred, "ACCEPTING WRAP CRED FOR ENCRYPTING");
}


//
// Keychain ACL subjects
//
void keychainAcls()
{
    printf("* Keychain (interactive) ACL test\n");
	CssmAllocator &alloc = CssmAllocator::standard();
	ClientSession ss(alloc, alloc);
	
	// create key with initial ACL
	AclEntryPrototype initialAcl;
	initialAcl.TypedSubject = TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT,
        new(alloc) ListElement(alloc, "Test Key"));
	AclEntryInput initialAclInput(initialAcl);
	AclTester tester(ss, &initialAclInput);
	
	// get the owner and verify
	AclOwnerPrototype owner;
	ss.getKeyOwner(tester.keyRef, owner);
	assert(owner.subject().type() == CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT);
	assert(owner.subject().length() == 2);
	
	// get the acl entry and verify
	{
		uint32 count;
		AclEntryInfo *acls;
		ss.getKeyAcl(tester.keyRef, NULL/*tag*/, count, acls);
		assert(count == 1);
		const AclEntryInfo &acl1 = acls[0];
		const TypedList &subject1 = acl1.proto().subject();
		assert(subject1.type() == CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT);
		assert(subject1.length() == 2);
		assert(static_cast<string>(subject1[1]) == "Test Key");
	}
	
	// try to use the key and see...
	tester.testWrap(NULL, "ACCEPTING NULL CREDENTIAL");
	AutoCredentials cred(alloc);
	cred += TypedList(alloc, CSSM_SAMPLE_TYPE_PASSWORD,
		new(alloc) ListElement(StringData("Test Key")));
	tester.testWrap(&cred, "ACCEPTING PASSWORD CREDENTIAL");
	cred += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT);
	tester.testWrap(&cred);
	// once again, for allow-this-pid feature testing
	tester.testWrap(&cred);
}


//
// Code-signing ACL subjects
//
void codeSigning()
{
    printf("* Code Signing ACL test\n");
	CssmAllocator &alloc = CssmAllocator::standard();
	ClientSession ss(alloc, alloc);
	
	// sign ourselves
	OSXSigner signer;
	OSXCode *main = OSXCode::main();
	Signature *mySignature = signer.sign(*main);
	detail("Code signature for testclient obtained");
	
	// make a variant signature that isn't right
	Signature *badSignature;
	{
		char buffer[512];
		assert(mySignature->length() <= sizeof(buffer));
		memcpy(buffer, mySignature->data(), mySignature->length());
		memcpy(buffer, "xyz!", 4);	// 1 in 2^32 this is right...
		badSignature = signer.restore(mySignature->type(), buffer, mySignature->length());
	}
	
	// create key with good code signature ACL
	AclEntryPrototype initialAcl;
	initialAcl.subject() = TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE,
		new(alloc) ListElement(mySignature->type()),
		new(alloc) ListElement(alloc.alloc(*mySignature)));
	AclEntryInput initialAclInput(initialAcl);
	AclTester tester(ss, &initialAclInput);
	
	// get the owner and verify
	AclOwnerPrototype owner;
	ss.getKeyOwner(tester.keyRef, owner);
	assert(owner.subject().type() == CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE);
	assert(owner.subject().length() == 3);
	
	// we are us, so the SecurityServer should accept us
	tester.testWrap(&nullCred);
	
	// now try this again with a *bad* signature...
	AclEntryPrototype badAcl;
	badAcl.TypedSubject = TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE,
		new(alloc) ListElement(badSignature->type()),
		new(alloc) ListElement(alloc.alloc(*badSignature)));
	AclEntryInput badAclInput(badAcl);
	AclTester badTester(ss, &badAclInput);
	badTester.testWrap(&nullCred, "BAD CODE SIGNATURE ACCEPTED");
	
	// make sure the optional comment field makes it back out intact
	// (reusing original initialAcl structures)
	StringData comment("Walla Walla Washington!\nAbra cadabra.\n\n");
	initialAcl.subject() += new(alloc) ListElement(alloc, comment);
	AclEntryInput initialAclInputWithComment(initialAcl);
	AclTester commentTester(ss, &initialAclInputWithComment);
	ss.getKeyOwner(commentTester.keyRef, owner);
	assert(owner.subject().type() == CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE);
	assert(owner.subject().length() == 4);
	assert(owner.subject()[3] == comment);
	detail("Verified comment field intact");
}
