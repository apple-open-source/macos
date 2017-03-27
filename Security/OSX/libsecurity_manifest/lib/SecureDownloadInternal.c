#include <CoreFoundation/CoreFoundation.h>

#include "SecureDownloadInternal.h"

//
// SecureDownloadXML: SecureDownloadXML.c
//        cc -g -framework CoreFoundation -o $@ $^
//

#define LOCAL_DEBUG 0

#if LOCAL_DEBUG
extern CFDataRef read_data(char* path);
#endif

#define SD_XML_NAMESPACE	CFSTR("http://www.apple.com/2006/SecureDownload/1")
#define SD_XML_ROOT		CFSTR("SecureDownload")
#define SD_XML_RESOURCE		CFSTR("resource")
#define SD_XML_SITES		CFSTR("sites")
#define SD_XML_VERIFICATION		CFSTR("verification")
#define SD_XML_ATTR_ALGORITHM		CFSTR("algorithm")

struct parseState {
	CFDictionaryRef namespaces; // array of dictionaries of namespace declarations
#if LOCAL_DEBUG
	char* prefix;
#endif
	CFMutableArrayRef plists;	// array of all resource plists
	CFMutableDictionaryRef plist;	// most recent entry in the plists array
};


static inline unsigned char decode64(unsigned char c)
{
	switch(c) {
	case 'A': return 0;
	case 'B': return 1;
	case 'C': return 2;
	case 'D': return 3;
	case 'E': return 4;
	case 'F': return 5;
	case 'G': return 6;
	case 'H': return 7;
	case 'I': return 8;
	case 'J': return 9;
	case 'K': return 10;
	case 'L': return 11;
	case 'M': return 12;
	case 'N': return 13;
	case 'O': return 14;
	case 'P': return 15;
	case 'Q': return 16;
	case 'R': return 17;
	case 'S': return 18;
	case 'T': return 19;
	case 'U': return 20;
	case 'V': return 21;
	case 'W': return 22;
	case 'X': return 23;
	case 'Y': return 24;
	case 'Z': return 25;
	case 'a': return 26;
	case 'b': return 27;
	case 'c': return 28;
	case 'd': return 29;
	case 'e': return 30;
	case 'f': return 31;
	case 'g': return 32;
	case 'h': return 33;
	case 'i': return 34;
	case 'j': return 35;
	case 'k': return 36;
	case 'l': return 37;
	case 'm': return 38;
	case 'n': return 39;
	case 'o': return 40;
	case 'p': return 41;
	case 'q': return 42;
	case 'r': return 43;
	case 's': return 44;
	case 't': return 45;
	case 'u': return 46;
	case 'v': return 47;
	case 'w': return 48;
	case 'x': return 49;
	case 'y': return 50;
	case 'z': return 51;
	case '0': return 52;
	case '1': return 53;
	case '2': return 54;
	case '3': return 55;
	case '4': return 56;
	case '5': return 57;
	case '6': return 58;
	case '7': return 59;
	case '8': return 60;
	case '9': return 61;
	case '+': return 62;
	case '/': return 63;
	}
	return 255;
}

// Decodes base64 data into a binary CFData object
// If first character on a line is not in the base64 alphabet, the line 
// is ignored.
static CF_RETURNS_RETAINED CFDataRef decodeBase64Data(const UInt8* ptr, size_t len) {
	CFMutableDataRef result = CFDataCreateMutable(NULL, len); // data can't exceed len bytes
	if (!result) return NULL;
	
	CFIndex i, j;
	
	int skip = 0;
	
	UInt8 triplet[3] = {0, 0, 0};

/*
http://www.faqs.org/rfcs/rfc3548.html
         +--first octet--+-second octet--+--third octet--+
         |7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|
         +-----------+---+-------+-------+---+-----------+
         |5 4 3 2 1 0|5 4 3 2 1 0|5 4 3 2 1 0|5 4 3 2 1 0|
         +--1.index--+--2.index--+--3.index--+--4.index--+
*/
	
	for (i = 0, j = 0; i < len; ++i) {
		unsigned char c = ptr[i];
		if (c == ' ')  { continue; }
		if (c == '\t') { continue; }
		if (c == '\r') { continue; }
		if (c == '\n') { skip = 0; continue; }
		if (skip)      { continue; }
		if (!skip && c == '=')  { --j; skip = 1; continue; }
		unsigned char x = decode64(c);
		if (x == 255) { skip = 1; continue; }
		
		if (j == 0) {
			triplet[0] |= ((x << 2) & 0xFC);
			++j;
		} else if (j == 1) {
			triplet[0] |= ((x >> 4) & 0x03);
			triplet[1] |= ((x << 4) & 0xF0);
			++j;
		} else if (j == 2) {
			triplet[1] |= ((x >> 2) & 0x0F);
			triplet[2] |= ((x << 6) & 0xC0);
			++j;
		} else if (j == 3) {
			triplet[2] |= ((x) & 0x3F);
			CFDataAppendBytes(result, triplet, j);
			memset(triplet, 0, sizeof(triplet));
			j = 0;
		}
	}
	if (j > 0) {
		CFDataAppendBytes(result, triplet, j);
	}
	return result;
}

