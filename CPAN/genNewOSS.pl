#!/usr/bin/perl

# genNewOSS.pl
#
# genNewOSS.pl, given a list of modules + metadata and corresponding tarballs
# (downloaded via getCPAN.pl), creates a Modules subdirectory containing
# subdirectories with the tarball, Makefile, LICENSE and oss.partial (querying
# the CPAN servers for OSS information), suitable to be used in the CPAN
# project's Modules directory.
#
# By default, genNewOSS.pl prints out what it would do; use the -w option to
# actually create the subdirectories and write the files.  The tarballs are
# expected to be in the current directory, or else the path of the directory
# containing the tarballs can be passed on the command line.
#
# The -o options prints out all the opensource licensing info, useful for
# including in an opensource approval request.
#
# The %modules hash should be modified to specify metadata for the modules.
# The versioned module name is the hash key, and the value is a hash reference
# containing three key/value pair.  The "copyright" key points to a string
# containing copyright information about the module, while the "license"
# key points to a string giving the license name.
#
# The third key/value pair can be any one of the following:
#
# licensestr => string specifying the license terms
# licensefile => string containing path to a file containing license terms
# licensefilelist => list reference containing multiple path string to files
#                    containing license terms
#
# (licensing term can be a URL where the terms are stated)

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
http://opensource.org/licenses/Artistic-2.0
EOF
my $Apache20License = <<EOF;
http://www.apache.org/licenses/LICENSE-2.0
EOF

