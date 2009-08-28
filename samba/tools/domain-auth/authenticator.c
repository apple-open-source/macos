/*
 * Copyright (c) 2003-2009 Apple Inc. All rights reserved.
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

#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>
#include <sysexits.h>
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#define PROGNAME "smb-odauth-helper"

#define KEYCHAIN_SERVICE "com.apple.samba"
#define SAMBA_APP_ID CFSTR("com.apple.samba")
#define credentialfile "/var/db/samba/opendirectorysam"
#define opendirectory_secret_sig 'odsa'

typedef struct opendirectory_secret_header {
    uint32_t signature;
    uint32_t authenticator_len;
    uint32_t secret_len;
    uint32_t authenticatorid_len;
} opendirectory_secret_header;

int verbose;

static const struct option longopts[] =
{
    { "help", no_argument, NULL, 'h' },
    { "verbose", no_argument, NULL, 'v' },
    { "keychain", no_argument, NULL, 'k' },
    { "credfile", no_argument, NULL, 'c' },

    { NULL, 0, NULL, 0 }
};

static void
usage(void)
{
    static const char message[] =
        PROGNAME ": fetch DomainAdmin credentials\n"
        "\n"
        "Usage: " PROGNAME " -k [options]\n"
        "       " PROGNAME " -c [options]\n"
        "        --help, -h          print usage\n"
        "        --verbose, -v       print debugging messages\n"
        "        --keychain, -k      use Keychain credentials\n"
        "        --credfile, -c      use pre-Keychain credentials file\n"
        ;

    fprintf(stdout, "%s", message);
}

__printflike(1,2)
static void
message(const char * fmt, ...)
{
    if (verbose) {
        va_list args;

        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

static char *
get_admin_account(void)
{
    CFPropertyListRef   pref = NULL;
    char *              account = NULL;
    int                 accountLength = 1024;

    pref = CFPreferencesCopyAppValue (CFSTR("DomainAdmin"), SAMBA_APP_ID);
    if (pref != 0) {
        if (CFGetTypeID(pref) == CFStringGetTypeID()) {
            account = calloc(1, accountLength);
            if (!CFStringGetCString((CFStringRef)pref, account,
                        accountLength, kCFStringEncodingUTF8)) {
                free(account);
                account = NULL;
            }
        }

        CFRelease(pref);
    }

    return account;
}

static void *
get_password_from_keychain(
        const char * account,
        unsigned accountLength)
{
    OSStatus status ;
    SecKeychainItemRef item;
    void * passwordData = NULL;
    UInt32 passwordLength = 0;
    void * password = NULL;

    // Set the domain to System (daemon)
    status = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
    status = SecKeychainGetUserInteractionAllowed (false);

    status = SecKeychainFindGenericPassword (
                 NULL,                      // default keychain
                 strlen(KEYCHAIN_SERVICE),  // length of service name
                 KEYCHAIN_SERVICE,          // service name
                 accountLength,             // length of account name
                 account,                   // account name
                 &passwordLength,           // length of password
                 &passwordData,             // pointer to password data
                 &item                      // the item reference
                );

    if (status != noErr) {
        message("%s: SecKeychainFindGenericPassword failed with error %d\n",
                PROGNAME, (int)status);
        return NULL;
    }

    if (item == NULL || passwordLength == 0) {
        message("%s: SecKeychainFindGenericPassword found no results for %s\n",
                PROGNAME, account);
        return NULL;
    }

    password = calloc(1, passwordLength + 1);
    memcpy(password, (const void *)passwordData, passwordLength);
    return password;
}

static int
keychain_authenticator(
        opendirectory_secret_header ** header)
{
    char * password = NULL;
    char * account = NULL;

    account = get_admin_account();
    if (!account) {
        message("%s: no DomainAdmin account configured\n", PROGNAME);
        return EX_NOUSER;
    }

    password = get_password_from_keychain(account, strlen(account));
    if (!password) {
        free(account);
        return EX_UNAVAILABLE;
    }

    *header = calloc(1, sizeof(opendirectory_secret_header)
                        + strlen(account) + strlen(password));

    (*header)->authenticator_len = strlen(account);
    (*header)->secret_len = strlen(password);
    (*header)->signature = opendirectory_secret_sig;

    memcpy((uint8_t *)(*header) + sizeof(opendirectory_secret_header),
            account, strlen(account));
    memcpy((uint8_t *)(*header) + sizeof(opendirectory_secret_header)
                                    + strlen(account),
            password, strlen(password));

    free(password);
    free(account);
    return EX_OK;
}

static int
credfile_authenticator(
        opendirectory_secret_header ** header)
{
    int fd = -1;
    ssize_t len;
    opendirectory_secret_header hdr;

    fd = open(credentialfile, O_RDONLY, 0);
    if (fd == -1) {
        message("%s: unable to open %s (%s)\n",
                    PROGNAME, credentialfile, strerror(errno));
        return EX_OSFILE;
    }

    len = read(fd, &hdr, sizeof(opendirectory_secret_header));
    if (len != sizeof(opendirectory_secret_header)) {
        goto cleanup;
    }

    if (hdr.signature != opendirectory_secret_sig) {
            goto cleanup;
    }

    if ((hdr.authenticator_len + hdr.secret_len) > getpagesize()) {
        goto cleanup;
    }

    *header = calloc(1, sizeof(opendirectory_secret_header) +
                            hdr.authenticator_len + hdr.secret_len);

    memcpy(*header, &hdr, sizeof(opendirectory_secret_header));
    len = read(fd, (uint8_t *)(*header) + sizeof(opendirectory_secret_header),
                            hdr.authenticator_len + hdr.secret_len);
    if (len != (hdr.authenticator_len + hdr.secret_len)) {
        goto cleanup;
    }

    return EX_OK;

cleanup:
    close(fd);
    return EX_DATAERR;
}

static int
write_data(int fd, uint8_t * buf, size_t len)
{
    int err;
    size_t nwritten = 0;

    do {
        err = write(fd, buf + nwritten, len - nwritten);
        if (err == -1 && (errno == EAGAIN || errno == EINTR)) {
            continue;
        }

        if (err == -1) {
            return EX_IOERR;
        }

        nwritten += err;
    } while (nwritten < len);

    return EX_OK;
}

int main(int argc, char * const * argv)
{
    opendirectory_secret_header * secret = NULL;

    bool use_keychain = false;
    bool use_credfile = false;

    int c;
    int err;

    setprogname(PROGNAME);

    while ((c = getopt_long(argc, argv, "vkc", longopts, NULL)) != -1) {
        switch (c) {
            case 'v': ++verbose; break;
            case 'k': use_keychain = true; break;
            case 'c': use_credfile = true; break;
            default: usage(); exit(EX_USAGE);
        }
    }

    if (use_keychain) {
        err = keychain_authenticator(&secret);
    } else if (use_credfile) {
        err = credfile_authenticator(&secret);
    } else {
        usage();
        return EX_USAGE;
    }

    if (err) {
        return err;
    }

    return write_data(STDOUT_FILENO, (uint8_t *)secret,
        sizeof(opendirectory_secret_header)
                + secret->authenticator_len
                + secret->secret_len);
}

/* vim: set cindent ts=8 sts=4 tw=79 et : */
