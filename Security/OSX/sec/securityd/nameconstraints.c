/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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
 * nameconstraints.c - rfc5280 section 4.2.1.10 and later name constraints implementation.
 */

#include "nameconstraints.h"
#include <AssertMacros.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecCertificateInternal.h>
#include <securityd/SecPolicyServer.h>
#include <libDER/asn1Types.h>

/* RFC 5280 Section 4.2.1.10:
 DNS name restrictions are expressed as host.example.com.  Any DNS
 name that can be constructed by simply adding zero or more labels to
 the left-hand side of the name satisfies the name constraint.  For
 example, www.host.example.com would satisfy the constraint but
 host1.example.com would not.
*/
static bool SecDNSNameConstraintsMatch(CFStringRef DNSName, CFStringRef constraint) {
    CFIndex clength = CFStringGetLength(constraint);
    CFIndex dlength = CFStringGetLength(DNSName);

    if (dlength < clength) return false;

    /* Ensure that character to the left of the constraint in the DNSName is a '.'
     so that badexample.com does not match example.com, but good.example.com does.
     */
    if ((dlength != clength) &&
        ('.' != CFStringGetCharacterAtIndex(DNSName, dlength - clength -1))) {
        return false;
    }

    CFRange compareRange = { dlength - clength, clength};

    if (!CFStringCompareWithOptions(DNSName, constraint, compareRange, kCFCompareCaseInsensitive)) {
        return true;
    }

    return false;
}

/* RFC 5280 Section 4.2.1.10:
 For URIs, the constraint applies to the host part of the name.  The
 constraint MUST be specified as a fully qualified domain name and MAY
 specify a host or a domain.  Examples would be "host.example.com" and
 ".example.com".  When the constraint begins with a period, it MAY be
 expanded with one or more labels.  That is, the constraint
 ".example.com" is satisfied by both host.example.com and
 my.host.example.com.  However, the constraint ".example.com" is not
 satisfied by "example.com".  When the constraint does not begin with
 a period, it specifies a host.
 */
static bool SecURIMatch(CFStringRef URI, CFStringRef hostname) {
    bool result = false;
    CFStringRef URI_hostname =  NULL;
    CFCharacterSetRef port_or_path_separator = NULL;
    /* URI must have scheme specified */
    CFRange URI_scheme = CFStringFind(URI, CFSTR("://"), 0);
    require_quiet(URI_scheme.location != kCFNotFound, out);
    
    /* Remove scheme prefix and port or resource path suffix */
    CFRange URI_hostname_range = { URI_scheme.location + URI_scheme.length,
        CFStringGetLength(URI) - URI_scheme.location - URI_scheme.length };
    port_or_path_separator = CFCharacterSetCreateWithCharactersInString(kCFAllocatorDefault, CFSTR(":/"));
    CFRange separator = {kCFNotFound, 0};
    if(CFStringFindCharacterFromSet(URI, port_or_path_separator, URI_hostname_range, 0, &separator)) {
        URI_hostname_range.length -= (CFStringGetLength(URI) - separator.location);
    }
    URI_hostname = CFStringCreateWithSubstring(kCFAllocatorDefault, URI, URI_hostname_range);
    
    /* Hostname in URI must not begin with '.' */
    require_quiet('.' != CFStringGetCharacterAtIndex(URI_hostname, 0), out);
    
    CFIndex ulength = CFStringGetLength(URI_hostname);
    CFIndex hlength = CFStringGetLength(hostname);
    require_quiet(ulength >= hlength, out);
    CFRange compare_range = { 0, hlength };
    
    /* Allow one or more preceding labels */
    if ('.' == CFStringGetCharacterAtIndex(hostname, 0)) {
        compare_range.location = ulength - hlength;
    }
    
    if(kCFCompareEqualTo == CFStringCompareWithOptions(URI_hostname,
                                                       hostname,
                                                       compare_range,
                                                       kCFCompareCaseInsensitive)) {
        result = true;
    }
    
out:
    CFReleaseNull(port_or_path_separator);
    CFReleaseNull(URI_hostname);
    return result;
}

/* RFC 5280 Section 4.2.1.10:
 A name constraint for Internet mail addresses MAY specify a
 particular mailbox, all addresses at a particular host, or all
 mailboxes in a domain.  To indicate a particular mailbox, the
 constraint is the complete mail address.  For example,
 "root@example.com" indicates the root mailbox on the host
 "example.com".  To indicate all Internet mail addresses on a
 particular host, the constraint is specified as the host name.  For
 example, the constraint "example.com" is satisfied by any mail
 address at the host "example.com".  To specify any address within a
 domain, the constraint is specified with a leading period (as with
 URIs).
 */
