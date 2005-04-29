Copyright (C) 1997-2000 Matt Newman <matt@novadigm.com>
TLS 1.4.1 Copyright (C) 2000 Ajuba Solutions

$Header: /cvsroot/tls/tls/README.txt,v 1.6 2004/02/17 21:27:20 razzell Exp $

TLS (aka SSL) Channel - can be layered on any bi-directional Tcl_Channel.

Both client and server-side sockets are possible, and this code should work
on any platform as it uses a generic mechanism for layering on SSL and Tcl.

Full filevent sematics should also be intact - see tests directory for
blocking and non-blocking examples.

The current release is TLS 1.5.0, with binaries built against OpenSSL 0.9.7c.
For best security and function, always compile from source use the latest
official release of OpenSSL.

The TLS 1.4 release requires Tcl 8.2.0+, with 8.3.2+ preferred.  The
stacked channel implementation in Tcl was originally introduced in 8.2.0
(previously the Trf patch) and rewritten for 8.3.2+ due to inherent
limitations in the earlier implementation.  TLS 1.4 should compile with
any stubs-capable Tcl interpreter, but will require 8.2+ when loaded.
There are known limitations in the 8.2.0-8.3.1 stacked channel
implementation, so it is encouraged that people use TLS 1.4+ with an
8.3.2+ Tcl interpreter.  These modifications are by Jeff Hobbs
<jeff@hobbs.org>.

Addition credit is due for Andreas Kupries (a.kupries@westend.com), for
providing the Tcl_ReplaceChannel mechanism and working closely with me
to enhance it to support full fileevent semantics.

Also work done by the follow people provided the impetus to do this "right":-
tclSSL (Colin McCormack, Shared Technology)
SSLtcl (Peter Antman)

This code is licensed under the same terms as the Tcl Core.

I would also like to acknowledge the input of Marshall Rose, who convinced 
me that people need to be able to switch-to-encrypted mode part way
through a conversation.

Also I would like to acknowledge the kind support of Novadigm Inc, my
current employer, which made this possible.


Matt Newman 
