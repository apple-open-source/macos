# Crypt::SSLeay - OpenSSL support for LWP

## Synopsis

    lwp-request https://www.example.com

    use LWP::UserAgent;
    my $ua  = LWP::UserAgent->new;
    my $response = $ua->get('https://www.example.com/');
    print $response->content, "\n";

## Description

this Perl module provides support for the HTTPS protocol under LWP, to allow
an "LWP::UserAgent" object to perform GET, HEAD and POST requests.  Please
see LWP for more information on POST requests.

The "Crypt::SSLeay" package provides "Net::SSL", which is loaded by
"LWP::Protocol::https" for https requests and provides the necessary SSL
glue.

This distribution also makes following deprecated modules available:

    Crypt::SSLeay::CTX
    Crypt::SSLeay::Conn
    Crypt::SSLeay::X509

Work on Crypt::SSLeay has been continued only to provide https support
for the LWP (libwww-perl) libraries.

## Environment Variables

the following environment variables change the way "Crypt::SSLeay" and
"Net::SSL" behave.

### Proxy Support

    $ENV{HTTPS_PROXY} = 'http://proxy_hostname_or_ip:port';

### Proxy Basic Authentication

    $ENV{HTTPS_PROXY_USERNAME} = 'username';
    $ENV{HTTPS_PROXY_PASSWORD} = 'password';

### SSL Diagnostics and Debugging

    $ENV{HTTPS_DEBUG} = 1;

### Default SSL Version

    $ENV{HTTPS_VERSION} = '3';

### Client Certificate Support

    $ENV{HTTPS_CERT_FILE} = 'certs/notacacert.pem';
    $ENV{HTTPS_KEY_FILE}  = 'certs/notacakeynopass.pem';

### CA cert peer verification

    $ENV{HTTPS_CA_FILE}   = 'certs/ca-bundle.crt';
    $ENV{HTTPS_CA_DIR}    = 'certs/';

### Client PKCS12 cert support
    $ENV{HTTPS_PKCS12_FILE}     = 'certs/pkcs12.pkcs12';
    $ENV{HTTPS_PKCS12_PASSWORD} = 'PKCS12_PASSWORD';

## Installation

### OpenSSL

You must have OpenSSL installed before compiling this module.  You can
get the latest OpenSSL package from <http://www.openssl.org/>. We no longer
support pre-2000 versions of OpenSSL.

If you are building OpenSSL from source, please follow the directions
included in the package.

If you are going to use an OpenSSL library which you built from source or
whose header and library files are not in a place searched by your compiler
by default, make sure you set appropriate environment variables before
trying to build `Crypt::SSLeay`.

For example, if you are using ActiveState Perl and MinGW installed using
ppm, and you installed OpenSSL in `C:\opt\openssl-1.0.1c`, then you would
issue the following commands to build `Crypt::SSLeay`:

    C:\temp\Crypt-SSLeay> set LIBRARY_PATH=C:\opt\openssl-1.0.1c\lib;%LIBRARY_PATH%
    C:\temp\Crypt-SSLeay> set CPATH=C:\opt\openssl-1.0.1c\include;%CPATH%
    C:\temp\Crypt-SSLeay> perl Makefile.PL --live-tests
    C:\temp\Crypt-SSLeay> dmake test

On Linux/BSD/Solaris/GNU etc systems, you would use `make` rather than
`dmake`, but you would need to set the same variables if your OpenSSL
library is in a custom location. If everything builds OK, but you get
failures when during tests, ensure that `LD_LIBRARY_PATH` points to the
location where the correct shared libraries are located.

If you are using a Microsoft compiler (keep in mind that `perl` and OpenSSL
need to have been built using the same compiler as well), you would use:

    C:\temp\Crypt-SSLeay> set LIB=C:\opt\openssl-1.0.1c\lib;%LIB%
    C:\temp\Crypt-SSLeay> set INCLUDE=C:\opt\openssl-1.0.1c\include;%INCLUDE%
    C:\temp\Crypt-SSLeay> perl Makefile.PL --live-tests
    C:\temp\Crypt-SSLeay> nmake test

Depending on your OS, pre-built OpenSSL packages may be available. You may
need to install a development version of your operating system's OpenSSL
library package. The key is that Crypt::SSLeay makes calls to the OpenSSL
library, and how to do so is specified in the C header files that come
with the library. Some systems break out the header files into a separate
package from that of the libraries. Once the program has been built, you
don't need the headers any more.

### `Crypt::SSLeay`

The latest `Crypt::SSLeay` can be found at your nearest CPAN, as well as
<http://search.cpan.org/dist/Crypt-SSLeay/>.

Once you have downloaded it, `Crypt::SSLeay` installs easily using the
standard build process:

    perl Makefile.PL
    make
    make test
    make install

