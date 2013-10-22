package Crypt::SSLeay;

use strict;
use vars '$VERSION';
$VERSION = '0.64';

eval {
    require XSLoader;
    XSLoader::load('Crypt::SSLeay', $VERSION);
    1;
}
or do {
    require DynaLoader;
    use vars '@ISA'; # not really locally scoped, it just looks that way
    @ISA = qw(DynaLoader);
    bootstrap Crypt::SSLeay $VERSION;
};

use vars qw(%CIPHERS);
%CIPHERS = (
   'NULL-MD5'     => "No encryption with a MD5 MAC",
   'RC4-MD5'      => "128 bit RC4 encryption with a MD5 MAC",
   'EXP-RC4-MD5'  => "40 bit RC4 encryption with a MD5 MAC",
   'RC2-CBC-MD5'  => "128 bit RC2 encryption with a MD5 MAC",
   'EXP-RC2-CBC-MD5' => "40 bit RC2 encryption with a MD5 MAC",
   'IDEA-CBC-MD5' => "128 bit IDEA encryption with a MD5 MAC",
   'DES-CBC-MD5'  => "56 bit DES encryption with a MD5 MAC",
   'DES-CBC-SHA'  => "56 bit DES encryption with a SHA MAC",
   'DES-CBC3-MD5' => "192 bit EDE3 DES encryption with a MD5 MAC",
   'DES-CBC3-SHA' => "192 bit EDE3 DES encryption with a SHA MAC",
   'DES-CFB-M1'   => "56 bit CFB64 DES encryption with a one byte MD5 MAC",
);

use Crypt::SSLeay::X509;

# A xsupp bug made this nessesary
sub Crypt::SSLeay::CTX::DESTROY  { shift->free; }
sub Crypt::SSLeay::Conn::DESTROY { shift->free; }
sub Crypt::SSLeay::X509::DESTROY { shift->free; }

1;

__END__

=head1 NAME

Crypt::SSLeay - OpenSSL support for LWP

=head1 SYNOPSIS

    lwp-request https://www.example.com

    use LWP::UserAgent;
    my $ua  = LWP::UserAgent->new;
    my $response = $ua->get('https://www.example.com/');
    print $response->content, "\n";

=head1 DESCRIPTION

This Perl module provides support for the HTTPS protocol under LWP,
to allow an C<LWP::UserAgent> object to perform GET, HEAD and POST
requests. Please see LWP for more information on POST requests.

The C<Crypt::SSLeay> package provides C<Net::SSL>, which is loaded
by C<LWP::Protocol::https> for https requests and provides the
necessary SSL glue.

This distribution also makes following deprecated modules available:

    Crypt::SSLeay::CTX
    Crypt::SSLeay::Conn
    Crypt::SSLeay::X509

Work on Crypt::SSLeay has been continued only to provide https
support for the LWP (libwww-perl) libraries.

=head1 ENVIRONMENT VARIABLES

The following environment variables change the way
C<Crypt::SSLeay> and C<Net::SSL> behave.

=head2 Proxy Support

    $ENV{HTTPS_PROXY} = 'http://proxy_hostname_or_ip:port';

=head2 Proxy Basic Authentication

    $ENV{HTTPS_PROXY_USERNAME} = 'username';
    $ENV{HTTPS_PROXY_PASSWORD} = 'password';

=head2 SSL diagnostics and Debugging

    $ENV{HTTPS_DEBUG} = 1;

=head2 Default SSL Version

    $ENV{HTTPS_VERSION} = '3';

=head2 Client Certificate Support

    $ENV{HTTPS_CERT_FILE} = 'certs/notacacert.pem';
    $ENV{HTTPS_KEY_FILE}  = 'certs/notacakeynopass.pem';

=head2 CA cert Peer Verification

    $ENV{HTTPS_CA_FILE}   = 'certs/ca-bundle.crt';
    $ENV{HTTPS_CA_DIR}    = 'certs/';

=head2 Client PKCS12 cert support

    $ENV{HTTPS_PKCS12_FILE}     = 'certs/pkcs12.pkcs12';
    $ENV{HTTPS_PKCS12_PASSWORD} = 'PKCS12_PASSWORD';

