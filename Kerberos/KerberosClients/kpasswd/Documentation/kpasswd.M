.\" clients/kpasswd/kpasswd.M
.\" 
.\" Copyright 1995 by the Massachusetts Institute of Technology.
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
.TH KPASSWD 1 \*h
.SH NAME
kpasswd \- change a user's Kerberos password
.SH SYNOPSIS
.B kpasswd
[\fIprincipal\fP]
.SH DESCRIPTION
.PP
The kpasswd command is used to change a Kerberos principal's password.
kpasswd prompts for the current Kerberos password, which is used to 
obtain a \fIchangepw\fP ticket from the KDC for the user's Kerberos realm.  
If kpasswd successfully obtains the \fIchangepw\fP ticket, the user 
is prompted twice for the new password, and the password is changed.
.PP
If the principal is governed by a policy that specifies the length and/or
number of character classes required in the new password, the new
password must conform to the policy.  (The five character classes are
lower case, upper case, numbers, punctuation, and all other characters.)
.SH OPTIONS
.TP
.I principal
change the password for the Kerberos principal \fIprincipal\fP.
Otherwise, \fIkpasswd\fP uses the principal name from the system 
default ccache if there is one; if not, the principal is derived 
from the identity of the user invoking the kpasswd command.
.SH PORTS
kpasswd looks first for kpasswd_server = host:port in the 
[realms] section of the edu.mit.Kerberos file under the current realm.  
If that is missing, kpasswd looks for the admin_server entry, 
but substitutes 464 for the port.
.SH SEE ALSO
kinit(1), klist(1), kdestroy(1), kswitch(1)
