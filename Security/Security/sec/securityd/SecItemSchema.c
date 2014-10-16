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

// MARK -
// MARK Keychain version 6 schema

#define __FLAGS(ARG, ...) SECDBFLAGS(__VA_ARGS__)
#define SECDBFLAGS(ARG, ...) __FLAGS_##ARG | __FLAGS(__VA_ARGS__)

#define SecDbFlags(P,L,I,S,A,D,R,C,H,B,Z,E,N,U) (__FLAGS_##P|__FLAGS_##L|__FLAGS_##I|__FLAGS_##S|__FLAGS_##A|__FLAGS_##D|__FLAGS_##R|__FLAGS_##C|__FLAGS_##H|__FLAGS_##B|__FLAGS_##Z|__FLAGS_##E|__FLAGS_##N|__FLAGS_##U)

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

//                                                                   ,-------------- P : Part of primary key
//                                                                  / ,------------- L : Stored in local database
//                                                                 / / ,------------ I : Attribute wants an index in the database
//                                                                / / / ,----------- S : SHA1 hashed attribute value in database (implies L)
//                                                               / / / / ,---------- A : Returned to client as attribute in queries
//                                                              / / / / / ,--------- D : Returned to client as data in queries
//                                                             / / / / / / ,-------- R : Returned to client as ref/persistant ref in queries
//                                                            / / / / / / / ,------- C : Part of encrypted blob
//                                                           / / / / / / / / ,------ H : Attribute is part of item SHA1 hash (Implied by C)
//                                                          / / / / / / / / / ,----- B : Attribute is part of iTunes/iCloud backup bag
//                                                         / / / / / / / / / / ,---- Z : Attribute has a default value of 0
//                                                        / / / / / / / / / / / ,--- E : Attribute has a default value of "" or empty data
//                                                       / / / / / / / / / / / / ,-- N : Attribute must have a value
//                                                      / / / / / / / / / / / / / ,- U : Attribute is stored in authenticated, but not necessarily encrypted data
//                                                     / / / / / / / / / / / / / /
//                                                    / / / / / / / / / / / / / /
//                                                    | | | | | | | | | | | | | |
// common to all                                      | | | | | | | | | | | | | |
SECDB_ATTR(v6rowid, "rowid", RowId,        SecDbFlags( ,L, , , , ,R, , ,B, , , , ));
SECDB_ATTR(v6cdat, "cdat", CreationDate,   SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6mdat, "mdat",ModificationDate,SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6labl, "labl", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ));
SECDB_ATTR(v6data, "data", EncryptedData,  SecDbFlags( ,L, , , , , , , ,B, , , , ));
SECDB_ATTR(v6agrp, "agrp", String,         SecDbFlags(P,L, , ,A, , , ,H, , , , ,U));
SECDB_ATTR(v6pdmn, "pdmn", Access,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6sync, "sync", Sync,           SecDbFlags(P,L,I, ,A, , , ,H, ,Z, ,N,U));
SECDB_ATTR(v6tomb, "tomb", Tomb,           SecDbFlags( ,L, , , , , , ,H, ,Z, ,N,U));
SECDB_ATTR(v6sha1, "sha1", SHA1,           SecDbFlags( ,L,I, ,A, ,R, , , , , , , ));
SECDB_ATTR(v6accc, "accc", AccessControl,  SecDbFlags( , , , ,A, , , , , , , , , ));
SECDB_ATTR(v6v_Data, "v_Data", Data,       SecDbFlags( , , , , ,D, ,C,H, , , , , ));
SECDB_ATTR(v6v_pk, "v_pk", PrimaryKey,     SecDbFlags( , , , , , , , , , , , , , ));
// genp and inet and keys                             | | | | | | | | | | | | |
SECDB_ATTR(v6crtr, "crtr", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6alis, "alis", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ));
// genp and inet                                      | | | | | | | | | | | | |
SECDB_ATTR(v6desc, "desc", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ));
SECDB_ATTR(v6icmt, "icmt", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ));
SECDB_ATTR(v6type, "type", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6invi, "invi", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6nega, "nega", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6cusi, "cusi", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6prot, "prot", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ));
SECDB_ATTR(v6scrp, "scrp", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6acct, "acct", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ));
// genp only                                          | | | | | | | | | | | | |
SECDB_ATTR(v6svce, "svce", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ));
SECDB_ATTR(v6gena, "gena", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ));
// inet only                                          | | | | | | | | | | | | |
SECDB_ATTR(v6sdmn, "sdmn", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ));
SECDB_ATTR(v6srvr, "srvr", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ));
SECDB_ATTR(v6ptcl, "ptcl", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ));
SECDB_ATTR(v6atyp, "atyp", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ));
SECDB_ATTR(v6port, "port", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ));
SECDB_ATTR(v6path, "path", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ));
// cert only                                          | | | | | | | | | | | | |
SECDB_ATTR(v6ctyp, "ctyp", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ));
SECDB_ATTR(v6cenc, "cenc", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6subj, "subj", Data,           SecDbFlags( ,L,I,S,A, , ,C,H, , , , , ));
SECDB_ATTR(v6issr, "issr", Data,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ));
SECDB_ATTR(v6slnr, "slnr", Data,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ));
SECDB_ATTR(v6skid, "skid", Data,           SecDbFlags( ,L,I,S,A, , ,C,H, , , , , ));
SECDB_ATTR(v6pkhh, "pkhh", Data,           SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ));
// cert attributes that share names with common ones but have different flags
SECDB_ATTR(v6certalis, "alis", Blob,       SecDbFlags( ,L,I,S,A, , ,C,H, , , , , ));
// keys only                                          | | | | | | | | | | | | |
SECDB_ATTR(v6kcls, "kcls", Number,         SecDbFlags(P,L,I,S,A, , ,C,H, ,Z, ,N, ));
SECDB_ATTR(v6perm, "perm", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6priv, "priv", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6modi, "modi", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6klbl, "klbl", Data,           SecDbFlags(P,L,I, ,A, , ,C,H, , ,E,N, ));
SECDB_ATTR(v6atag, "atag", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ));
SECDB_ATTR(v6bsiz, "bsiz", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ));
SECDB_ATTR(v6esiz, "esiz", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ));
SECDB_ATTR(v6sdat, "sdat", Date,           SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ));
SECDB_ATTR(v6edat, "edat", Date,           SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ));
SECDB_ATTR(v6sens, "sens", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6asen, "asen", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6extr, "extr", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6next, "next", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6encr, "encr", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6decr, "decr", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6drve, "drve", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6sign, "sign", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6vrfy, "vrfy", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6snrc, "snrc", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6vyrc, "vyrc", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6wrap, "wrap", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ));
SECDB_ATTR(v6unwp, "unwp", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ));
// keys attributes that share names with common ones but have different flags
SECDB_ATTR(v6keytype, "type", Number,      SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ));
SECDB_ATTR(v6keycrtr, "crtr", Number,      SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ));

const SecDbClass genp_class = {
    .name = CFSTR("genp"),
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
        NULL
    },
};

const SecDbClass inet_class = {
    .name = CFSTR("inet"),
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

const SecDbClass cert_class = {
    .name = CFSTR("cert"),
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

const SecDbClass keys_class = {
    .name = CFSTR("keys"),
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

/* An identity which is really a cert + a key, so all cert and keys attrs are
 allowed. */
const SecDbClass identity_class = {
    .name = CFSTR("idnt"),
    .attrs = {
        0
    },
};