// Returns a CFString containing the base64 representation of the data.
// boolean argument for whether to line wrap at 64 columns or not.
static CF_RETURNS_RETAINED CFStringRef encodeBase64String(const UInt8* ptr, size_t len, int wrap) {
	const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/=";

	// base64 encoded data uses 4 ASCII characters to represent 3 octets.
	// There can be up to two == at the end of the base64 data for padding.
	// If we are line wrapping then we need space for one newline character
	// every 64 characters of output.
	// Rounded 4/3 up to 2 to avoid floating point math.
	
	//CFIndex max_len = (2*len) + 2;
	//if (wrap) len = len + ((2*len) / 64) + 1;

	CFMutableStringRef string = CFStringCreateMutable(NULL, 0);
	if (!string) return NULL;

/*
http://www.faqs.org/rfcs/rfc3548.html
         +--first octet--+-second octet--+--third octet--+
         |7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|
         +-----------+---+-------+-------+---+-----------+
         |5 4 3 2 1 0|5 4 3 2 1 0|5 4 3 2 1 0|5 4 3 2 1 0|
         +--1.index--+--2.index--+--3.index--+--4.index--+
*/
	int i = 0;		// octet offset into input data
	int column = 0;		// output column number (used for line wrapping)
	for (;;) {
		UniChar c[16];	// buffer of characters to add to output
		int j = 0;	// offset to place next character in buffer
		int index;	// index into output alphabet

#define ADDCHAR(_X_) do { c[j++] = _X_; if (wrap && (++column == 64)) { column = 0; c[j++] = '\n'; } } while (0);

		// 1.index
		index = (ptr[i] >> 2) & 0x3F;
		ADDCHAR(alphabet[index]);
		
		// 2.index
		index = (ptr[i] << 4) & 0x30;
		if ((i+1) < len) {
			index = index | ((ptr[i+1] >> 4) & 0x0F);
			ADDCHAR(alphabet[index]);
		} else {	// end of input, pad as necessary
			ADDCHAR(alphabet[index]);
			ADDCHAR('=');
			ADDCHAR('=');
		}

		// 3.index
		if ((i+1) < len) {
			index = (ptr[i+1] << 2) & 0x3C;
			if ((i+2) < len) {
				index = index | ((ptr[i+2] >> 6) & 0x03);
				ADDCHAR(alphabet[index]);
			} else {	// end of input, pad as necessary
				ADDCHAR(alphabet[index]);
				ADDCHAR('=');
			}
		}

		// 4.index
		if ((i+2) < len) {
			index = (ptr[i+2]) & 0x3F;
			ADDCHAR(alphabet[index]);
		}
		
		CFStringAppendCharacters(string, c, j);
		i += 3; // we processed 3 bytes of input
		if (i >= len) {
			// end of data, append newline if we haven't already
			if (wrap && c[j-1] != '\n') {
				c[0] = '\n';
				CFStringAppendCharacters(string, c, 1);
			}
			break;
		}
	}
	return string;
}


