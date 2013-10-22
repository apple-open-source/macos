#!/usr/bin/perl

use strict;
use CPAN;
use File::Basename ();
use File::Copy ();
use File::stat ();
use Getopt::Long ();
use IO::File;

my $Modules = 'Modules';
my $PerlLicense = <<EOF;
Licensed under the same terms as Perl:
http://perldoc.perl.org/perlartistic.html
http://perldoc.perl.org/perlgpl.html
EOF
my $ArtisticLicense = <<EOF;
http://opensource.org/licenses/artistic-license-2.0.php
EOF

my %modules = (
    'CPAN-Meta-2.120921' => {
	copyright => 'This software is copyright (c) 2010 by David Golden and Ricardo Signes.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'CPAN-Meta-Check-0.004' => {
	copyright => 'This software is copyright (c) 2012 by Leon Timmermans.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'CPAN-Meta-Requirements-2.122' => {
	copyright => 'This software is copyright (c) 2010 by David Golden and Ricardo Signes.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'CPAN-Meta-YAML-0.008' => {
	copyright => 'This software is copyright (c) 2010 by Adam Kennedy.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Class-Load-XS-0.06' => {
	copyright => 'This software is Copyright (c) 2012 by Dave Rolsky.',
	license => 'Artistic 2.0',
	licensestr => $ArtisticLicense,
    },
    'Class-Method-Modifiers-1.10' => {
	copyright => 'Copyright 2007-2009 Shawn M Moore.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'File-Which-1.09' => {
	copyright => 'Copyright 2002 Per Einar Ellefsen.  Some parts copyright 2009 Adam Kennedy.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'IO-HTML-0.04' => {
	copyright => 'This software is copyright (c) 2012 by Christopher J. Madsen.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'IPC-Run3-0.045' => {
	copyright => 'Copyright 2003, R. Barrie Slaymaker, Jr., All Rights Reserved',
	license => 'BSD',
	licensestr => <<EOF,
You may use this module under the terms of the BSD, Artistic, or GPL licenses,
any version.

See more information at:

  BSD: http://www.opensource.org/licenses/bsd-license.php
  GPL: http://www.opensource.org/licenses/gpl-license.php
  Artistic: http://opensource.org/licenses/artistic-license.php
EOF
    },
    'Module-Implementation-0.06' => {
	copyright => 'This software is Copyright (c) 2012 by Dave Rolsky.',
	license => 'Artistic 2.0',
	licensestr => $ArtisticLicense,
    },
    'Module-Metadata-1.000011' => {
	copyright => <<EOF,
Original code Copyright (c) 2001-2011 Ken Williams.
Additional code Copyright (c) 2010-2011 Matt Trout and David Golden.
All rights reserved.
EOF
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Moo-1.000005' => {
	copyright => 'Copyright (c) 2010-2011 the Moo "AUTHOR" and "CONTRIBUTORS" as listed above.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Probe-Perl-0.01' => {
	copyright => 'Copyright (C) 2005 Randy W. Sims',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Role-Tiny-1.002001' => {
	copyright => <<'EOC',
Copyright (c) 2010-2012 the Role::Tiny
mst - Matt S. Trout (cpan:MSTROUT) <mst@shadowcat.co.uk>
dg - David Leadbeater (cpan:DGL) <dgl@dgl.cx>
frew - Arthur Axel "fREW" Schmidt (cpan:FREW) <frioux@gmail.com>
hobbs - Andrew Rodland (cpan:ARODLAND) <arodland@cpan.org>
jnap - John Napiorkowski (cpan:JJNAPIORK) <jjn1056@yahoo.com>
ribasushi - Peter Rabbitson (cpan:RIBASUSHI) <ribasushi@cpan.org>
chip - Chip Salzenberg (cpan:CHIPS) <chip@pobox.com>
ajgb - Alex J. G. Burzyński (cpan:AJGB) <ajgb@cpan.org>
doy - Jesse Luehrs (cpan:DOY) <doy at tozt dot net>
perigrin - Chris Prather (cpan:PERIGRIN) <chris@prather.org>
Mithaldu - Christian Walde (cpan:MITHALDU)
<walde.christian@googlemail.com>
ilmari - Dagfinn Ilmari Mannsåker (cpan:ILMARI) <ilmari@ilmari.org>
tobyink - Toby Inkster (cpan:TOBYINK) <tobyink@cpan.org>
EOC
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Sub-Exporter-Progressive-0.001006' => {
	copyright => <<'EOC',
Copyright (c) 2012 the Sub::Exporter::Progressive
frew - Arthur Axel Schmidt (cpan:FREW) <frioux+cpan@gmail.com>
ilmari - Dagfinn Ilmari Mannsåker (cpan:ILMARI) <ilmari@ilmari.org>
mst - Matt S. Trout (cpan:MSTROUT) <mst@shadowcat.co.uk>
leont - Leon Timmermans (cpan:LEONT) <leont@cpan.org>
EOC
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Syntax-Keyword-Junction-0.003001' => {
	copyright => 'This software is copyright (c) 2012 by Arthur Axel "fREW" Schmidt.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Test-CheckDeps-0.002' => {
	copyright => 'This software is copyright (c) 2011 by Leon Timmermans',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Test-Script-1.07' => {
	copyright => 'Copyright 2006 - 2009 Adam Kennedy.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Test-Trap-v0.2.2' => {
	copyright => 'Copyright (C) 2006-2012 Eirik Berg Hanssen',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'XML-SAX-Expat-0.40' => {
	copyright => 'Copyright (c) 2001-2008 Robin Berjon. All rights reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'strictures-1.004002' => {
	copyright => 'Copyright (c) 2010 mst - Matt S. Trout (cpan:MSTROUT) <mst@shadowcat.co.uk>',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'syntax-0.004' => {
	copyright => "This software is copyright (c) 2012 by Robert 'phaylon' Sedlacek.",
	license => 'Perl',
	licensestr => $PerlLicense,
    },
);

my $URLprefix = 'http://search.cpan.org/CPAN/authors/id';

my $opensource; # output opensource copyright and license info
my $write;
Getopt::Long::GetOptions('o' => \$opensource, 'w' => \$write);

sub importDate {
    my($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = scalar(@_) > 0 ? localtime(shift) : localtime;
    sprintf('%d-%02d-%02d', $year + 1900, $mon + 1, $mday);
}

sub nameVers {
    my $x = shift;
    my @parts = split('-', $x);
    my $vers = pop(@parts);
    (join('-', @parts), $vers)
}

if($opensource) {
    # Legal now says that the full text of the license file is not needed, if
    # it is just one of the standard licenses.  Next time we update CPAN, we
    # should use licensefile more sparingly.
    for my $m (sort(keys(%modules))) {
	print "******** $m ********\n";
	my $h = $modules{$m};
	my @list;
	if(defined($h->{licensefilelist})) {
	    @list = @{$h->{licensefilelist}};
	} elsif(defined($h->{licensefile})) {
	    push(@list, $h->{licensefile});
	}
	die "$m: no copyright\n" unless defined($h->{copyright});
	chomp($h->{copyright});
	if(length($h->{copyright}) > 0) {
	    print "$h->{copyright}\n\n";
	} elsif(scalar(@list) <= 0) {
	    die "$m: copyright empty and no licence file\n";
	}
	if(scalar(@list) > 0) {
	    for(@list) {
		system("cat $_") == 0 or die "\"cat $_\" failed\n";
		print "\n";
	    }
	} else {
	    die "$m: no licensestr\n" unless defined($h->{licensestr});
	    chomp($h->{licensestr});
	    print "$h->{licensestr}\n\n";
	}
    }
    exit(0);
}

CPAN::HandleConfig->load;
CPAN::Shell::setup_output;
CPAN::Index->reload;

my($dist, $name, $vers, $url);
my($OUT, $license, $importDate);
#my @svncmd = qw(svn add);

if($write) {
    if(!-d $Modules) {
	mkdir $Modules or die "Can't mkdir $Modules\n";
    }
} else {
    $OUT = \*STDOUT;
}

my $tardir = '.';
$tardir = $ARGV[0] if scalar(@ARGV) > 0;
for my $m (sort(keys(%modules))) {
    printf "Looking for %s\n", $m;
    my($n, $v) = nameVers($m);
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
    my $t = File::Spec->join($tardir, "$m.tar.gz");
    if($write) {
	if(!-d "$Modules/$m") {
	    mkdir "$Modules/$m" or die "Can't mkdir $Modules/$m\n";
	}
	File::Copy::syscopy($t, "$Modules/$m/$m.tar.gz") or die "Can't copy $t: $!\n";
	$OUT = IO::File->new("$Modules/$m/Makefile", 'w');
	if(!defined($OUT)) {
	    warn "***Can't create $Modules/$m/Makefile\n";
	    next;
	}
    } else {
	if(!-f $t) {
	    warn "No $t\n";
	    next;
	}
	print "    Would copy $t\n";
	print "=== $m/Makefile ===\n";
    }

    print $OUT <<EOF;
NAME = $name
VERSION = $vers

include ../Makefile.inc
EOF
    if($write) {
	undef($OUT);
	$OUT = IO::File->new("$Modules/$m/oss.partial", 'w');
	if(!defined($OUT)) {
	    warn "***Can't create $Modules/$m/oss.partial\n";
	    next;
	}
    } else {
	print "=== $m/oss.partial ===\n";
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
    my $stat = File::stat::stat($t);
    $importDate = defined($h->{date}) ? $h->{date} : importDate($stat->mtime);
    print $OUT <<EOF;
        <key>OpenSourceImportDate</key>
        <string>$importDate</string>
EOF
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
		}
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
#    if($write) {
#	system(@svncmd, $license, "$Modules/$m/oss.partial") == 0 or die "\"@svncmd $license $Modules/$m/oss.partial\" failed\n";
#    }
}
