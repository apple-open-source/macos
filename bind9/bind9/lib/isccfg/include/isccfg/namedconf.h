/*
 * Copyright (C) 2004-2007, 2009, 2010  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: namedconf.h,v 1.18 2010/08/11 18:14:20 each Exp $ */

#ifndef ISCCFG_NAMEDCONF_H
#define ISCCFG_NAMEDCONF_H 1

/*! \file isccfg/namedconf.h
 * \brief
 * This module defines the named.conf, rndc.conf, and rndc.key grammars.
 */

#include <isccfg/cfg.h>

/*
 * Configuration object types.
 */
LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_namedconf;
/*%< A complete named.conf file. */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_bindkeys;
/*%< A bind.keys file. */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_newzones;
/*%< A new-zones file (for zones added by 'rndc addzone'). */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_addzoneconf;
/*%< A single zone passed via the addzone rndc command. */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_rndcconf;
/*%< A complete rndc.conf file. */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_rndckey;
/*%< A complete rndc.key file. */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_sessionkey;
/*%< A complete session.key file. */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_keyref;
/*%< A key reference, used as an ACL element */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_acl;
/*%< A complete ACL */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_bracketed_aml;
/*%< A complete bracketed address match list */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_category;
/*%< A complete logging category statement */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_channel;
/*%< A complete logging channel statement */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_debuglevel;
/*%< Log debug level */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_destinationlist;
/*%< A logging category destination list */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_grant;
/*%< A complete update policy list entry */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_listenon;
/*%< A complete listen-on statement */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_logging;
/*%< A complete logging statement */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_logfile;
/*%< Log file destination */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_matchname;
/*%< Update policy match name */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_matchtype;
/*%< Update policy match type */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_mode;
/*%< Update policy mode */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_options;
/*%< Configuration options */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_portiplist;
/*%< A list of socket addresses with an optional port */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_rrtypelist;
/*%< Update policy resource record type list */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_bracketed_sockaddrlist;
/*%< A bracketed socket address list */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_updatepolicy;
/*%< A list of update policies */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_view;
/*%< A complete view statement */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_viewopts;
/*%< View options */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_zone;
/*%< A complete zone statement */

LIBISCCFG_EXTERNAL_DATA extern cfg_type_t cfg_type_zoneopts;
/*%< Zone options */

#endif /* ISCCFG_NAMEDCONF_H */
