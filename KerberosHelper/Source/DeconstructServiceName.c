/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

/* NOTE:  This code is a *copy* of code that exists in SMB, AFP, ScreenSharing and CFNetwork.
 * These routines are not exported and therefore have to be copied.
 * This code should be removed when realy API / SPI exists
 */
 
#include <dns_sd.h>
#include <CoreFoundation/CoreFoundation.h>

#define MAX_DOMAIN_LABEL 63
#define MAX_DOMAIN_NAME 255
#define MAX_ESCAPED_DOMAIN_NAME 1005

typedef struct { UInt8 c[ 64]; } domainlabel;   // One label: length byte and up to 63 characters
typedef struct { UInt8 c[256]; } domainname;    // Up to 255 bytes of length-prefixed domainlabels


// Returns length of a domain name INCLUDING the byte for the final null label
// e.g. for the root label "." it returns one
// For the FQDN "com." it returns 5 (length byte, three data bytes, final zero)
// Legal results are 1 (just root label) to 255 (MAX_DOMAIN_NAME)
// If the given domainname is invalid, result is 256 (MAX_DOMAIN_NAME+1)
static UInt16 DomainNameLengthLimit(const domainname *const name, const UInt8 *limit) {
    const UInt8 *src = name->c;
    while (src < limit && *src <= MAX_DOMAIN_LABEL) {
	if (*src == 0) return((UInt16)(src - name->c + 1));
	src += 1 + *src;
    }
    return(MAX_DOMAIN_NAME+1);
}

#define DomainNameLength(name) DomainNameLengthLimit((name), (name)->c + MAX_DOMAIN_NAME + 1)
#define mdnsIsDigit(X)     ((X) >= '0' && (X) <= '9')

// AppendDNSNameString appends zero or more labels to an existing (possibly empty) domainname.
// The C string is in conventional DNS syntax:
// Textual labels, escaped as necessary using the usual DNS '\' notation, separated by dots.
// If successful, AppendDNSNameString returns a pointer to the next unused byte
// in the domainname bufer (i.e. the next byte after the terminating zero).
// If unable to construct a legal domain name (i.e. label more than 63 bytes, or total more than 255 bytes)
// AppendDNSNameString returns NULL.
static UInt8 *AppendDNSNameString(domainname *const name, const char *cstring) {
    const char *cstr = cstring;
    UInt8 *ptr = name->c + DomainNameLength(name) - 1;              // Find end of current name
    const UInt8 *const lim = name->c + MAX_DOMAIN_NAME - 1;         // Limit of how much we can add (not counting final zero)
    while (*cstr && ptr < lim)                                      // While more characters, and space to put them...
	    {
	    UInt8 *lengthbyte = ptr++;                              // Record where the length is going to go
	    if (*cstr == '.') { /* fprintf(stderr, "AppendDNSNameString: Illegal empty label in name \"%s\"", cstring); */ return(NULL); }
	    while (*cstr && *cstr != '.' && ptr < lim)              // While we have characters in the label...
		    {
		    UInt8 c = (UInt8)*cstr++;                       // Read the character
		    if (c == '\\')                                  // If escape character, check next character
			    {
			    c = (UInt8)*cstr++;                     // Assume we'll just take the next character
			    if (mdnsIsDigit(c) && mdnsIsDigit(cstr[0]) && mdnsIsDigit(cstr[1])) {  // If three decimal digits,
				    int v0 = c        - '0';        // then interpret as three-digit decimal
				    int v1 = cstr[ 0] - '0';
				    int v2 = cstr[ 1] - '0';
				    int val = v0 * 100 + v1 * 10 + v2;
				    if (val <= 255) { c = (UInt8)val; cstr += 2; }	// If valid three-digit decimal value, use it
				    }
			    }
		    *ptr++ = c;                                     // Write the character
		    }
	    if (*cstr) cstr++;                                      // Skip over the trailing dot (if present)
	    if (ptr - lengthbyte - 1 > MAX_DOMAIN_LABEL)            // If illegal label, abort
		    return(NULL);
	    *lengthbyte = (UInt8)(ptr - lengthbyte - 1);            // Fill in the length byte
	    }

    *ptr++ = 0;                                                     // Put the null root label on the end
    if (*cstr) return(NULL);                                        // Failure: We didn't successfully consume all input
    else return(ptr);                                               // Success: return new value of ptr
}
	

