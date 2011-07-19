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
    CFMutableDataRef   data;

    int                idrefNumRefs;
    
/* For each CFType, we track whether a given plist value hasRefs,
 * and what the ids used for those refs are. 'hasRefs' is set to
 * kCFBooleanFalse the first time we see a given value, so we know
 * we saw it, and then kCFBooleanTrue if we see it again, so we know
 * we need an id for it. On writing out the XML, we generate ids as
 * we encounter the need.
 */
    CFMutableDictionaryRef stringIDRefDictionary;
    CFMutableDictionaryRef numberIDRefDictionary;
    CFMutableDictionaryRef dataIDRefDictionary;
    CFMutableDictionaryRef dictionaryIDRefDictionary;
    CFMutableDictionaryRef arrayIDRefDictionary;
    CFMutableDictionaryRef setIDRefDictionary;

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

	if (type == CFStringGetTypeID()) return "string";
	if (type == CFNumberGetTypeID()) return "integer";
	if (type == CFDataGetTypeID()) return "data";
	if (type == CFDictionaryGetTypeID()) return "dict";
	if (type == CFArrayGetTypeID()) return "array";
	if (type == CFSetGetTypeID()) return "set";

	return "internal error";
}

static CFMutableDictionaryRef
idRefDictionaryForObject(
    CFTypeRef              object,
    IOCFSerializeState   * state)
{
    CFMutableDictionaryRef result     = NULL;
    CFTypeID               objectType = CFNullGetTypeID();  // do not release

    objectType = CFGetTypeID(object);

   /* Sorted by rough order of % occurence in big plists.
    */
	if (objectType == CFDictionaryGetTypeID()) {
        result = state->dictionaryIDRefDictionary;
    } else if (objectType == CFStringGetTypeID()) {
        result = state->stringIDRefDictionary;
    } else if (objectType == CFArrayGetTypeID()) {
        result = state->arrayIDRefDictionary;
    } else if (objectType == CFNumberGetTypeID()) {
        result = state->numberIDRefDictionary;
    } else if (objectType == CFDataGetTypeID()) {
        result = state->dataIDRefDictionary;
    } else if (objectType == CFSetGetTypeID()) {
        result = state->setIDRefDictionary;
    }

	return result;
}

void
recordObjectInIDRefDictionary(
    CFTypeRef              object,
    IOCFSerializeState   * state)
{
    CFTypeRef              refEntry        = NULL;  // do not release
    CFMutableDictionaryRef idRefDictionary = NULL;  // do not release

    if (!object || !state) {
        goto finish;
    }

    idRefDictionary = idRefDictionaryForObject(object, state);
    if (!idRefDictionary) {
        goto finish;
    }

	refEntry = CFDictionaryGetValue(idRefDictionary, object);

   /* If we have never seen this object value, then add an entry
    * in the dictionary with value false, indicating we have seen
    * it once.
    *
    * If we have seen this object value, then set its entry to true
    * to indicate that we have now seen a second occurrence
    * of the object value, which means we will generate an ID and IDREFs
    * in the XML.
    */
    if (!refEntry) {
       /* Use AddValue here so as not to overwrite.
        */
        CFDictionaryAddValue(idRefDictionary, object, kCFBooleanFalse);
    } else {
        CFDictionarySetValue(idRefDictionary, object, kCFBooleanTrue);
    }

finish:
    return;
}

Boolean 
previouslySerialized(
    CFTypeRef            object,
    IOCFSerializeState * state)
{
    Boolean                result     = FALSE;
    CFMutableDictionaryRef idRefDict  = NULL;  // do not release
    CFTypeRef              idRefEntry = NULL;  // do not release
    CFTypeID               idRefType  = CFNullGetTypeID();  // do not release
    char                   temp[64];
    int                    idInt      = -1;

    if (!object || !state) {
        goto finish;
    }

   /* If we don't get a lookup dict or an entry for the object,
    * then no ID or IDREF will be involved,
    * so treat is if never before serialized.
    */
    idRefDict = idRefDictionaryForObject(object, state);
    if (!idRefDict) {
        goto finish;
    }

    idRefEntry = (CFTypeRef)CFDictionaryGetValue(idRefDict, object);
    if (!idRefEntry) {
        goto finish;
    }

    idRefType = CFGetTypeID(idRefEntry);

   /* If the entry is of Boolean type, then an ID/IDREF may be involved,
    * but we haven't created one yet, so not yet serialized.
    */
    if (idRefType == CFBooleanGetTypeID()) {
        goto finish;
    }

   /* At this point the entry should be a CFNumber.
    */
    if (idRefType != CFNumberGetTypeID()) {
        goto finish;
    }

   /* Finally, get the IDREF value out of the idRef entry and write the
    * XML for it. Bail on extraction error.
    */
    if (!CFNumberGetValue((CFNumberRef)idRefEntry, kCFNumberIntType, &idInt)) {
        goto finish;
    }
    snprintf(temp, sizeof(temp), "<%s IDREF=\"%d\"/>", getTagString(object), idInt);
    result = addString(temp, state);

finish:
	return result;
}

