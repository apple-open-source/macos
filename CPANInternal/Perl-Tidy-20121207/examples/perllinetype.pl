#!/usr/bin/perl -w
use strict;

# For each line in a perl script, write to STDOUT lines of the form
# line number : line type : line text
#
# usage:
# perllinetype myfile.pl >myfile.new
# perllinetype <myfile.pl >myfile.new
#
# This file is one of the examples distributed with perltidy and is a
# simple demonstration of using a callback object with Perl::Tidy.
#
# Steve Hancock, July 2003
#
use Getopt::Std;
use Perl::Tidy;
use IO::File;
$| = 1;
use vars qw($opt_h);
my $usage = <<EOM;
   usage: perllinetype filename >outfile
EOM
getopts('h') or die "$usage";
if ($opt_h) { die $usage }

# Make the source for perltidy, which will be a filehandle
# or just '-' if the source is stdin
my ($file, $fh, $source);
if ( @ARGV == 0 ) {
    $source = '-';
}
elsif ( @ARGV == 1 ) {
    $file = $ARGV[0];
    $fh = IO::File->new( $file, 'r' );
    unless ($fh) { die "cannot open '$file': $!\n" }
    $source = $fh;
}
else { die $usage }

# make the callback object
my $formatter = MyFormatter->new(); 

my $dest;

# start perltidy, which will start calling our write_line()
my $err=perltidy(
    'formatter'   => $formatter,     # callback object
    'source'      => $source,
    'destination' => \$dest,         # (not really needed)
    'argv'        => "-npro -se",    # dont need .perltidyrc
                                     # errors to STDOUT
);
if ($err) {
    die "Error calling perltidy\n";
}
$fh->close() if $fh;

package MyFormatter;

sub new {
    my ($class) = @_;
    bless {}, $class;
}

sub write_line {

    # This is called from perltidy line-by-line
    my $self              = shift;
    my $line_of_tokens    = shift;
    my $line_type         = $line_of_tokens->{_line_type};
    my $input_line_number = $line_of_tokens->{_line_number};
    my $input_line        = $line_of_tokens->{_line_text};
    print "$input_line_number:$line_type:$input_line";
}

# called once after the last line of a file
sub finish_formatting {
    my $self = shift;
    return;
}