// makes a copy of the current namespaces dictionary, adding in any
// namespaces defined by the current node.
static CFDictionaryRef copyNamespacesForNode(CFDictionaryRef namespaces, CFXMLNodeRef node) {
	CFMutableDictionaryRef result = NULL;

	CFXMLNodeTypeCode type = CFXMLNodeGetTypeCode(node);
	
	// careful, don't use the info unless we ensure type == kCFXMLNodeTypeElement
	CFXMLElementInfo* info = (CFXMLElementInfo*)CFXMLNodeGetInfoPtr(node);

	//
	// create our result dictionary
	// there are four possible configurations:
	// 1. previous dictionary exists, this is an element, and has attributes:
	//	clone existing dictionary, we may be adding to it
	// 2. previous dictionary exists, not an element or no attributes:
	//	retain existing dictionary and return
	// 3. no previous dictionary, this is an element, and has attributes:
	//	create new dictionary, we may be adding to it
	// 4. no previous dictionary, not an element or no attributes:
	//	create new dictionary and return
	//
	if (namespaces && type == kCFXMLNodeTypeElement && info->attributes && info->attributeOrder) {
		result = CFDictionaryCreateMutableCopy(NULL, 0, namespaces);
	} else if (namespaces) {
		result = (CFMutableDictionaryRef)CFRetain(namespaces);
		return result;
	} else if (type == kCFXMLNodeTypeElement && info->attributes && info->attributeOrder) {
		result = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (result) CFDictionarySetValue(result, CFSTR(""), CFSTR(""));
	} else {
		result = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (result) CFDictionarySetValue(result, CFSTR(""), CFSTR(""));
		return result;
	}
	if (!result) return NULL;

	//
	// if we got this far, we're dealing with an XML element with
	// attributes.  check to see if any are xml namespace attributes.
	//
	CFArrayRef attrs = info->attributeOrder;
	CFIndex i, count = CFArrayGetCount(attrs);
	for (i = 0; i < count; ++i) {
		CFStringRef attr = CFArrayGetValueAtIndex(attrs, i);
		
		if (CFEqual(CFSTR("xmlns"), attr)) {
			// default namespace
			CFStringRef value = CFDictionaryGetValue(info->attributes, attr);
			if (value) {
				CFDictionarySetValue(result, CFSTR(""), value);
			}
		} else {
			// if the attribute is in the "xmlns" namespace, then it's
			// really a declaration of a new namespace.  record it in our dictionary.
			CFArrayRef parts = CFStringCreateArrayBySeparatingStrings(NULL, attr, CFSTR(":"));
			CFIndex numparts = parts ? CFArrayGetCount(parts) : 0;
			if (numparts == 2) {
				CFStringRef prefix = CFArrayGetValueAtIndex(parts, 0);
				CFStringRef suffix = CFArrayGetValueAtIndex(parts, 1);
				if (CFEqual(CFSTR("xmlns"), prefix)) {
					CFStringRef value = CFDictionaryGetValue(info->attributes, attr);
					if (value) {
						CFDictionarySetValue(result, suffix, value);
					}
				}
			}
			if (parts) CFRelease(parts);
		}
	}
	return result;
}

// returns the current node's element name and namespace URI
// based on the currently defined namespaces.
static void copyNodeNamespaceAndName(CFDictionaryRef namespaces, CFXMLNodeRef node, CFStringRef* namespace, CFStringRef* name) {
	CFXMLNodeTypeCode type = CFXMLNodeGetTypeCode(node);
	*namespace = NULL;
	*name = NULL;
	if (type == kCFXMLNodeTypeElement) {
		CFStringRef qname = CFXMLNodeGetString(node);
		CFArrayRef parts = CFStringCreateArrayBySeparatingStrings(NULL, qname, CFSTR(":"));
		CFIndex numparts = parts ? CFArrayGetCount(parts) : 0;
		if (numparts == 1) {
			// default namespace
			*namespace = CFRetain(CFDictionaryGetValue(namespaces, CFSTR("")));
			*name = CFRetain(CFArrayGetValueAtIndex(parts, 0));
		} else if (numparts == 2) {
			CFStringRef prefix = CFArrayGetValueAtIndex(parts, 0);
			CFStringRef ns = CFDictionaryGetValue(namespaces, prefix);
			*namespace = ns ? CFRetain(ns) : NULL;
			*name = CFRetain(CFArrayGetValueAtIndex(parts, 1));
		} else {
			// bogus xml
		}
		if (parts) CFRelease(parts);
	}
}

