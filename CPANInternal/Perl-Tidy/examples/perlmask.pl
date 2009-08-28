#!/usr/bin/perl -w
use strict;

# Walk through a perl script and create a masked file which is
# similar but which masks comments, quotes, patterns, and non-code
# lines so that it is easy to parse with regular expressions.
#
# usage:
# perlmask [-cn]  myfile.pl >myfile.new
# perlmask [-cn] <myfile.pl >myfile.new
#
# In the masked file,
#  -comments and pod will be masked (or removed)
#  -here-doc text lines will be masked (or removed)
#  -quotes and patterns, qw quotes, and here doc << operators will be
#   replaced by the letters 'Q', 'q', or 'h'
#
# The result is a file in which all braces, parens, and square brackets
# are balanced, and it can be parsed relatively easily by regular
# expressions.
#
# -cn is an optional 'compression' flag.  By default the masked file will have
# the same number of characters as the input file, with the difference being
# that certain characters will be changed (masked).
#
# If character position correspondence is not required, the size of the masked
# file can be significantly reduced by increasing the 'compression' level as
# follows:
#
# -c0 all mask file line numbers and character positions agree with
#     original file (DEFAULT)
# -c1 line numbers agree and character positions agree within lines of code
# -c2 line numbers agree but character positions do not
# -c3 no correspondence between line numbers or character positions
#
# Try each of these on a file of significant size to see how they work.
# The default, -c0, is required if you are working with character positions
# that span multiple lines.  The other levels may be useful if you
# do not need this level of correspondence.
#
# This file is one of the examples distributed with perltidy and demonstrates
# using a callback object with Perl::Tidy to walk through a perl file and find
# all of its tokens.  It can be useful for simple perl code parsing tasks.  It
# might even be helpful in debugging.  Or you may want to modify it to suit
# your own purposes.
#
use Getopt::Std;
use IO::File;
$| = 1;
use vars qw($opt_c $opt_h);
my $usage = <<EOM;
   usage: perlmask [ -cn ] filename >outfile
EOM
getopts('c:h') or die "$usage";
if ($opt_h) { die $usage }
unless ( defined($opt_c) ) { $opt_c = 0 }
if (@ARGV > 1) { die $usage }

my $source=$ARGV[0];   # an undefined filename will become stdin

# strings to hold the files (arrays could be used to)
my ( $masked_file, $original_file );  

PerlMask::perlmask(
    _source         => $source,
    _rmasked_file   => \$masked_file,
    _roriginal_file => \$original_file,    # optional
    _compression    => $opt_c              # optional, default=0
);

# Now we have the masked and original files in strings of equal length.
# We could search for specific text in the masked file here.  But here
# we'll just print the masked file:
if ($masked_file) { print $masked_file; }

#####################################################################
#
# The PerlMask package is an interface to perltidy which accepts a
# source filehandle and returns a 'masked' version of the source as
# a string or array.  It can also optionally return the original file
# as a string or array.
#
# It works by making a a callback object with a write_line() method to
# receive tokenized lines from perltidy.  This write_line method
# selectively replaces tokens with either their original text or with a
# benign masking character (such as '#' or 'Q').
#
# Usage:
#
#   PerlMask::perlmask(
#       _source         => $fh,             # required source
#       _rmasked_file   => \$masked_file,   # required ref to ARRAY or SCALAR
#       _roriginal_file => \$original_file, # optional ref to ARRAY or SCALAR
#       _compression    => $opt_c           # optional
#   );
#
# _source is any source that perltidy will accept, including a
# filehandle or reference to SCALAR or ARRAY
#
# The compression flag may have these values:
#  0 all mask file line numbers and character positions agree with
#    original file (DEFAULT)
#  1 line numbers agree and character positions agree within lines of code
#  2 line numbers agree but character positions do not
#  3 no correspondence between line numbers or character positions
#
#####################################################################

package PerlMask;
use Carp;
use Perl::Tidy;

