#!/usr/bin/perl

use strict;
use CPAN;
use File::Basename ();
use File::chdir;
use Getopt::Long ();
use IO::File;
use Proc::Reliable;

my $FileCurrent = '5.14.inc';
my @FilePreviousList = qw(5.12.inc 5.10.inc);
my $URLprefix = 'http://search.cpan.org/CPAN/authors/id';

my $download;
my @skip;
my $skipfile;
Getopt::Long::GetOptions('d' => \$download, 's=s' => \@skip, 'S=s' => \$skipfile);
if(defined($skipfile)) {
    my $s = IO::File->new($skipfile, 'r') or die "Can't open $skipfile\n";
    while(<$s>) {
	chomp;
	push(@skip, $_);
    }
}
my %Skip = map {($_, 1)} @skip;

CPAN::HandleConfig->load;
CPAN::Shell::setup_output;
CPAN::Index->reload;

sub importDate {
    my($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime;
    sprintf('%d-%02d-%02d', $year + 1900, $mon + 1, $mday);
}

sub nameVers {
    local $_;
    my $x = shift;
    my @parts = split('-', $x);
    my $vers = pop(@parts);
    (join('-', @parts), $vers)
}

sub updatePlist {
    my($pl, $vers, $url, $date) = @_;
    my %h = (
	OpenSourceVersion => $vers,
	OpenSourceURL => $url,
	OpenSourceImportDate => $date,
    );
    my $in = IO::File->new($pl, 'r') || die "Can't open $pl\n";
    my $outf = "$pl.out";
    my $out = IO::File->new($outf, 'w') || die "Can't create $outf\n";
    local $_;
    while(<$in>) {
	last if m|^</dict>|;
	if(m|<key>(\w+)</key>| && exists($h{$1})) {
	    $out->print($_);
	    $out->printf("        <string>%s</string>\n", $h{$1});
	    $in->getline(); # skip next line
	    delete($h{$1});
	    next;
	}
	$out->print($_);
    }
    for(keys(%h)) {
	$out->printf("        <key>%s</key>\n", $_);
	$out->printf("        <string>%s</string>\n", $h{$_});
    }
    $out->print("</dict>\n");
    undef($in);
    undef($out);
    rename($outf, $pl) || die "Can't rename $outf to $pl\n";
}

my($dist, $found, $foundvers, $name, $vers, %projectsCurrent, %projectsPrevious);
my $F = IO::File->new($FileCurrent) or die "Can't open $FileCurrent\n";
while(<$F>) {
    next unless /-/;
    chomp;
    s/^\s+//;
    s/\s+\\.*$//;
    ($name, $vers) = nameVers($_);
    my $v = $projectsCurrent{$name};
    die "***Multiple entries for $name $v and $vers (possibly others)\n" if defined($v) && $v ne $vers;
    $projectsCurrent{$name} = $vers;
}
undef($F);
for my $prev (@FilePreviousList) {
    $F = IO::File->new($prev) or die "Can't open $prev\n";
    while(<$F>) {
	next unless /-/;
	chomp;
	s/^\s+//;
	s/\s+\\.*$//;
	$projectsPrevious{$_} = 1;
    }
    undef($F);
}

my(%downloaded, %old2new);
my $curl = Proc::Reliable->new(num_tries => 5, time_per_try => 30);
my @curlargs = qw(curl -O);
my @sedcmd = qw(sed -i .bak);
my $importDate = importDate();
for my $proj (sort(keys(%projectsCurrent))) {
    my $oldvers = $projectsCurrent{$proj};
    my $old = "$proj-$oldvers";
    if($Skip{$old}) {
	print "Skipping $old\n";
	next;
    }
    print "Update for $old\n";
    undef($found);
    $_ = $proj;
    s/-/::/g;
    for my $mod (CPAN::Shell->expand("Module", "/$_/")) {
	next unless $_ eq $mod->id;
	$dist = $mod->distribution;
	($name, $vers) = nameVers($dist->base_id);
	if(defined($found)) {
	    unless($vers gt $foundvers) {
		print "    Previous $name-$foundvers preferred over $name-$vers\n";
		next;
	    }
	    print "    Preferring $name-$vers over previous $name-$foundvers\n";
	} else {
	    print "    Found $name-$vers\n";
	}
	$found = $dist;
	$foundvers = $vers;
    }
    if(!defined($found)) {
	for my $dist (CPAN::Shell->expand("Distribution", "/\/$proj-/")) {
	    ($name, $vers) = nameVers($dist->base_id);
	    next unless $proj eq $name;
	    if(defined($found)) {
		unless($vers gt $foundvers) {
		    print "    Previous $name-$foundvers preferred over $name-$vers\n";
		    next;
		}
		print "    Preferring $name-$vers over previous $name-$foundvers\n";
	    } else {
		print "    Found $name-$vers\n";
	    }
	    $found = $dist;
	    $foundvers = $vers;
	}
	if(!defined($found)) {
	    print "***Can't find $proj\n";
	    next;
	}
    }
    my $new = $found->base_id;
    if($downloaded{$new}) {
	printf "    %s downloaded %s\n", $download ? 'Already' : 'Would have already', $new;
	$old2new{$old} = "-$new";
	next;
    }
    ($name, $vers) = nameVers($new);
    if($name ne $proj) {
	print "    *** Module $proj combined into $name\n";
	next;
    }
    if($vers eq $oldvers) {
	print "    Already have $name-$vers\n";
	next;
    }
    my $url = $found->pretty_id;
    my $tarball = File::Basename::basename($url);
    my $a = substr($url, 0, 1);
    my $a2 = substr($url, 0, 2);
    $url = join('/', $URLprefix, $a, $a2, $url);
    if(!$download) {
	print "    Would download $url\n";
	if(defined($projectsPrevious{$old})) {
	    print "    Would make new directory $new by copying $old\n";
	} else {
	    print "    Would rename directory $old to $new\n";
	}
	next;
    } else {
	local $CWD = 'Modules'; # will return to current directory automatically on exiting block
	print "    Downloading $url\n";
	$curlargs[2] = $url;
	my($out, $err, $status, $msg) = $curl->run(\@curlargs);
	if($status != 0 || `file $tarball` !~ /gzip compressed data/) {
	    warn "***\"@curlargs\" failed: $msg\n";
	    next;
	}
	if(defined($projectsPrevious{$old})) {
	    print "    Copying $old to $new\n";
	    if(system('svn', 'cp', $old, $new) != 0) {
		warn "***Can't svn cp $old $new\n";
		unlink($tarball);
		next;
	    }
	} else {
	    print "    Renaming $old to $new\n";
	    if(system('svn', 'mv', $old, $new) != 0) {
		warn "***Can't svn mv $old $new\n";
		unlink($tarball);
		next;
	    }
	}
	$CWD = $new;
	if(system('svn', 'mv', "$old.tar.gz", $tarball) != 0) {
	    warn "***Can't rename $old.tar.gz to $tarball\n";
	}
	rename("../$tarball", $tarball) or warn "***Couldn't move $tarball into $new\n";
	my $svers = $oldvers;
	$svers =~ s/\./\\./g;
	my @args;
	push(@args, '-e', "s/$proj/$name/") if $proj ne $name;
	push(@args, '-e', "s/$svers/$vers/");
	print "    Editing Makefile \"@sedcmd @args Makefile\"\n";
	if(system(@sedcmd, @args, 'Makefile') != 0) {
	    warn "***\"@sedcmd @args Makefile\" failed\n";
	}
	print "    Editing oss.partial\n";
	updatePlist('oss.partial', $vers, $url, $importDate);
	$downloaded{$new} = 1;
	$old2new{$old} = $new;
    }
}
exit 0 unless $download;

print "\nUpdating $FileCurrent\n";
my $old = "$FileCurrent.bak";
rename($FileCurrent, $old) or die "Can't rename $FileCurrent to $old\n";
$F = IO::File->new($old) or die "Can't open $old\n";
my $T = IO::File->new($FileCurrent, 'w') or die "Can't create $old\n";
while(<$F>) {
    unless(/-/) {
	print $T $_;
	next;
    }
    my $line = $_;
    chomp;
    s/^\s+//;
    s/\s+\\.*$//;
    my $new = $old2new{$_};
    if(defined($new)) {
	if(substr($new, 0, 1) eq '-') {
	    printf "Removing %s (now part of %s)\n", $_, substr($new, 1);
	    next;
	}
	$line =~ s/$_/$new/;
    }
    print $T $line;
}
undef($T);
undef($F);