static char *ConvertDomainLabelToCString_withescape(const domainlabel *const label, char *ptr, char esc) {
    const UInt8 *src = label->c;                             // Domain label we're reading
    const UInt8 len = *src++;                                // Read length of this (non-null) label
    const UInt8 *const end = src + len;                      // Work out where the label ends
    if (len > MAX_DOMAIN_LABEL) return(NULL);                // If illegal label, abort
    while (src < end) {                                      // While we have characters in the label
	UInt8 c = *src++;
	if (esc) {
	    if (c == '.' || c == esc)                        // If character is a dot or the escape character
		*ptr++ = esc;                                // Output escape character
	    else if (c <= ' ') {                             // If non-printing ascii,
                                                             // Output decimal escape sequence
		*ptr++ = esc;
		*ptr++ = (char)  ('0' + (c / 100)     );
		*ptr++ = (char)  ('0' + (c /  10) % 10);
		c      = (UInt8)('0' + (c      ) % 10);
	    }
	}
	*ptr++ = (char)c;                                    // Copy the character
    }
    *ptr = 0;                                                // Null-terminate the string
    return(ptr);                                             // and return
}

// Note: To guarantee that there will be no possible overrun, cstr must be at least MAX_ESCAPED_DOMAIN_NAME (1005 bytes)
static char *ConvertDomainNameToCString_withescape(const domainname *const name, char *ptr, char esc) {
    const UInt8 *src = name->c;                              // Domain name we're reading
    const UInt8 *const max = name->c + MAX_DOMAIN_NAME;      // Maximum that's valid

    if (*src == 0) *ptr++ = '.';                             // Special case: For root, just write a dot

    while (*src) {                                           // While more characters in the domain name
	if (src + 1 + *src >= max) return(NULL);
	ptr = ConvertDomainLabelToCString_withescape((const domainlabel *)src, ptr, esc);
	if (!ptr) return(NULL);
	src += 1 + *src;
	*ptr++ = '.';                                        // Write the dot after the label
    }

    *ptr++ = 0;                                              // Null-terminate the string
    return(ptr);                                             // and return
}


#define ConvertDomainLabelToCString_unescaped(D,C)  ConvertDomainLabelToCString_withescape((D), (C), 0)
#define ConvertDomainLabelToCString(D,C)            ConvertDomainLabelToCString_withescape((D), (C), '\\')
#define ConvertDomainNameToCString_unescaped(D,C)   ConvertDomainNameToCString_withescape((D), (C), 0)
#define ConvertDomainNameToCString(D,C)             ConvertDomainNameToCString_withescape((D), (C), '\\')


// MakeDomainNameFromDNSNameString makes a native DNS-format domainname from a C string.
// The C string is in conventional DNS syntax:
// Textual labels, escaped as necessary using the usual DNS '\' notation, separated by dots.
// If successful, MakeDomainNameFromDNSNameString returns a pointer to the next unused byte
// in the domainname bufer (i.e. the next byte after the terminating zero).
// If unable to construct a legal domain name (i.e. label more than 63 bytes, or total more than 255 bytes)
// MakeDomainNameFromDNSNameString returns NULL.
static UInt8 *MakeDomainNameFromDNSNameString(domainname *const name, const char *cstr) {
    name->c[0] = 0;                                   // Make an empty domain name
    return(AppendDNSNameString(name, cstr));          // And then add this string to it
}


#define ValidTransportProtocol(X) ( (X)[0] == 4 && (X)[1] == '_' && \
	((((X)[2] | 0x20) == 'u' && ((X)[3] | 0x20) == 'd') || (((X)[2] | 0x20) == 't' && ((X)[3] | 0x20) == 'c')) && \
	((X)[4] | 0x20) == 'p')

