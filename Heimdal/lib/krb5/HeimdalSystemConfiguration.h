/*
 * Copyright (c) 2011 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef HEIMDAL_SYSTEM_CONFIGURATION_H
#define HEIMDAL_SYSTEM_CONFIGURATION_H 1

/**
 * SPI constants for OD/Heimdal communication for default realm and
 * location data using SystemConfiguration. This is a private
 * interface and can change any time.
 */

/**
 * Order array with list of default realm, first default realm is
 * listed first.
 */
#define HEIMDAL_SC_DEFAULT_REALM CFSTR("Kerberos-Default-Realms")

/**
 * Prefix for location of realm, append realm to key. Data is
 * Dictionary of types and then array of dict with host and port of
 * each servers within this type.
 */
#define HEIMDAL_SC_LOCATE_REALM_PREFIX CFSTR("Kerberos:")

/**
 * Locate type KDC
 */
#define HEIMDAL_SC_LOCATE_TYPE_KDC CFSTR("kdc")

/**
 * Locate type Kerberos change/set password
 */
#define HEIMDAL_SC_LOCATE_TYPE_KPASSWD CFSTR("kpasswd")

/**
 * Locate type Kerberos admin
 */
#define HEIMDAL_SC_LOCATE_TYPE_ADMIN CFSTR("kadmin")

/**
 *
 */
#define HEIMDAL_SC_LOCATE_PORT CFSTR("port")
#define HEIMDAL_SC_LOCATE_HOST CFSTR("host")


/**
 *
 */
#define HEIMDAL_SC_DOMAIN_REALM_MAPPING CFSTR("Kerberos-Domain-Realm-Mappings")




#endif /* HEIMDAL_SYSTEM_CONFIGURATION_H */
