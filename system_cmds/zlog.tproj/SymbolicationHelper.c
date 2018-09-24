//
//  SymbolicationHelper.c
//  zlog
//
//  Created by Rasha Eqbal on 2/26/18.
//

#include "SymbolicationHelper.h"

/*
 * Most of the CoreSymbolication code here has been copied from ioclasscount in the IOKitTools project.
 */

#define kAddressKey CFSTR("Address")
#define kNameKey CFSTR("Name")
#define kPathKey CFSTR("Path")
#define kSegmentsKey CFSTR("Segments")
#define kSizeKey CFSTR("Size")
#define kUuidKey CFSTR("UUID")

static void AddSymbolOwnerSummary(CSSymbolOwnerRef owner, CFMutableDictionaryRef binaryImages);
static void ShowBinaryImage(const void *key, const void *value, void *context);

/*
 * Symbolicates 'addr' using the 'symbolicator' passed in.
 * Adds owner info to 'binaryImages' for offline symbolication.
 *
 * Top-level function that needs to be called on each frame address in the backtrace.
 */
void PrintSymbolicatedAddress(CSSymbolicatorRef symbolicator, mach_vm_address_t addr, CFMutableDictionaryRef binaryImages)
{
	printf("0x%llx", addr);

	CSSymbolOwnerRef ownerInfo = CSSymbolicatorGetSymbolOwnerWithAddressAtTime(symbolicator, addr, kCSNow);
	if (!CSIsNull(ownerInfo)) {
		const char *moduleName = CSSymbolOwnerGetName(ownerInfo);
		if (moduleName) {
			printf(" <%s>", moduleName);
		}
	}

	CSSymbolRef symbolInfo = CSSymbolicatorGetSymbolWithAddressAtTime(symbolicator, addr, kCSNow);
	if (!CSIsNull(symbolInfo)) {
		printf(" %s", CSSymbolGetName(symbolInfo));
	}

	CSSourceInfoRef sourceInfo = CSSymbolicatorGetSourceInfoWithAddressAtTime(symbolicator, addr, kCSNow);
	if (!CSIsNull(sourceInfo)) {
		const char *fileName = CSSourceInfoGetPath(sourceInfo);
		if (fileName) {
			printf(" at %s:%d", fileName, CSSourceInfoGetLineNumber(sourceInfo));
		}
	}
	printf("\n");

	AddSymbolOwnerSummary(ownerInfo, binaryImages);
}

/*
 * Adds symbolication information for 'owner' to 'binaryImages' to help with offline symbolication.
 *
 * This is called from PrintSymbolicatedAddress() on the symbol owner for each address it symbolicates.
 */