// helper function for copyTreeString() below
// appends text nodes to the mutable string context
static void _appendTreeString(const void *value, void *context) {
	CFXMLTreeRef tree = (CFXMLTreeRef)value;
	CFMutableStringRef result = (CFMutableStringRef)context;
	
	CFXMLNodeRef node = CFXMLTreeGetNode(tree);
	CFXMLNodeTypeCode type = CFXMLNodeGetTypeCode(node);
	if (type == kCFXMLNodeTypeElement) {
		CFTreeApplyFunctionToChildren(tree, _appendTreeString, result);
	} else if (type == kCFXMLNodeTypeText) {
		CFStringRef str = CFXMLNodeGetString(node);
		if (str) CFStringAppend(result, str);
	}
}

// equivalent to the XPATH string() function
// concatenates all text nodes into a single string
static CFMutableStringRef copyTreeString(CFXMLTreeRef tree) {
	CFMutableStringRef result = CFStringCreateMutable(NULL, 0);
	CFTreeApplyFunctionToChildren(tree, _appendTreeString, result);
	return result;
}


// returns an array of CFXMLTreeRef objects that are immediate
// children of the context node that match the specified element
// name and namespace URI
static CFArrayRef copyChildrenWithName(CFXMLTreeRef tree, CFDictionaryRef inNamespaces, CFStringRef namespace, CFStringRef name) {
	CFMutableArrayRef result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	tree = CFTreeGetFirstChild(tree);
	while (tree) {
		CFXMLNodeRef node = CFXMLTreeGetNode(tree);
		CFXMLNodeTypeCode type = CFXMLNodeGetTypeCode(node);
		if (type == kCFXMLNodeTypeElement) {
			CFDictionaryRef namespaces = copyNamespacesForNode(inNamespaces, node);
			if (namespaces) {
				CFStringRef ns, n;
				copyNodeNamespaceAndName(namespaces, node, &ns, &n);
		
				if (ns && n && CFEqual(ns, namespace) && CFEqual(n, name)) {
					CFArrayAppendValue(result, tree);
				}
				if (ns) CFRelease(ns);
				if (n) CFRelease(n);
			
				CFRelease(namespaces);
			}
		}
		tree = CFTreeGetNextSibling(tree);
	}
	return result;
}

// convenience function to find the first child element
// with the given element name and namespace URI
static CFXMLTreeRef getChildWithName(CFXMLTreeRef tree, CFDictionaryRef inNamespaces, CFStringRef namespace, CFStringRef name) {
	CFXMLTreeRef result = NULL;
	CFArrayRef array = copyChildrenWithName(tree, inNamespaces, namespace, name);
	if (array && CFArrayGetCount(array) > 0) {
		result = (CFXMLTreeRef)CFArrayGetValueAtIndex(array, 0);
	}
	if (array) CFRelease(array);
	return result;
}

// returns the string value of the specified child node
static CFStringRef copyChildWithNameAsString(CFXMLTreeRef tree, CFDictionaryRef inNamespaces, CFStringRef namespace, CFStringRef name) {
	CFStringRef result = NULL;
	CFXMLTreeRef child = getChildWithName(tree, inNamespaces, namespace, name);
	if (child) result = copyTreeString(child);
	return result;
}

// returns the integer value of the specified child node
static CFNumberRef copyChildWithNameAsInteger(CFXMLTreeRef tree, CFDictionaryRef inNamespaces, CFStringRef namespace, CFStringRef name) {
	CFNumberRef result = NULL;
	CFXMLTreeRef child = getChildWithName(tree, inNamespaces, namespace, name);
	if (child) {
		CFStringRef str = copyTreeString(child);
		if (str) {
			SInt32 size = CFStringGetIntValue(str);
			result = CFNumberCreate(NULL, kCFNumberSInt32Type, &size);
			CFRelease(str);
		}
	}
	return result;
}

// returns an array of URLs aggregated from the child
// nodes matching the given name and namespace URI.
static CFArrayRef copyChildrenWithNameAsURLs(CFXMLTreeRef tree, CFDictionaryRef inNamespaces, CFStringRef namespace, CFStringRef name) {
	CFMutableArrayRef result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayRef children = copyChildrenWithName(tree, inNamespaces, namespace, name);
	if (children) {
		CFIndex i, count = CFArrayGetCount(children);
		for (i = 0; i < count; ++i) {
			CFXMLTreeRef child = (CFXMLTreeRef)CFArrayGetValueAtIndex(children, i);
			CFStringRef str = copyTreeString(child);
			if (str) {
				CFURLRef url = CFURLCreateWithString(NULL, str, NULL);
				if (url) {
					CFArrayAppendValue(result, url);
					CFRelease(url);
				}
				CFRelease(str);
			}
		}
		CFRelease(children);
	}
	return result;
}