=head1 INSTALL

=head2 OpenSSL

You must have OpenSSL installed before compiling this module. You can get
the latest OpenSSL package from L<http://www.openssl.org/>. We no longer
support pre-2000 versions of OpenSSL.

If you are building OpenSSL from source, please follow the directions
included in the package.

If you are going to use an OpenSSL library which you built from source or
whose header and library files are not in a place searched by your compiler
by default, make sure you set appropriate environment variables before
trying to build C<Crypt::SSLeay>.

For example, if you are using ActiveState Perl and MinGW installed using
ppm, and you installed OpenSSL in C<C:\opt\openssl-1.0.1c>, then you would
issue the following commands to build C<Crypt::SSLeay>:

    C:\...\> set LIBRARY_PATH=C:\opt\openssl-1.0.1c\lib;%LIBRARY_PATH%
    C:\...\> set CPATH=C:\opt\openssl-1.0.1c\include;%CPATH%
    C:\...\> perl Makefile.PL --live-tests
    C:\...\> dmake test

On Linux/BSD/Solaris/GNU etc systems, you would use make rather than dmake,
but you would need to set the same variables if your OpenSSL library is in a
custom location. If everything builds OK, but you get failures when during
tests, ensure that C<LD_LIBRARY_PATH> points to the location where the
correct shared libraries are located.

If you are using a Microsoft compiler (keep in mind that perl and OpenSSL
need to have been built using the same compiler as well), you would use:

    C:\...\> set LIB=C:\opt\openssl-1.0.1c\lib;%LIB%
    C:\...\> set INCLUDE=C:\opt\openssl-1.0.1c\include;%INCLUDE%
    C:\...\> perl Makefile.PL --live-tests
    C:\...\> nmake test

Depending on your OS, pre-built OpenSSL packages may be available. You may
need to install a development version of your operating system's OpenSSL
library package. The key is that C<Crypt::SSLeay> makes calls to the OpenSSL
library, and how to do so is specified in the C header files that come
with the library. Some systems break out the header files into a separate
package from that of the libraries. Once the program has been built, you
don't need the headers any more.

=head2 Crypt::SSLeay

The latest Crypt::SSLeay can be found at your nearest CPAN, as well as
L<http://search.cpan.org/dist/Crypt-SSLeay/>.

Once you have downloaded it, C<Crypt::SSLeay> installs easily using the
standard build process:

    perl Makefile.PL
    make
    make test
    make install

On Windows systems, both Strawberry Perl and ActiveState (as a separate
download via ppm) projects include a MingW based compiler distribution and
dmake which can be used to build both OpenSSL and C<Crypt::SSLeay>. If you
have such a set up, use dmake above.

F<Makefile.PL> takes two optional arguments:

=over 4

=item C<--live-tests>

Boolean. Specifies whether we should try to connect to an HTTPS URL during
testing. Default is false.

To skip live tests, you can use

    perl Makefile.PL --no-live-tests

and to force live tests, you can use

    perl Makefile.PL --live-tests

=item C<--static>

Boolean. Default is false. (B<TODO>: Does it work?)

=back

For unattended (batch) installations, to be absolutely certain that
F<Makefile.PL> does not prompt for questions on STDIN, set the environment
variable C<PERL_MM_USE_DEFAULT=1> as with any CPAN module built using
L<ExtUtils::MakeMaker>.

=head3 Windows

C<Crypt::SSLeay> builds correctly with Strawberry Perl and ActiveState Perl
using the bundled MinGW.

For ActiveState Perl users, the ActiveState company does not have a permit
from the Canadian Federal Government to distribute cryptographic software.
This prevents C<Crypt::SSLeay> from being distributed as a PPM package from
their repository.

See L<http://docs.activestate.com/activeperl/5.16/faq/ActivePerl-faq2.html#crypto_packages>
for more information on this issue. You may be able to download a PPM for
C<Crypt::SSLeay> from an alternative repository (see L<PPM::Repositories>).

=head3 VMS

I do not have any experience with VMS. If OpenSSL headers and libraries are
not in standard locations searched by your build system by default, please
set things up so that they are. If you have generic instructions on how to
do it, please open a ticket on RT with the information so I can add it to
this document.