static Boolean
addStartTag(
    CFTypeRef            object,
    const char         * additionalTags,
    IOCFSerializeState * state)
{
    CFMutableDictionaryRef idRefDict  = NULL;  // do not release
    CFTypeRef              idRefEntry = NULL;  // do not release
	CFNumberRef            idRef = NULL;  // must release
	char                   temp[128];

    idRefDict = idRefDictionaryForObject(object, state);
    if (idRefDict) {
        idRefEntry = (CFTypeRef)CFDictionaryGetValue(idRefDict, object);
    }

   /* If the IDRef entry is 'true', then we know we have an object value
    * with multiple references and need to emit an ID. So we create one
    * by incrementing the state's counter, making a CFNumber, and *replacing*
    * the boolean value in the IDRef dictionary with that number.
    */
	if (idRefEntry == kCFBooleanTrue) {
        int idInt = state->idrefNumRefs++;
        
        idRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &idInt);
        assert(idRef);

        CFDictionarySetValue(idRefDict, object, idRef);
        CFRelease(idRef);

		if (additionalTags) {
			snprintf(temp, sizeof(temp) * sizeof(char),
                        	"<%s ID=\"%d\" %s>", getTagString(object), idInt, additionalTags);
		} else {
			snprintf(temp, sizeof(temp) * sizeof(char),
                        	"<%s ID=\"%d\">", getTagString(object), idInt);
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

	if (!CFNumberGetValue(object, kCFNumberLongLongType, &value)) {
		return(false);
    }

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

	recordObjectInIDRefDictionary(object, state);

	type = CFGetTypeID(object);

	if (type == CFDictionaryGetTypeID()) {
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
    } else if (type == CFArrayGetTypeID()) {
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

   /* Sorted by rough order of % occurrence in big plists.
    */
    if (type == CFDictionaryGetTypeID()) {
        ok = DoCFSerializeDictionary((CFDictionaryRef) object, state);
    } else if (type == CFStringGetTypeID()) {
        ok = DoCFSerializeString((CFStringRef) object, state);
    } else if (type == CFArrayGetTypeID()) {
        ok = DoCFSerializeArray((CFArrayRef) object, state);
    } else if (type == CFNumberGetTypeID()) {
        ok = DoCFSerializeNumber((CFNumberRef) object, state);
    } else if (type == CFDataGetTypeID()) {
        ok = DoCFSerializeData((CFDataRef) object, state);
    } else if (type == CFBooleanGetTypeID()) {
        ok = DoCFSerializeBoolean((CFBooleanRef) object, state);
    } else if (type == CFSetGetTypeID()) {
        ok = DoCFSerializeSet((CFSetRef) object, state);
    } else {
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
    IOCFSerializeState       state;
    Boolean			         ok   = FALSE;
    CFDictionaryKeyCallBacks idrefKeyCallbacks;

    if ((!object) || (options)) return 0;

    state.data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    assert(state.data);

    state.idrefNumRefs = 0;

    idrefKeyCallbacks = kCFTypeDictionaryKeyCallBacks;
    // only use pointer equality for these keys
    idrefKeyCallbacks.equal = NULL;

    state.stringIDRefDictionary = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &idrefKeyCallbacks, &kCFTypeDictionaryValueCallBacks);
    assert(state.stringIDRefDictionary);

    state.numberIDRefDictionary = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &idrefKeyCallbacks, &kCFTypeDictionaryValueCallBacks);
    assert(state.numberIDRefDictionary);

    state.dataIDRefDictionary = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &idrefKeyCallbacks, &kCFTypeDictionaryValueCallBacks);
    assert(state.dataIDRefDictionary);

    state.dictionaryIDRefDictionary = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &idrefKeyCallbacks, &kCFTypeDictionaryValueCallBacks);
    assert(state.dictionaryIDRefDictionary);

    state.arrayIDRefDictionary = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &idrefKeyCallbacks, &kCFTypeDictionaryValueCallBacks);
    assert(state.arrayIDRefDictionary);

    state.setIDRefDictionary = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &idrefKeyCallbacks, &kCFTypeDictionaryValueCallBacks);
    assert(state.setIDRefDictionary);

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
    if (!ok && state.data) {
        CFRelease(state.data);
        state.data = NULL;  // it's returned
    }

    if (state.stringIDRefDictionary)     CFRelease(state.stringIDRefDictionary);
    if (state.numberIDRefDictionary)     CFRelease(state.numberIDRefDictionary);
    if (state.dataIDRefDictionary)       CFRelease(state.dataIDRefDictionary);
    if (state.dictionaryIDRefDictionary) CFRelease(state.dictionaryIDRefDictionary);
    if (state.arrayIDRefDictionary)      CFRelease(state.arrayIDRefDictionary);
    if (state.setIDRefDictionary)        CFRelease(state.setIDRefDictionary);

    return state.data;
}