// returns the base 64 decoded value of the specified child node
static CFDataRef copyChildWithNameAsData(CFXMLTreeRef tree, CFDictionaryRef inNamespaces, CFStringRef namespace, CFStringRef name) {
	CFDataRef result = NULL;
	CFXMLTreeRef child = getChildWithName(tree, inNamespaces, namespace, name);
	if (child) {
		CFStringRef str = copyTreeString(child);
		if (str) {
			CFIndex len = CFStringGetLength(str);
			CFIndex used;
			UInt8* buffer = malloc(len);
			// ASCII is one byte per character.  Any inconvertible characters
			// are assigned to whitespace and skipped.
			if (buffer) {
				if (CFStringGetBytes(str, CFRangeMake(0, len), kCFStringEncodingASCII, ' ', 0, buffer, len, &used)) {
					result = decodeBase64Data(buffer, used);
				}
				free(buffer);
			}
			CFRelease(str);
		}
	}
	return result;
}

// returns the CFDate value of the specified child node
// whose string value is interpreted in W3C DateTime format
static CFDateRef copyChildWithNameAsDate(CFXMLTreeRef tree, CFDictionaryRef inNamespaces, CFStringRef namespace, CFStringRef name) {
	CFDateRef result = NULL;
	CFXMLTreeRef child = getChildWithName(tree, inNamespaces, namespace, name);
	if (child) {
		CFMutableStringRef str = copyTreeString(child);
		if (str) {
			CFStringTrimWhitespace(str);
			if (CFStringGetLength(str) > 21) {
				CFStringRef year = CFStringCreateWithSubstring(NULL, str, CFRangeMake(0, 4));
				CFStringRef month = CFStringCreateWithSubstring(NULL, str, CFRangeMake(5, 2));
				CFStringRef day = CFStringCreateWithSubstring(NULL, str, CFRangeMake(8, 2));
				CFStringRef hour = CFStringCreateWithSubstring(NULL, str, CFRangeMake(11, 2));
				CFStringRef minute = CFStringCreateWithSubstring(NULL, str, CFRangeMake(14, 2));
				CFStringRef second = CFStringCreateWithSubstring(NULL, str, CFRangeMake(17, 2));
				CFStringRef tenth = CFStringCreateWithSubstring(NULL, str, CFRangeMake(20, 1));

				CFGregorianDate gregory;
				memset(&gregory, 0, sizeof(gregory));
				if (year) { gregory.year = CFStringGetIntValue(year); CFRelease(year); }
				if (month) { gregory.month = CFStringGetIntValue(month); CFRelease(month); }
				if (day) { gregory.day = CFStringGetIntValue(day); CFRelease(day); }
				if (hour) { gregory.hour = CFStringGetIntValue(hour); CFRelease(hour); }
				if (minute) { gregory.minute = CFStringGetIntValue(minute); CFRelease(minute); }
				if (second) { gregory.second = (double)CFStringGetIntValue(second); CFRelease(second); }
				if (tenth) { gregory.second += ((double)CFStringGetIntValue(tenth)/(double)10.0); CFRelease(tenth); }

				CFTimeZoneRef tz = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, 0);
				if (tz) {
					CFAbsoluteTime at = CFGregorianDateGetAbsoluteTime(gregory, tz);
					result = CFDateCreate(NULL, at);
					CFRelease(tz);
				}
			}
			CFRelease(str);
		}
	}
	return result;
}