=head1 PROXY SUPPORT

L<LWP::UserAgent> and L<Crypt::SSLeay> have their own versions of
proxy support. Please read these sections to see which one
is appropriate.

=head2 LWP::UserAgent proxy support

C<LWP::UserAgent> has its own methods of proxying which may work for you
and is likely to be incompatible with C<Crypt::SSLeay> proxy support.
To use C<LWP::UserAgent> proxy support, try something like:

    my $ua = LWP::UserAgent->new;
    $ua->proxy([qw( https http )], "$proxy_ip:$proxy_port");

At the time of this writing, libwww v5.6 seems to proxy https requests
fine with an Apache F<mod_proxy> server.  It sends a line like:

    GET https://www.example.com HTTP/1.1

to the proxy server, which is not the C<CONNECT> request that some
proxies would expect, so this may not work with other proxy servers than
F<mod_proxy>. The C<CONNECT> method is used by C<Crypt::SSLeay>'s
internal proxy support.

=head2 Crypt::SSLeay proxy support

For native C<Crypt::SSLeay> proxy support of https requests,
you need to set the environment variable C<HTTPS_PROXY> to your
proxy server and port, as in:

    # proxy support
    $ENV{HTTPS_PROXY} = 'http://proxy_hostname_or_ip:port';
    $ENV{HTTPS_PROXY} = '127.0.0.1:8080';

Use of the C<HTTPS_PROXY> environment variable in this way
is similar to C<LWP::UserAgent->env_proxy()> usage, but calling
that method will likely override or break the C<Crypt::SSLeay>
support, so do not mix the two.

Basic auth credentials to the proxy server can be provided
this way:

    # proxy_basic_auth
    $ENV{HTTPS_PROXY_USERNAME} = 'username';
    $ENV{HTTPS_PROXY_PASSWORD} = 'password';

For an example of LWP scripting with C<Crypt::SSLeay> native proxy
support, please look at the F<eg/lwp-ssl-test> script in the
C<Crypt::SSLeay> distribution.

=head1 CLIENT CERTIFICATE SUPPORT

Client certificates are supported. PEM encoded certificate and
private key files may be used like this:

    $ENV{HTTPS_CERT_FILE} = 'certs/notacacert.pem';
    $ENV{HTTPS_KEY_FILE}  = 'certs/notacakeynopass.pem';

You may test your files with the F<eg/net-ssl-test> program,
bundled with the distribution, by issuing a command like:

    perl eg/net-ssl-test -cert=certs/notacacert.pem \
        -key=certs/notacakeynopass.pem -d GET $HOST_NAME

Additionally, if you would like to tell the client where
the CA file is, you may set these.

    $ENV{HTTPS_CA_FILE} = "some_file";
    $ENV{HTTPS_CA_DIR}  = "some_dir";

Note that, if specified, C<$ENV{HTTPS_CA_FILE}> must point to the actual
certificate file. That is, C<$ENV{HTTPS_CA_DIR}> is *not* the path were
C<$ENV{HTTPS_CA_FILE}> is located.

For certificates in C<$ENV{HTTPS_CA_DIR}> to be picked up, follow the
instructions on
L<http://www.openssl.org/docs/ssl/SSL_CTX_load_verify_locations.html>

There is no sample CA cert file at this time for testing,
but you may configure F<eg/net-ssl-test> to use your CA cert
with the -CAfile option.

(TODO: then what is the F<./certs> directory in the distribution?)

=head2 Creating a test certificate

To create simple test certificates with OpenSSL, you may
run the following command:

    openssl req -config /usr/local/openssl/openssl.cnf \
        -new -days 365 -newkey rsa:1024 -x509 \
        -keyout notacakey.pem -out notacacert.pem

To remove the pass phrase from the key file, run:

    openssl rsa -in notacakey.pem -out notacakeynopass.pem

=head2 PKCS12 support

The directives for enabling use of PKCS12 certificates is:

    $ENV{HTTPS_PKCS12_FILE}     = 'certs/pkcs12.pkcs12';
    $ENV{HTTPS_PKCS12_PASSWORD} = 'PKCS12_PASSWORD';

