/*
 * Copyright (c) 1999-2007 Apple Inc. All rights reserved.
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
/*
 * HISTORY
 *
 * 25 Nov 98 from IOSerialize.cpp created by rsulack
 * 31 Aug 99 sdouglas CF version.
 * 09 Dec 99 rsulack converted to serialize to "XML" syntax
 */


#include <CoreFoundation/CoreFoundation.h>
#include <assert.h>
#include <syslog.h>


typedef struct {
    int       			idrefNumRefs;
    CFMutableDictionaryRef	idrefDict;
    CFMutableDictionaryRef	idrefCountDict;
    CFMutableDictionaryRef	defined;  // whether original def'n output
    CFMutableDataRef		data;
} IOCFSerializeState;


static Boolean
DoCFSerialize(CFTypeRef object, IOCFSerializeState * state);

static Boolean
addChar(char chr, IOCFSerializeState * state)
{
	CFDataAppendBytes(state->data, (UInt8 *) &chr, 1);

	return true;
}

static Boolean
addString(const char * str, IOCFSerializeState * state)
{
	CFIndex	len = strlen(str);
	CFDataAppendBytes(state->data, (UInt8 *) str, len);

	return true;
}

static const char *
getTagString(CFTypeRef object)
{
	CFTypeID type;

	assert(object);

	type = CFGetTypeID(object);

	if (type == CFDictionaryGetTypeID()) return "dict";
	if (type == CFArrayGetTypeID()) return "array";
	if (type == CFSetGetTypeID()) return "set";
	if (type == CFStringGetTypeID()) return "string";
	if (type == CFDataGetTypeID()) return "data";
	if (type == CFNumberGetTypeID()) return "integer";

	return "internal error";
}

static void
recordIdref(CFTypeRef object,
	    IOCFSerializeState * state)
{
	CFNumberRef idref;
	CFNumberRef refCount;
	int count;

	idref = CFDictionaryGetValue(state->idrefDict, object);
	if (idref) {
		refCount = CFDictionaryGetValue(state->idrefCountDict, object);
		if (refCount) {
			assert(CFNumberGetValue(refCount, kCFNumberIntType, &count));
			count++;
		} else {
			count = 1;
		}
		// ok to lose original refCount, we are replacing it
		refCount = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &count);
		assert(refCount);
		CFDictionarySetValue(state->idrefCountDict, object, refCount);
                CFRelease(refCount);
		return;
	}

	idref = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &state->idrefNumRefs);
	CFDictionaryAddValue(state->idrefDict, object, idref);
	CFRelease(idref);
	state->idrefNumRefs++;
	count = 0;
	refCount = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &count);
	assert(refCount);
	CFDictionarySetValue(state->idrefCountDict, object, refCount);
	CFRelease(refCount);
	return;
}

static Boolean 
previouslySerialized(CFTypeRef object,
		     IOCFSerializeState * state)
{
	CFBooleanRef defined;
	CFNumberRef refCount;
	int count = 0;
	CFNumberRef idref;

	defined = CFDictionaryGetValue(state->defined, object);
	if (!defined || !CFBooleanGetValue(defined)) {
		return false;
	}
	refCount = CFDictionaryGetValue(state->idrefCountDict, object);
	if (!refCount) {
		return false;
	}
	CFNumberGetValue(refCount, kCFNumberIntType, &count);
	if (!count) {
		return false;
	}
	idref = CFDictionaryGetValue(state->idrefDict, object);
	if (idref) {
		char temp[64];
		int id = -1;
		CFNumberGetValue(idref, kCFNumberIntType, &id);
		snprintf(temp, sizeof(temp), "<%s IDREF=\"%d\"/>", getTagString(object), id);
		return addString(temp, state);
	}

	return false;
}

