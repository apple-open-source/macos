/*
 * KLPreferences.h
 *
 * $Header$
 *
 * Copyright 2003 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#pragma once

KLBoolean __KLPreferencesGetLibDefaultBoolean (const char *inLibDefaultName, KLBoolean inDefaultBoolean);

KLStatus __KLPreferencesGetKerberosLoginName (char **outName);
KLStatus __KLPreferencesSetKerberosLoginName (const char *inName);

KLStatus __KLPreferencesGetKerberosLoginInstance (char **outInstance);
KLStatus __KLPreferencesSetKerberosLoginInstance (const char *inInstance);

KLStatus __KLPreferencesGetKerberosLoginMinimumTicketLifetime (KLLifetime *outMinimumTicketLifetime);
KLStatus __KLPreferencesSetKerberosLoginMinimumTicketLifetime (KLLifetime inMinimumTicketLifetime);

KLStatus __KLPreferencesGetKerberosLoginMaximumTicketLifetime (KLLifetime *outMaximumTicketLifetime);
KLStatus __KLPreferencesSetKerberosLoginMaximumTicketLifetime (KLLifetime inMaximumTicketLifetime);

KLStatus __KLPreferencesGetKerberosLoginDefaultTicketLifetime (KLLifetime *outDefaultTicketLifetime);
KLStatus __KLPreferencesSetKerberosLoginDefaultTicketLifetime (KLLifetime inDefaultTicketLifetime);

KLStatus __KLPreferencesGetKerberosLoginDefaultRenewableTicket (KLBoolean *outDefaultRenewableTicket);
KLStatus __KLPreferencesSetKerberosLoginDefaultRenewableTicket (KLBoolean inDefaultRenewableTicket);

KLStatus __KLPreferencesGetKerberosLoginMinimumRenewableLifetime (KLLifetime *outMinimumRenewableLifetime);
KLStatus __KLPreferencesSetKerberosLoginMinimumRenewableLifetime (KLLifetime inMinimumRenewableLifetime);

KLStatus __KLPreferencesGetKerberosLoginMaximumRenewableLifetime (KLLifetime *outMaximumRenewableLifetime);
KLStatus __KLPreferencesSetKerberosLoginMaximumRenewableLifetime (KLLifetime inMaximumRenewableLifetime);

KLStatus __KLPreferencesGetKerberosLoginDefaultRenewableLifetime (KLLifetime *outDefaultRenewableLifetime);
KLStatus __KLPreferencesSetKerberosLoginDefaultRenewableLifetime (KLLifetime inDefaultRenewableLifetime);

KLStatus __KLPreferencesGetKerberosLoginDefaultForwardableTicket (KLBoolean *outDefaultForwardableTicket);
KLStatus __KLPreferencesSetKerberosLoginDefaultForwardableTicket (KLBoolean inDefaultForwardableTicket);

KLStatus __KLPreferencesGetKerberosLoginDefaultProxiableTicket (KLBoolean *outDefaultProxiableTicket);
KLStatus __KLPreferencesSetKerberosLoginDefaultProxiableTicket (KLBoolean inDefaultProxiableTicket);

KLStatus __KLPreferencesGetKerberosLoginDefaultAddresslessTicket (KLBoolean *outAddresslessTicket);
KLStatus __KLPreferencesSetKerberosLoginDefaultAddresslessTicket (KLBoolean inDefaultAddresslessTicket);

KLStatus __KLPreferencesGetKerberosLoginShowOptions (KLBoolean *outShowOptions);
KLStatus __KLPreferencesSetKerberosLoginShowOptions (KLBoolean inShowOptions);

KLStatus __KLPreferencesGetKerberosLoginLongLifetimeDisplay (KLBoolean *outLongLifetimeDisplay);
KLStatus __KLPreferencesSetKerberosLoginLongLifetimeDisplay (KLBoolean inLongTimeDisplay);

KLStatus __KLPreferencesGetKerberosLoginRememberShowOptions (KLBoolean *outRememberShowOptions);
KLStatus __KLPreferencesSetKerberosLoginRememberShowOptions (KLBoolean inRememberShowOptions);

KLStatus __KLPreferencesGetKerberosLoginRememberPrincipal (KLBoolean *outRememberPrincipal);
KLStatus __KLPreferencesSetKerberosLoginRememberPrincipal (KLBoolean inRememberPrincipal);

KLStatus __KLPreferencesGetKerberosLoginRememberExtras (KLBoolean *outRememberExtras);
KLStatus __KLPreferencesSetKerberosLoginRememberExtras (KLBoolean inRememberExtras);

KLStatus __KLPreferencesGetKerberosLoginRealm (KLIndex inIndex, char **outRealm);
KLStatus __KLPreferencesGetKerberosLoginRealmByName (const char *inName, KLIndex *outIndex);
KLStatus __KLPreferencesSetKerberosLoginRealm (KLIndex inIndex, const char *inName);
KLStatus __KLPreferencesRemoveKerberosLoginRealm (KLIndex inIndex);
KLStatus __KLPreferencesInsertKerberosLoginRealm (KLIndex inIndex, const char *inName);
KLStatus __KLPreferencesRemoveAllKerberosLoginRealms (void);
KLStatus __KLPreferencesCountKerberosLoginRealms (KLIndex *outIndex);

KLStatus __KLPreferencesGetKerberosLoginDefaultRealm (KLIndex *outIndex);
KLStatus __KLPreferencesGetKerberosLoginDefaultRealmByName (char **outDefaultRealm);
KLStatus __KLPreferencesSetKerberosLoginDefaultRealm (KLIndex inIndex);
KLStatus __KLPreferencesSetKerberosLoginDefaultRealmByName (const char *inName);
