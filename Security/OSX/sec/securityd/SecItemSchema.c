/*
 * Copyright (c) 2006-2014 Apple Inc. All Rights Reserved.
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
 *  SecItemSchema.c - CoreFoundation-based constants and functions for
    access to Security items (certificates, keys, identities, and
    passwords.)
 */

#include "SecItemSchema.h"
#include <securityd/SecDbKeychainItem.h>
#include <keychain/ckks/CKKS.h>

// MARK -
// MARK Keychain version 6 schema

#define __FLAGS(ARG, ...) SECDBFLAGS(__VA_ARGS__)
#define SECDBFLAGS(ARG, ...) __FLAGS_##ARG | __FLAGS(__VA_ARGS__)

#define SecDbFlags(P,L,I,S,A,D,R,C,H,B,Z,E,N,U,V,Y) (__FLAGS_##P|__FLAGS_##L|__FLAGS_##I|__FLAGS_##S|__FLAGS_##A|__FLAGS_##D|__FLAGS_##R|__FLAGS_##C|__FLAGS_##H|__FLAGS_##B|__FLAGS_##Z|__FLAGS_##E|__FLAGS_##N|__FLAGS_##U|__FLAGS_##V|__FLAGS_##Y)

#define __FLAGS_   0
#define __FLAGS_P  kSecDbPrimaryKeyFlag
#define __FLAGS_L  kSecDbInFlag
#define __FLAGS_I  kSecDbIndexFlag
#define __FLAGS_S  kSecDbSHA1ValueInFlag
#define __FLAGS_A  kSecDbReturnAttrFlag
#define __FLAGS_D  kSecDbReturnDataFlag
#define __FLAGS_R  kSecDbReturnRefFlag
#define __FLAGS_C  kSecDbInCryptoDataFlag
#define __FLAGS_H  kSecDbInHashFlag
#define __FLAGS_B  kSecDbInBackupFlag
#define __FLAGS_Z  kSecDbDefault0Flag
#define __FLAGS_E  kSecDbDefaultEmptyFlag
#define __FLAGS_N  kSecDbNotNullFlag
#define __FLAGS_U  kSecDbInAuthenticatedDataFlag
#define __FLAGS_V0 kSecDbSyncPrimaryKeyV0
#define __FLAGS_V2 (kSecDbSyncPrimaryKeyV0 | kSecDbSyncPrimaryKeyV2)
#define __FLAGS_Y  kSecDbSyncFlag