Use of this type of certificate takes precedence over previous
certificate settings described.

(TODO: unclear? Meaning "the presence of this type of certificate"?)

=head1 SSL versions

C<Crypt::SSLeay> tries very hard to connect to I<any> SSL web server
accomodating servers that are buggy, old or simply not
standards-compliant. To this effect, this module will try SSL
connections in this order:

=over 4

=item SSL v23

should allow v2 and v3 servers to pick their best type

=item SSL v3

best connection type

=item SSL v2

old connection type

=back

Unfortunately, some servers seem not to handle a reconnect to SSL v3 after a
failed connect of SSL v23 is tried, so you may set before using LWP or
Net::SSL:

    $ENV{HTTPS_VERSION} = 3;

to force a version 3 SSL connection first. At this time only a
version 2 SSL connection will be tried after this, as the connection
attempt order remains unchanged by this setting.

=head1 ACKNOWLEDGEMENTS

Many thanks to the following individuals who helped improve
C<Crypt-SSLeay>:

I<Gisle Aas> for writing this module and many others including libwww, for
perl. The web will never be the same :)

I<Ben Laurie> deserves kudos for his excellent patches for better error
handling, SSL information inspection, and random seeding.

I<Dongqiang Bai> for host name resolution fix when using a proxy.

I<Stuart Horner> of Core Communications, Inc. who found the need for
building C<--shared> OpenSSL libraries.

I<Pavel Hlavnicka> for a patch for freeing memory when using a pkcs12
file, and for inspiring more robust C<read()> behavior.

I<James Woodyatt> is a champ for finding a ridiculous memory leak that
has been the bane of many a Crypt::SSLeay user.

I<Bryan Hart> for his patch adding proxy support, and thanks to I<Tobias
Manthey> for submitting another approach.

I<Alex Rhomberg> for Alpha linux ccc patch.

I<Tobias Manthey> for his patches for client certificate support.

I<Daisuke Kuroda> for adding PKCS12 certificate support.

I<Gamid Isayev> for CA cert support and insights into error messaging.

I<Jeff Long> for working through a tricky CA cert SSLClientVerify issue.

I<Chip Turner> for a patch to build under perl 5.8.0.

I<Joshua Chamas> for the time he spent maintaining the module.

I<Jeff Lavallee> for help with alarms on read failures (CPAN bug #12444).

I<Guenter Knauf> for significant improvements in configuring things in
Win32 and Netware lands and Jan Dubois for various suggestions for
improvements.

and I<many others> who provided bug reports, suggestions, fixes and
patches.

=head1 SEE ALSO

=over 4

=item Net::SSL

If you have downloaded this distribution as of a dependency of another
distribution, it's probably due to this module (which is included in
this distribution).

=item Net::SSLeay

L<Net::SSLeay> provides access to the OpenSSL API directly
from Perl. See L<http://search.cpan.org/dist/Net-SSLeay/>.

=item OpenSSL binary packages for Windows

See L<http://www.openssl.org/related/binaries.html>.

=back

=head1 SUPPORT

For use of C<Crypt::SSLeay> & C<Net::SSL> with Perl's L<LWP>, please
send email to C<libwww@perl.org>.

For OpenSSL or general SSL support, including issues associated with
building and installing OpenSSL on your system, please email the OpenSSL
users mailing list at C<openssl-users@openssl.org>. See
L<http://www.openssl.org/support/community.html> for other mailing lists
and archives.

Please report all bugs using
L<rt.cpan.org|http://rt.cpan.org/NoAuth/Bugs.html?Dist=Crypt-SSLeay>.

=head1 AUTHORS

This module was originally written by Gisle Aas, and was subsequently
maintained by Joshua Chamas, David Landgren, brian d foy and Sinan Unur.

=head1 COPYRIGHT

Copyright (c) 2010-2012 A. Sinan Unur

Copyright (c) 2006-2007 David Landgren

Copyright (c) 1999-2003 Joshua Chamas

Copyright (c) 1998 Gisle Aas

=head1 LICENSE

This program is free software; you can redistribute it and/or modify it
under the terms of Artistic License 2.0 (see
L<http://www.perlfoundation.org/artistic_license_2_0>).

=cut