sub perlmask {

    my %args = ( _compression => 0, @_ );
    my $rfile = $args{_rmasked_file};
    unless ( defined($rfile) ) {
        croak
          "Missing required parameter '_rmasked_file' in call to perlmask\n";
    }
    my $ref=ref($rfile);
    unless ( $ref =~ /^(SCALAR|ARRAY)$/ ) {
            croak <<EOM;
Expecting _rmasked_file = ref to SCALAR or ARRAY in perlmask but got : ($ref)
EOM
    }

    # run perltidy, which will call $formatter's write_line() for each line
    perltidy(
        'source'    => $args{_source},
        'formatter' => bless( \%args, __PACKAGE__ ),    # callback object
        'argv'        => "-npro -se",    # -npro : ignore .perltidyrc,
                                         # -se   : errors to STDOUT
    );
}

sub print_line {

    # called from write_line to dispatch one line (either masked or original)..
    # here we'll either append it to a string or array, as appropriate
    my ( $rfile, $line ) = @_;
    if ( defined($rfile) ) {
        if ( ref($rfile) eq 'SCALAR' ) {
            $$rfile .= $line . "\n";
        }
        elsif ( ref($rfile) eq 'ARRAY' ) {
            push @{$rfile}, $line . "\n";
        }
    }
}

sub write_line {

    # This is called from perltidy line-by-line
    my ( $self, $line_of_tokens ) = @_;
    my $rmasked_file   = $self->{_rmasked_file};
    my $roriginal_file = $self->{_roriginal_file};
    my $opt_c          = $self->{_compression};

    my $line_type         = $line_of_tokens->{_line_type};
    my $input_line_number = $line_of_tokens->{_line_number};
    my $input_line        = $line_of_tokens->{_line_text};
    my $rtoken_type       = $line_of_tokens->{_rtoken_type};
    my $rtokens           = $line_of_tokens->{_rtokens};
    chomp $input_line;

    # mask non-CODE lines
    if ( $line_type ne 'CODE' ) {
        return if ( $opt_c == 3 );
        my $len = length($input_line);
        if ( $opt_c == 0 && $len > 0 ) {
            print_line( $roriginal_file, $input_line ) if $roriginal_file;
            print_line( $rmasked_file, '#' x $len ); 
        }
        else {
            print_line( $roriginal_file, $input_line ) if $roriginal_file;
            print_line( $rmasked_file, "" );
        }
        return;
    }

    # we'll build the masked line token by token
    my $masked_line = "";

    # add leading spaces if not in a higher compression mode
    if ( $opt_c <= 1 ) {

        # Find leading whitespace.  But be careful..we don't want the
        # whitespace if it is part of quoted text, because it will 
        # already be contained in a token.
        if ( $input_line =~ /^(\s+)/ && !$line_of_tokens->{_starting_in_quote} )
        {
            $masked_line = $1;
        }
    }

    # loop over tokens to construct one masked line
    for ( my $j = 0 ; $j < @$rtoken_type ; $j++ ) {

        # Mask certain token types by replacing them with their type code:
        # type  definition
        # ----  ----------
        # Q     quote or pattern
        # q     qw quote
        # h     << here doc operator
        # #     comment
        #
        # This choice will produce a mask file that has balanced
        # container tokens and does not cause parsing problems.
        if ( $$rtoken_type[$j] =~ /^[Qqh]$/ ) {
            if ( $opt_c <= 1 ) {
                $masked_line .= $$rtoken_type[$j] x length( $$rtokens[$j] );
            }
            else {
                $masked_line .= $$rtoken_type[$j];
            }
        }

        # Mask a comment
        elsif ( $$rtoken_type[$j] eq '#' ) {
            if ( $opt_c == 0 ) {
                $masked_line .= '#' x length( $$rtokens[$j] );
            }
        }

        # All other tokens go out verbatim
        else {
            $masked_line .= $$rtokens[$j];
        }
    }
    print_line( $roriginal_file, $input_line ) if $roriginal_file;
    print_line( $rmasked_file, $masked_line );

    # self-check lengths; this error should never happen
    if ( $opt_c == 0 && length($masked_line) != length($input_line) ) {
        my $lmask  = length($masked_line);
        my $linput = length($input_line);
        print STDERR
"$input_line_number: length ERROR, masked length=$lmask but input length=$linput\n";
    }
}

# called once after the last line of a file
sub finish_formatting {
    my $self = shift;
    return;
}
