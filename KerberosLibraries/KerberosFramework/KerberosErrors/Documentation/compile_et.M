.\" compile_et.M
.\"
.\" Copyright 2004 by the Massachusetts Institute of Technology.
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
.TH COMPILE_ET 1
.SH NAME
compile_et \- tool for generating com_err error tables
.SH SYNOPSIS
.B compile_et
[ \fB--base\fP \fItable_base\fP ] 
[ \fB--etout\fP \fIout_dir\fP ] 
[ \fB--stringsout\fP \fIout_strings_dir\fP ] 
\fIerror_table\fP
.br
.SH DESCRIPTION
.I compile_et
is a tool to produce com_err style error code to string mappings for use with 
the com_err error_message() API.
.PP
.I compile_et
takes the com_err error table file \fIerror_table\fP and generates a com_err source and 
header file for use with add_error_table() and remove_error_table().  It also generates an 
optional strings file for use with bundled applications.  If the contents of the strings file 
are added to Localizable.strings, then the application does not need to use add_error_table() 
and remove_error_table(). 
.PP
.I compile_et
is intended to be used by the Kerberos implementation and third party plugins 
(eg: KLLoginLogoutNotification and KLPrincipalTranslation plugins).
.SH OPTIONS
.TP
\fB--etout\fP \fIout_dir\fP
output source, header and strings files to the directory \fIout_dir\fP.  By default 
\fIcompile_et\fP places files in the current working directory.
.TP
\fB--stringsout\fP \fIout_strings_dir\fP
output the strings file to the directory \fIout_strings_dir\fP.  By default \fIcompile_et\fP 
places the strings file in \fIout_dir\fP (if it is specified) or the current working directory.
.TP
\fB--base\fP \fItable_base\fP
use \fItable_base\fP as the error table base name.  This is a four character code
which identifies the error table in the error table database.
.SH SEE ALSO
kerberos(1)
.SH BUGS
Currently \fIcompile_et\fP has no support for generating strings files for multiple
localizations.  Strings files must be manually localized.