static bool SecRFC822NameMatch(CFStringRef emailAddress, CFStringRef constraint) {
    CFRange mailbox_range = CFStringFind(constraint,CFSTR("@"),0);

    /* Constraint specifies a particular mailbox. Perform full comparison. */
    if (mailbox_range.location != kCFNotFound) {
        if (!CFStringCompare(emailAddress, constraint, kCFCompareCaseInsensitive)) {
            return true;
        }
        else return false;
    }

    mailbox_range = CFStringFind(emailAddress, CFSTR("@"), 0);
    require_quiet(mailbox_range.location != kCFNotFound, out);
    CFRange hostname_range = {mailbox_range.location + 1,
        CFStringGetLength(emailAddress) - mailbox_range.location - 1 };

    /* Constraint specificies a particular host. Compare hostname of address. */
    if ('.' != CFStringGetCharacterAtIndex(constraint, 0)) {
        if (!CFStringCompareWithOptions(emailAddress, constraint, hostname_range, kCFCompareCaseInsensitive)) {
            return true;
        }
        else return false;
    }

    /* Constraint specificies a domain. Match hostname of address to domain name. */
    require_quiet('.' != CFStringGetCharacterAtIndex(emailAddress, mailbox_range.location +1), out);
    if (CFStringHasSuffix(emailAddress, constraint)) {
        return true;
    }

out:
    return false;
}

static bool nc_compare_directoryNames(const DERItem *certName, const DERItem *subtreeName) {
    /* Get content of certificate name and subtree name */
    DERDecodedInfo certName_content;
    require_noerr_quiet(DERDecodeItem(certName, &certName_content), out);
    
    DERDecodedInfo subtreeName_content;
    require_noerr_quiet(DERDecodeItem(subtreeName, &subtreeName_content), out);
    
    if (certName->length > subtreeName->length) {
        if(0 == memcmp(certName_content.content.data,
                       subtreeName_content.content.data,
                       subtreeName_content.content.length)) {
            return true;
        }
    }
    
out:
    return false;
}

static bool nc_compare_DNSNames(const DERItem *certName, const DERItem *subtreeName) {
    bool result = false;
    CFStringRef certName_str = CFStringCreateWithBytes(kCFAllocatorDefault,
                                                       certName->data, certName->length,
                                                       kCFStringEncodingUTF8, FALSE);
    CFStringRef subtreeName_str = CFStringCreateWithBytes(kCFAllocatorDefault,
                                                          subtreeName->data, subtreeName->length,
                                                          kCFStringEncodingUTF8, FALSE);
    require_quiet(certName_str, out);
    require_quiet(subtreeName_str, out);

    if (SecDNSNameConstraintsMatch(certName_str, subtreeName_str)) {
        result = true;
    }
    
out:
    CFReleaseNull(certName_str) ;
    CFReleaseNull(subtreeName_str);
    return result;
}

static bool nc_compare_URIs(const DERItem *certName, const DERItem *subtreeName) {
    bool result = false;
    CFStringRef certName_str = CFStringCreateWithBytes(kCFAllocatorDefault,
                                                       certName->data, certName->length,
                                                       kCFStringEncodingUTF8, FALSE);
    CFStringRef subtreeName_str = CFStringCreateWithBytes(kCFAllocatorDefault,
                                                          subtreeName->data, subtreeName->length,
                                                          kCFStringEncodingUTF8, FALSE);
    require_quiet(certName_str, out);
    require_quiet(subtreeName_str, out);
    
    if (SecURIMatch(certName_str, subtreeName_str)) {
        result = true;
    }
    
out:
    CFReleaseNull(certName_str);
    CFReleaseNull(subtreeName_str);
    return result;
}

static bool nc_compare_RFC822Names(const DERItem *certName, const DERItem *subtreeName) {
    bool result = false;
    CFStringRef certName_str = CFStringCreateWithBytes(kCFAllocatorDefault,
                                                       certName->data, certName->length,
                                                       kCFStringEncodingUTF8, FALSE);
    CFStringRef subtreeName_str = CFStringCreateWithBytes(kCFAllocatorDefault,
                                                          subtreeName->data, subtreeName->length,
                                                          kCFStringEncodingUTF8, FALSE);
    require_quiet(certName_str, out);
    require_quiet(subtreeName_str, out);
    
    if (SecRFC822NameMatch(certName_str, subtreeName_str)) {
        result = true;
    }
    
out:
    CFReleaseNull(certName_str);
    CFReleaseNull(subtreeName_str);
    return result;
}

