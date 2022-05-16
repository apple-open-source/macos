/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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
#include "encdiskimagerep.h"
#include "notarization.h"
#include "sigblob.h"
#include "CodeSigner.h"

#include <security_utilities/endian.h>

namespace Security {
namespace CodeSigning {

// This value comes from a DiskImages header that is not exposed in the SDK. The validation
// is strict though, so we must use a known value, as images with any unknown values
// are rejected.
static const uint32_t kSignatureAuthMechanism = 3;

using Security::n2h;
using Security::h2n;
using namespace UnixPlusPlus;

#pragma mark AuthTable Helpers

AuthTable::AuthTable(UnixPlusPlus::FileDesc &fd)
{
	uint32_t expectedATECount = 0;

	if (fd.read(&expectedATECount, sizeof(expectedATECount)) != sizeof(expectedATECount)) {
		UnixError::throwMe(errSecCSBadDiskImageFormat);
	}
	expectedATECount = n2h(expectedATECount);
	mEntries.reserve(expectedATECount);

	if (expectedATECount > 1024) {
		UnixError::throwMe(errSecCSBadDiskImageFormat);
	}

	for (int i = 0; i < expectedATECount; i++) {
		std::shared_ptr<AuthTableEntry> entry = make_shared<AuthTableEntry>(fd);
		mEntries.push_back(entry);
	}
}

void AuthTable::serialize(UnixPlusPlus::FileDesc &fd)
{
	// Ensure the auth table is correct.
	prepareEntries();

	// First write the count, serializing it on the way out.
	uint32_t ateCount = (uint32_t)mEntries.size();
	ateCount = h2n(ateCount);
	fd.write(&ateCount, sizeof(ateCount));

	// Now write each entry.
	for (auto &entry : getEntries()) {
		entry->serialize(fd);
	}
}

void AuthTable::prepareEntries()
{
	// Prepares the auth table entries by ensuring that their
	// offsets are correct.  The data offsets start at the end of the
	// header plus the total size of the auth table.
	uint64_t currentDataOffset = sizeof(struct __Encrypted_Header_V2);
	currentDataOffset += sizeof(uint32_t);
	currentDataOffset += (mEntries.size() * sizeof(struct __AuthTableEntry));

	// Now for each auth entry, we set its offset to the first available
	// and then bump it by that entries length.
	for (auto &entry : getEntries()) {
		entry->setOffset(currentDataOffset);
		currentDataOffset += entry->length();
	}
}

void AuthTable::addEntry(uint32_t mechanism, void *data, size_t length)
{
	auto entry = make_shared<AuthTableEntry>(mechanism, 0, length);
	entry->setData(data, length);
	mEntries.push_back(entry);
	prepareEntries();
}

uint64_t AuthTable::findFirstEmptyDataOffset()
{
	uint64_t firstEmptyByte = 0;

	// Go through all the entries and look for the furthest
	// end of any auth table blob.
	for (auto &entry : getEntries()) {
		uint64_t lastByte = entry->offset() + entry->length();
		if (lastByte > firstEmptyByte) {
			firstEmptyByte = lastByte;
		}
	}

	return firstEmptyByte;
}

AuthTableEntry::AuthTableEntry(UnixPlusPlus::FileDesc &fd)
	: mData(NULL), mFreeData(false)
{
	struct __AuthTableEntry ate = {0};
	fd.read(&ate, sizeof(struct __AuthTableEntry));
	mMechanism = n2h(ate.mechanism);
	mOffset = n2h(ate.offset);
	mLength = n2h(ate.length);
}

AuthTableEntry::AuthTableEntry(uint32_t mechanism, uint64_t offset, uint64_t length)
	: mMechanism(mechanism), mOffset(offset), mLength(length), mData(NULL), mFreeData(false)
{
}

AuthTableEntry::~AuthTableEntry()
{
	clearData();
}

void AuthTableEntry::clearData()
{
	if (mData != NULL) {
		if (mFreeData) {
			free(mData);
		}
		mData = NULL;
	}
	mFreeData = false;
}

void AuthTableEntry::loadData(UnixPlusPlus::FileDesc &fd)
{
	clearData();
	mData = malloc(mLength);
	mFreeData = true;
	fd.read(mData, mLength, mOffset);
}

void AuthTableEntry::setOffset(uint64_t newOffset)
{
	mOffset = newOffset;
}

void AuthTableEntry::setData(void *data, size_t length)
{
	clearData();
	mData = data;
	mLength = length;
	mFreeData = false;
}

void AuthTableEntry::serialize(UnixPlusPlus::FileDesc &fd)
{
	// This is called with the file descriptor positioned at
	// where the entry should go, so just start writing.
	struct __AuthTableEntry ate = {0};
	ate.mechanism = h2n(mMechanism);
	ate.offset = h2n(mOffset);
	ate.length = h2n(mLength);
	fd.write(&ate, sizeof(ate));

	// Now we need to jump ahead to write the data, but if
	// so we need to remember the position and jump back
	// so future entries can be written.
	if (mData) {
		size_t curPos = fd.position();
		fd.write(mData, mLength, mOffset);
		fd.seek(curPos);
	}
}

#pragma mark EncDiskImageRep Object

EncDiskImageRep::EncDiskImageRep(const char *path)
: SingleDiskRep(path), mSigningData(NULL)
{
	this->setup();
}

EncDiskImageRep::~EncDiskImageRep()
{
	free((void*)mSigningData);
}

void EncDiskImageRep::flush()
{
	this->setup();
}

bool EncDiskImageRep::readHeader(FileDesc& fd, struct __Encrypted_Header_V2& header)
{
	static const size_t headerLength = sizeof(struct __Encrypted_Header_V2);
	size_t length = fd.fileSize();
	if (length < headerLength) {
		secinfo("encdiskimage", "file too short: %s", fd.realPath().c_str());
		return false;
	}

	// Reading the header always starts at the beginning of the file.
	fd.seek(0);
	if (fd.read(&header, headerLength) != headerLength) {
		secinfo("encdiskimage", "could not read: %s", fd.realPath().c_str());
		return false;
	}

	if (n2h(header.signature1) != 'encr' && n2h(header.signature2) != 'cdsa') {
		secinfo("encdiskimage", "signatures didn't match: %d, %d, %s", header.signature1,
				 header.signature2, fd.realPath().c_str());
		return false;
	}

	// Version 1 is no longer supported, and we can't know what version 3 will be.
	// DiskImages folks do not expect to need to roll this given the extensibility
	// within the current format.
	if (n2h(header.version) != 2) {
		secinfo("encdiskimage", "version wasn't right: %d, %s", header.version, fd.realPath().c_str());
		return false;
	}
	return true;
}

void EncDiskImageRep::setup()
{
	free((void*)mSigningData);
	mSigningData = NULL;

	if (!readHeader(fd(), this->mHeader)) {
		UnixError::throwMe(errSecCSBadDiskImageFormat);
	}

	// Ensure the file is large enough to contain all the data before the actual
	// data fork start.
	if (fd().fileSize() < n2h(mHeader.dataForkStartOffset)) {
		UnixError::throwMe(errSecCSBadDiskImageFormat);
	}

	// Find out where the signature is by iterating the auth table blobs.
	mAuthTable = AuthTable(fd());

	boolean_t foundSignature = false;
	size_t signatureOffset = 0;
	size_t signatureLength = 0;
	for (auto &entry : mAuthTable.getEntries()) {
		if (entry->mechanism() == kSignatureAuthMechanism) {
			// If we've already found a signature entry and find another, this image is
			// invalid so just bail out and refuse to use it.
			if (foundSignature) {
				secerror("Multiple signature entries found: %s", fd().realPath().c_str());
				UnixError::throwMe(errSecCSBadDiskImageFormat);
			}

			signatureOffset = entry->offset();
			signatureLength = entry->length();
			if ((signatureOffset != 0 && signatureLength == 0) ||
				(signatureOffset == 0 && signatureLength != 0)) {
				secerror("Invalid signature entry found: %zu, %zu, %s", signatureOffset,
						 signatureLength, fd().realPath().c_str());
				UnixError::throwMe(errSecCSBadDiskImageFormat);
			}

			foundSignature = true;
		}
	}

	// No more work to do for any unsigned items
	if (!foundSignature) {
		return;
	}

	// Read the actual signature superblob.
	if (EmbeddedSignatureBlob* blob = EmbeddedSignatureBlob::readBlob(fd(), signatureOffset, signatureLength)) {
		if (blob->length() != signatureLength || !blob->strictValidateBlob(signatureLength)) {
			free(blob);
			MacOSError::throwMe(errSecCSBadDiskImageFormat);
		}
		mSigningData = blob;
	}
}

//
// The default binary identification of a EncDiskImageRep is the SHA-256 hash
// of the header.
//
CFDataRef EncDiskImageRep::identification()
{
	SHA256 hash;
	hash(&mHeader, sizeof(mHeader));
	SHA256::Digest digest;
	hash.finish(digest);
	return makeCFData(digest, sizeof(digest));
}

//
// Sniffer function for encrypted disk image files.
// This just looks for the header and an appropriate magic and version.
//
bool EncDiskImageRep::candidate(FileDesc &fd)
{
	struct __Encrypted_Header_V2 header = {0};
	return readHeader(fd, header) == true;
}

size_t EncDiskImageRep::signingBase()
{
	// Signing base is the beginning of the data fork, since everything before
	// that is part of the auth table data and could change.
	return n2h(mHeader.dataForkStartOffset);
}

size_t EncDiskImageRep::signingLimit()
{
	// Signing limit is the remainder of the file after the signing base, leaving
	// no room to add a trailer or append additional data to the file.
	// NOTE: we don't use the dataForkStartOffset + dataForkSize as there is actually
	// some padding to round up to the nearest dataBlockSize. We could re-do that
	// padding calculation ourselves, but there seems no reason to allow additional
	// data at the end of the file, s
	size_t restOfFile = fd().fileSize() - signingBase();
	return restOfFile;
}

void EncDiskImageRep::strictValidate(const CodeDirectory* cd, const ToleratedErrors& tolerated, SecCSFlags flags)
{
	DiskRep::strictValidate(cd, tolerated, flags);

	if (cd) {
		size_t cd_limit = cd->signingLimit();
		size_t dr_limit = signingLimit();
		if (cd_limit != dr_limit &&         // must cover exactly the entire data
			cd_limit != fd().fileSize())    // or, for legacy detached sigs, the entire file
			MacOSError::throwMe(errSecCSSignatureInvalid);
	}
}

CFDataRef EncDiskImageRep::component(CodeDirectory::SpecialSlot slot)
{
	switch (slot) {
		case cdRepSpecificSlot:
			return makeCFData(&mHeader, sizeof(mHeader));
		default:
			return mSigningData ? mSigningData->component(slot) : NULL;
	}
}

string EncDiskImageRep::format()
{
	return "encrypted disk image";
}

void EncDiskImageRep::prepareForSigning(SigningContext& context)
{
	if (context.digestAlgorithms().empty()) {
		context.setDigestAlgorithm(kSecCodeSignatureHashSHA256);
	}
}

DiskRep::Writer *EncDiskImageRep::writer()
{
	return new Writer(this);
}

void EncDiskImageRep::Writer::component(CodeDirectory::SpecialSlot slot, CFDataRef data)
{
	assert(slot != cdRepSpecificSlot);
	EmbeddedSignatureBlob::Maker::component(slot, data);
}

void EncDiskImageRep::Writer::flush()
{
	free((void*)mSigningData);		// ditch previous blob just in case
	mSigningData = Maker::make();	// assemble new signature SuperBlob

	// Read all ATE blobs so we can shift them out later.
	for (auto &entry : rep->mAuthTable.getEntries()) {
		entry->loadData(fd());
	}

	// Check all the auth table entries to see if there is already a signature entry
	// so if there we can just update its data.
	bool hasSignature = false;
	for (auto &entry : rep->mAuthTable.getEntries()) {
		if (entry->mechanism() == kSignatureAuthMechanism) {
			entry->setData(mSigningData->data(), mSigningData->length());
			hasSignature = true;
		}
	}

	// If there was no signature, just add a new entry.
	if (!hasSignature) {
		rep->mAuthTable.addEntry(kSignatureAuthMechanism, mSigningData->data(), mSigningData->length());
	}

	// Sanity check that this auth table would not put us into the data section.
	uint64_t endOfAuthData = rep->mAuthTable.findFirstEmptyDataOffset();
	uint64_t startOfDiskData = n2h(rep->mHeader.dataForkStartOffset);
	int64_t bytesAfterAuthData = startOfDiskData - endOfAuthData;
	if (bytesAfterAuthData <= 0) {
		// Unsafe to write this out, there's not enough room.
		secerror("Disk image auth table would run into data segment, failing.");
		MacOSError::throwMe(errSecCSBadDiskImageFormat);
	}

	// Now serialize the entire AuthTable and contents back into the file, which
	// starts at the end of the header.
	fd().seek(sizeof(__Encrypted_Header_V2));
	rep->mAuthTable.serialize(fd());

	// Clear out the rest of the auth data until the start of the data fork.
	fd().seek(endOfAuthData);
	size_t bytesRemaining = (size_t)bytesAfterAuthData;
	size_t bufferSize = 16 * 1024;
	CFMallocData buffer(bufferSize);
	while (bytesRemaining > 0) {
		size_t bytesToWrite = min(bytesRemaining, buffer.length());
		fd().writeAll(buffer.data(), bytesToWrite);
		bytesRemaining -= bytesToWrite;
	}
}

void EncDiskImageRep::Writer::remove()
{
	int signaturePosition = -1;
	for (int i = 0; i < rep->mAuthTable.getEntries().size(); i++) {
		if (rep->mAuthTable.getEntries()[i]->mechanism() == kSignatureAuthMechanism) {
			signaturePosition = i;
		}
	}
	if (signaturePosition >= 0) {
		auto entries = rep->mAuthTable.getEntries();
		entries.erase(entries.begin() + signaturePosition);
	}
}

void EncDiskImageRep::registerStapledTicket()
{
	CFRef<CFDataRef> data = NULL;
	if (mSigningData) {
		data.take(mSigningData->component(cdTicketSlot));
		registerStapledTicketInDMG(data);
	}
}

}
}