// generic parser for XML nodes in our ticket format
static void _parseXMLNode(const void *value, void *context) {
	CFXMLTreeRef tree = (CFXMLTreeRef)value;
	struct parseState* state = (struct parseState*)context;

	CFXMLNodeRef node = CFXMLTreeGetNode(tree);
	assert(node);

	CFDictionaryRef namespaces = copyNamespacesForNode(state->namespaces, node);
	
	CFXMLNodeTypeCode type = CFXMLNodeGetTypeCode(node);
	
	int descend = 0;
	
	if (type == kCFXMLNodeTypeElement) {
		CFStringRef ns, name;
		copyNodeNamespaceAndName(namespaces, node, &ns, &name);
		
		if (ns && name) {
		
			if (CFEqual(ns, SD_XML_NAMESPACE)) {
				if (CFEqual(name, SD_XML_ROOT)) {
					
					descend = 1;

				} else if (CFEqual(name, SD_XML_RESOURCE)) {
				
					state->plist = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
					if (state->plist) {
						CFArrayAppendValue(state->plists, state->plist);
						CFRelease(state->plist);
					}
				
					CFStringRef name = copyChildWithNameAsString(tree, namespaces, SD_XML_NAMESPACE, SD_XML_NAME);
					if (name && state->plist) {
						CFDictionarySetValue(state->plist, SD_XML_NAME, name);
						CFRelease(name);
					}
								
					CFNumberRef size = copyChildWithNameAsInteger(tree, namespaces, SD_XML_NAMESPACE, SD_XML_SIZE);
					if (size && state->plist) {
						CFDictionarySetValue(state->plist, SD_XML_SIZE, size);
						CFRelease(size);
					}

					CFDateRef created = copyChildWithNameAsDate(tree, namespaces, SD_XML_NAMESPACE, SD_XML_CREATED);
					if (created && state->plist) {
						CFDictionarySetValue(state->plist, SD_XML_CREATED, created);
						CFRelease(created);
					}
					
					descend = 1;

				} else if (CFEqual(name, SD_XML_SITES)) {
					CFArrayRef urls = copyChildrenWithNameAsURLs(tree, namespaces, SD_XML_NAMESPACE, SD_XML_URL);
					if (urls && state->plist) {
						CFDictionarySetValue(state->plist, SD_XML_URL, urls);
						CFRelease(urls);
					}
					
				} else if (CFEqual(name, SD_XML_VERIFICATIONS)) {
					CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
					if (dict && state->plist) {
						CFDictionarySetValue(state->plist, SD_XML_VERIFICATIONS, dict);
						CFRelease(dict);
						descend = 1;
					}
			
				} else if (CFEqual(name, SD_XML_VERIFICATION) && state->plist) {
					CFMutableDictionaryRef verifications = (CFMutableDictionaryRef)CFDictionaryGetValue(state->plist, SD_XML_VERIFICATIONS);
					CFXMLElementInfo* info = (CFXMLElementInfo*)CFXMLNodeGetInfoPtr(node);
					if (verifications && info && info->attributes) {
						CFStringRef algorithm = CFDictionaryGetValue(info->attributes, SD_XML_ATTR_ALGORITHM);
						if (algorithm) {
							CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
							if (dict) {
								CFXMLTreeRef child;
								#pragma unused(child)
	
								CFNumberRef sector_size = copyChildWithNameAsInteger(tree, namespaces, SD_XML_NAMESPACE, SD_XML_SECTOR_SIZE);
								if (sector_size) {
									CFDictionarySetValue(dict, SD_XML_SECTOR_SIZE, sector_size);
									CFRelease(sector_size);
								}

								CFDataRef digest = copyChildWithNameAsData(tree, namespaces, SD_XML_NAMESPACE, SD_XML_DIGEST);
								if (digest) {
									CFDictionarySetValue(dict, SD_XML_DIGEST, digest);
									CFRelease(digest);
								}

								CFDataRef digests = copyChildWithNameAsData(tree, namespaces, SD_XML_NAMESPACE, SD_XML_DIGESTS);
								if (digests) {
									CFDictionarySetValue(dict, SD_XML_DIGESTS, digests);
									CFRelease(digests);
								}
							
								CFDictionarySetValue(verifications, algorithm, dict);
								CFRelease(dict);
							}
						}
					}
				}
			}
#if LOCAL_DEBUG
			cfprintf(stderr, "%sELEM:\t%@\t[%@]\n", state->prefix, name, ns);
#endif
		}
		if (ns) CFRelease(ns);
		if (name) CFRelease(name);
	} else if (type == kCFXMLNodeTypeWhitespace) {
		// do nothing
	} else {
#if LOCAL_DEBUG
		CFStringRef str = CFXMLNodeGetString(node);
		cfprintf(stderr, "%s% 4d:\t%@\n", state->prefix, type, str);
#endif
	}
	
	// only recurse further if we have been specifically instructed to
	// do so.
	if (descend) {
		struct parseState local;
		memcpy(&local, state, sizeof(struct parseState));
		local.namespaces = namespaces;
#if LOCAL_DEBUG
		asprintf(&local.prefix, "%s  ", state->prefix);
#endif
		CFTreeApplyFunctionToChildren(tree, _parseXMLNode, &local);
#if LOCAL_DEBUG
		free(local.prefix);
#endif
	}
	if (namespaces) CFRelease(namespaces);
}