static Boolean
addStartTag(CFTypeRef object,
	    const char *additionalTags,
	    IOCFSerializeState * state)
{
	CFNumberRef idref;
	CFNumberRef refCount;
	char temp[128];
	int id = -1;
	int count = 0;

	idref = CFDictionaryGetValue(state->idrefDict, object);
	refCount = CFDictionaryGetValue(state->idrefCountDict, object);
	if (refCount) {
		CFNumberGetValue(refCount, kCFNumberIntType, &count);
	}
	if (idref && count) {
		CFNumberGetValue(idref, kCFNumberIntType, &id);

		if (additionalTags) {
			snprintf(temp, sizeof(temp) * sizeof(char),
                        	"<%s ID=\"%d\" %s>", getTagString(object), id, additionalTags);
		} else {
			snprintf(temp, sizeof(temp) * sizeof(char),
                        	"<%s ID=\"%d\">", getTagString(object), id);
		}
	} else {
		if (additionalTags) {
			snprintf(temp, sizeof(temp) * sizeof(char),
                        	"<%s %s>", getTagString(object), additionalTags);
		} else {
			snprintf(temp, sizeof(temp) * sizeof(char),
                        	"<%s>", getTagString(object));
		}
	}
        CFDictionarySetValue(state->defined, object, kCFBooleanTrue);
	return addString(temp, state);
}


static Boolean
addEndTag(CFTypeRef object,
	  IOCFSerializeState * state)
{
	char temp[128];

	snprintf(temp, sizeof(temp) * sizeof(char), "</%s>", getTagString(object));

	return addString(temp, state);
}

static Boolean
DoCFSerializeNumber(CFNumberRef object, IOCFSerializeState * state)
{
	char		temp[32];
	long long	value;
	int		size;

	if (previouslySerialized(object, state)) return true;

	if (!CFNumberGetValue(object, kCFNumberLongLongType, &value))
		return(false);

	switch(CFNumberGetType(object)) {
	case kCFNumberSInt8Type:
	case kCFNumberCharType:
		size = 8;
		break;

	case kCFNumberSInt16Type:
	case kCFNumberShortType:
		size = 16;
		break;

	case kCFNumberSInt32Type:
	case kCFNumberIntType:
		size = 32;
		break;

	case kCFNumberLongType:
#if __LP64__
		size = 64;
#else
		size = 32;
#endif
		break;
		
	case kCFNumberSInt64Type:
	case kCFNumberLongLongType:
	default:
		size = 64;
		break;
	}

	snprintf(temp, sizeof(temp), "size=\"%d\"", size);
	if (!addStartTag(object, temp, state)) return false;

	if (size <= 32)
		snprintf(temp, sizeof(temp), "0x%lx", (unsigned long int) value);
	else
		snprintf(temp, sizeof(temp), "0x%qx", value);

	if (!addString(temp, state)) return false;

	return addEndTag(object, state);
}

static Boolean
DoCFSerializeBoolean(CFBooleanRef object, IOCFSerializeState * state)
{
	return addString(CFBooleanGetValue(object) ? "<true/>" : "<false/>", state);
}

static Boolean
DoCFSerializeString(CFStringRef object, IOCFSerializeState * state)
{
	CFDataRef dataBuffer = 0;
	const char * buffer;
	CFIndex length;
	bool conversionFailed = false;
	char c;
	int i;

	if (previouslySerialized(object, state)) return true;

	if (!addStartTag(object, 0, state)) return false;

	dataBuffer = CFStringCreateExternalRepresentation(kCFAllocatorDefault, object, kCFStringEncodingUTF8, 0);	
	if (!dataBuffer) {
		dataBuffer = CFStringCreateExternalRepresentation(kCFAllocatorDefault, object, kCFStringEncodingUTF8, (UInt8)'?');
		conversionFailed = true;
	}

	if (dataBuffer) {
		length = CFDataGetLength(dataBuffer);
		buffer = (char *) CFDataGetBytePtr(dataBuffer);
	} else {
		length = 0;
		buffer = "";
		conversionFailed = true;
	}
	
	if (conversionFailed) {
		char * tempBuffer;
		if (buffer && (tempBuffer = malloc(length + 1))) {
			bcopy(buffer, tempBuffer, length);
			tempBuffer[length] = 0;

			syslog(LOG_ERR, "FIXME: IOCFSerialize has detected a string that can not be converted to UTF-8, \"%s\"", tempBuffer);

			free(tempBuffer);
		}
	}
	
	// this works because all bytes in a multi-byte utf-8 character have the high order bit set
	for (i = 0; i < length; i++) {
		c = buffer[i];
		if (c == '<') {
			if (!addString("&lt;", state)) return false;
		} else if (c == '>') {
			if (!addString("&gt;", state)) return false;
		} else if (c == '&') {
			if (!addString("&amp;", state)) return false;
		} else {
			if (!addChar(c, state)) return false;
		}
	}

	if (dataBuffer) CFRelease(dataBuffer);

	return addEndTag(object, state);
}