static bool nc_compare_IPAddresses(const DERItem *certAddr, const DERItem *subtreeAddr) {
    bool result = false;
    
    /* Verify Subtree Address has correct number of bytes for IP and mask */
    require_quiet((subtreeAddr->length == 8) || (subtreeAddr->length == 32), out);
    /* Verify Cert Address has correct number of bytes for IP */
    require_quiet((certAddr->length == 4) || (certAddr->length ==16), out);
    /* Verify Subtree Address and Cert Address are the same version */
    require_quiet(subtreeAddr->length == 2*certAddr->length, out);
    
    DERByte * mask = subtreeAddr->data + certAddr->length;
    for (DERSize i = 0; i < certAddr->length; i++) {
        if((subtreeAddr->data[i] & mask[i]) != (certAddr->data[i] & mask[i])) {
            return false;
        }
    }
    return true;
    
out:
    return result;
}

typedef struct {
    bool present;
    bool isMatch;
} match_t;

typedef struct {
    const SecCEGeneralNameType gnType;
    const DERItem *cert_item;
    match_t *match;
} nc_match_context_t;

typedef struct {
    const CFArrayRef subtrees;
    match_t *match;
} nc_san_match_context_t;

static OSStatus nc_compare_subtree(void *context, SecCEGeneralNameType gnType, const DERItem *generalName) {
    nc_match_context_t *item_context = context;
    if (item_context && gnType == item_context->gnType
        && item_context->match && item_context->cert_item) {
        
        item_context->match->present = true;
        /*
         * We set isMatch such that if there are multiple subtrees of the same type, matching to any one
         * of them is considered a match.
         */
        switch (gnType) {
            case GNT_DirectoryName: {
                item_context->match->isMatch |= nc_compare_directoryNames(item_context->cert_item, generalName);
                return errSecSuccess;
            }
            case GNT_DNSName: {
                item_context->match->isMatch |= nc_compare_DNSNames(item_context->cert_item, generalName);
                return errSecSuccess;
            }
            case GNT_URI: {
                item_context->match->isMatch |= nc_compare_URIs(item_context->cert_item, generalName);
                return errSecSuccess;
            }
            case GNT_RFC822Name: {
                item_context->match->isMatch |= nc_compare_RFC822Names(item_context->cert_item, generalName);
                return errSecSuccess;
            }
            case  GNT_IPAddress: {
                item_context->match->isMatch |= nc_compare_IPAddresses(item_context->cert_item, generalName);
                return errSecSuccess;
            }
            default: {
                /* If the name form is not supported, reject the certificate. */
                return errSecInvalidCertificate;
            }
        }
    }
    
    return errSecInvalidCertificate;
}

static void nc_decode_and_compare_subtree(const void *value, void *context) {
    CFDataRef subtree = value;
    nc_match_context_t *match_context = context;
    if(subtree) {
        /* convert subtree to DERItem */
        const DERItem general_name = { (unsigned char *)CFDataGetBytePtr(subtree), CFDataGetLength(subtree) };
        DERDecodedInfo general_name_content;
        require_noerr_quiet(DERDecodeItem(&general_name, &general_name_content),out);
        
        OSStatus status = SecCertificateParseGeneralNameContentProperty(general_name_content.tag,
                                                                        &general_name_content.content,
                                                                        match_context,
                                                                        nc_compare_subtree);
        if (status == errSecInvalidCertificate) {
            secdebug("policy","can't parse general name or not a type we support");
        }
    }
out:
    return;
}

static bool isEmptySubject(CFDataRef subject) {
    const DERItem subject_der = { (unsigned char *)CFDataGetBytePtr(subject), CFDataGetLength(subject) };
    
    /* Get content of certificate name */
    DERDecodedInfo subject_content;
    require_noerr_quiet(DERDecodeItem(&subject_der, &subject_content), out);
    if (subject_content.content.length) return false;
    
out:
    return true;
}

