/*
 *  spi.c
 *  Security
 *
 *  Created by Michael Brouwer on 1/28/09.
 *  Copyright (c) 2009-2010 Apple Inc.. All Rights Reserved.
 *
 */

#include <securityd/spi.h>
#include <securityd_client.h>
#include <securityd_server.h>
#include <securityd/SecPolicyServer.h>

static struct securityd spi = {
    _SecItemAdd,
    _SecItemCopyMatching,
    _SecItemUpdate,
    _SecItemDelete,
    SecTrustStoreForDomainName,
    SecTrustStoreContainsCertificateWithDigest,
    _SecTrustStoreSetTrustSettings,
    SecTrustStoreRemoveCertificateWithDigest,
    _SecTrustStoreRemoveAll,
    _SecItemDeleteAll,
    SecTrustServerEvaluate,
    _SecServerRestoreKeychain,
    _SecServerMigrateKeychain,
    _SecServerKeychainBackup,
    _SecServerKeychainRestore
};

void securityd_init(void) {
    gSecurityd = &spi;
    SecPolicyServerInitalize();
}