On Windows systems, both Strawberry Perl and ActiveState (as a separate
download via ppm) projects include a MingW based compiler distribution and
dmake which can be used to build both OpenSSL and `Crypt::SSLeay`. If you have
such a set up, use dmake above.

Makefile.PL takes two optional arguments:

*   `--live-tests` : Boolean. Specifies whether we should try to connect to
    an HTTPS URL during testing. Default is false.

    To skip live tests, you can use

        perl Makefile.PL --no-live-tests

    and to force live tests, you can use

        perl Makefile.PL --live-tests

*   `--static` : Boolean. Default is false. (TODO: Does it work?)

For unattended (batch) installations, to be absolutely certain that
`Makefile.PL` does not prompt for questions on `STDIN`, set the environment
variable `PERL_MM_USE_DEFAULT=1` as with any CPAN module built using
`ExtUtils::MakeMaker`.

### Windows

`Crypt::SSLeay` builds correctly with Strawberry Perl and ActiveState Perl
using the bundled MinGW.

For ActiveState Perl users, the ActiveState company does not have a
permit from the Canadian Federal Government to distribute cryptographic
software. This prevents `Crypt::SSLeay` from being distributed as a PPM
package from their repository. See <http://docs.activestate.com/activeperl/5.16/faq/ActivePerl-faq2.html#crypto_packages>
for more information on this issue. You may be able to download a PPM for
`Crypt::SSLeay` from an alternative repository (see `PPM::Repositories`).

### VMS

I do not have any experience with VMS. If OpenSSL headers and libraries are
not in standard locations searched by your build system by default, please
set things up so that they are. If you have generic instructions on how to
do it, please open a ticket on RT with the information so I can add it to
this document.

## Proxy Support

`LWP::UserAgent` and `Crypt::SSLeay` have their own versions of proxy
support.  Please read these sections to see which one is appropriate.

### `LWP::UserAgent` proxy support

`LWP::UserAgent` has its own methods of proxying which may work for you and
is likely to be incompatible with `Crypt::SSLeay` proxy support. To use
`LWP::UserAgent` proxy support, try something like:

    my $ua = LWP::UserAgent->new;
    $ua->proxy([qw( https http )], "$proxy_ip:$proxy_port");

At the time of this writing, libwww v5.6 seems to proxy https requests fine
with an Apache mod_proxy server. It sends a line like:

    GET https://www.example.com HTTP/1.1

to the proxy server, which is not the `CONNECT` request that some proxies
would expect, so this may not work with other proxy servers than mod_proxy.
The `CONNECT` method is used by `Crypt::SSLeay`'s internal proxy support.

### `Crypt::SSLeay` proxy support

For native `Crypt::SSLeay` proxy support of https requests, you need to set
the environment variable `HTTPS_PROXY` to your proxy server and port, as
in:

    # proxy support
    $ENV{HTTPS_PROXY} = 'http://proxy_hostname_or_ip:port';
    $ENV{HTTPS_PROXY} = '127.0.0.1:8080';

Use of the `HTTPS_PROXY` environment variable in this way is similar to
`LWP::UserAgent->env_proxy()` usage, but calling that method will likely
override or break the `Crypt::SSLeay` support, so do not mix the two.

Basic authentication credentials to the proxy server can be provided this
way:

    # proxy_basic_auth
    $ENV{HTTPS_PROXY_USERNAME} = 'username';
    $ENV{HTTPS_PROXY_PASSWORD} = 'password';

For an example of LWP scripting with `Crypt::SSLeay` native proxy support,
please look at the `eg/lwp-ssl-test` script in the `Crypt::SSLeay`
distribution.

## Client Certificate Support

Client certificates are supported. PEM encoded certificate and private key
files may be used like this:

    $ENV{HTTPS_CERT_FILE} = 'certs/notacacert.pem';
    $ENV{HTTPS_KEY_FILE}  = 'certs/notacakeynopass.pem';

You may test your files with the `eg/net-ssl-test` program, bundled with the
distribution, by issuing a command like:

    perl eg/net-ssl-test -cert=certs/notacacert.pem \
        -key=certs/notacakeynopass.pem -d GET $HOST_NAME

Additionally, if you would like to tell the client where the CA file is, you
may set these.

        $ENV{HTTPS_CA_FILE} = "some_file";
        $ENV{HTTPS_CA_DIR}  = "some_dir";

Note that, if specified, `$ENV{HTTPS_CA_FILE}` must point to the actual
certificate file. That is, `$ENV{HTTPS_CA_DIR}` is *not* the path where
`$ENV{HTTPS_CA_FILE}` is located.

For certificates in `$ENV{HTTPS_CA_DIR}` to be picked up, follow the
instructions on <http://www.openssl.org/docs/ssl/SSL_CTX_load_verify_locations.html>.

There is no sample CA cert file at this time for testing, but you may
configure `eg/net-ssl-test` to use your CA cert with the -CAfile option.
(TODO: then what is the ./certs directory in the distribution?)

