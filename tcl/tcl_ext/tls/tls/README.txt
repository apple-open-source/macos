Original TLS Copyright (C) 1997-2000 Matt Newman <matt@novadigm.com>
TLS 1.4.1    Copyright (C) 2000 Ajuba Solutions
TLS 1.6      Copyright (C) 2008 ActiveState Software Inc.

$Header: /cvsroot/tls/tls/README.txt,v 1.7 2008/03/19 22:49:12 hobbs2 Exp $

TLS (aka SSL) Channel - can be layered on any bi-directional Tcl_Channel.

Both client and server-side sockets are possible, and this code should work
on any platform as it uses a generic mechanism for layering on SSL and Tcl.

Full filevent sematics should also be intact - see tests directory for
blocking and non-blocking examples.

The current release is TLS 1.6, with binaries built against OpenSSL 0.9.8g.
For best security and function, always compile from source with the latest
official release of OpenSSL (http://www.openssl.org/).

TLS requires Tcl 8.2.0+, with 8.3.2+ preferred.  The stacked channel
implementation in Tcl was originally introduced in 8.2.0 (previously the
Trf patch) and rewritten for 8.3.2+ due to inherent limitations in the
earlier implementation.  TLS should compile with any stubs-capable Tcl
interpreter, but will require 8.2+ when loaded.  There are known
limitations in the 8.2.0-8.3.1 stacked channel implementation, so it is
encouraged that people use TLS with an 8.3.2+ Tcl interpreter.  These
modifications are by Jeff Hobbs.

Non-exclusive credits for TLS are:
   Original work: Matt Newman @ Novadigm
   Updates: Jeff Hobbs @ ActiveState
   Tcl Channel mechanism: Andreas Kupries
   Impetus/Related work: tclSSL (Colin McCormack, Shared Technology)
                         SSLtcl (Peter Antman)

This code is licensed under the same terms as the Tcl Core.