// A service name has the form: instance.application-protocol.transport-protocol.domain
// DeconstructServiceName is currently fairly forgiving: It doesn't try to enforce character
// set or length limits for the protocol names, and the final domain is allowed to be empty.
// However, if the given FQDN doesn't contain at least three labels,
// DeconstructServiceName will reject it and return false.
static Boolean DeconstructServiceName(const domainname *const fqdn, domainlabel *const name, domainname *const type, domainname *const domain) {
    int i, len;
    const UInt8 *src = fqdn->c;
    const UInt8 *max = fqdn->c + MAX_DOMAIN_NAME;
    UInt8 *dst;

    dst = name->c;    // Extract the service name
    len = *src;
    if (!len)                         { /*fprintf(stderr, "DeconstructServiceName: FQDN empty!");*/                               return(false); }
    if (len > MAX_DOMAIN_LABEL)       { /*fprintf(stderr, "DeconstructServiceName: Instance name too long");*/                    return(false); }
    for (i=0; i<=len; i++) *dst++ = *src++;

    dst = type->c;    // Extract the service type
    len = *src;
    if (!len)                         { /*fprintf(stderr, "DeconstructServiceName: FQDN contains only one label!");*/             return(false); }
    if (len > MAX_DOMAIN_LABEL)       { /*fprintf(stderr, "DeconstructServiceName: Application protocol name too long");*/        return(false); }
    if (src[1] != '_')                { /*fprintf(stderr, "DeconstructServiceName: No _ at start of application protocol");*/     return(false); }
    for (i=0; i<=len; i++) *dst++ = *src++;

    len = *src;
    if (!len)                          { /*fprintf(stderr, "DeconstructServiceName: FQDN contains only two labels!");*/           return(false); }
    if (!ValidTransportProtocol(src))  { /*fprintf(stderr, "DeconstructServiceName: Transport protocol must be _udp or _tcp");*/  return(false); }
    for (i=0; i<=len; i++) *dst++ = *src++;
    *dst++ = 0;       // Put terminator on the end of service type

    dst = domain->c;  // Extract the service domain
    while (*src) {
	len = *src;
	if (len > MAX_DOMAIN_LABEL)    { /*fprintf(stderr, "DeconstructServiceName: Label in service domain too long");*/         return(false); }
	if (src + 1 + len + 1 >= max)  { /*fprintf(stderr, "DeconstructServiceName: Total service domain too long");*/            return(false); }
	for (i=0; i<=len; i++) *dst++ = *src++;
    }
    *dst++ = 0;      // Put the null root label on the end

    return(true);
}


static void mDNSServiceCallBack(
								  DNSServiceRef serviceRef,
								  DNSServiceFlags flags,
								  uint32_t interface,
								  DNSServiceErrorType errorCode,
								  const char *fullname,
								  const char *hostTarget,
								  uint16_t	port,
								  uint16_t	txtlen,
								  const unsigned char *txtRecord,
								  void *ctx
								  )
{
	char **hostname = (char **)ctx;
	
	if (errorCode == kDNSServiceErr_NoError && hostname) {
		*hostname = strdup (hostTarget);
	}
}

/*
 * Some glue logic specifically for KerberosHelper, based on _CFNetServiceDeconstructServiceName
 */

Boolean
_CFNetServiceDeconstructServiceName(CFStringRef inHostName, char **inHostNameString)
{

	Boolean result = false;
	char serviceNameStr[MAX_ESCAPED_DOMAIN_NAME];
	DNSServiceRef serviceRef = NULL;

	if (CFStringGetCString(inHostName, serviceNameStr, MAX_ESCAPED_DOMAIN_NAME, kCFStringEncodingUTF8)) {
		domainname domainName;

		if (MakeDomainNameFromDNSNameString(&domainName, serviceNameStr)) {
			domainlabel nameLabel;
			domainname typeDomain;
			domainname domainDomain;

			if (DeconstructServiceName(&domainName, &nameLabel, &typeDomain, &domainDomain)) {
				char namestr   [MAX_DOMAIN_LABEL+1];
				char typestr   [MAX_ESCAPED_DOMAIN_NAME];
				char domainstr [MAX_ESCAPED_DOMAIN_NAME];
				DNSServiceErrorType error;
				
				ConvertDomainLabelToCString_unescaped(&nameLabel, namestr);
				ConvertDomainNameToCString(&typeDomain, typestr);
				ConvertDomainNameToCString(&domainDomain, domainstr);

				error = DNSServiceResolve (&serviceRef,
										   0,	// No flags
										   0,	// All network interfaces
										   namestr,
										   typestr,
										   domainstr,	// domain
										   (DNSServiceResolveReply) mDNSServiceCallBack,
										   inHostNameString);

				if (kDNSServiceErr_NoError != error) {
					goto Error;
				}

				error = DNSServiceProcessResult(serviceRef);
				if (kDNSServiceErr_NoError != error) {
					goto Error;
				}
				
				result = true;
			}
		}
	}
Error:
	if (serviceRef) { DNSServiceRefDeallocate(serviceRef); }

	return result;
}