static void nc_compare_subject_to_subtrees(CFDataRef subject, CFArrayRef subtrees, match_t *match) {
    /* An empty subject name is considered not present */
    if (isEmptySubject(subject)) {
        return;
    }
    
    CFIndex num_trees = CFArrayGetCount(subtrees);
    CFRange range = { 0, num_trees };
    const DERItem subject_der = { (unsigned char *)CFDataGetBytePtr(subject), CFDataGetLength(subject) };
    nc_match_context_t context = {GNT_DirectoryName, &subject_der, match};
    CFArrayApplyFunction(subtrees, range, nc_decode_and_compare_subtree, &context);
}

static void nc_compare_RFC822Name_to_subtrees(const void *value, void *context) {
    CFStringRef rfc822Name = value;
    nc_san_match_context_t *san_context = context;
    CFArrayRef subtrees = NULL;
    if (san_context) {
        subtrees = san_context->subtrees;
    }
    if (subtrees) {
        CFIndex num_trees = CFArrayGetCount(subtrees);
        CFRange range = { 0, num_trees };
        match_t match = { false, false };
        const DERItem addr = { (unsigned char *)CFStringGetCStringPtr(rfc822Name, kCFStringEncodingUTF8),
                              CFStringGetLength(rfc822Name) };
        nc_match_context_t match_context = {GNT_RFC822Name, &addr, &match};
        CFArrayApplyFunction(subtrees, range, nc_decode_and_compare_subtree, &match_context);
        
        /*
         * We set the SAN context match struct as follows:
         * 'present' is true if there's any subtree of the same type as any SAN
         * 'match' is false if the present type(s) is/are not supported or the subtree(s) and SAN(s) don't match.
         * Note: the state of 'match' is meaningless without 'present' also being true.
         */
        if (match.present && san_context->match) {
            san_context->match->present = true;
            san_context->match->isMatch &= match.isMatch;
        }
    }

}

static OSStatus nc_compare_subjectAltName_to_subtrees(void *context, SecCEGeneralNameType gnType, const DERItem *generalName) {
    nc_san_match_context_t *san_context = context;
    CFArrayRef subtrees = NULL;
    if (san_context) {
        subtrees = san_context->subtrees;
    }
    if (subtrees) {
        CFIndex num_trees = CFArrayGetCount(subtrees);
        CFRange range = { 0, num_trees };
        match_t match = { false, false };
        nc_match_context_t match_context = {gnType, generalName, &match};
        CFArrayApplyFunction(subtrees, range, nc_decode_and_compare_subtree, &match_context);
        
        /*
         * We set the SAN context match struct as follows:
         * 'present' is true if there's any subtree of the same type as any SAN
         * 'match' is false if the present type(s) is/are not supported or the subtree(s) and SAN(s) don't match.
         * Note: the state of 'match' is meaningless without 'present' also being true.
         */
        if (match.present && san_context->match) {
            san_context->match->present = true;
            san_context->match->isMatch &= match.isMatch;
        }
        
        return errSecSuccess;
    }
    
    return errSecInvalidCertificate;
}

OSStatus SecNameContraintsMatchSubtrees(SecCertificateRef certificate, CFArrayRef subtrees, bool *matched, bool permit) {
    CFDataRef subject = NULL;
    OSStatus status = errSecSuccess;
    CFArrayRef rfc822Names = NULL;
    
    require_action_quiet(subject = SecCertificateCopySubjectSequence(certificate),
                         out,
                         status = errSecInvalidCertificate);
    const DERItem *subjectAltNames = SecCertificateGetSubjectAltName(certificate);

    /* Reject certificates with neither Subject Name nor SubjectAltName */
    require_action_quiet(!isEmptySubject(subject) || subjectAltNames, out, status = errSecInvalidCertificate);
    
    /* Verify that the subject name is within any of the subtrees for X.500 distinguished names */
    match_t subject_match = { false, false };
    nc_compare_subject_to_subtrees(subject,subtrees,&subject_match);

    match_t san_match = { false, true };
    nc_san_match_context_t san_context = {subtrees, &san_match};
    
    /* If there are no subjectAltNames, then determine if there's a matching emailAddress in the Subject */
    if (!subjectAltNames) {
        rfc822Names = SecCertificateCopyRFC822Names(certificate);
        /* If there's also no emailAddress field then subject match is enough. */
        if (rfc822Names) {
            CFRange range = { 0 , CFArrayGetCount(rfc822Names) };
            CFArrayApplyFunction(rfc822Names, range, nc_compare_RFC822Name_to_subtrees, &san_context);
        }
    }
    else {
        /* And verify that each of the alternative names in the subjectAltName extension (critical or non-critical)
         * is within any of the subtrees for that name type. */
        status = SecCertificateParseGeneralNames(subjectAltNames,
                                                 &san_context,
                                                 nc_compare_subjectAltName_to_subtrees);
        /* If failed to parse */
        require_action_quiet(status == errSecSuccess, out, *matched = false);
    }

    /* If we are excluding based on the subtrees, lack of names of the
       same type is not a match. But if we are permitting, it is.
       This logic is unfortunately complicated and could be cleaned up with
       two separate functions for excluded and permitted subtrees.
     */
    if (subject_match.present) {
        if (san_match.present &&
            ((subject_match.isMatch && !san_match.isMatch) ||
            (!subject_match.isMatch && san_match.isMatch))) {
            *matched = permit ? false : true;
        }
        else {
            *matched = subject_match.isMatch;
        }
    }
    else if (san_match.present) {
        *matched = san_match.isMatch;
    }
    else {
        *matched = permit ? true : false;
    }
    
out:
    CFReleaseNull(subject);
    CFReleaseNull(rfc822Names);
    return status;
}

