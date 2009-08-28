# File-chdir-0.1002 produces the warning:
#
# Warning: prerequisite File::Spec 3.27 not found. We have 3.2501.
#
# THe Changes file says:
#
# File::Spec 3.2501 broke File::chdir on Win32;
#
# so it should be fine on Mac OS X

CPAN_PROJECTS = \
    MLDBM-2.01 \
    Mac-Errors-1.14 \
    Time-Epoch-0.02 \
    URI-1.37 \
    libwww-perl-5.813 \
    Mac-Carbon-0.77 \
    Mac-Apps-Launch-1.93 \
    Mac-AppleEvents-Simple-1.18 \
    Mac-OSA-Simple-1.09 \
    Mac-Glue-1.30 \
    Convert-BinHex-1.119 \
    Convert-UUlib-1.12 \
    Compress-Zlib-1.42 \
    IO-String-1.08 \
    IO-Zlib-1.09 \
    Package-Constants-0.02 \
    Archive-Tar-1.38 \
    Archive-Zip-1.26 \
    BerkeleyDB-0.36 \
    Test-Pod-1.26 \
    TimeDate-1.16 \
    MailTools-2.04 \
    IO-stringy-2.110 \
    MIME-tools-5.427 \
    Convert-TNEF-0.17 \
    Digest-SHA1-2.11 \
    HTML-Tagset-3.20 \
    HTML-Parser-3.59 \
    Net-Server-0.97 \
    Unix-Syslog-1.1 \
    Socket6-0.23 \
    IO-Socket-INET6-2.56 \
    Digest-HMAC-1.01 \
    Net-IP-1.25 \
    Net-DNS-0.65 \
    Proc-Reliable-1.16 \
    Sub-Uplevel-0.2002 \
    Test-Exception-0.27 \
    Carp-Clan-6.00 \
    Bit-Vector-6.4 \
    Date-Calc-5.4 \
    Convert-ASN1-0.22 \
    GSSAPI-0.26 \
    Net-SSLeay-1.35 \
    IO-Socket-SSL-1.22 \
    Authen-SASL-2.12 \
    XML-NamespaceSupport-1.09 \
    XML-SAX-0.96 \
    perl-ldap-0.36 \
    DBI-1.607 \
    DBD-SQLite-1.14 \
    XML-LibXML-Common-0.13 \
    XML-LibXML-1.69 \
    XML-Parser-2.36 \
    XML-Writer-0.606 \
    XML-XPath-1.13 \
    XML-Simple-2.18 \
    IO-Stty-.02 \
    IO-Tty-1.07 \
    Expect-1.21 \
    Regexp-Common-2.122 \
    Pod-Readme-0.09 \
    ExtUtils-CBuilder-0.24 \
    version-0.76 \
    Module-Build-0.31012 \
    Alien-wxWidgets-0.42 \
    Wx-0.89 \
    Class-Accessor-0.31 \
    File-Slurp-9999.13 \
    UNIVERSAL-require-0.11 \
    File-chdir-0.1002 \
    Wx-Demo-0.10 \
    Net-CIDR-Lite-0.20 \
    Sys-Hostname-Long-1.4 \
    Mail-SPF-Query-1.999.1

SVK_PROJECTS = \
    Algorithm-Diff-1.1902 \
    Algorithm-Annotate-0.10 \
    YAML-Syck-1.05 \
    Data-Hierarchy-0.34 \
    PerlIO-via-dynamic-0.13 \
    PerlIO-via-symlink-0.05 \
    IO-Digest-0.10 \
    PerlIO-eol-0.14 \
    Class-Autouse-1.29 \
    Locale-Maketext-Simple-0.18 \
    App-CLI-0.07 \
    List-MoreUtils-0.22 \
    Class-Data-Inheritable-0.08 \
    Path-Class-0.16 \
    TermReadKey-2.30 \
    Locale-Maketext-Lexicon-0.77 \
    IO-Pager-0.06 \
    FreezeThaw-0.43 \
    Time-Progress-1.4 \
    Compress-Bzip2-2.09 \
    PerlIO-via-Bzip2-0.02 \
    PerlIO-gzip-0.18

PROJECTS = $(CPAN_PROJECTS) $(SVK_PROJECTS)
