/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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
#include "SecAssessment.h"
#include "notarization.h"
#include <security_utilities/unix++.h>

typedef struct __attribute__((packed)) _package_trailer {
	uint8_t magic[4];
	uint16_t version;
	uint16_t type;
	uint32_t length;
	uint8_t reserved[4];
} package_trailer_t;

enum TrailerType {
	TrailerTypeInvalid = 0,
	TrailerTypeTerminator,
	TrailerTypeTicket,
};

static const char *TrailerMagic = "t8lr";

namespace Security {
namespace CodeSigning {

static void
registerStapledTicketWithSystem(CFDataRef data)
{
	secinfo("notarization", "Registering stapled ticket with system");

#if TARGET_OS_OSX
	CFErrorRef error = NULL;
	if (!SecAssessmentTicketRegister(data, &error)) {
		secerror("Error registering stapled ticket: %@", data);
	}
#endif // TARGET_OS_OSX
}

bool
checkNotarizationServiceForRevocation(CFDataRef hash, SecCSDigestAlgorithm hashType, double *date)
{
	bool is_revoked = false;

	secinfo("notarization", "checking with online notarization service for hash: %@", hash);

#if TARGET_OS_OSX
	CFRef<CFErrorRef> error;
	if (!SecAssessmentTicketLookup(hash, hashType, kSecAssessmentTicketFlagForceOnlineCheck, date, &error.aref())) {
		CFIndex err = CFErrorGetCode(error);
		if (err == EACCES) {
			secerror("Notarization daemon found revoked hash: %@", hash);
			is_revoked = true;
		} else {
			secerror("Error checking with notarization daemon: %ld", err);
		}
	}
#endif

	return is_revoked;
}

bool
isNotarized(const Requirement::Context *context)
{
	CFRef<CFDataRef> cd;
	CFRef<CFErrorRef> error;
	bool is_notarized = false;
	SecCSDigestAlgorithm hashType = kSecCodeSignatureNoHash;

	if (context == NULL) {
		is_notarized = false;
		goto lb_exit;
	}

	if (context->directory) {
		cd.take(context->directory->cdhash());
		hashType = (SecCSDigestAlgorithm)context->directory->hashType;
	} else if (context->packageChecksum) {
		cd = context->packageChecksum;
		hashType = context->packageAlgorithm;
	}

	if (cd.get() == NULL) {
		// No cdhash means we can't check notarization.
		is_notarized = false;
		goto lb_exit;
	}

	secinfo("notarization", "checking notarization on %d, %@", hashType, cd.get());

#if TARGET_OS_OSX
	if (SecAssessmentTicketLookup(cd, hashType, kSecAssessmentTicketFlagDefault, NULL, &error.aref())) {
		is_notarized = true;
	} else {
		is_notarized = false;
		if (error.get() != NULL) {
			secerror("Error checking with notarization daemon: %ld", CFErrorGetCode(error));
		}
	}
#endif

lb_exit:
	secinfo("notarization", "isNotarized = %d", is_notarized);
	return is_notarized;
}

void
registerStapledTicketInPackage(const std::string& path)
{
	int fd = 0;
	package_trailer_t trailer;
	off_t readOffset = 0;
	size_t bytesRead = 0;
	off_t backSeek = 0;
	uint8_t *ticketData = NULL;
	boolean_t ticketTrailerFound = false;
	CFRef<CFDataRef> data;

	secinfo("notarization", "Extracting ticket from package: %s", path.c_str());

	fd = open(path.c_str(), O_RDONLY);
	if (fd <= 0) {
		secerror("cannot open package for reading");
		goto lb_exit;
	}

	bzero(&trailer, sizeof(trailer));
	readOffset = lseek(fd, -sizeof(trailer), SEEK_END);
	if (readOffset <= 0) {
		secerror("could not scan for first trailer on package (error - %d)", errno);
		goto lb_exit;
	}

	while (!ticketTrailerFound) {
		bytesRead = read(fd, &trailer, sizeof(trailer));
		if (bytesRead != sizeof(trailer)) {
			secerror("could not read next trailer from package (error - %d)", errno);
			goto lb_exit;
		}

		if (memcmp(trailer.magic, TrailerMagic, strlen(TrailerMagic)) != 0) {
			// Most packages will not be stapled, so this isn't really an error.
			secdebug("notarization", "package did not end in a trailer");
			goto lb_exit;
		}

		switch (trailer.type) {
			case TrailerTypeTicket:
				ticketTrailerFound = true;
				break;
			case TrailerTypeTerminator:
				// Found a terminator before a trailer, so just exit.
				secinfo("notarization", "package had a trailer, but no ticket trailers");
				goto lb_exit;
			case TrailerTypeInvalid:
				secinfo("notarization", "package had an invalid trailer");
				goto lb_exit;
			default:
				// it's an unsupported trailer type, so skip it.
				break;
		}

		// If we're here, it's either a ticket or an unknown trailer.  In both cases we can definitely seek back to the
		// beginning of the data pointed to by this trailer, which is the length of its data and the size of the trailer itself.
		backSeek = -1 * (sizeof(trailer) + trailer.length);
		if (!ticketTrailerFound) {
			// If we didn't find a ticket, we're about to iterate again and want to read the next trailer so seek back an additional
			// trailer blob to prepare for reading it.
			backSeek -= sizeof(trailer);
		}
		readOffset = lseek(fd, backSeek, SEEK_CUR);
		if (readOffset <= 0) {
			secerror("could not scan backwards (%lld) for next trailer on package (error - %d)", backSeek, errno);
			goto lb_exit;
		}
	}

	// If we got here, we have a valid ticket trailer and already seeked back to the beginning of its data.
	ticketData = (uint8_t*)malloc(trailer.length);
	if (ticketData == NULL) {
		secerror("could not allocate memory for ticket");
		goto lb_exit;
	}

	bytesRead = read(fd, ticketData, trailer.length);
	if (bytesRead != trailer.length) {
		secerror("unable to read entire ticket (error - %d)", errno);
		goto lb_exit;
	}

	data = CFDataCreateWithBytesNoCopy(NULL, ticketData, trailer.length, NULL);
	if (data.get() == NULL) {
		secerror("unable to create cfdata for notarization");
		goto lb_exit;
	}

	secinfo("notarization", "successfully found stapled ticket for: %s", path.c_str());
	registerStapledTicketWithSystem(data);

lb_exit:
	if (fd) {
		close(fd);
	}
	if (ticketData) {
		free(ticketData);
	}
}

void
registerStapledTicketInBundle(const std::string& path)
{
	int fd = 0;
	struct stat st;
	uint8_t *ticketData = NULL;
	size_t ticketLength = 0;
	size_t bytesRead = 0;
	CFRef<CFDataRef> data;
	std::string ticketLocation = path + "/Contents/CodeResources";

	secinfo("notarization", "Extracting ticket from bundle: %s", path.c_str());

	fd =  open(ticketLocation.c_str(), O_RDONLY);
	if (fd <= 0) {
		// Only print an error if the file exists, otherwise its an expected early exit case.
		if (errno != ENOENT) {
			secerror("cannot open stapled file for reading: %d", errno);
		}
		goto lb_exit;
	}

	if (fstat(fd, &st)) {
		secerror("unable to stat stapling file: %d", errno);
		goto lb_exit;
	}

	if ((st.st_mode & S_IFREG) != S_IFREG) {
		secerror("stapling is not a regular file");
		goto lb_exit;
	}

	if (st.st_size <= INT_MAX) {
		ticketLength = (size_t)st.st_size;
	} else {
		secerror("ticket size was too large: %lld", st.st_size);
		goto lb_exit;
	}

	ticketData = (uint8_t*)malloc(ticketLength);
	if (ticketData == NULL) {
		secerror("unable to allocate data for ticket");
		goto lb_exit;
	}

	bytesRead = read(fd, ticketData, ticketLength);
	if (bytesRead != ticketLength) {
		secerror("unable to read entire ticket from bundle");
		goto lb_exit;
	}

	data = CFDataCreateWithBytesNoCopy(NULL, ticketData, ticketLength, NULL);
	if (data.get() == NULL) {
		secerror("unable to create cfdata for notarization");
		goto lb_exit;
	}

	secinfo("notarization", "successfully found stapled ticket for: %s", path.c_str());
	registerStapledTicketWithSystem(data);

lb_exit:
	if (fd) {
		close(fd);
	}
	if (ticketData) {
		free(ticketData);
	}
}

void
registerStapledTicketInDMG(CFDataRef ticketData)
{
	if (ticketData == NULL) {
		return;
	}
	secinfo("notarization", "successfully found stapled ticket in DMG");
	registerStapledTicketWithSystem(ticketData);
}

}
}