CFPropertyListRef _SecureDownloadParseTicketXML(CFDataRef xmlData) {
	if (!xmlData) return NULL;
	CFURLRef url = NULL;

	CFXMLTreeRef tree = CFXMLTreeCreateFromData(NULL, xmlData, url, kCFXMLParserNoOptions, kCFXMLNodeCurrentVersion);
	if (!tree) return NULL;
	
	struct parseState state;
	memset(&state, 0, sizeof(state));
#if LOCAL_DEBUG
	state.prefix = "";
#endif
	state.plists = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (state.plists) {
		CFTreeApplyFunctionToChildren(tree, _parseXMLNode, &state);
	}
	CFRelease(tree);

	CFPropertyListRef result = NULL;
	// For now, only return the first resource encountered
	if (state.plists && CFArrayGetCount(state.plists) > 0) {
		result = CFArrayGetValueAtIndex(state.plists, 0);
		CFRetain(result);
	}
	if (state.plists) CFRelease(state.plists);
	return result;
}

static void _appendCString(CFMutableDataRef data, const char* cstring) {
	CFDataAppendBytes(data, (UInt8*)cstring, strlen(cstring));
}

static void _appendCFString(CFMutableDataRef data, CFStringRef string) {
	CFDataRef utf8 = CFStringCreateExternalRepresentation(NULL, string, kCFStringEncodingUTF8, '?');
	if (utf8) {
		CFDataAppendBytes(data, CFDataGetBytePtr(utf8), CFDataGetLength(utf8));
		CFRelease(utf8);
	}
}

static void _appendCFNumber(CFMutableDataRef data, CFNumberRef number) {
	CFStringRef str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@"), number);
	if (str) {
		_appendCFString(data, str);
		CFRelease(str);
	}
}

static void _appendCFURL(CFMutableDataRef data, CFURLRef url) {
	CFURLRef abs = CFURLCopyAbsoluteURL(url);
	if (abs) {
		CFStringRef str = CFURLGetString(abs);
		if (str) {
			_appendCFString(data, str);
		}
		CFRelease(abs);
	}
}

// appends data in base64 encoded form
static void _appendCFData(CFMutableDataRef data, CFDataRef moreData) {
	CFStringRef str = encodeBase64String(CFDataGetBytePtr(moreData), CFDataGetLength(moreData), 0);
	if (str) {
		_appendCFString(data, str);
		CFRelease(str);
	}
}

// appends a date in W3C DateTime format
static void _appendCFDate(CFMutableDataRef data, CFDateRef date) {
	CFLocaleRef locale = CFLocaleCreate(NULL, CFSTR("en_US"));
	if (locale) {
		CFDateFormatterRef formatter = CFDateFormatterCreate(NULL, locale, kCFDateFormatterNoStyle, kCFDateFormatterNoStyle);
		if (formatter) {
			CFDateFormatterSetFormat(formatter, CFSTR("yyyy-MM-dd'T'HH:mm:ss.S'Z'"));
			CFTimeZoneRef tz = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, 0);
			if (tz) {
				CFDateFormatterSetProperty(formatter, kCFDateFormatterTimeZone, tz);
				CFStringRef str = CFDateFormatterCreateStringWithDate(NULL, formatter, date);
				if (str) {
					_appendCFString(data, str);
					CFRelease(str);
				}
				CFRelease(tz);
			}
			CFRelease(formatter);
		}
		CFRelease(locale);
	}
}

static CFArrayRef dictionaryGetSortedKeys(CFDictionaryRef dictionary) {
        CFIndex count = CFDictionaryGetCount(dictionary);

        const void** keys = malloc(sizeof(CFStringRef) * count);
        CFDictionaryGetKeysAndValues(dictionary, keys, NULL);
        CFArrayRef keysArray = CFArrayCreate(NULL, keys, count, &kCFTypeArrayCallBacks);
        CFMutableArrayRef sortedKeys = CFArrayCreateMutableCopy(NULL, count, keysArray);
        CFRelease(keysArray);
        free(keys);

        CFArraySortValues(sortedKeys, CFRangeMake(0, count), (CFComparatorFunction)CFStringCompare, 0);
        return sortedKeys;
}

