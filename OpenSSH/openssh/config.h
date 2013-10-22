/*
 *  config.h
 *  openssh
 *
 *  Created by Timothy Weiand on 6/23/09.
 *  Copyright 2009 Apple Inc. All rights reserved.
 *
 */


#define SSH_RAND_HELPER "/usr/libexec/ssh-rand-helper"
#define _PATH_PRIVSEP_CHROOT_DIR "/var/empty"
#define _PATH_SSH_PIDDIR "/var/run"
#define _PATH_SSH_KEY_SIGN "/usr/libexec/ssh-keysign"
#define _PATH_SFTP_SERVER "/usr/libexec/sftp-server"
#define _PATH_SSH_ASKPASS_DEFAULT "/usr/libexec/ssh-askpass"
#define _PATH_SSH_PROGRAM "/usr/bin/ssh"
#define SSHDIR "/etc"


#define _UTMPX_COMPAT
#define __APPLE_UTMPX__
#define __APPLE_LAUNCHD__

/*
 * Define '__APPLE_CRYPTO__' to use libosslshim instead of libcrypto (OpenSSL).
 * You must also change '-lcrypto' to '-losslshim in
 * openssh.xcodeproj/project.pbxproj (XCode).
 */
#ifndef FORCE_USE_OPENSSL
#define	__APPLE_CRYPTO__
#endif


#if TARGET_OS_EMBEDDED
#  define _FORTIFY_SOURCE 2
#  define __APPLE_PRIVPTY__
#  define cannot_audit
#else
#  define __APPLE_SACL_DEPRECATED__
#  define __APPLE_MEMBERSHIP__
#  define __APPLE_CROSS_REALM__
#  define __APPLE_XSAN__
#  define __APPLE_KEYCHAIN__
#  define __APPLE_SANDBOX_NAMED_EXTERNAL__
#  define HAVE_CONFIG_H
#  define USE_CCAPI
#endif

#if TARGET_OS_EMBEDDED
#  include "config_embedded.h"
#else
#  include "config_macosx.h"
#endif