static Boolean
DoCFSerializeKey(CFStringRef object, IOCFSerializeState * state)
{
	CFDataRef dataBuffer = 0;
	const char * buffer;
	CFIndex length;
	bool conversionFailed = false;
        char c;
	int i;

	if (!addString("<key>", state)) return false;

	dataBuffer = CFStringCreateExternalRepresentation(kCFAllocatorDefault, object, kCFStringEncodingUTF8, 0);	
	if (!dataBuffer) {
		dataBuffer = CFStringCreateExternalRepresentation(kCFAllocatorDefault, object, kCFStringEncodingUTF8, (UInt8)'?');
		conversionFailed = true;
	}

	if (dataBuffer) {
		length = CFDataGetLength(dataBuffer);
		buffer = (char *) CFDataGetBytePtr(dataBuffer);
	} else {
		length = 0;
		buffer = "";
		conversionFailed = true;
	}
	
	if (conversionFailed) {
		char * tempBuffer;
		if (buffer && (tempBuffer = malloc(length + 1))) {
			bcopy(buffer, tempBuffer, length);
			tempBuffer[length] = 0;

			syslog(LOG_ERR, "FIXME: IOCFSerialize has detected a string that can not be converted to UTF-8, \"%s\"", tempBuffer);

			free(tempBuffer);
		}
	}
	
	// this works because all bytes in a multi-byte utf-8 character have the high order bit set
	for (i = 0; i < length; i++) {
		c = buffer[i];
		if (c == '<') {
			if (!addString("&lt;", state)) return false;
		} else if (c == '>') {
			if (!addString("&gt;", state)) return false;
		} else if (c == '&') {
			if (!addString("&amp;", state)) return false;
		} else {
			if (!addChar(c, state)) return false;
		}
	}

	if (dataBuffer) CFRelease(dataBuffer);

	return addString("</key>", state);
}

//this was taken from CFPropertyList.c 
static const char __CFPLDataEncodeTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static Boolean
DoCFSerializeData(CFDataRef object, IOCFSerializeState * state)
{
	CFIndex	  length;
	const UInt8 * bytes;
        CFIndex i;
        const unsigned char *p;
        unsigned char c = 0;

	if (previouslySerialized(object, state)) return true;

	length = CFDataGetLength(object);
	bytes = CFDataGetBytePtr(object);

        if (!addStartTag(object, 0, state)) return false;

        for (i = 0, p = (unsigned char *)bytes; i < length; i++, p++) {
		/* 3 bytes are encoded as 4 */
		switch (i % 3) {
                case 0:
			c = __CFPLDataEncodeTable [ ((p[0] >> 2) & 0x3f)];
			if (!addChar(c, state)) return false;
			break;
                case 1:
			c = __CFPLDataEncodeTable [ ((((p[-1] << 8) | p[0]) >> 4) & 0x3f)];
			if (!addChar(c, state)) return false;
			break;
                case 2:
			c = __CFPLDataEncodeTable [ ((((p[-1] << 8) | p[0]) >> 6) & 0x3f)];
			if (!addChar(c, state)) return false;
			c = __CFPLDataEncodeTable [ (p[0] & 0x3f)];
			if (!addChar(c, state)) return false;
			break;
		}
        }
        switch (i % 3) {
	case 0:
                break;
	case 1:
                c = __CFPLDataEncodeTable [ ((p[-1] << 4) & 0x30)];
                if (!addChar(c, state)) return false;
                if (!addChar('=', state)) return false;
                if (!addChar('=', state)) return false;
                break;
	case 2:
                c = __CFPLDataEncodeTable [ ((p[-1] << 2) & 0x3c)];
                if (!addChar(c, state)) return false;
                if (!addChar('=', state)) return false;
                break;
        }

	return addEndTag(object, state);
}