/* The recommended processing algorithm states:
 *    If permittedSubtrees is present in the certificate, set the permitted_subtrees state variable to the intersection
 *    of its previous value and the value indicated in the extension field.
 * However, in practice, certs are issued with permittedSubtrees whose intersection would be the empty set. Wherever
 * a new permittedSubtree is a subset of an existing subtree, we'll replace the existing subtree; otherwise, we just
 * append the new subtree.
 */
static void nc_intersect_tree_with_subtrees (const void *value, void *context) {
    CFDataRef new_subtree = value;
    CFMutableArrayRef *existing_subtrees = context;
    
    if (!new_subtree || !*existing_subtrees) return;
    
    /* convert new subtree to DERItem */
    const DERItem general_name = { (unsigned char *)CFDataGetBytePtr(new_subtree), CFDataGetLength(new_subtree) };
    DERDecodedInfo general_name_content;
    if(DR_Success != DERDecodeItem(&general_name, &general_name_content)) return;
    
    SecCEGeneralNameType gnType;
    DERItem *new_subtree_item = &general_name_content.content;
    
    /* Attempt to intersect if one of the supported types: DirectoryName and DNSName.
     * Otherwise, just append the new tree. 
     */
    switch (general_name_content.tag) {
        case ASN1_CONTEXT_SPECIFIC | 2: {
            gnType = GNT_DNSName;
            break;
        }
        case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 4: {
            gnType = GNT_DirectoryName;
            break;
        }
        default: {
            CFArrayAppendValue(*existing_subtrees, new_subtree);
            return;
        }
    }
    
    CFIndex subtreeIX;
    CFIndex num_existing_subtrees = CFArrayGetCount(*existing_subtrees);
    match_t match = { false, false };
    nc_match_context_t match_context = { gnType, new_subtree_item, &match};
    for (subtreeIX = 0; subtreeIX < num_existing_subtrees; subtreeIX++) {
        CFDataRef candidate_subtree = CFArrayGetValueAtIndex(*existing_subtrees, subtreeIX);
        /* Convert candidate subtree to DERItem */
        const DERItem candidate = { (unsigned char *)CFDataGetBytePtr(candidate_subtree), CFDataGetLength(candidate_subtree) };
        DERDecodedInfo candidate_content;
        /* We could probably just delete any subtrees in the array that don't decode */
        if(DR_Success != DERDecodeItem(&candidate, &candidate_content)) continue;
        
        OSStatus status = SecCertificateParseGeneralNameContentProperty(candidate_content.tag,
                                                                        &candidate_content.content,
                                                                        &match_context,
                                                                        nc_compare_subtree);
        if((status == errSecSuccess) && match.present && match.isMatch) {
            break;
        }
    }
    if (subtreeIX == num_existing_subtrees) {
        /* No matches found. Append new subtree */
        CFArrayAppendValue(*existing_subtrees, new_subtree);
    }
    else {
        CFArraySetValueAtIndex(*existing_subtrees, subtreeIX, new_subtree);
    }
    return;
    
}

void SecNameConstraintsIntersectSubtrees(CFMutableArrayRef subtrees_state, CFArrayRef subtrees_new) {
    assert(subtrees_state);
    assert(subtrees_new);
    
    CFIndex num_new_trees = CFArrayGetCount(subtrees_new);
    CFRange range = { 0, num_new_trees };
    CFArrayApplyFunction(subtrees_new, range, nc_intersect_tree_with_subtrees, &subtrees_state);
}
