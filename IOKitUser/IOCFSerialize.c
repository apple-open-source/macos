/*
 * Copyright (c) 1999-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
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

typedef struct {
    long       			idrefCount;
    CFMutableDictionaryRef	idrefDict;
    CFMutableDataRef		data;
} IOCFSerializeState;


static Boolean
DoCFSerialize(CFTypeRef object, IOCFSerializeState * state);

static Boolean
addChar(char chr, IOCFSerializeState * state)
{
	CFDataAppendBytes(state->data, &chr, 1);

	return true;
}

static Boolean
addString(const char * str, IOCFSerializeState * state)
{
	CFIndex	len = strlen(str);

	CFDataAppendBytes(state->data, str, len);

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

static Boolean 
previouslySerialized(CFTypeRef object,
		     IOCFSerializeState * state)
{
	CFNumberRef idref;

	idref = CFDictionaryGetValue(state->idrefDict, object);
	if (idref) {
		char temp[64];
		long id = -1;
		CFNumberGetValue(idref, kCFNumberLongType, &id);
		sprintf(temp, "<%s IDREF=\"%ld\"/>", getTagString(object), id);
		return addString(temp, state);
	}

	idref = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &state->idrefCount);
	CFDictionaryAddValue(state->idrefDict, object, idref);
        CFRelease(idref);
	state->idrefCount++;
    
	return false;
}

static Boolean
addStartTag(CFTypeRef object,
	    const char *additionalTags,
	    IOCFSerializeState * state)
{
	CFNumberRef idref;
	char temp[128];
	long id = -1;

	idref = CFDictionaryGetValue(state->idrefDict, object);
	assert(idref);
	CFNumberGetValue(idref, kCFNumberLongType, &id);

	if (additionalTags) {
		snprintf(temp, 128, "<%s ID=\"%ld\" %s>", getTagString(object), id, additionalTags);
	} else {
		snprintf(temp, 128, "<%s ID=\"%ld\">", getTagString(object), id);
	}

	return addString(temp, state);
}


static Boolean
addEndTag(CFTypeRef object,
	  IOCFSerializeState * state)
{
	char temp[128];

	snprintf(temp, 128, "</%s>", getTagString(object));

	return addString(temp, state);
}

static Boolean
DoCFSerializeNumber(CFNumberRef object, IOCFSerializeState * state)
{
	char	temp[32];
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
	case kCFNumberLongType:
		size = 32;
		break;

	case kCFNumberSInt64Type:
	case kCFNumberLongLongType:
	default:
		size = 64;
		break;
	}

	sprintf(temp, "size=\"%d\"", size);
	if (!addStartTag(object, temp, state)) return false;

	if (size <= 32)
		sprintf(temp, "0x%lx", (unsigned long int) value);
	else
		sprintf(temp, "0x%qx", value);

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
	CFIndex	len = 0;
	char *	buffer;
        char c;
	int i;

	if (previouslySerialized(object, state)) return true;

	if (!addStartTag(object, 0, state)) return false;

	buffer = CFStringGetCStringPtr(object, kCFStringEncodingMacRoman);
	if (!buffer) {
		len = CFStringGetLength(object) + 1;
		buffer = malloc(len);
		if (!buffer || !CFStringGetCString(object, buffer, len, kCFStringEncodingMacRoman))
			return false;
	}

	for (i = 0; (c = buffer[i]); i++) {
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

	if (len)
		free(buffer);

	return addEndTag(object, state);
}

static Boolean
DoCFSerializeKey(CFStringRef object, IOCFSerializeState * state)
{
	CFIndex	len = 0;
	char *	buffer;
        char c;
	int i;

	if (!addString("<key>", state)) return false;

	buffer = CFStringGetCStringPtr(object, kCFStringEncodingMacRoman);
	if (!buffer) {
		len = CFStringGetLength(object) + 1;
		buffer = malloc(len);
		if (!buffer || !CFStringGetCString(object, buffer, len, kCFStringEncodingMacRoman))
			return false;
	}

	for (i = 0; (c = buffer[i]); i++) {
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

	if (len)
		free(buffer);

	return addString("</key>", state);
}

//this was taken from CFPropertyList.c 
static const char __CFPLDataEncodeTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static Boolean
DoCFSerializeData(CFDataRef object, IOCFSerializeState * state)
{
	CFIndex	  length;
	const UInt8 * bytes;
        unsigned int i;
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
		if (!(ok = DoCFSerializeKey((CFTypeRef) keys[i], state)))
			break;
		if (!(ok = DoCFSerialize((CFTypeRef) values[i], state)))
			break;
	}

	if (values)
		free(values);

	return ok && addEndTag(object, state);
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
        char temp[ 64 ];
	sprintf(temp, "\"typeID 0x%x not serializable\"", (int) type);
	ok = addString(temp, state);
    }

    return ok;
}

CFDataRef
IOCFSerialize(CFTypeRef object, CFOptionFlags options)
{
    IOCFSerializeState		state;
    CFMutableDataRef		data;
    CFMutableDictionaryRef	idrefDict;
    Boolean			ok;

    if ((!object) || (options)) return 0;

    idrefDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                              &kCFTypeDictionaryKeyCallBacks,
                              &kCFTypeDictionaryValueCallBacks);
    data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    assert(data && idrefDict);

    state.data = data;
    state.idrefCount = 0;
    state.idrefDict = idrefDict;

    ok = DoCFSerialize(object, &state);

    if (ok)
	ok = addChar(0, &state);
    if (!ok) {
	CFRelease(data);
	data = 0;
    }

    CFRelease(idrefDict);

    return data;
}
