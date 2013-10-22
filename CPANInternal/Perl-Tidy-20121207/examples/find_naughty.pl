#!/usr/bin/perl -w
use strict;

# Walk through a perl script and look for 'naughty match variables'
# $`, $&, and $', which may cause poor performance.
#
# usage:
# find_naughty file1 [file2 [...]]
# find_naughty <file.pl 
#
# Author: Steve Hancock, July 2003
#
# TODO:
# - recursive processing might be nice
#
# Inspired by the discussion of naughty match variables at:
# http://www.perlmonks.org/index.pl?node_id=276549
#
use Getopt::Std;
use IO::File;
$| = 1;
use vars qw($opt_h);
my $usage = <<EOM;
usage:
  find_naughty file1 [file2 [...]]
  find_naughty <file.pl 
EOM
getopts('h') or die "$usage";
if ($opt_h) { die $usage }

unless (@ARGV) { unshift @ARGV, '-' }    # stdin
foreach my $source (@ARGV) {
    PerlTokenSearch::find_naughty(
        _source   => $source,
    );
}

#####################################################################
#
# The PerlTokenSearch package is an interface to perltidy which accepts a
# source filehandle and looks for selected variables.
#
# It works by making a a callback object with a write_line() method to
# receive tokenized lines from perltidy.  
#
# Usage:
#
#   PerlTokenSearch::find_naughty(
#       _source         => $fh,             # required source
#   );
#
# _source is any source that perltidy will accept, including a
# filehandle or reference to SCALAR or ARRAY
#
#####################################################################

package PerlTokenSearch;
use Carp;
use Perl::Tidy;

sub find_naughty {

    my %args = ( @_ );
    print "Testing File: $args{_source}\n";

    # run perltidy, which will call $formatter's write_line() for each line
    my $err=perltidy(
        'source'    => $args{_source},
        'formatter' => bless( \%args, __PACKAGE__ ),    # callback object
        'argv' => "-npro -se",    # -npro : ignore .perltidyrc,
                                  # -se   : errors to STDOUT
    );
    if ($err) {
        die "Error calling perltidy\n";
    }
}

sub write_line {

    # This is called back from perltidy line-by-line
    # We're looking for $`, $&, and $'
    my ( $self, $line_of_tokens ) = @_;
    my $source            = $self->{_source};

    # pull out some stuff we might need
    my $line_type         = $line_of_tokens->{_line_type};
    my $input_line_number = $line_of_tokens->{_line_number};
    my $input_line        = $line_of_tokens->{_line_text};
    my $rtoken_type       = $line_of_tokens->{_rtoken_type};
    my $rtokens           = $line_of_tokens->{_rtokens};
    chomp $input_line;

    # skip comments, pod, etc
    return if ( $line_type ne 'CODE' );

    # loop over tokens looking for $`, $&, and $'
    for ( my $j = 0 ; $j < @$rtoken_type ; $j++ ) {

        # we only want to examine token types 'i' (identifier)
        next unless $$rtoken_type[$j] eq 'i';

        # pull out the actual token text
        my $token = $$rtokens[$j];

        # and check it
        if ( $token =~ /^\$[\`\&\']$/ ) {
            print STDERR
              "$source:$input_line_number: $token\n";
        }
    }
}

# optional routine, called once after the last line of a file
sub finish_formatting {
    my $self = shift;
    return;
}