CFDataRef _SecureDownloadCreateTicketXML(CFPropertyListRef plist) {
	CFMutableDataRef data = CFDataCreateMutable(NULL, 0);
	if (!data) return NULL;
	
	_appendCString(data, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	_appendCString(data, "<SecureDownload xmlns=\"http://www.apple.com/2006/SecureDownload/1\">\n");
	_appendCString(data, "    <resource>\n");
	
	CFStringRef name = CFDictionaryGetValue(plist, SD_XML_NAME);
	if (name) {
		_appendCString(data, "\t<name>");
		_appendCFString(data, name);
		_appendCString(data, "</name>\n");
	}
	
	CFNumberRef num = CFDictionaryGetValue(plist, SD_XML_SIZE);
	if (num) {
		_appendCString(data, "\t<size>");
		_appendCFNumber(data, num);
		_appendCString(data, "</size>\n");
	}
	
	CFDateRef created = CFDictionaryGetValue(plist, SD_XML_CREATED);
	if (created) {
		_appendCString(data, "\t<created>");
		_appendCFDate(data, created);
		_appendCString(data, "</created>\n");
	}
	
	_appendCString(data, "\t<sites>\n");
	CFArrayRef urls = CFDictionaryGetValue(plist, SD_XML_URL);
	if (urls) {
		CFIndex i, count = CFArrayGetCount(urls);
		for (i = 0; i < count; ++i) {
			_appendCString(data, "\t\t<url>");
			_appendCFURL(data, CFArrayGetValueAtIndex(urls, i));
			_appendCString(data, "</url>\n");
		}
	}
	_appendCString(data, "\t</sites>\n");
	
	CFDictionaryRef verifications = CFDictionaryGetValue(plist, SD_XML_VERIFICATIONS);
	if (verifications) {
		_appendCString(data, "\t<verifications>\n");
		CFArrayRef algorithms = dictionaryGetSortedKeys(verifications);
		if (algorithms) {
			CFIndex i, count = CFArrayGetCount(algorithms);
			for (i = 0; i < count; ++i) {
				CFStringRef algorithm = CFArrayGetValueAtIndex(algorithms, i);
				if (algorithm) {
					_appendCString(data, "\t\t<verification algorithm=\"");
					_appendCFString(data, algorithm);
					_appendCString(data, "\">\n");					
					CFDictionaryRef dict = CFDictionaryGetValue(verifications, algorithm);
					if (dict) {
						CFDataRef digest = CFDictionaryGetValue(dict, SD_XML_DIGEST);
						if (digest) {
							_appendCString(data, "\t\t\t<digest>");
							_appendCFData(data, digest);
							_appendCString(data, "</digest>\n");
						}
						
						CFNumberRef sector_size = CFDictionaryGetValue(dict, SD_XML_SECTOR_SIZE);
						if (sector_size) {
							_appendCString(data, "\t\t\t<sector_size>");
							_appendCFNumber(data, sector_size);
							_appendCString(data, "</sector_size>\n");
						}
						
						CFDataRef digests = CFDictionaryGetValue(dict, SD_XML_DIGESTS);
						if (digest) {
							_appendCString(data, "\t\t\t<digests>");
							_appendCFData(data, digests);
							_appendCString(data, "</digests>\n");
						}
					}
					_appendCString(data, "\t\t</verification>\n");
				}
			}
			CFRelease(algorithms);
		}
		_appendCString(data, "\t</verifications>\n");
	}
	
	_appendCString(data, "    </resource>\n");
	_appendCString(data, "</SecureDownload>\n");

	return data;
}

#if LOCAL_DEBUG
#include <unistd.h>
int main(int argc, char* argv[]) {

	CFDataRef data = read_data("/Users/kevin/Desktop/SecureDownloadXML/SecureDownload.xml");
	if (data) {
		CFPropertyListRef plist = _SecureDownloadParseTicketXML(data);
		CFShow(plist);
		if (plist) {
			CFDataRef output = _SecureDownloadCreateTicketXML(plist);
			if (output) {
				write(STDOUT_FILENO, CFDataGetBytePtr(output), CFDataGetLength(output));
				CFRelease(output);
			}
			CFRelease(plist);
		}
		CFRelease(data);
	}
	
	// help look for leaks
	cfprintf(stderr, "pid = %d\n", getpid());
	sleep(1000);
}
#endif