static Boolean
DoCFSerializeArray(CFArrayRef object, IOCFSerializeState * state)
{
	CFIndex	count, i;
	Boolean	ok = true;
 
	if (previouslySerialized(object, state)) return true;

        if (!addStartTag(object, 0, state)) return false;

	count = CFArrayGetCount(object);
	if (count) {
		for (i = 0; ok && (i < count); i++) {
			ok = DoCFSerialize((CFTypeRef) CFArrayGetValueAtIndex(object, i),
					   state);
		}
	}

	return ok && addEndTag(object, state);
}

static Boolean
DoCFSerializeSet(CFSetRef object, IOCFSerializeState * state)
{
	CFIndex	  count, i;
	const void ** values;
	Boolean	  ok = true;
 
	if (previouslySerialized(object, state)) return true;

        if (!addStartTag(object, 0, state)) return false;

	count = CFSetGetCount(object);

	if (count) {
		values = (const void **) malloc(count * sizeof(void *));
		if (!values)
			return false;
		CFSetGetValues(object, values);
	} else {
		values = 0;
	}

	for (i = 0; ok && (i < count); i++) {
		ok = DoCFSerialize((CFTypeRef) values[i], state);
	}

	if (values)
		free(values);

	return ok && addEndTag(object, state);
}

static Boolean
DoCFSerializeDictionary(CFDictionaryRef object,
				IOCFSerializeState * state)
{
	CFIndex	  count, i;
	const void ** values;
	const void ** keys;
	Boolean	  ok = true;
 
	if (previouslySerialized(object, state)) return true;

        if (!addStartTag(object, 0, state)) return false;

	count = CFDictionaryGetCount(object);

	if (count) {
		values = (const void **) malloc(2 * count * sizeof(void *));
		if (!values)
			return false;
		keys = values + count;
		CFDictionaryGetKeysAndValues(object, keys, values);
	} else {
		values = keys = 0;
	}

	for (i = 0; ok && (i < count); i++) {
		ok = DoCFSerializeKey((CFTypeRef) keys[i], state);
                if (!ok)
			break;
		if (!(ok = DoCFSerialize((CFTypeRef) values[i], state)))
			break;
	}

	if (values)
		free(values);

	return ok && addEndTag(object, state);
}

static Boolean
DoIdrefScan(CFTypeRef object, IOCFSerializeState * state)
{
	CFTypeID type;
	Boolean	ok = true;  // such an optimist
	CFIndex count, i;
	const void ** keys;
	const void ** values;

	assert(object);

	recordIdref(object, state);

	type = CFGetTypeID(object);

	if (type == CFArrayGetTypeID()) {
		CFArrayRef array = (CFArrayRef)object;
		count = CFArrayGetCount(array);
		for (i = 0; i < count; i++) {
			CFTypeRef element = CFArrayGetValueAtIndex(array, i);
			ok = DoIdrefScan(element, state);
			if (!ok) {
				return ok;
			}
		}
	} else if (type == CFSetGetTypeID()) {
		CFSetRef set = (CFSetRef)object;
		count = CFSetGetCount(set);
		if (count) {
			values = (const void **)malloc(count * sizeof(void *));
			if (!values) {
				return false;
			}
			CFSetGetValues(set, values);
			for (i = 0; ok && (i < count); i++) {
				ok = DoIdrefScan((CFTypeRef)values[i], state);
			}

			free(values);
			if (!ok) {
				return ok;
			}
                }
	} else if (type == CFDictionaryGetTypeID()) {
		CFDictionaryRef dict = (CFDictionaryRef)object;
		count = CFDictionaryGetCount(dict);
		if (count) {
			keys = (const void **)malloc(count * sizeof(void *));
			values = (const void **)malloc(count * sizeof(void *));
			if (!keys || !values) {
				return false;
			}
			CFDictionaryGetKeysAndValues(dict, keys, values);
			for (i = 0; ok && (i < count); i++) {
				// don't record dictionary keys
				ok &= DoIdrefScan((CFTypeRef)values[i], state);
			}

			free(keys);
			free(values);
			if (!ok) {
				return ok;
			}
		}
	}

	return ok;
}