static void AddSymbolOwnerSummary(CSSymbolOwnerRef owner, CFMutableDictionaryRef binaryImages)
{
	const CFUUIDBytes *uuidBytes = NULL;
	CFUUIDRef uuid = NULL;
	CFStringRef uuidString = NULL, path = NULL, name = NULL;
	CFMutableDictionaryRef summaryDict = NULL;
	__block CSSegmentRef textSegment = kCSNull, textExecSegment = kCSNull;
	CSSegmentRef segment = kCSNull;
	CSRange range;
	CFNumberRef address = NULL, size = NULL;

#define RETURN_IF_NULL(ptr)	\
if (!(ptr))	{			\
goto cleanup;		\
}

	uuidBytes = CSSymbolOwnerGetCFUUIDBytes(owner);
	if (uuidBytes) {
		uuid = CFUUIDCreateFromUUIDBytes(NULL, *uuidBytes);
		if (uuid) {
			uuidString = CFUUIDCreateString(kCFAllocatorDefault, uuid);
		}
	}
	RETURN_IF_NULL(uuidString);

	if (!CFDictionaryContainsKey(binaryImages, uuidString)) {
		summaryDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		RETURN_IF_NULL(summaryDict);

		CFDictionarySetValue(summaryDict, kUuidKey, uuidString);

		path = CFStringCreateWithCString(kCFAllocatorDefault, CSSymbolOwnerGetPath(owner), kCFStringEncodingUTF8);
		RETURN_IF_NULL(path);
		CFDictionarySetValue(summaryDict, kPathKey, path);

		name = CFStringCreateWithCString(kCFAllocatorDefault, CSSymbolOwnerGetName(owner), kCFStringEncodingUTF8);
		RETURN_IF_NULL(name);
		CFDictionarySetValue(summaryDict, kNameKey, name);

		CSSymbolOwnerForeachSegment(owner, ^(CSSegmentRef segment) {
			if (strcmp(CSRegionGetName(segment), "__TEXT SEGMENT") == 0) {
				textSegment = segment;
				CSRetain(textSegment);
			} else if (strcmp(CSRegionGetName(segment), "__TEXT_EXEC SEGMENT") == 0) {
				textExecSegment = segment;
				CSRetain(textExecSegment);
			}
		});

		segment = !CSIsNull(textExecSegment) ? textExecSegment : textSegment;
		if (CSIsNull(segment)) {
			goto cleanup;
		}
		range = CSRegionGetRange(segment);

		address = CFNumberCreate(NULL, kCFNumberLongLongType, &range.location);
		RETURN_IF_NULL(address);
		CFDictionarySetValue(summaryDict, kAddressKey, address);

		size = CFNumberCreate(NULL, kCFNumberLongLongType, &range.length);
		RETURN_IF_NULL(size);
		CFDictionarySetValue(summaryDict, kSizeKey, size);

		CFDictionarySetValue(binaryImages, uuidString, summaryDict);
	}

cleanup:
	if (size) CFRelease(size);
	if (address) CFRelease(address);
	if (!CSIsNull(textExecSegment)) CSRelease(textExecSegment);
	if (!CSIsNull(textSegment)) CSRelease(textSegment);
	if (name) CFRelease(name);
	if (path) CFRelease(path);
	if (summaryDict) CFRelease(summaryDict);
	if (uuidString) CFRelease(uuidString);
	if (uuid) CFRelease(uuid);
}

/*
 * Prints offline symbolication information for the images passed in 'binaryImages'.
 *
 * Top-level function that needs to be called if the tool wants to include support
 * for offline symbolication.
 */
void PrintBinaryImagesInfo(CFMutableDictionaryRef binaryImages)
{
	if (CFDictionaryGetCount(binaryImages) > 0) {
		printf("\nBinary Images:\n");
		CFDictionaryApplyFunction(binaryImages, ShowBinaryImage, NULL);
	} else {
		printf("No binary images\n");
	}
}

/*
 * Prints information about a binary image necessary for offline symbolication.
 *
 * This is called from PrintBinaryImagesInfo() on each element in 'binaryImages'.
 */
static void ShowBinaryImage(const void *key, const void *value, void *context)
{
	char nameString[256] = {0}, uuidString[256] = {0}, pathString[256] = {0};
	CFStringRef uuid = (CFStringRef)key;
	CFStringGetCString(uuid, uuidString, sizeof(uuidString), kCFStringEncodingASCII);
	CFDictionaryRef summary = (CFDictionaryRef)value;

	CFStringRef name = CFDictionaryGetValue(summary, kNameKey);
	CFStringGetCString(name, nameString, sizeof(nameString), kCFStringEncodingASCII);
	CFStringRef path = CFDictionaryGetValue(summary, kPathKey);
	CFStringGetCString(path, pathString, sizeof(pathString), kCFStringEncodingASCII);
	CFNumberRef addressNumber = CFDictionaryGetValue(summary, kAddressKey);
	CFNumberRef sizeNumber = CFDictionaryGetValue(summary, kSizeKey);
	int64_t address, size;
	CFNumberGetValue(addressNumber, kCFNumberSInt64Type, &address);
	CFNumberGetValue(sizeNumber, kCFNumberSInt64Type, &size);

	printf("%p - %p %s <%s> %s\n", (void*)address, (void*)address + size, nameString, uuidString, pathString);
}
