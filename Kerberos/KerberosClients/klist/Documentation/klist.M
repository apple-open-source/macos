.\" clients/klist/klist.M
.\"
.\" Copyright 1990 by the Massachusetts Institute of Technology.
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
.TH KLIST 1
.SH NAME
klist \- list cached Kerberos tickets
.SH SYNOPSIS
\fBklist\fP [\fB\-5\fP] [\fB\-4\fP] [\fB\-e\fP] [[\fB\-c\fP] [\fB-A\fP]
[\fB\-f\fP] [\fB\-s\fP] [\fB\-a\fP  [\fB\-n\fP]]]
[\fB\-k\fP [\fB\-t\fP] [\fB\-K\fP]]
[\fIcache_name\fP | \fIkeytab_name\fP]
.br
.SH DESCRIPTION
.I Klist
lists the Kerberos principal and Kerberos tickets held in a credentials
cache, or the keys held in a \fBkeytab\fP file.
.SH OPTIONS
.TP
.B \-5
list Kerberos 5 credentials.  This option will not list Kerberos 4 credentials 
unless \fB-4\fP is also specified.
.
.TP
.B \-4
list Kerberos 4 credentials.  This option will not list Kerberos 5 credentials 
unless \fB-5\fP is also specified.
.
.TP
.B \-e
displays the encryption types of the session key and the ticket for each
credential in the credential cache, or each key in the keytab file.
.TP
.B \-c
List tickets held in a credentials cache.  This is the default if
neither \fB-c\fP nor \fB-k\fP is specified.
.TP
.B \-A
lists all caches in the credentials cache.  
.TP
.B \-f
shows the flags present in the credentials, using the following
abbreviations:
.sp
.nf
.in +.5i
F	\fBF\fPorwardable
f	\fBf\fPorwarded
P	\fBP\fProxiable
p	\fBp\fProxy
D	post\fBD\fPateable
d	post\fBd\fPated
R	\fBR\fPenewable
I	\fBI\fPnitial
i	\fBi\fPnvalid
.in -.5i
.fi
.TP
.B \-s
causes \fBklist\fP to run silently (produce no output), but to still set 
the exit status according to whether it finds the credentials cache.  
The exit status is '0' if \fBklist\fP finds a credentials cache, with
a valid ticket granting ticket and '1' if it does not.
.TP
.B \-a
display list of addresses in credentials.
.TP
.B \-n
show numeric addresses instead of reverse-resolving addresses.
.TP
\fB\-k\fP
List keys held in a \fBkeytab\fP file.  
.TP
.B \-t
display the time entry timestamps for each keytab entry in the keytab file.
.TP
.B \-K
display the value of the encryption key in each keytab entry in the keytab file.
.PP
If
.I cache_name
or
.I keytab_name
is not specified, klist will display the credentials in the system default
credentials cache or keytab file as appropriate.
.SH FILES
.TP
/etc/krb5.keytab
default location for the local host's
.B keytab
file.
.SH SEE ALSO
kinit(1), kdestroy(1), kpasswd(1), kswitch(1)
