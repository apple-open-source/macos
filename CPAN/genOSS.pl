#!/usr/bin/perl

use strict;
use CPAN;
use File::Basename ();
use Getopt::Long ();
use IO::File;

my $CPAN = '/Volumes/XDisk/build/Roots/CPAN.roots/CPAN~obj'; #OBJROOT
my $Modules = 'Modules';
my $PerlLicense = <<EOF;
Licensed under the same terms as Perl:
http://perldoc.perl.org/perlartistic.html
http://perldoc.perl.org/perlgpl.html
EOF
my $YAMLLicense = <<EOF;
This software is released under the MIT license cited below.

The libsyck code bundled with this library is released by "why the
lucky stiff", under a BSD-style license. See the COPYING file for
details.

The "MIT" License

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject
to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

EOF
my $YAMLCOPYING = "$CPAN/5.10/YAML-Syck-1.07/COPYING";
my $F = IO::File->new($YAMLCOPYING) or die "Can't open $YAMLCOPYING\n";
$YAMLLicense .= join('', $F->getlines);
undef($F);

my %outdated = (
    'Compress-Zlib-1.42' => 'http://search.cpan.org/CPAN/authors/id/P/PM/PMQS/Compress-Zlib-1.42.tar.gz',
    'XML-LibXML-Common-0.13' => 'http://search.cpan.org/CPAN/authors/id/P/PH/PHISH/XML-LibXML-Common-0.13.tar.gz',
);
my %modules = (
    'Algorithm-Annotate-0.10' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Algorithm-Diff-1.1902' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'App-CLI-0.07' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'App-CLI-0.08' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Archive-Tar-1.38' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Archive-Zip-1.26' => {
	license => 'Perl',
	licensefile => "$CPAN/5.8/Archive-Zip-1.26/LICENSE",
    },
    'Archive-Zip-1.30' => {
	date => '2010-03-15',
	license => 'Perl',
	licensefile => "$CPAN/5.10/Archive-Zip-1.30/LICENSE",
    },
    'Authen-SASL-2.12' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Authen-SASL-2.14' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'BerkeleyDB-0.36' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'BerkeleyDB-0.42' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Bit-Vector-6.4' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Bit-Vector-7.1' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Carp-Clan-6.00' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Carp-Clan-6.04' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Class-Accessor-0.31' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Class-Accessor-0.34' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Class-Autouse-1.29' => {
	license => 'Perl',
	licensefile => "$CPAN/5.10/Class-Autouse-1.29/LICENSE",
    },
    'Class-Data-Inheritable-0.08' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Compress-Bzip2-2.09' => {
	license => 'Other',
	licensefile => "$CPAN/5.10/Compress-Bzip2-2.09/bzlib-src/LICENSE",
    },
    'Compress-Zlib-1.42' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Convert-ASN1-0.22' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Convert-BinHex-1.119' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Convert-TNEF-0.17' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Convert-UUlib-1.12' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Convert-UUlib-1.33' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'DBD-SQLite-1.14' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'DBD-SQLite-1.29' => {
	date => '2010-03-15',
	license => 'Perl',
	licensefile => "$CPAN/5.10/DBD-SQLite-1.29/LICENSE",
    },
    'DBI-1.607' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'DBI-1.609' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Data-Hierarchy-0.34' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Date-Calc-5.4' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Date-Calc-6.3' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Digest-HMAC-1.01' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Digest-HMAC-1.02' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Digest-SHA1-2.11' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Digest-SHA1-2.12' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Expect-1.21' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'ExtUtils-CBuilder-0.24' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'File-Slurp-9999.13' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'File-chdir-0.1002' => {
	license => 'Perl',
	licensefile => "$CPAN/5.10/File-chdir-0.1002/LICENSE",
    },
    'FreezeThaw-0.43' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'FreezeThaw-0.50' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'GSSAPI-0.26' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'HTML-Parser-3.59' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'HTML-Parser-3.64' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'HTML-Tagset-3.20' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'IO-Digest-0.10' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'IO-Pager-0.06' => {
	license => 'Other',
	licensestr => <<EOF,
* Thou shalt not claim ownership of unmodified materials.

* Thou shalt not claim whole ownership of modified materials.

* Thou shalt grant the indemnity of the provider of materials.

* Thou shalt use and dispense freely without other restrictions.
EOF
    },
    'IO-Socket-INET6-2.56' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'IO-Socket-INET6-2.57' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'IO-Socket-SSL-1.22' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'IO-Socket-SSL-1.32' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'IO-String-1.08' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'IO-Stty-.02' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'IO-Tty-1.07' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'IO-Tty-1.08' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'IO-Zlib-1.09' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'IO-stringy-2.110' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Internals-1.1' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'List-MoreUtils-0.22' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Locale-Maketext-Lexicon-0.77' => {
	license => 'MIT',
	licensefile => "$CPAN/5.8/Locale-Maketext-Lexicon-0.77/LICENSE",
    },
    'Locale-Maketext-Lexicon-0.79' => {
	date => '2010-03-15',
	license => 'MIT',
	licensefile => "$CPAN/5.10/Locale-Maketext-Lexicon-0.79/LICENSE",
    },
    'Locale-Maketext-Simple-0.18' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'MIME-tools-5.427' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'MLDBM-2.01' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'MLDBM-2.04' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Mac-AppleEvents-Simple-1.18' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Mac-Apps-Launch-1.93' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Mac-Carbon-0.77' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Mac-Carbon-0.82' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Mac-Errors-1.14' => {
	license => 'Perl',
	licensefile => "$CPAN/5.10/Mac-Errors-1.14/LICENSE",
    },
    'Mac-Glue-1.30' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Mac-OSA-Simple-1.09' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Mail-SPF-Query-1.999.1' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'MailTools-2.04' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'MailTools-2.06' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Module-Build-0.31012' => {
	license => 'Perl',
	licensefile => "$CPAN/5.8/Module-Build-0.31012/LICENSE",
    },
    'Net-CIDR-Lite-0.20' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Net-DNS-0.65' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Net-DNS-0.66' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Net-IP-1.25' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Net-SSLeay-1.35' => {
	license => 'OpenSSL',
	licensefile => '/Network/Servers/xs1/release/Software/Barolo/Updates/NewestBarolo/Projects/OpenSSL098/src/LICENSE',
    },
    'Net-SSLeay-1.36' => {
	license => 'OpenSSL',
	licensefile => '/Network/Servers/xs1/release/Software/Barolo/Updates/NewestBarolo/Projects/OpenSSL098/src/LICENSE',
	date => '2010-03-15',
    },
    'Net-Server-0.97' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Package-Constants-0.02' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Path-Class-0.16' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Path-Class-0.18' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'PerlIO-eol-0.14' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'PerlIO-gzip-0.18' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'PerlIO-via-Bzip2-0.02' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'PerlIO-via-dynamic-0.13' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'PerlIO-via-symlink-0.05' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Pod-Readme-0.09' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Proc-Reliable-1.16' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Regexp-Common-2.122' => {
	license => 'Perl/BSD/MIT',
	licensefilelist => [
	    "$CPAN/5.8/Regexp-Common-2.122/COPYRIGHT",
	    "$CPAN/5.8/Regexp-Common-2.122/COPYRIGHT.AL",
	    "$CPAN/5.8/Regexp-Common-2.122/COPYRIGHT.AL2",
	    "$CPAN/5.8/Regexp-Common-2.122/COPYRIGHT.BSD",
	    "$CPAN/5.8/Regexp-Common-2.122/COPYRIGHT.MIT",
	],
    },
    'Regexp-Common-2010010201' => {
	license => 'Perl/BSD/MIT',
	licensefilelist => [
	    "$CPAN/5.10/Regexp-Common-2010010201/COPYRIGHT",
	    "$CPAN/5.10/Regexp-Common-2010010201/COPYRIGHT.AL",
	    "$CPAN/5.10/Regexp-Common-2010010201/COPYRIGHT.AL2",
	    "$CPAN/5.10/Regexp-Common-2010010201/COPYRIGHT.BSD",
	    "$CPAN/5.10/Regexp-Common-2010010201/COPYRIGHT.MIT",
	],
	date => '2010-03-15',
    },
    'Socket6-0.23' => {
	license => 'Other',
	licensestr => <<EOF,
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the project nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.
#
THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
EOF
    },
    'Sub-Uplevel-0.2002' => {
	license => 'Perl',
	licensefile => "$CPAN/5.8/Sub-Uplevel-0.2002/LICENSE",
    },
    'Sub-Uplevel-0.22' => {
	date => '2010-03-15',
	license => 'Perl',
	licensefile => "$CPAN/5.10/Sub-Uplevel-0.22/LICENSE",
    },
    'Sys-Hostname-Long-1.4' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'TermReadKey-2.30' => {
	license => 'Other',
	licensestr => <<EOF,
Copyright (C) 1994-1999 Kenneth Albanowski. 
2001-2005 Jonathan Stowe and others

Unlimited distribution and/or modification is allowed as long as this 
copyright notice remains intact.
EOF
    },
    'Test-Exception-0.27' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Test-Exception-0.29' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Test-Pod-1.26' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Test-Pod-1.42' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Time-Epoch-0.02' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Time-Progress-1.4' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Time-Progress-1.5' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'TimeDate-1.16' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'TimeDate-1.20' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'UNIVERSAL-require-0.11' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'UNIVERSAL-require-0.13' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'URI-1.37' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'URI-1.53' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'Unix-Syslog-1.1' => {
	license => 'Artistic',
	licensefile => "$CPAN/5.10/Unix-Syslog-1.1/Artistic",
    },
    'XML-LibXML-1.69' => {
	license => 'Perl',
	licensefile => "$CPAN/5.8/XML-LibXML-1.69/LICENSE",
    },
    'XML-LibXML-1.70' => {
	date => '2010-03-15',
	license => 'Perl',
	licensefile => "$CPAN/5.10/XML-LibXML-1.70/LICENSE",
    },
    'XML-LibXML-Common-0.13' => {
	license => 'Perl',
	licensefile => "$CPAN/5.8/XML-LibXML-Common-0.13/LICENSE",
    },
    'XML-NamespaceSupport-1.09' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'XML-NamespaceSupport-1.10' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'XML-Parser-2.36' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'XML-SAX-0.96' => {
	license => 'Perl',
	licensefile => "$CPAN/5.10/XML-SAX-0.96/LICENSE",
    },
    'XML-Simple-2.18' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'XML-Writer-0.606' => {
	license => 'MIT',
	licensefile => "$CPAN/5.10/XML-Writer-0.606/LICENSE",
    },
    'XML-XPath-1.13' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'YAML-Syck-1.05' => {
	license => 'MIT/BSD',
	licensestr => $YAMLLicense,
    },
    'YAML-Syck-1.07' => {
	license => 'MIT/BSD',
	licensestr => $YAMLLicense,
	date => '2010-03-15',
    },
    'libwww-perl-5.813' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'libwww-perl-5.834' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'perl-ldap-0.36' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'perl-ldap-0.40' => {
	license => 'Perl',
	licensestr => $PerlLicense,
	date => '2010-03-15',
    },
    'version-0.76' => {
	license => 'Perl',
	licensestr => $PerlLicense,
    },
);