### Creating a test certificate

To create simple test certificates with OpenSSL, you may run the following
command:

    openssl req -config /usr/local/openssl/openssl.cnf \
        -new -days 365 -newkey rsa:1024 -x509 \
        -keyout notacakey.pem -out notacacert.pem

To remove the pass phrase from the key file, run:

    openssl rsa -in notacakey.pem -out notacakeynopass.pem

### PKCS12 support

The directives for enabling use of PKCS12 certificates is:

    $ENV{HTTPS_PKCS12_FILE}     = 'certs/pkcs12.pkcs12';
    $ENV{HTTPS_PKCS12_PASSWORD} = 'PKCS12_PASSWORD';

Use of this type of certificate takes precedence over previous certificate
settings described. (TODO: unclear? Meaning "the presence of this type of
certificate"?)

## SSL versions

`Crypt::SSLeay` tries very hard to connect to *any* SSL web server
accomodating servers that are buggy, old or simply not standards-compliant.
To this effect, this module will try SSL connections in this order:

*   SSL v23 : should allow v2 and v3 servers to pick their best type

*   SSL v3 :  best connection type

*   SSL v2 :  old connection type

Unfortunately, some servers seem not to handle a reconnect to SSL v3 after a
failed connect of SSL v23 is tried, so you may set before using LWP or
`Net::SSL`:

    $ENV{HTTPS_VERSION} = 3;

to force a version 3 SSL connection first. At this time, only a version 2 SSL
connection will be tried after this, as the connection attempt order remains
unchanged by this setting.

## Acknowledgements

many thanks to the following individuals who helped improve Crypt-SSLeay:

* _Gisle Aas_ for writing this module and many others including libwww,
for perl. The web will never be the same :)

* _Ben Laurie_ deserves kudos for his excellent patches for better error
handling, SSL information inspection, and random seeding.

* _Dongqiang Bai_ for host name resolution fix when using a proxy.

* _Stuart Horner_ of Core Communications, Inc. who found the need for
building `--shared` OpenSSL libraries.

* _Pavel Hlavnicka_ for a patch for freeing memory when using a pkcs12
file, and for inspiring more robust `read()` behavior.

* _James Woodyatt_ is a champ for finding a ridiculous memory leak that
has been the bane of many a `Crypt::SSLeay` user.

* _Bryan Hart_ for his patch adding proxy support, and thanks to _Tobias
Manthey_ for submitting another approach.

* _Alex Rhomberg_ for Alpha linux ccc patch.

* _Tobias Manthey_ for his patches for client certificate support.

* _Daisuke Kuroda_ for adding PKCS12 certificate support.

* _Gamid Isayev_ for CA cert support and insights into error messaging.

* _Jeff Long_ for working through a tricky CA cert SSLClientVerify issue.

* _Chip Turner_ for a patch to build under perl 5.8.0.

* _Joshua Chamas_ for the time he spent maintaining the module.

* _Jeff Lavallee_ for help with alarms on read failures (CPAN bug #12444).

* _Guenter Knauf_ for significant improvements in configuring things in
Win32 and Netware lands and Jan Dubois for various suggestions for
improvements.

and _many others_ who provided bug reports, suggestions, fixes and
patches.

###	TODO: Update acknowledgements list.

## See Also

*   `Net::SSL`

    If you have downloaded this distribution as of a dependency of
    another distribution, it's probably due to this module (which is
    included in this distribution).

*   `Net::SSLeay`

    Net::SSLeay provides access to the OpenSSL API directly from Perl.
    See <http://search.cpan.org/dist/Net-SSLeay/>.

*   OpenSSL binary packages for Windows, see
    <http://www.openssl.org/related/binaries.html>.

## Support

*   For use of `Crypt::SSLeay` & `Net::SSL` with Perl's LWP, please send email
    to libwww@perl.org <mailto:libwww@perl.org>.

*   For OpenSSL or general SSL support, including issues associated with
    building and installing OpenSSL on your system, please email the OpenSSL
    users mailing list at openssl-users@openssl.org
    <mailto:openssl-users@openssl.org>. See
    <http://www.openssl.org/support/community.html> for other mailing lists
    and archives.

*   Please report all bugs at
    <http://rt.cpan.org/NoAuth/Bugs.html?Dist=Crypt-SSLeay>.

## Authors

This module was originally written by Gisle Aas, and was subsequently
maintained by Joshua Chamas, David Landgren, brian d foy, and A. Sinan
Unur.

## Copyright

Copyright &copy; 2010-2012 A. Sinan Unur

Copyright &copy; 2006-2007 David Landgren

Copyright &copy; 1999-2003 Joshua Chamas

Copyright &copy; 1998 Gisle Aas

## License

this program is free software; you can redistribute it and/or modify it
under the terms of Artistic License 2.0. See <http://www.perlfoundation.org/artistic_license_2_0>.

