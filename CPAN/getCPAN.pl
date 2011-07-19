#!/usr/bin/perl

use strict;
use CPAN;
use File::Basename ();
use Getopt::Long ();
use IO::File;
use Proc::Reliable;

my $URLprefix = 'http://search.cpan.org/CPAN/authors/id';

my($download, $file);
Getopt::Long::GetOptions('d' => \$download, 'f=s', \$file);

CPAN::HandleConfig->load;
CPAN::Shell::setup_output;
CPAN::Index->reload;

sub nameVers {
    my $x = shift;
    my @parts = split('-', $x);
    my $vers = pop(@parts);
    (join('-', @parts), $vers)
}

my @modules;
if(defined($file)) {
    my $F = IO::File->new($file) or die "Can't open $file\n";
    while(<$F>) {
	chomp;
	push(@modules, $_);
    }
    undef($F);
} else {
    die "Usage: $0 [-f file] [module ...]\n" unless scalar(@ARGV) > 0;
    @modules = @ARGV;
}

my($dist, $found, $foundvers, $name, $vers, %projects);
my $curl = Proc::Reliable->new(); # use default retry count and times
my @curlargs = qw(curl -O);
my %downloaded;
for my $m (@modules) {
    printf "Looking for %s\n", $m;
    undef($found);
    my $mname = $m;
    $mname =~ s/-/::/g;
    for my $mod (CPAN::Shell->expand("Module", "/$mname/")) {
	$dist = $mod->distribution;
	next unless defined($dist);
	($name, $vers) = nameVers($dist->base_id);
	next unless $name eq $mname;
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
	for my $dist (CPAN::Shell->expand("Distribution", "/\/$m-/")) {
	    ($name, $vers) = nameVers($dist->base_id);
	    next unless $name eq $m;
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
	    print "***Can't find $m\n";
	    next;
	}
    }
    if($downloaded{$found->base_id}) {
	printf "    %s downloaded %s\n", $download ? 'Already' : 'Would have already', $found->base_id;
	next;
    }
    $downloaded{$found->base_id} = 1;
    ($name, $vers) = nameVers($found->base_id);
    my $url = $found->pretty_id;
    my $tarball = File::Basename::basename($url);
    my $a = substr($url, 0, 1);
    my $a2 = substr($url, 0, 2);
    $url = join('/', $URLprefix, $a, $a2, $url);
    #printf "%s-%s => %s-%s\n", $m, $projects{$m}, $name, $vers;
    if($download) {
	print "    Downloading $url\n";
	$curlargs[2] = $url;
	my($out, $err, $status, $msg) = $curl->run(\@curlargs);
	if($status != 0 || `file $tarball` !~ /gzip compressed data/) {
	    warn "***\"@curlargs\" failed: $msg\n";
	}
    } else {
	print "    Would download $url\n";
    }
}
