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
 * SOSViewQueries.c -  Implementation of a manifest caching views and children
 */

#include <Security/SecureObjectSync/SOSViewQueries.h>

// Views for backup
const CFStringRef kSOSViewiCloudBackupV0        = CFSTR("iCloudBackupV0");
const CFStringRef kSOSViewiTunesBackupV0        = CFSTR("iTunesBackupV0");

// Query syntax:
// orquery ::=  (or andquery,...)
// andquery ::= (and attrquery, ...)
// attrquery ::= (attrname optail)
// optail ::= [ null | any | true | false | eq value... | neq value... ]
// attrname ::= string
// value ::= string | data | number | digest
// string ::= identifer
// number ::= [0-9]+
// digest ::= hexdata
// hexdata ::= X' hexwords... '
// hexwords ::= hexdigit hexdigit
// hexdigit ::= [0-9A-F]

// Keychain Sync View Queries
const CFStringRef kSOSViewQueryKeychainV0            =
CFSTR("(and (class eq genp inet keys) (sync true) (pdmn ak ck dk aku cku dku) (tkid null) (vwht null) (any genpv6 inetv6 keysv6)");
const CFStringRef kSOSViewQueryKeychainV2            =
CFSTR("(and (class eq genp inet keys cert) (sync true) (pdmn ak ck dk aku cku dku) (vwht null) (all genpv8 ))");

// Backup View Queries
const CFStringRef kSOSViewQueryiCloudBackupV0            =
CFSTR("(and (class eq genp inet keys cert) (sync true) (pdmn ak ck dk aku cku dku) (tkid null) (vwht null))");
const CFStringRef kSOSViewQueryiCloudBackupV2              =
CFSTR("(and (class eq genp inet keys cert) (pdmn ak ck dk aku cku dku) (tkid null) (vwht null))");
const CFStringRef kSOSViewQueryiTunesBackupV0              =
CFSTR("(and (class eq genp inet keys cert) (pdmn ak ck dk aku cku dku) (tkid null) (vwht null))");
const CFStringRef kSOSViewQueryiTunesBackupV2              =
CFSTR("(and (class eq genp inet keys cert) (pdmn ak ck dk aku cku dku) (tkid null) (vwht null))");

// General View Queries
const CFStringRef kSOSViewQueryAppleTV               =
CFSTR("(and (class eq genp inet keys cert) (sync true) (pdmn ak ck dk aku cku dku) (vwht eq AppleTV))");
const CFStringRef kSOSViewQueryHomeKit               =
CFSTR("(and (class eq genp inet keys cert) (sync true) (pdmn ak ck dk aku cku dku) (vwht eq HomeKit))");

// PCS View Queries
const CFStringRef kSOSViewQueryPCSMasterKey          =
CFSTR("(and (class eq genp inet keys cert) (sync true) (pdmn ak ck dk aku cku dku) (vwht eq PCS-MasterKey))");
const CFStringRef kSOSViewQueryPCSiCloudDrive        =
CFSTR("(and (class eq genp inet keys cert) (sync true) (pdmn ak ck dk aku cku dku) (vwht eq PCS-iCloudDrive))");
const CFStringRef kSOSViewQueryPCSPhotos             =
CFSTR("(and (class eq genp inet keys cert) (sync true) (pdmn ak ck dk aku cku dku) (vwht eq PCS-Photos))");
const CFStringRef kSOSViewQueryPCSCloudKit           =
CFSTR("(and (class eq genp inet keys cert) (sync true) (pdmn ak ck dk aku cku dku) (vwht eq PCS-CloudKit))");
const CFStringRef kSOSViewQueryPCSEscrow             =
CFSTR("(and (class eq genp inet keys cert) (sync true) (pdmn ak ck dk aku cku dku) (vwht eq PCS-Escrow))");
const CFStringRef kSOSViewQueryPCSFDE                =
CFSTR("(and (class eq genp inet keys cert) (sync true) (pdmn ak ck dk aku cku dku) (vwht eq PCS-FDE))");
const CFStringRef kSOSViewQueryPCSMailDrop           =
CFSTR("(and (class eq genp inet keys cert) (sync true) (pdmn ak ck dk aku cku dku) (vwht eq PCS-Maildrop))");
const CFStringRef kSOSViewQueryPCSiCloudBackup       =
CFSTR("(and (class eq genp inet keys cert) (sync true) (pdmn ak ck dk aku cku dku) (vwht eq PCS-Backup))");
const CFStringRef kSOSViewQueryPCSNotes              =
CFSTR("(and (class eq genp inet keys cert) (sync true) (pdmn ak ck dk aku cku dku) (vwht eq PCS-Notes))");
const CFStringRef kSOSViewQueryPCSiMessage           =
CFSTR("(and (class eq genp inet keys cert) (sync true) (pdmn ak ck dk aku cku dku) (vwht eq PCS-iMessage))");
const CFStringRef kSOSViewQueryPCSFeldspar           =
CFSTR("(and (class eq genp inet keys cert) (sync true) (pdmn ak ck dk aku cku dku) (vwht eq PCS-Feldspar))");