static Boolean
DoCFSerialize(CFTypeRef object, IOCFSerializeState * state)
{
    CFTypeID	type;
    Boolean	ok;

    assert(object);

    type = CFGetTypeID(object);

    if (type == CFNumberGetTypeID())
	ok = DoCFSerializeNumber((CFNumberRef) object, state);
    else if (type == CFBooleanGetTypeID())
	ok = DoCFSerializeBoolean((CFBooleanRef) object, state);
    else if (type == CFStringGetTypeID())
	ok = DoCFSerializeString((CFStringRef) object, state);
    else if (type == CFDataGetTypeID())
	ok = DoCFSerializeData((CFDataRef) object, state);
    else if (type == CFArrayGetTypeID())
	ok = DoCFSerializeArray((CFArrayRef) object, state);
    else if (type == CFSetGetTypeID())
	ok = DoCFSerializeSet((CFSetRef) object, state);
    else if (type == CFDictionaryGetTypeID())
	ok = DoCFSerializeDictionary((CFDictionaryRef) object, state);
    else {
        CFStringRef temp = NULL;
        temp = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
				CFSTR("<string>typeID 0x%x not serializable</string>"), (int) type);
        if (temp) {
            ok = DoCFSerializeString(temp, state);
            CFRelease(temp);
        } else {
            ok = false;
        }
        
    }

    return ok;
}

CFDataRef
IOCFSerialize(CFTypeRef object, CFOptionFlags options)
{
    IOCFSerializeState		state;
    CFMutableDataRef		data;
    CFMutableDictionaryRef	idrefDict;
    CFMutableDictionaryRef	idrefCountDict;
    CFMutableDictionaryRef	defined;
    Boolean			ok;
    CFDictionaryKeyCallBacks	idrefCallbacks;

    if ((!object) || (options)) return 0;

    idrefCallbacks = kCFTypeDictionaryKeyCallBacks;
    // only use pointer equality for these keys
    idrefCallbacks.equal = NULL;
    idrefDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                              &idrefCallbacks,
                              &kCFTypeDictionaryValueCallBacks);
    idrefCountDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                              &idrefCallbacks,
                              &kCFTypeDictionaryValueCallBacks);
    defined = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                              &idrefCallbacks,
                              &kCFTypeDictionaryValueCallBacks);
    data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    assert(data && idrefDict && idrefCountDict);

    state.data = data;
    state.idrefNumRefs = 0;
    state.idrefDict = idrefDict;
    state.idrefCountDict = idrefCountDict;
    state.defined = defined;

    ok = DoIdrefScan(object, &state);
    if (!ok) {
        goto finish;
    }

    ok = DoCFSerialize(object, &state);

    if (ok) {
	ok = addChar(0, &state);
    }
    if (!ok) {
        goto finish;
    }

finish:
    if (!ok && data) {
        CFRelease(data);
        data = 0;
    }
    if (idrefDict) CFRelease(idrefDict);
    if (idrefCountDict) CFRelease(idrefCountDict);
    if (defined) CFRelease(defined);

    return data;
}