my $URLprefix = 'http://search.cpan.org/CPAN/authors/id';

my $write;
Getopt::Long::GetOptions('w' => \$write);

sub nameVers {
    my $x = shift;
    my @parts = split('-', $x);
    my $vers = pop(@parts);
    (join('-', @parts), $vers)
}

CPAN::HandleConfig->load;
CPAN::Shell::setup_output;
CPAN::Index->reload;

my($dist, $name, $vers, $url);
my($OUT, $license);
my @svncmd = qw(svn add);

$OUT = \*STDOUT if !$write;
for my $m (sort(keys(%modules))) {
    printf "Looking for %s\n", $m;
    my($n, $v) = nameVers($m);
    $url = $outdated{$m};
    if(!defined($url)) {
	my $found;
	my $mname = $n;
	$mname =~ s/-/::/g;
	for my $mod (CPAN::Shell->expand("Module", "/$mname/")) {
	    $dist = $mod->distribution;
	    next unless defined($dist);
	    ($name, $vers) = nameVers($dist->base_id);
	    next unless $name eq $mname;
	    print "    Found $name-$vers\n";
	    $found = $dist;
	    last;
	}
	if(!defined($found)) {
	    for my $dist (CPAN::Shell->expand("Distribution", "/\/$n-/")) {
		($name, $vers) = nameVers($dist->base_id);
		next unless $name eq $n;
		print "    Found $name-$vers\n";
		$found = $dist;
		last
	    }
	    if(!defined($found)) {
		print "***Can't find $m\n";
		next;
	    }
	}
	$url = $found->pretty_id;
	my $base = $found->base_id;
	$url =~ s/$base/$m/ unless $base eq $m;
	my $a = substr($url, 0, 1);
	my $a2 = substr($url, 0, 2);
	$url = join('/', $URLprefix, $a, $a2, $url);
    }
    if($write) {
	$OUT = IO::File->new("$Modules/$m/oss.partial", 'w');
	if(!defined($OUT)) {
	    warn "***Can't create $Modules/$m/oss.partial\n";
	    next;
	}
    }
    my $h = $modules{$m};
    die "$m: no license\n" unless defined($h->{license});
    print $OUT <<EOF;
<dict>
        <key>OpenSourceProject</key>
        <string>$n</string>
        <key>OpenSourceVersion</key>
        <string>$v</string>
        <key>OpenSourceWebsiteURL</key>
        <string>http://search.cpan.org/</string>
        <key>OpenSourceURL</key>
        <string>$url</string>
EOF

    if(defined($h->{date})) {
	print $OUT <<EOF;
        <key>OpenSourceImportDate</key>
        <string>$h->{date}</string>
EOF
    }
    print $OUT <<EOF;
        <key>OpenSourceLicense</key>
        <string>$h->{license}</string>
        <key>OpenSourceLicenseFile</key>
        <string>CPAN.txt</string>
</dict>
EOF
    if($write) {
	undef($OUT);
	$license = "$Modules/$m/LICENSE";
    }
    my @list;
    if(defined($h->{licensefilelist})) {
	@list = @{$h->{licensefilelist}};
    } elsif(defined($h->{licensefile})) {
	push(@list, $h->{licensefile});
    }
    if(scalar(@list) > 0) {
	if(!$write) {
	    print "License Files:\n";
	}
	for(@list) {
	    if($write) {
		system("cat $_ >> $license") == 0 or die "\"cat $_ >> $license\" failed\n";
	    } else {
		if(!-f $_) {
		    warn "***No $_\n";
		    next;
		print "    $_\n";
	    }
	}
    } else {
	die "$m: no licensestr\n" unless defined($h->{licensestr});
	if($write) {
	    $OUT = IO::File->new($license, 'w') or die "Can't create $license\n";
	    print $OUT $h->{licensestr};
	    undef($OUT);
	} else {
	    print "=========== License String ==========\n";
	    print $h->{licensestr};
	    print "=====================================\n";
	}
    }
    if($write) {
	system(@svncmd, $license, "$Modules/$m/oss.partial") == 0 or die "\"@svncmd $license $Modules/$m/oss.partial\" failed\n";
    }
}
