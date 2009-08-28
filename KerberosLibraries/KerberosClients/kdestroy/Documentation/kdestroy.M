.\" clients/kdestroy/kdestroy.M
.\"
.\" Copyright 1992 by the Massachusetts Institute of Technology.
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
.TH KDESTROY 1
.SH NAME
kdestroy \- destroy Kerberos tickets
.SH SYNOPSIS
.B kdestroy
[\fB\-q\fP] [\fB\-a\fP | \fB-A\fP | \fB\-c\fP \fIcache_name\fP | 
\fB\-p\fP \fIprincipal_name\fP]
.br
.SH DESCRIPTION
The
.I kdestroy
utility destroys the user's active Kerberos authentication tickets by
writing zeros to the specified credentials cache that contains them.  If
the credentials cache is not specified, the default credentials cache is
destroyed.
kdestroy will destroy both Kerberos 4 and Kerberos 5 tickets if they
are present.
.SH OPTIONS
.TP
.B \-q
Run quietly.  Normally \fBkdestroy\fP
prints an error if it fails to destroy the user's tickets.  The
\fB\-q\fP flag suppresses this behavior.
.TP
.B \-a
Destroy all credentials caches (same as \fB-A\fP).  This option cannot 
be combined with \fB-c\fP or \fB-p\fP.
.TP
.B \-A
Destroy all credentials caches (same as \fB-a\fP).  This option cannot 
be combined with \fB-c\fP or \fB-p\fP.
.TP
\fB\-c\fP \fIcache_name\fP
Use
.I cache_name
as the credentials (ticket) cache name and location. This option cannot 
be combined with \fB-a\fP, \fB-A\fP or \fB-p\fP.
.sp
The default credentials cache may vary between systems.
.TP
\fB\-p\fP \fIprincipal_name\fP
Destroy all credentials caches which contain tickets for
\fIprincipal_name\fP. \fIprincipal_name\fP
should be in the Kerberos 5 principal format.  This option 
cannot be combined with \fB-a\fP, \fB-A\fP or \fB-c\fP.
.PP
Most Kerberos installations recommend that you place the
\fIkdestroy -A\fP command in your
.I .logout
file, so that your tickets are destroyed 
automatically when you log out.
.SH SEE ALSO
kinit(1), klist(1), kpasswd(1), kswitch(1)