my %modules = (
    'Capture-Tiny-0.23' => {
	copyright => 'This software is Copyright (c) 2009 by David Golden.',
	license => 'Apache 2.0',
	licensestr => $Apache20License,
    },
    'Class-Tiny-0.014' => {
	copyright => 'This software is Copyright (c) 2013 by David Golden.',
	license => 'Apache 2.0',
	licensestr => $Apache20License,
    },
    'Devel-CallChecker-0.008' => {
	copyright => 'This software is copyright (c) 2011, 2012, 2013, 2015, 2017 Andrew Main (Zefram) <zefram@fysh.org>',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Devel-Declare-0.006019' => {
	copyright => 'This software is copyright (c) 2007, 2008, 2009 by Matt S Trout, 2008, 2009 by Florian Ragwitz.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Devel-StackTrace-1.31' => {
	copyright => 'This software is Copyright (c) 2014 by Dave Rolsky.',
	license => 'ArtisticLicense',
	licensestr => $ArtisticLicense,
    },
    'DynaLoader-Functions-0.003' => {
	copyright => 'This software is copyright (C) 2011, 2012, 2013, 2017 Andrew Main (Zefram) <zefram@fysh.org>',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Exporter-Tiny-1.002001' => {
	copyright => 'This software is copyright (c) 2013-2014, 2017 by Toby Inkster.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'ExtUtils-Config-0.007' => {
	copyright => 'This software is copyright (c) 2006 by Ken Williams, Leon Timmermans.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'ExtUtils-Helpers-0.021' => {
	copyright => 'This software is copyright (c) 2004 by Ken Williams, Leon Timmermans.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'ExtUtils-InstallPaths-0.010' => {
	copyright => 'This software is copyright (c) 2011 by Ken Williams, Leon Timmermans.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'File-Remove-1.58' => {
	copyright => 'This software is copyright (c) 1998 by Gabor Egressy.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'HTML-Tree-5.07' => {
	copyright => 'This software is copyright (c) 1995-1998 by Gisle Aas, 1999-2004 by Sean M. Burke, 2005 by Andy Lester, 2006 by Pete Krawczyk, 2010 by Jeff Fearn, 2012 by Christopher J. Madsen. (Except the articles contained in HTML::Tree::AboutObjects, HTML::Tree::AboutTrees, and HTML::Tree::Scanning, which are all copyright (c) 2000 by The Perl Journal.)',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Import-Into-1.002000' => {
	copyright => 'Copyright (c) 2012 mst - Matt S. Trout (cpan:MSTROUT) <mst@shadowcat.co.uk>, haarg - Graham Knop (cpan:HAARG) <haarg@haarg.org>',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'IPC-System-Simple-1.26' => {
	copyright => 'This software is copyright (c) 2006-2008 by Paul Fenwick.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Lexical-SealRequireHints-0.007' => {
	copyright => 'Copyright (C) 2009, 2010, 2011, 2012 Andrew Main (Zefram) <zefram@fysh.org>',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'List-MoreUtils-XS-0.428' => {
	copyright => 'This software is copyright (c) 2011 by Aaron Crane, 2004-2010 by Tassilo von Parseval, 2013-2017 by Jens Rehsack.',
	license => 'Apache 2.0',
	licensestr => $Apache20License,
    },
    'List-MoreUtils-0.428' => {
	copyright => 'This software is copyright (c) 2011 by Aaron Crane, 2004-2010 by Tassilo von Parseval, 2013-2017 by Jens Rehsack.',
	license => 'Apache 2.0',
	licensestr => $Apache20License,
    },
    'Module-Build-Tiny-0.034' => {
	copyright => 'This software is copyright (c) 2011 by Leon Timmermans, David Golden.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Module-Build-0.4229' => {
	copyright => 'This software is copyright (c) 2001-2006 by Ken Williams.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Module-Install-1.19' => {
	copyright => 'This software is copyright (c) 2002-2012 by Brian Ingerson, Audrey Tang and Adam Kennedy.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Module-Package-0.30' => {
	copyright => 'This software is copyright (c) 2011 Ingy döt Net.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'PadWalker-2.3' => {
	copyright => 'This software is copyright (c) 2000-2009 by Robin Houston.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Params-Classify-0.015' => {
	copyright => 'This software is copyright (c) 2004, 2006, 2007, 2009, 2010, 2017 by Andrew Main (Zefram) <zefram@fysh.org>, 2009, 2010 by PhotoBox Ltd.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Scope-Guard-0.21' => {
	copyright => 'This software is copyright (c) 2005-2015, chocolateboy.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'TermReadKey-2.38' => {
	copyright => 'This software is copyright (c) 1994-1999 by Kenneth Albanowski, 2001-2016 by Jonathan Stowe and others.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'XSLoader-0.24' => {
	copyright => 'Copyright (c) 1990-2011 by Larry Wall and others.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'YAML-Tiny-1.73' => {
	copyright => 'This software is copyright (c) 2006-2013 Adam Kennedy.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'autobox-2.86' => {
	copyright => 'This software is copyright (c) 2003-2018 by chocolateboy.',
	license => 'ArtisticLicense',
	licensestr => $ArtisticLicense,
    },
    'bareword-filehandles-0.007' => {
	copyright => 'This software is copyright (c) 2011-2016 by Dagfinn Ilmari Mannsåker.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'indirect-0.39' => {
	copyright => 'Copyright 2008,2009,2010,2011,2012,2013,2014,2015,2016,2017,2019 Vincent Pit, all rights reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'multidimensional-0.014' => {
	copyright => 'This software is copyright (c) 2010-2016 by Dagfinn Ilmari Mannsåker.',
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
	next unless $vers eq $v;
	print "    Found $name-$vers\n";
	$found = $dist;
	last;
    }
    if(!defined($found)) {
	for my $dist (CPAN::Shell->expand("Distribution", "/\/$n-/")) {
	    ($name, $vers) = nameVers($dist->base_id);
	    next unless $name eq $n;
	    next unless $vers eq $v;
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
    my $t = File::Spec->join($tardir, "$m.*");
    my @t = glob($t);
    die "\"$t\" produces no matches\n" if scalar(@t) == 0;
    die "\"$t\" produces multiple matches\n" if scalar(@t) > 1;
    $t = $t[0];
    my($tail, $dir, $suf) = File::Basename::fileparse($t, qr/\.(tar\.gz|tgz)/);
    die "$t has unknown suffix\n" if $suf eq '';
    if($write) {
	if(!-d "$Modules/$m") {
	    mkdir "$Modules/$m" or die "Can't mkdir $Modules/$m\n";
	}
	File::Copy::syscopy($t, "$Modules/$m/$tail$suf") or die "Can't copy $t: $!\n";
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
    if($suf ne '.tar.gz') {
	print $OUT <<EOF;

TARBALL := \$(NAMEVERSION)$suf
EOF
    }
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
