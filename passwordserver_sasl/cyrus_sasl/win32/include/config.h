/* config.h--SASL configuration for win32
 * Ryan Troll
 */
/* 
 * Copyright (c) 2000 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>

/* Our package */
#define PACKAGE "cyrus-sasl"

/* Our version */
#define VERSION "2.1.1"

/* Registry key that contains the locations of the plugins */
#define SASL_KEY "SOFTWARE\\Carnegie Mellon\\Project Cyrus\\SASL Library\\Available Plugins"

/* We only want minimal server functionality.  Cripple the server functionality when necessary to get
 * things to compile.
 *
 * Currently only cripples PLAIN.
 */
#define SASL_MINIMAL_SERVER 1

/* DB Type */
#undef SASL_DB_TYPE

/* ------------------------------------------------------------ */

/* Things that are fetched via autoconf under Unix
 */
#define HAVE_MEMCPY 1

#define SASL_PATH_ENV_VAR "SASL_PATH"
#define PLUGINDIR "\\sasl-plugins"

/* Windows calls these functions something else
 */
#define strcasecmp   stricmp
#define snprintf    _snprintf
#define strncasecmp  strnicmp

#define MAXHOSTNAMELEN 1024

/* ------------------------------------------------------------ */

#define WITHOUT_NANA
#define L_DEFAULT_GUARD (0)
#define I_DEFAULT_GUARD (0)
#define I(foo)
//#define VL(foo)
#define VL(foo)  printf foo;
#define VLP(foo,bar)

#define __attribute__(foo)

#endif /* CONFIG_H */
