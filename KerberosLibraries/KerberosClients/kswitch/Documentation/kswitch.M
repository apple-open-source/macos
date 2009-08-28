.\" kswitch.M
.\"
.\" Copyright 2002 by the Massachusetts Institute of Technology.
.\"
.\" Export of this software from the United States of America may
.\"   require a specific license from the United States Government.
.\"   It is the responsibility of any person or organization contemplating
.\"   export to obtain such a license before exporting.
.\" 
.\" WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
.\" distribute this software and its documentation for any purpose and
.\" without fee is hereby granted, provided that the above copyright
.\" notice appear in all copies and that both that copyright notice and
.\" this permission notice appear in supporting documentation, and that
.\" the name of M.I.T. not be used in advertising or publicity pertaining
.\" to distribution of the software without specific, written prior
.\" permission.  Furthermore if you modify this software you must label
.\" your software as modified software and not distribute it in such a
.\" fashion that it might be confused with the original M.I.T. software.
.\" M.I.T. makes no representations about the suitability of
.\" this software for any purpose.  It is provided "as is" without express
.\" or implied warranty.
.\" "
.TH KSWITCH 1
.SH NAME
kswitch \- set the Kerberos system default credentials cache
.SH SYNOPSIS
.B kswitch
[\fB\-c\fP \fIcache_name\fP | \fB\-p\fP \fIprincipal_name\fP]
.br
.SH DESCRIPTION
The
.I kswitch
utility sets the user's system default credentials cache either from
a credentials cache name or a principal name.  kswitch will set the system
default cache for both Kerberos 4 and Kerberos 5.
.SH OPTIONS
.TP
\fB\-c\fP \fIcache_name\fP
set
.I cache_name
to the system default cache. This option cannot be combined with \fB-p\fP.
.TP
\fB\-p\fP \fIprincipal_name\fP
set the first cache containing tickets for principal \fIprincipal_name\fP
to the system default cache. \fIprincipal_name\fP
should be in the Kerberos 5 principal format.  This option 
cannot be combined with \fB-c\fP.
.SH SEE ALSO
kinit(1), klist(1), kpasswd(1), kdestroy(1)