//                                                                   ,----------------- P : Part of primary key
//                                                                  / ,---------------- L : Stored in local database
//                                                                 / / ,--------------- I : Attribute wants an index in the database
//                                                                / / / ,-------------- S : SHA1 hashed attribute value in database (implies L)
//                                                               / / / / ,------------- A : Returned to client as attribute in queries
//                                                              / / / / / ,------------ D : Returned to client as data in queries
//                                                             / / / / / / ,----------- R : Returned to client as ref/persistent ref in queries
//                                                            / / / / / / / ,---------- C : Part of encrypted blob
//                                                           / / / / / / / / ,--------- H : Attribute is part of item SHA1 hash (Implied by C)
//                                                          / / / / / / / / / ,-------- B : Attribute is part of iTunes/iCloud backup bag
//                                                         / / / / / / / / / / ,------- Z : Attribute has a default value of 0
//                                                        / / / / / / / / / / / ,------ E : Attribute has a default value of "" or empty data
//                                                       / / / / / / / / / / / / ,----- N : Attribute must have a value
//                                                      / / / / / / / / / / / / / ,---- U : Attribute is stored in authenticated, but not necessarily encrypted data
//                                                     / / / / / / / / / / / / / / ,--- V0: Sync primary key version
//                                                    / / / / / / / / / / / / / / /  ,- Y : Attribute should be synced
//                                                    | | | | | | | | | | | | | | |  |
// common to all                                      | | | | | | | | | | | | | | |  |
SECDB_ATTR(v6rowid, "rowid", RowId,        SecDbFlags( ,L, , , , ,R, , ,B, , , , ,  , ), NULL, NULL);
SECDB_ATTR(v6cdat, "cdat", CreationDate,   SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), SecDbKeychainItemCopyCurrentDate, NULL);
SECDB_ATTR(v6mdat, "mdat",ModificationDate,SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), SecDbKeychainItemCopyCurrentDate, NULL);
SECDB_ATTR(v6labl, "labl", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6data, "data", EncryptedData,  SecDbFlags( ,L, , , , , , , ,B, , , , ,  , ), SecDbKeychainItemCopyEncryptedData, NULL);
SECDB_ATTR(v6agrp, "agrp", String,         SecDbFlags(P,L, , ,A, , , ,H, , , ,N,U,V0,Y), NULL, NULL);
SECDB_ATTR(v6pdmn, "pdmn", Access,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6sync, "sync", Sync,           SecDbFlags(P,L,I, ,A, , , ,H, ,Z, ,N,U,V0, ), NULL, NULL);
SECDB_ATTR(v6tomb, "tomb", Tomb,           SecDbFlags( ,L, , , , , , ,H, ,Z, ,N,U,  ,Y), NULL, NULL);
SECDB_ATTR(v6sha1, "sha1", SHA1,           SecDbFlags( ,L,I, ,A, ,R, , , , , , , ,  ,Y), SecDbKeychainItemCopySHA1, NULL);
SECDB_ATTR(v6accc, "accc", AccessControl,  SecDbFlags( , , , ,A, , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v6v_Data, "v_Data", Data,       SecDbFlags( , , , , ,D, ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6v_pk, "v_pk", PrimaryKey,     SecDbFlags( , , , , , , , , , , , , , ,  , ), SecDbKeychainItemCopyPrimaryKey, NULL);
SECDB_ATTR(v7vwht, "vwht", String,         SecDbFlags(P,L, , ,A, , , ,H, , , , ,U,V2,Y), NULL, NULL);
SECDB_ATTR(v7tkid, "tkid", String,         SecDbFlags(P,L, , ,A, , , ,H, , , , ,U,V2,Y), NULL, NULL);
SECDB_ATTR(v7utomb, "u_Tomb", UTomb,       SecDbFlags( , , , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v8musr, "musr", UUID,           SecDbFlags(P,L,I, , , , , , , , , ,N,U,  ,Y), NULL, NULL);
// genp and inet and keys                             | | | | | | | | | | | | | | |  |
SECDB_ATTR(v6crtr, "crtr", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6alis, "alis", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
// genp and inet                                      | | | | | | | | | | | | | | |  |
SECDB_ATTR(v6desc, "desc", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6icmt, "icmt", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6type, "type", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6invi, "invi", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6nega, "nega", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6cusi, "cusi", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6prot, "prot", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6scrp, "scrp", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6acct, "acct", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0,Y), NULL, NULL);
// genp only                                          | | | | | | | | | | | | | | |  |
SECDB_ATTR(v6svce, "svce", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6gena, "gena", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
// inet only                                          | | | | | | | | | | | | | | |  |
SECDB_ATTR(v6sdmn, "sdmn", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6srvr, "srvr", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6ptcl, "ptcl", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6atyp, "atyp", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6port, "port", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6path, "path", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0,Y), NULL, NULL);
// cert only                                          | | | | | | | | | | | | | |  | |
SECDB_ATTR(v6ctyp, "ctyp", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6cenc, "cenc", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6subj, "subj", Data,           SecDbFlags( ,L,I,S,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6issr, "issr", Data,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6slnr, "slnr", Data,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6skid, "skid", Data,           SecDbFlags( ,L,I,S,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6pkhh, "pkhh", Data,           SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
// cert attributes that share names with common ones but have different flags
SECDB_ATTR(v6certalis, "alis", Blob,       SecDbFlags( ,L,I,S,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
// keys only                                          | | | | | | | | | | | | | | |  |
SECDB_ATTR(v6kcls, "kcls", Number,         SecDbFlags(P,L,I,S,A, , ,C,H, ,Z, ,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6perm, "perm", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6priv, "priv", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6modi, "modi", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6klbl, "klbl", Data,           SecDbFlags(P,L,I, ,A, , ,C,H, , ,E,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6atag, "atag", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6bsiz, "bsiz", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6esiz, "esiz", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6sdat, "sdat", Date,           SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6edat, "edat", Date,           SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6sens, "sens", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6asen, "asen", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6extr, "extr", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6next, "next", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6encr, "encr", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6decr, "decr", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6drve, "drve", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6sign, "sign", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6vrfy, "vrfy", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6snrc, "snrc", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6vyrc, "vyrc", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6wrap, "wrap", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v6unwp, "unwp", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
// keys attributes that share names with common ones but have different flags
SECDB_ATTR(v6keytype, "type", Number,      SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0,Y), NULL, NULL);
SECDB_ATTR(v6keycrtr, "crtr", Number,      SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0,Y), NULL, NULL);
//                                                    | | | | | | | | | | | | | | |
SECDB_ATTR(v6version, "version", Number,   SecDbFlags(P,L, , , , , , , , , , ,N, ,  ,Y), NULL, NULL);
SECDB_ATTR(v91minor, "minor", Number,      SecDbFlags( ,L, , , , , , , , ,Z, ,N, ,  ,Y), NULL, NULL);

SECDB_ATTR(v10_1pcsservice,       "pcss",     Number,  SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v10_1pcspublickey,     "pcsk",     Blob,    SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);
SECDB_ATTR(v10_1pcspublicidentity,"pcsi",     Blob,    SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ,Y), NULL, NULL);

SECDB_ATTR(v10itemuuid,      "UUID",          String,  SecDbFlags( ,L, , , , , , , , , , , ,U,  , ), NULL, NULL);
SECDB_ATTR(v10syncuuid,      "UUID",          String,  SecDbFlags(P,L, , , , , , , , , , , ,U,  , ), NULL, NULL);
SECDB_ATTR(v10parentKeyUUID, "parentKeyUUID", String,  SecDbFlags( ,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10currentKeyUUID,"currentKeyUUID",String,  SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10wrappedkey,    "wrappedkey",    Blob,    SecDbFlags( ,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10encrypteditem, "encitem",       Blob,    SecDbFlags( ,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10gencount,      "gencount",      Number,  SecDbFlags( ,L, , , , , , , , ,Z, ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10action,        "action",        String,  SecDbFlags( ,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10state,         "state",         String,  SecDbFlags(P,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10waituntiltime, "waituntil",     String,  SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10encodedCKRecord, "ckrecord",    Blob,    SecDbFlags( ,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10_1wasCurrent,  "wascurrent",    Number,  SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10accessgroup,   "accessgroup",   String,  SecDbFlags( ,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10keyclass,      "keyclass",      String,  SecDbFlags(P,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10currentkey,    "currentkey",    Number,  SecDbFlags( ,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10ckzone,        "ckzone",        String,  SecDbFlags(P,L, , , , , , , , , , ,N,U,  , ), NULL, NULL);
SECDB_ATTR(v10ckzonecreated, "ckzonecreated", Number,  SecDbFlags( ,L, , , , , , , , ,Z, , ,N,  , ), NULL, NULL);
SECDB_ATTR(v10ckzonesubscribed,"ckzonesubscribed", Number,  SecDbFlags( ,L, , , , , , , , ,Z, ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10ratelimiter,   "ratelimiter",   Blob,    SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10changetoken,   "changetoken",   String,  SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10lastfetchtime, "lastfetch",     String,  SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10itempersistentref,"persistref", UUID,    SecDbFlags( ,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10sysbound,      "sysb",          Number,  SecDbFlags( ,L, , ,A, , ,C,H, ,Z, , , ,  , ), NULL, NULL);
SECDB_ATTR(v10encryptionver, "encver",        Number,  SecDbFlags( ,L, , , , , , , , ,Z, ,N,U,  , ), NULL, NULL);

SECDB_ATTR(v10primaryKey,    "primaryKey",    String,  SecDbFlags(P,L, , ,A, , , , , , , ,N,U,  , ), NULL, NULL);
SECDB_ATTR(v10publickeyHash, "publickeyHash", Blob,    SecDbFlags(P,L, , , , , , , , , , ,N,U,  , ), NULL, NULL);
SECDB_ATTR(v10publickey,     "publickey",     Blob,    SecDbFlags( ,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10backupData,    "backupData",    Blob,    SecDbFlags( ,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);

SECDB_ATTR(v10_1digest,      "digest",        Blob,    SecDbFlags( ,L, , , , , , , , , , ,N,U,  , ), NULL, NULL);
SECDB_ATTR(v10_1signatures,  "signatures",    Blob,    SecDbFlags( ,L, , , , , , , , , , ,N,U,  , ), NULL, NULL);
SECDB_ATTR(v10_1signerID,    "signerID",      String,  SecDbFlags( ,L, , , , , , , , , , ,N,U,  , ), NULL, NULL);
SECDB_ATTR(v10_1leafIDs,     "leafIDs",       Blob,    SecDbFlags( ,L, , , , , , , , , , ,N,U,  , ), NULL, NULL);
SECDB_ATTR(v10_1peerManIDs,  "peerManifests", Blob,    SecDbFlags( ,L, , , , , , , , , , ,N,U,  , ), NULL, NULL);
SECDB_ATTR(v10_1entryDigests,"entryDigests",  Blob,    SecDbFlags( ,L, , , , , , , , , , ,N,U,  , ), NULL, NULL);
SECDB_ATTR(v10_2currentItems,"currentItems",  Blob,    SecDbFlags( ,L, , , , , , , , , , ,N,U,  , ), NULL, NULL);
SECDB_ATTR(v10_2futureData,  "futureData",    Blob,    SecDbFlags( ,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10_2schema,      "schema",        Blob,    SecDbFlags( ,L, , , , , , , , , , ,N,U,  , ), NULL, NULL);
SECDB_ATTR(v10_1encRecord,   "ckrecord",      Blob,    SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);

SECDB_ATTR(v10_1keyArchiveHash,  "key_archive_hash", String, SecDbFlags(P,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10_1keyArchive,      "key_archive",      String, SecDbFlags(P,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10_1archivedKey,     "archived_key",     String, SecDbFlags( ,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10_1keyArchiveName, "keyarchive_name",  String, SecDbFlags( ,L, , , , , , , , , , ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10_1optionalEncodedCKRecord, "ckrecord", String, SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10_1archiveEscrowID,"archive_escrowid", String, SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);

SECDB_ATTR(v10_1itempersistentref,"persistref", UUID,  SecDbFlags( ,L,I, , , , , , , , , ,N,U,  , ), NULL, NULL);

SECDB_ATTR(v10_1currentItemUUID,"currentItemUUID",String,  SecDbFlags(P,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10_4currentItemUUID,"currentItemUUID",String,  SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10_1currentPtrIdentifier,"identifier",String,  SecDbFlags(P,L, , , , , , , , , , , , ,  , ), NULL, NULL);

SECDB_ATTR(v10_2device,      "device",        String,      SecDbFlags(P,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10_2peerid,      "peerid",        String,      SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10_2circleStatus,"circlestatus",  String,      SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10_2keyState,    "keystate",      String,      SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10_2currentTLK,  "currentTLK",    String,      SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10_2currentClassA,"currentClassA",String,      SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10_2currentClassC,"currentClassC",String,      SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);

SECDB_ATTR(v10_4lastFixup,    "lastfixup",    Number,      SecDbFlags( ,L, , , , , , , , ,Z, , ,N,  , ), NULL, NULL);

SECDB_ATTR(v10_5senderPeerID,"senderpeerid",  String,     SecDbFlags(P,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10_5recvPeerID,  "recvpeerid",    String,     SecDbFlags(P,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10_5recvPubKey,  "recvpubenckey", Blob,       SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10_5curve,       "curve",         Number,     SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10_5poisoned,    "poisoned",      Number,     SecDbFlags( ,L, , , , , , , , ,Z, ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10_5epoch,       "epoch",         Number,     SecDbFlags( ,L, , , , , , , , ,Z, ,N, ,  , ), NULL, NULL);
SECDB_ATTR(v10_5signature,   "signature",     Blob,       SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v10_5version,     "version",       Number,     SecDbFlags( ,L, , , , , , , , ,Z, ,N,U,  , ), NULL, NULL);

SECDB_ATTR(v11_1osversion,   "osversion",     String,     SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);
SECDB_ATTR(v11_1lastunlock,  "lastunlock",    String,     SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);

SECDB_ATTR(v11_2actualKeyclass, "actualKeyclass", String, SecDbFlags( ,L, , , , , , , , , , , , ,  , ), NULL, NULL);

const SecDbClass v11_2_metadatakeys_class = {
    .name = CFSTR("metadatakeys"),
    .itemclass = false,
    .attrs = {
        &v10keyclass,
        &v11_2actualKeyclass,
        &v6data,
        0
    }
};

const SecDbClass v11_1_ckdevicestate_class = {
    .name = CFSTR("ckdevicestate"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10_2device,
        &v11_1osversion,
        &v11_1lastunlock,
        &v10_2peerid,
        &v10_2circleStatus,
        &v10_2keyState,
        &v10_2currentTLK,
        &v10_2currentClassA,
        &v10_2currentClassC,
        &v10_1encRecord,
        0
    }
};

const SecDbClass v11_metadatakeys_class = {
    .name = CFSTR("metadatakeys"),
    .itemclass = false,
    .attrs = {
        &v10keyclass,
        &v6data,
        0
    }
};

const SecDbClass v10_5_tlkshare_class = {
    .name = CFSTR("tlkshare"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10syncuuid,
        &v10_5senderPeerID,
        &v10_5recvPeerID,
        &v10_5recvPubKey,
        &v10_5curve,
        &v10_5poisoned,
        &v10_5epoch,
        &v10wrappedkey,
        &v10_5signature,
        &v10_1encRecord,
        &v10_5version,
        0
    }
};


const SecDbClass v10_4_current_item_class = {
    .name = CFSTR("currentitems"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10_1currentPtrIdentifier,
        &v10_4currentItemUUID,
        &v10state,
        &v10encodedCKRecord,
        0
    }
};

const SecDbClass v10_4_ckstate_class = {
    .name = CFSTR("ckstate"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10ckzonecreated,
        &v10ckzonesubscribed,
        &v10lastfetchtime,
        &v10changetoken,
        &v10ratelimiter,
        &v10_4lastFixup,
        0
    }
};

const SecDbClass v10_3_ckdevicestate_class = {
    .name = CFSTR("ckdevicestate"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10_2device,
        &v10_2peerid,
        &v10_2circleStatus,
        &v10_2keyState,
        &v10_2currentTLK,
        &v10_2currentClassA,
        &v10_2currentClassC,
        &v10_1encRecord,
        0
    }
};

const SecDbClass v10_2_ckmanifest_class = {
    .name = CFSTR("ckmanifest"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10gencount,
        &v10_1digest,
        &v10_1signatures,
        &v10_1signerID,
        &v10_1leafIDs,
        &v10_1peerManIDs,
        &v10_2currentItems,
        &v10_2futureData,
        &v10_2schema,
        &v10_1encRecord,
        0
    }
};

const SecDbClass v10_2_pending_manifest_class = {
    .name = CFSTR("pending_manifest"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10gencount,
        &v10_1digest,
        &v10_1signatures,
        &v10_1signerID,
        &v10_1leafIDs,
        &v10_1peerManIDs,
        &v10_2currentItems,
        &v10_2futureData,
        &v10_2schema,
        &v10_1encRecord,
        0
    }
};

const SecDbClass v10_1_ckmanifest_class = {
    .name = CFSTR("ckmanifest"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10gencount,
        &v10_1digest,
        &v10_1signatures,
        &v10_1signerID,
        &v10_1leafIDs,
        &v10_1peerManIDs,
        &v10_1encRecord,
        0
    }
};

const SecDbClass v10_1_pending_manifest_class = {
    .name = CFSTR("pending_manifest"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10gencount,
        &v10_1digest,
        &v10_1signatures,
        &v10_1signerID,
        &v10_1leafIDs,
        &v10_1peerManIDs,
        &v10_1encRecord,
        0
    }
};

const SecDbClass v10_1_ckmanifest_leaf_class = {
    .name = CFSTR("ckmanifest_leaf"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10syncuuid,
        &v10_1digest,
        &v10_1entryDigests,
        &v10_1encRecord,
        0
    }
};

const SecDbClass v10_1_pending_manifest_leaf_class = {
    .name = CFSTR("pending_manifest_leaf"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10syncuuid,
        &v10_1digest,
        &v10_1entryDigests,
        &v10_1encRecord,
        0
    }
};

const SecDbClass v10_1_genp_class = {
    .name = CFSTR("genp"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6svce,
        &v6gena,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        &v10itemuuid,
        &v10sysbound,
        &v10_1pcsservice,
        &v10_1pcspublickey,
        &v10_1pcspublicidentity,
        &v10_1itempersistentref,
        0
    },
};

const SecDbClass v10_1_inet_class = {
    .name = CFSTR("inet"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6sdmn,
        &v6srvr,
        &v6ptcl,
        &v6atyp,
        &v6port,
        &v6path,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        &v10itemuuid,
        &v10sysbound,
        &v10_1pcsservice,
        &v10_1pcspublickey,
        &v10_1pcspublicidentity,
        &v10_1itempersistentref,
        0
    },
};

const SecDbClass v10_1_cert_class = {
    .name = CFSTR("cert"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6ctyp,
        &v6cenc,
        &v6labl,
        &v6certalis,
        &v6subj,
        &v6issr,
        &v6slnr,
        &v6skid,
        &v6pkhh,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        &v10itemuuid,
        &v10sysbound,
        &v10_1pcsservice,
        &v10_1pcspublickey,
        &v10_1pcspublicidentity,
        &v10_1itempersistentref,
        0
    },
};

const SecDbClass v10_1_keys_class = {
    .name = CFSTR("keys"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6kcls,
        &v6labl,
        &v6alis,
        &v6perm,
        &v6priv,
        &v6modi,
        &v6klbl,
        &v6atag,
        &v6keycrtr,
        &v6keytype,
        &v6bsiz,
        &v6esiz,
        &v6sdat,
        &v6edat,
        &v6sens,
        &v6asen,
        &v6extr,
        &v6next,
        &v6encr,
        &v6decr,
        &v6drve,
        &v6sign,
        &v6vrfy,
        &v6snrc,
        &v6vyrc,
        &v6wrap,
        &v6unwp,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        &v10itemuuid,
        &v10sysbound,
        &v10_1pcsservice,
        &v10_1pcspublickey,
        &v10_1pcspublicidentity,
        &v10_1itempersistentref,
        0
    }
};

const SecDbClass v10_0_tversion_class = {
    .name = CFSTR("tversion"),
    .itemclass = false,
    .attrs = {
        &v6rowid,
        &v6version,
        &v91minor,
        0
    }
};

const SecDbClass v10_2_outgoing_queue_class = {
    .name = CFSTR("outgoingqueue"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10syncuuid,
        &v10parentKeyUUID,
        &v10action,
        &v10state,
        &v10waituntiltime,
        &v10accessgroup,
        &v10gencount,
        &v10wrappedkey,
        &v10encrypteditem,
        &v10encryptionver,
        &v10_1optionalEncodedCKRecord,
        &v10_1pcsservice,
        &v10_1pcspublickey,
        &v10_1pcspublicidentity,
        0
    }
};

const SecDbClass v10_2_incoming_queue_class = {
    .name = CFSTR("incomingqueue"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10syncuuid,
        &v10parentKeyUUID,
        &v10action,
        &v10state,
        &v10gencount,
        &v10wrappedkey,
        &v10encrypteditem,
        &v10encryptionver,
        &v10_1optionalEncodedCKRecord,
        &v10_1pcsservice,
        &v10_1pcspublickey,
        &v10_1pcspublicidentity,
        0
    }
};


const SecDbClass v10_1_outgoing_queue_class = {
    .name = CFSTR("outgoingqueue"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10syncuuid,
        &v10parentKeyUUID,
        &v10action,
        &v10state,
        &v10waituntiltime,
        &v10accessgroup,
        &v10gencount,
        &v10wrappedkey,
        &v10encrypteditem,
        &v10encryptionver,
        &v10_1pcsservice,
        &v10_1pcspublickey,
        &v10_1pcspublicidentity,
        0
    }
};

const SecDbClass v10_1_incoming_queue_class = {
    .name = CFSTR("incomingqueue"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10syncuuid,
        &v10parentKeyUUID,
        &v10action,
        &v10state,
        &v10gencount,
        &v10wrappedkey,
        &v10encrypteditem,
        &v10encryptionver,
        &v10_1pcsservice,
        &v10_1pcspublickey,
        &v10_1pcspublicidentity,
        0
    }
};


const SecDbClass v10_0_outgoing_queue_class = {
    .name = CFSTR("outgoingqueue"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10syncuuid,
        &v10parentKeyUUID,
        &v10action,
        &v10state,
        &v10waituntiltime,
        &v10accessgroup,
        &v10gencount,
        &v10wrappedkey,
        &v10encrypteditem,
        &v10encryptionver,
        0
    }
};

const SecDbClass v10_0_incoming_queue_class = {
    .name = CFSTR("incomingqueue"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10syncuuid,
        &v10parentKeyUUID,
        &v10action,
        &v10state,
        &v10gencount,
        &v10wrappedkey,
        &v10encrypteditem,
        &v10encryptionver,
        0
    }
};

const SecDbClass v10_0_sync_key_class = {
    .name = CFSTR("synckeys"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10syncuuid,
        &v10keyclass,
        &v10currentkey,
        &v10parentKeyUUID,
        &v10state,
        &v10wrappedkey,
        &v10encodedCKRecord,
        0
    }
};

// Stores the "Current Key" records, and parentKeyUUID refers to items in the synckeys table
// Wouldn't foreign keys be nice?
const SecDbClass v10_0_current_key_class = {
    .name = CFSTR("currentkeys"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10keyclass,
        &v10currentKeyUUID,
        &v10encodedCKRecord,
        0
    }
};

const SecDbClass v10_1_current_item_class = {
    .name = CFSTR("currentitems"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10_1currentPtrIdentifier,
        &v10_1currentItemUUID,
        &v10state,
        &v10encodedCKRecord,
        0
    }
};

const SecDbClass v10_1_ckmirror_class = {
    .name = CFSTR("ckmirror"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10syncuuid,
        &v10parentKeyUUID,
        &v10gencount,
        &v10wrappedkey,
        &v10encrypteditem,
        &v10encodedCKRecord,
        &v10encryptionver,
        &v10_1wasCurrent,
        &v10_1pcsservice,
        &v10_1pcspublickey,
        &v10_1pcspublicidentity,
        0
    }
};

const SecDbClass v10_0_ckmirror_class = {
    .name = CFSTR("ckmirror"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10syncuuid,
        &v10parentKeyUUID,
        &v10gencount,
        &v10wrappedkey,
        &v10encrypteditem,
        &v10encodedCKRecord,
        &v10encryptionver,
        0
    }
};

const SecDbClass v10_0_ckstate_class = {
    .name = CFSTR("ckstate"),
    .itemclass = false,
    .attrs = {
        &v10ckzone,
        &v10ckzonecreated,
        &v10ckzonesubscribed,
        &v10lastfetchtime,
        &v10changetoken,
        &v10ratelimiter,
        0
    }
};

/* Backup table */
/* Primary keys: v10primaryKey, v8musr */
const SecDbClass v10_0_item_backup_class = {
    .name = CFSTR("item_backup"),
    .itemclass = false,
    .attrs = {
        &v6rowid,
        &v10primaryKey,     // Primary key of the original item, from v6v_pk
        &v8musr,            //
        &v6sha1,            // Hash of the original item
        &v10backupData,     // Data wrapped to backup keybag
        &v6pkhh,            // Hash of the public key of the backup bag [v10publickeyHash]
        0
    }
};

/* Backup Keybag table */
/* Primary keys: v10publickeyHash, v8musr */
const SecDbClass v10_0_backup_keybag_class = {
    .name = CFSTR("backup_keybag"),
    .itemclass = false,
    .attrs = {
        &v6rowid,
        &v10publickeyHash,  // Hash of the public key of the backup bag
        &v8musr,            //
        &v10publickey,      // Public key for the asymmetric backup bag
        &v6agrp,            // Used for backup agent
        0
    }
};

const SecDbClass v10_1_backup_keyarchive_class = {
    .name = CFSTR("backup_keyarchive"),
    .itemclass = false,
    .attrs = {
        &v10_1keyArchiveHash, // Hash of the key archive
        &v8musr,              //
        &v10_1keyArchive,     // Serialised key archive
        &v10ckzone,
        &v10_1optionalEncodedCKRecord,
        &v10_1archiveEscrowID,
        0
    }
};

const SecDbClass v10_1_current_archived_keys_class = {
    .name = CFSTR("archived_key_backup"),
    .itemclass = false,
    .attrs = {
        &v6pdmn,
        &v10syncuuid,
        &v8musr,
        &v6agrp,
        &v10_1keyArchiveHash,
        &v10_1archivedKey,
        &v10ckzone,
        &v10_1optionalEncodedCKRecord,
        &v10_1archiveEscrowID,
        0
    }
};

const SecDbClass v10_1_current_keyarchive_class = {
    .name = CFSTR("currentkeyarchives"),
    .itemclass = false,
    .attrs = {
        &v10_1keyArchiveHash,
        &v10_1keyArchiveName,
        0
    }
};

/* An identity which is really a cert + a key, so all cert and keys attrs are
 allowed. */
const SecDbClass v_identity_class = {
    .name = CFSTR("idnt"),
    .itemclass = true,
    .attrs = {
        0
    },
};

/*
 * Version 11.2
 */
const SecDbSchema v11_2_schema = {
    .majorVersion = 11,
    .minorVersion = 2,
    .classes = {
        &v10_1_genp_class,
        &v10_1_inet_class,
        &v10_1_cert_class,
        &v10_1_keys_class,
        &v10_0_tversion_class,
        &v10_2_outgoing_queue_class,
        &v10_2_incoming_queue_class,
        &v10_0_sync_key_class,
        &v10_1_ckmirror_class,
        &v10_0_current_key_class,
        &v10_4_ckstate_class,
        &v10_0_item_backup_class,
        &v10_0_backup_keybag_class,
        &v10_2_ckmanifest_class,
        &v10_2_pending_manifest_class,
        &v10_1_ckmanifest_leaf_class,
        &v10_1_backup_keyarchive_class,
        &v10_1_current_keyarchive_class,
        &v10_1_current_archived_keys_class,
        &v10_1_pending_manifest_leaf_class,
        &v10_4_current_item_class,
        &v11_1_ckdevicestate_class,
        &v10_5_tlkshare_class,
        &v11_2_metadatakeys_class,
        0
    }
};

/*
 * Version 11.1
 */
const SecDbSchema v11_1_schema = {
    .majorVersion = 11,
    .minorVersion = 1,
    .classes = {
        &v10_1_genp_class,
        &v10_1_inet_class,
        &v10_1_cert_class,
        &v10_1_keys_class,
        &v10_0_tversion_class,
        &v10_2_outgoing_queue_class,
        &v10_2_incoming_queue_class,
        &v10_0_sync_key_class,
        &v10_1_ckmirror_class,
        &v10_0_current_key_class,
        &v10_4_ckstate_class,
        &v10_0_item_backup_class,
        &v10_0_backup_keybag_class,
        &v10_2_ckmanifest_class,
        &v10_2_pending_manifest_class,
        &v10_1_ckmanifest_leaf_class,
        &v10_1_backup_keyarchive_class,
        &v10_1_current_keyarchive_class,
        &v10_1_current_archived_keys_class,
        &v10_1_pending_manifest_leaf_class,
        &v10_4_current_item_class,
        &v11_1_ckdevicestate_class,
        &v10_5_tlkshare_class,
        &v11_metadatakeys_class,
        0
    }
};

/*
 * Version 11
 */
const SecDbSchema v11_schema = {
    .majorVersion = 11,
    .minorVersion = 0,
    .classes = {
        &v10_1_genp_class,
        &v10_1_inet_class,
        &v10_1_cert_class,
        &v10_1_keys_class,
        &v10_0_tversion_class,
        &v10_2_outgoing_queue_class,
        &v10_2_incoming_queue_class,
        &v10_0_sync_key_class,
        &v10_1_ckmirror_class,
        &v10_0_current_key_class,
        &v10_4_ckstate_class,
        &v10_0_item_backup_class,
        &v10_0_backup_keybag_class,
        &v10_2_ckmanifest_class,
        &v10_2_pending_manifest_class,
        &v10_1_ckmanifest_leaf_class,
        &v10_1_backup_keyarchive_class,
        &v10_1_current_keyarchive_class,
        &v10_1_current_archived_keys_class,
        &v10_1_pending_manifest_leaf_class,
        &v10_4_current_item_class,
        &v10_3_ckdevicestate_class,
        &v10_5_tlkshare_class,
        &v11_metadatakeys_class,
        0
    }
};


/*
 * Version 10.5
 */
const SecDbSchema v10_5_schema = {
    .majorVersion = 10,
    .minorVersion = 5,
    .classes = {
        &v10_1_genp_class,
        &v10_1_inet_class,
        &v10_1_cert_class,
        &v10_1_keys_class,
        &v10_0_tversion_class,
        &v10_2_outgoing_queue_class,
        &v10_2_incoming_queue_class,
        &v10_0_sync_key_class,
        &v10_1_ckmirror_class,
        &v10_0_current_key_class,
        &v10_4_ckstate_class,
        &v10_0_item_backup_class,
        &v10_0_backup_keybag_class,
        &v10_2_ckmanifest_class,
        &v10_2_pending_manifest_class,
        &v10_1_ckmanifest_leaf_class,
        &v10_1_backup_keyarchive_class,
        &v10_1_current_keyarchive_class,
        &v10_1_current_archived_keys_class,
        &v10_1_pending_manifest_leaf_class,
        &v10_4_current_item_class,
        &v10_3_ckdevicestate_class,
        &v10_5_tlkshare_class,
        0
    }
};

/*
 * Version 10.4
 */
const SecDbSchema v10_4_schema = {
    .majorVersion = 10,
    .minorVersion = 4,
    .classes = {
        &v10_1_genp_class,
        &v10_1_inet_class,
        &v10_1_cert_class,
        &v10_1_keys_class,
        &v10_0_tversion_class,
        &v10_2_outgoing_queue_class,
        &v10_2_incoming_queue_class,
        &v10_0_sync_key_class,
        &v10_1_ckmirror_class,
        &v10_0_current_key_class,
        &v10_4_ckstate_class,
        &v10_0_item_backup_class,
        &v10_0_backup_keybag_class,
        &v10_2_ckmanifest_class,
        &v10_2_pending_manifest_class,
        &v10_1_ckmanifest_leaf_class,
        &v10_1_backup_keyarchive_class,
        &v10_1_current_keyarchive_class,
        &v10_1_current_archived_keys_class,
        &v10_1_pending_manifest_leaf_class,
        &v10_4_current_item_class,
        &v10_3_ckdevicestate_class,
        0
    }
};

/*
 * Version 10.3
 */
const SecDbSchema v10_3_schema = {
    .majorVersion = 10,
    .minorVersion = 3,
    .classes = {
        &v10_1_genp_class,
        &v10_1_inet_class,
        &v10_1_cert_class,
        &v10_1_keys_class,
        &v10_0_tversion_class,
        &v10_2_outgoing_queue_class,
        &v10_2_incoming_queue_class,
        &v10_0_sync_key_class,
        &v10_1_ckmirror_class,
        &v10_0_current_key_class,
        &v10_0_ckstate_class,
        &v10_0_item_backup_class,
        &v10_0_backup_keybag_class,
        &v10_2_ckmanifest_class,
        &v10_2_pending_manifest_class,
        &v10_1_ckmanifest_leaf_class,
        &v10_1_backup_keyarchive_class,
        &v10_1_current_keyarchive_class,
        &v10_1_current_archived_keys_class,
        &v10_1_pending_manifest_leaf_class,
        &v10_1_current_item_class,
        &v10_3_ckdevicestate_class,
        0
    }
};

/*
 * Version 10.2
 */
const SecDbSchema v10_2_schema = {
    .majorVersion = 10,
    .minorVersion = 2,
    .classes = {
        &v10_1_genp_class,
        &v10_1_inet_class,
        &v10_1_cert_class,
        &v10_1_keys_class,
        &v10_0_tversion_class,
        &v10_2_outgoing_queue_class,
        &v10_2_incoming_queue_class,
        &v10_0_sync_key_class,
        &v10_1_ckmirror_class,
        &v10_0_current_key_class,
        &v10_0_ckstate_class,
        &v10_0_item_backup_class,
        &v10_0_backup_keybag_class,
        &v10_2_ckmanifest_class,
        &v10_2_pending_manifest_class,
        &v10_1_ckmanifest_leaf_class,
        &v10_1_backup_keyarchive_class,
        &v10_1_current_keyarchive_class,
        &v10_1_current_archived_keys_class,
        &v10_1_pending_manifest_leaf_class,
        &v10_1_current_item_class,
        0
    }
};

/*
 * Version 10.1
 */
const SecDbSchema v10_1_schema = {
    .majorVersion = 10,
    .minorVersion = 1,
    .classes = {
        &v10_1_genp_class,
        &v10_1_inet_class,
        &v10_1_cert_class,
        &v10_1_keys_class,
        &v10_0_tversion_class,
        &v10_1_outgoing_queue_class,
        &v10_1_incoming_queue_class,
        &v10_0_sync_key_class,
        &v10_1_ckmirror_class,
        &v10_0_current_key_class,
        &v10_0_ckstate_class,
        &v10_0_item_backup_class,
        &v10_0_backup_keybag_class,
        &v10_1_ckmanifest_class,
        &v10_1_pending_manifest_class,
        &v10_1_ckmanifest_leaf_class,
        &v10_1_backup_keyarchive_class,
        &v10_1_current_keyarchive_class,
        &v10_1_current_archived_keys_class,
        &v10_1_pending_manifest_leaf_class,
        &v10_1_current_item_class,
        0
    }
};

/*
 * Version 10.0
 */

const SecDbClass v10_0_genp_class = {
    .name = CFSTR("genp"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6svce,
        &v6gena,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        &v10itemuuid,
        &v10itempersistentref,
        &v10sysbound,
        0
    },
};

const SecDbClass v10_0_inet_class = {
    .name = CFSTR("inet"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6sdmn,
        &v6srvr,
        &v6ptcl,
        &v6atyp,
        &v6port,
        &v6path,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        &v10itemuuid,
        &v10itempersistentref,
        &v10sysbound,
        0
    },
};

const SecDbClass v10_0_cert_class = {
    .name = CFSTR("cert"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6ctyp,
        &v6cenc,
        &v6labl,
        &v6certalis,
        &v6subj,
        &v6issr,
        &v6slnr,
        &v6skid,
        &v6pkhh,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        &v10itemuuid,
        &v10itempersistentref,
        &v10sysbound,
        0
    },
};

const SecDbClass v10_0_keys_class = {
    .name = CFSTR("keys"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6kcls,
        &v6labl,
        &v6alis,
        &v6perm,
        &v6priv,
        &v6modi,
        &v6klbl,
        &v6atag,
        &v6keycrtr,
        &v6keytype,
        &v6bsiz,
        &v6esiz,
        &v6sdat,
        &v6edat,
        &v6sens,
        &v6asen,
        &v6extr,
        &v6next,
        &v6encr,
        &v6decr,
        &v6drve,
        &v6sign,
        &v6vrfy,
        &v6snrc,
        &v6vyrc,
        &v6wrap,
        &v6unwp,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        &v10itemuuid,
        &v10itempersistentref,
        &v10sysbound,
        0
    }
};

const SecDbSchema v10_0_schema = {
    .majorVersion = 10,
    .minorVersion = 0,
    .classes = {
        &v10_0_genp_class,
        &v10_0_inet_class,
        &v10_0_cert_class,
        &v10_0_keys_class,
        &v10_0_tversion_class,
        &v10_0_outgoing_queue_class,
        &v10_0_incoming_queue_class,
        &v10_0_sync_key_class,
        &v10_0_ckmirror_class,
        &v10_0_current_key_class,
        &v10_0_ckstate_class,
        &v10_0_item_backup_class,
        &v10_0_backup_keybag_class,
        0
    }
};

const SecDbClass v9_1_tversion_class = {
    .name = CFSTR("tversion91"),
    .itemclass = false,
    .attrs = {
        &v6rowid,
        &v6version,
        &v91minor,
        0
    }
};

const SecDbClass v9_1_genp_class = {
    .name = CFSTR("genp91"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6svce,
        &v6gena,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        0
    },
};

const SecDbClass v9_1_inet_class = {
    .name = CFSTR("inet91"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6sdmn,
        &v6srvr,
        &v6ptcl,
        &v6atyp,
        &v6port,
        &v6path,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        0
    },
};

const SecDbClass v9_1_cert_class = {
    .name = CFSTR("cert91"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6ctyp,
        &v6cenc,
        &v6labl,
        &v6certalis,
        &v6subj,
        &v6issr,
        &v6slnr,
        &v6skid,
        &v6pkhh,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        0
    },
};

const SecDbClass v9_1_keys_class = {
    .name = CFSTR("keys91"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6kcls,
        &v6labl,
        &v6alis,
        &v6perm,
        &v6priv,
        &v6modi,
        &v6klbl,
        &v6atag,
        &v6keycrtr,
        &v6keytype,
        &v6bsiz,
        &v6esiz,
        &v6sdat,
        &v6edat,
        &v6sens,
        &v6asen,
        &v6extr,
        &v6next,
        &v6encr,
        &v6decr,
        &v6drve,
        &v6sign,
        &v6vrfy,
        &v6snrc,
        &v6vyrc,
        &v6wrap,
        &v6unwp,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        0
    }
};

/*
 * Version 9.1 (iOS 10.0 and OSX 10.11.8/10.12 addded minor version.
 */
const SecDbSchema v9_1_schema = {
    .majorVersion = 9,
    .minorVersion = 1,
    .classes = {
        &v9_1_genp_class,
        &v9_1_inet_class,
        &v9_1_cert_class,
        &v9_1_keys_class,
        &v9_1_tversion_class,
        0
    }
};

const SecDbClass v9genp_class = {
    .name = CFSTR("genp9"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6svce,
        &v6gena,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        0
    },
};

const SecDbClass v9inet_class = {
    .name = CFSTR("inet9"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6sdmn,
        &v6srvr,
        &v6ptcl,
        &v6atyp,
        &v6port,
        &v6path,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        0
    },
};

const SecDbClass v9cert_class = {
    .name = CFSTR("cert9"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6ctyp,
        &v6cenc,
        &v6labl,
        &v6certalis,
        &v6subj,
        &v6issr,
        &v6slnr,
        &v6skid,
        &v6pkhh,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        0
    },
};

const SecDbClass v9keys_class = {
    .name = CFSTR("keys9"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6kcls,
        &v6labl,
        &v6alis,
        &v6perm,
        &v6priv,
        &v6modi,
        &v6klbl,
        &v6atag,
        &v6keycrtr,
        &v6keytype,
        &v6bsiz,
        &v6esiz,
        &v6sdat,
        &v6edat,
        &v6sens,
        &v6asen,
        &v6extr,
        &v6next,
        &v6encr,
        &v6decr,
        &v6drve,
        &v6sign,
        &v6vrfy,
        &v6snrc,
        &v6vyrc,
        &v6wrap,
        &v6unwp,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        0
    }
};

const SecDbClass v5tversion_class = {
    .name = CFSTR("tversion5"),
    .itemclass = false,
    .attrs = {
        &v6version,
        0
    }
};

/* Version 9 (iOS 9.3 and OSX 10.11.5) database schema
 * Same contents as v8 tables; table names changed to force upgrade
 * and correct default values in table.
 */
const SecDbSchema v9_schema = {
    .majorVersion = 9,
    .classes = {
        &v9genp_class,
        &v9inet_class,
        &v9cert_class,
        &v9keys_class,
        &v5tversion_class,
        0
    }
};

// Version 8 (Internal release iOS 9.3 and OSX 10.11.5) database schema
const SecDbClass v8genp_class = {
    .name = CFSTR("genp8"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6svce,
        &v6gena,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        0
    },
};

const SecDbClass v8inet_class = {
    .name = CFSTR("inet8"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6sdmn,
        &v6srvr,
        &v6ptcl,
        &v6atyp,
        &v6port,
        &v6path,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        0
    },
};

const SecDbClass v8cert_class = {
    .name = CFSTR("cert8"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6ctyp,
        &v6cenc,
        &v6labl,
        &v6certalis,
        &v6subj,
        &v6issr,
        &v6slnr,
        &v6skid,
        &v6pkhh,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        0
    },
};

const SecDbClass v8keys_class = {
    .name = CFSTR("keys8"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6kcls,
        &v6labl,
        &v6alis,
        &v6perm,
        &v6priv,
        &v6modi,
        &v6klbl,
        &v6atag,
        &v6keycrtr,
        &v6keytype,
        &v6bsiz,
        &v6esiz,
        &v6sdat,
        &v6edat,
        &v6sens,
        &v6asen,
        &v6extr,
        &v6next,
        &v6encr,
        &v6decr,
        &v6drve,
        &v6sign,
        &v6vrfy,
        &v6snrc,
        &v6vyrc,
        &v6wrap,
        &v6unwp,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        &v8musr,
        0
    }
};

const SecDbSchema v8_schema = {
    .majorVersion = 8,
    .classes = {
        &v8genp_class,
        &v8inet_class,
        &v8cert_class,
        &v8keys_class,
        &v5tversion_class,
        0
    }
};

// Version 7 (iOS 9 and OSX 10.11) database schema
const SecDbClass v7genp_class = {
    .name = CFSTR("genp7"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6svce,
        &v6gena,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        0
    },
};

const SecDbClass v7inet_class = {
    .name = CFSTR("inet7"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6sdmn,
        &v6srvr,
        &v6ptcl,
        &v6atyp,
        &v6port,
        &v6path,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        0
    },
};

const SecDbClass v7cert_class = {
    .name = CFSTR("cert7"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6ctyp,
        &v6cenc,
        &v6labl,
        &v6certalis,
        &v6subj,
        &v6issr,
        &v6slnr,
        &v6skid,
        &v6pkhh,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        0
    },
};

const SecDbClass v7keys_class = {
    .name = CFSTR("keys7"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6kcls,
        &v6labl,
        &v6alis,
        &v6perm,
        &v6priv,
        &v6modi,
        &v6klbl,
        &v6atag,
        &v6keycrtr,
        &v6keytype,
        &v6bsiz,
        &v6esiz,
        &v6sdat,
        &v6edat,
        &v6sens,
        &v6asen,
        &v6extr,
        &v6next,
        &v6encr,
        &v6decr,
        &v6drve,
        &v6sign,
        &v6vrfy,
        &v6snrc,
        &v6vyrc,
        &v6wrap,
        &v6unwp,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v7vwht,
        &v7tkid,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        &v7utomb,
        0
    }
};


const SecDbSchema v7_schema = {
    .majorVersion = 7,
    .classes = {
        &v7genp_class,
        &v7inet_class,
        &v7cert_class,
        &v7keys_class,
        &v5tversion_class,
        0
    }
};


// Version 6 (iOS 7 and OSX 10.9) database schema
static const SecDbClass v6genp_class = {
    .name = CFSTR("genp6"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6svce,
        &v6gena,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        0
    },
};

static const SecDbClass v6inet_class = {
    .name = CFSTR("inet6"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6sdmn,
        &v6srvr,
        &v6ptcl,
        &v6atyp,
        &v6port,
        &v6path,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        0
    },
};

static const SecDbClass v6cert_class = {
    .name = CFSTR("cert6"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6ctyp,
        &v6cenc,
        &v6labl,
        &v6certalis,
        &v6subj,
        &v6issr,
        &v6slnr,
        &v6skid,
        &v6pkhh,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        0
    },
};

static const SecDbClass v6keys_class = {
    .name = CFSTR("keys6"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6kcls,
        &v6labl,
        &v6alis,
        &v6perm,
        &v6priv,
        &v6modi,
        &v6klbl,
        &v6atag,
        &v6keycrtr,
        &v6keytype,
        &v6bsiz,
        &v6esiz,
        &v6sdat,
        &v6edat,
        &v6sens,
        &v6asen,
        &v6extr,
        &v6next,
        &v6encr,
        &v6decr,
        &v6drve,
        &v6sign,
        &v6vrfy,
        &v6snrc,
        &v6vyrc,
        &v6wrap,
        &v6unwp,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v6v_Data,
        &v6v_pk,
        &v6accc,
        0
    }
};

static const SecDbSchema v6_schema = {
    .majorVersion = 6,
    .classes = {
        &v6genp_class,
        &v6inet_class,
        &v6cert_class,
        &v6keys_class,
        &v5tversion_class,
        0
    }
};


// Version 5 (iOS 5 & iOS 6) database schema.
static const SecDbClass v5genp_class = {
    .name = CFSTR("genp5"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6svce,
        &v6gena,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6v_Data,
        0
    },
};

static const SecDbClass v5inet_class = {
    .name = CFSTR("inet5"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6sdmn,
        &v6srvr,
        &v6ptcl,
        &v6atyp,
        &v6port,
        &v6path,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6v_Data,
        0
    },
};

static const SecDbClass v5cert_class = {
    .name = CFSTR("cert5"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6ctyp,
        &v6cenc,
        &v6labl,
        &v6certalis,
        &v6subj,
        &v6issr,
        &v6slnr,
        &v6skid,
        &v6pkhh,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6v_Data,
        0
    },
};

static const SecDbClass v5keys_class = {
    .name = CFSTR("keys5"),
    .itemclass = true,
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6kcls,
        &v6labl,
        &v6alis,
        &v6perm,
        &v6priv,
        &v6modi,
        &v6klbl,
        &v6atag,
        &v6keycrtr,
        &v6keytype,
        &v6bsiz,
        &v6esiz,
        &v6sdat,
        &v6edat,
        &v6sens,
        &v6asen,
        &v6extr,
        &v6next,
        &v6encr,
        &v6decr,
        &v6drve,
        &v6sign,
        &v6vrfy,
        &v6snrc,
        &v6vyrc,
        &v6wrap,
        &v6unwp,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6v_Data,
        0
    }
};

static const SecDbSchema v5_schema = {
    .majorVersion = 5,
    .classes = {
        &v5genp_class,
        &v5inet_class,
        &v5cert_class,
        &v5keys_class,
        &v5tversion_class,
        0
    }
};

SecDbSchema const * const * kc_schemas = NULL;

const SecDbSchema *v10_kc_schemas[] = {
    &v11_2_schema,
    &v11_1_schema,
    &v11_schema,
    &v10_5_schema,
    &v10_4_schema,
    &v10_3_schema,
    &v10_2_schema,
    &v10_1_schema,
    &v10_0_schema,
    &v9_1_schema,
    &v9_schema,
    &v8_schema,
    &v7_schema,
    &v6_schema,
    &v5_schema,
    0
};

const SecDbSchema * const * all_schemas() {
    return v10_kc_schemas;
}

const SecDbSchema* current_schema() {
    // For now, the current schema is the first in the list.
    return all_schemas()[0];
}

// class accessors for current schema.
static const SecDbClass* find_class(const SecDbSchema* schema, CFStringRef class_name) {
    for (const SecDbClass * const *pclass = schema->classes; *pclass; ++pclass) {
        if( CFEqualSafe((*pclass)->name, class_name) ) {
            return *pclass;
        }
    }
    return NULL;
}

const SecDbClass* genp_class() {
    static const SecDbClass* genp = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        genp = find_class(current_schema(), CFSTR("genp"));
    });
    return genp;
}
const SecDbClass* inet_class() {
    static const SecDbClass* inet = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        inet = find_class(current_schema(), CFSTR("inet"));
    });
    return inet;
}
const SecDbClass* cert_class() {
    static const SecDbClass* cert = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        cert = find_class(current_schema(), CFSTR("cert"));
    });
    return cert;
}
const SecDbClass* keys_class() {
    static const SecDbClass* keys = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        keys = find_class(current_schema(), CFSTR("keys"));
    });
    return keys;
}

// Not really a class per-se
const SecDbClass* identity_class() {
    return &v_identity_class;
}

// Class with 1 element in it which is the database version->
const SecDbClass* tversion_class() {
    static const SecDbClass* tversion = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        tversion = find_class(current_schema(), CFSTR("tversion"));
    });
    return tversion;
}


