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

// MARK -
// MARK Keychain version 6 schema

#define __FLAGS(ARG, ...) SECDBFLAGS(__VA_ARGS__)
#define SECDBFLAGS(ARG, ...) __FLAGS_##ARG | __FLAGS(__VA_ARGS__)

#define SecDbFlags(P,L,I,S,A,D,R,C,H,B,Z,E,N,U,V) (__FLAGS_##P|__FLAGS_##L|__FLAGS_##I|__FLAGS_##S|__FLAGS_##A|__FLAGS_##D|__FLAGS_##R|__FLAGS_##C|__FLAGS_##H|__FLAGS_##B|__FLAGS_##Z|__FLAGS_##E|__FLAGS_##N|__FLAGS_##U|__FLAGS_##V)

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

//                                                                   ,-------------- P : Part of primary key
//                                                                  / ,------------- L : Stored in local database
//                                                                 / / ,------------ I : Attribute wants an index in the database
//                                                                / / / ,----------- S : SHA1 hashed attribute value in database (implies L)
//                                                               / / / / ,---------- A : Returned to client as attribute in queries
//                                                              / / / / / ,--------- D : Returned to client as data in queries
//                                                             / / / / / / ,-------- R : Returned to client as ref/persistent ref in queries
//                                                            / / / / / / / ,------- C : Part of encrypted blob
//                                                           / / / / / / / / ,------ H : Attribute is part of item SHA1 hash (Implied by C)
//                                                          / / / / / / / / / ,----- B : Attribute is part of iTunes/iCloud backup bag
//                                                         / / / / / / / / / / ,---- Z : Attribute has a default value of 0
//                                                        / / / / / / / / / / / ,--- E : Attribute has a default value of "" or empty data
//                                                       / / / / / / / / / / / / ,-- N : Attribute must have a value
//                                                      / / / / / / / / / / / / / ,- U : Attribute is stored in authenticated, but not necessarily encrypted data
//                                                     / / / / / / / / / / / / / / - S : Sync primpary key version
//                                                    / / / / / / / / / / / / / / /
//                                                    | | | | | | | | | | | | | | |
// common to all                                      | | | | | | | | | | | | | | |
SECDB_ATTR(v6rowid, "rowid", RowId,        SecDbFlags( ,L, , , , ,R, , ,B, , , , ,  ), NULL, NULL);
SECDB_ATTR(v6cdat, "cdat", CreationDate,   SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), SecDbKeychainItemCopyCurrentDate, NULL);
SECDB_ATTR(v6mdat, "mdat",ModificationDate,SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), SecDbKeychainItemCopyCurrentDate, NULL);
SECDB_ATTR(v6labl, "labl", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6data, "data", EncryptedData,  SecDbFlags( ,L, , , , , , , ,B, , , , ,  ), SecDbKeychainItemCopyEncryptedData, NULL);
SECDB_ATTR(v6agrp, "agrp", String,         SecDbFlags(P,L, , ,A, , , ,H, , , ,N,U,V0), NULL, NULL);
SECDB_ATTR(v6pdmn, "pdmn", Access,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6sync, "sync", Sync,           SecDbFlags(P,L,I, ,A, , , ,H, ,Z, ,N,U,V0), NULL, NULL);
SECDB_ATTR(v6tomb, "tomb", Tomb,           SecDbFlags( ,L, , , , , , ,H, ,Z, ,N,U,  ), NULL, NULL);
SECDB_ATTR(v6sha1, "sha1", SHA1,           SecDbFlags( ,L,I, ,A, ,R, , , , , , , ,  ), SecDbKeychainItemCopySHA1, NULL);
SECDB_ATTR(v6accc, "accc", AccessControl,  SecDbFlags( , , , ,A, , , , , , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6v_Data, "v_Data", Data,       SecDbFlags( , , , , ,D, ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6v_pk, "v_pk", PrimaryKey,     SecDbFlags( , , , , , , , , , , , , , ,  ), SecDbKeychainItemCopyPrimaryKey, NULL);
SECDB_ATTR(v7vwht, "vwht", String,         SecDbFlags(P,L, , ,A, , , ,H, , , , ,U,V2), NULL, NULL);
SECDB_ATTR(v7tkid, "tkid", String,         SecDbFlags(P,L, , ,A, , , ,H, , , , ,U,V2), NULL, NULL);
SECDB_ATTR(v7utomb, "u_Tomb", UTomb,       SecDbFlags( , , , , , , , , , , , , , ,  ), NULL, NULL);
SECDB_ATTR(v8musr, "musr", UUID,           SecDbFlags(P,L,I, , , , , , , , , ,N,U,  ), NULL, NULL);
// genp and inet and keys                             | | | | | | | | | | | | | | |
SECDB_ATTR(v6crtr, "crtr", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6alis, "alis", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ,  ), NULL, NULL);
// genp and inet                                      | | | | | | | | | | | | | | |
SECDB_ATTR(v6desc, "desc", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6icmt, "icmt", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6type, "type", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6invi, "invi", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6nega, "nega", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6cusi, "cusi", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6prot, "prot", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6scrp, "scrp", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6acct, "acct", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0), NULL, NULL);
// genp only                                          | | | | | | | | | | | | | | |
SECDB_ATTR(v6svce, "svce", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0), NULL, NULL);
SECDB_ATTR(v6gena, "gena", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , , ,  ), NULL, NULL);
// inet only                                          | | | | | | | | | | | | | | |
SECDB_ATTR(v6sdmn, "sdmn", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0), NULL, NULL);
SECDB_ATTR(v6srvr, "srvr", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0), NULL, NULL);
SECDB_ATTR(v6ptcl, "ptcl", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0), NULL, NULL);
SECDB_ATTR(v6atyp, "atyp", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0), NULL, NULL);
SECDB_ATTR(v6port, "port", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0), NULL, NULL);
SECDB_ATTR(v6path, "path", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0), NULL, NULL);
// cert only                                          | | | | | | | | | | | | | |  |
SECDB_ATTR(v6ctyp, "ctyp", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0), NULL, NULL);
SECDB_ATTR(v6cenc, "cenc", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6subj, "subj", Data,           SecDbFlags( ,L,I,S,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6issr, "issr", Data,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0), NULL, NULL);
SECDB_ATTR(v6slnr, "slnr", Data,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0), NULL, NULL);
SECDB_ATTR(v6skid, "skid", Data,           SecDbFlags( ,L,I,S,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6pkhh, "pkhh", Data,           SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ), NULL, NULL);
// cert attributes that share names with common ones but have different flags
SECDB_ATTR(v6certalis, "alis", Blob,       SecDbFlags( ,L,I,S,A, , ,C,H, , , , , ,  ), NULL, NULL);
// keys only                                          | | | | | | | | | | | | | | |
SECDB_ATTR(v6kcls, "kcls", Number,         SecDbFlags(P,L,I,S,A, , ,C,H, ,Z, ,N, ,V0), NULL, NULL);
SECDB_ATTR(v6perm, "perm", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6priv, "priv", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6modi, "modi", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6klbl, "klbl", Data,           SecDbFlags(P,L,I, ,A, , ,C,H, , ,E,N, ,V0), NULL, NULL);
SECDB_ATTR(v6atag, "atag", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N, ,V0), NULL, NULL);
SECDB_ATTR(v6bsiz, "bsiz", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0), NULL, NULL);
SECDB_ATTR(v6esiz, "esiz", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0), NULL, NULL);
SECDB_ATTR(v6sdat, "sdat", Date,           SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0), NULL, NULL);
SECDB_ATTR(v6edat, "edat", Date,           SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0), NULL, NULL);
SECDB_ATTR(v6sens, "sens", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6asen, "asen", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6extr, "extr", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6next, "next", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6encr, "encr", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6decr, "decr", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6drve, "drve", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6sign, "sign", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6vrfy, "vrfy", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6snrc, "snrc", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6vyrc, "vyrc", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6wrap, "wrap", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ), NULL, NULL);
SECDB_ATTR(v6unwp, "unwp", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , , ,  ), NULL, NULL);
// keys attributes that share names with common ones but have different flags
SECDB_ATTR(v6keytype, "type", Number,      SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0), NULL, NULL);
SECDB_ATTR(v6keycrtr, "crtr", Number,      SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N, ,V0), NULL, NULL);
//                                                    | | | | | | | | | | | | | | |
SECDB_ATTR(v6version, "version", Number,   SecDbFlags(P,L, , , , , , , , , , ,N, ,  ), NULL, NULL);
SECDB_ATTR(v91minor, "minor", Number,      SecDbFlags( ,L, , , , , , , , ,Z, ,N, ,  ), NULL, NULL);

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

const SecDbClass tversion_class = {
    .name = CFSTR("tversion"),
    .attrs = {
        &v6rowid,
        &v6version,
        &v91minor,
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

/*
 * Version 9.1 (iOS 10.0 and OSX 10.11.8/10.12 addded minor version.
 */
const SecDbSchema v9_1_schema = {
    .majorVersion = 9,
    .minorVersion = 1,
    .classes = {
        &genp_class,
        &inet_class,
        &cert_class,
        &keys_class,
        &tversion_class,
        0
    }
};

const SecDbClass v9genp_class = {
    .name = CFSTR("genp9"),
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

const SecDbSchema *kc_schemas[] = {
    &v9_1_schema,
    &v9_schema,
    &v8_schema,
    &v7_schema,
    &v6_schema,
    &v5_schema,
    0
};
