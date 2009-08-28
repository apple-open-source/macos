#!/usr/bin/perl -w
use strict;
#
# Convert a perl script into an xml file
#
# usage:
# perlxmltok myfile.pl >myfile.xml
# perlxmltok <myfile.pl >myfile.xml
#
# The script is broken at the line and token level. 
#
# This file is one of the examples distributed with perltidy and demonstrates
# using a callback object with Perl::Tidy to walk through a perl file and
# process its tokens.  It may or may not have any actual usefulness.  You can
# modify it to suit your own purposes; see sub get_line().
#
use Perl::Tidy;
use IO::File;
use Getopt::Std;
use vars qw($opt_h);
my $file;
my $usage = <<EOM;
   usage: perlxmltok filename >outfile
EOM
getopts('h') or die "$usage";
if ($opt_h) {die $usage}
if ( @ARGV == 1 ) {
    $file = $ARGV[0];
}
else { die $usage }
my $source;
my $fh;
if ($file) {
    $fh = IO::File->new( $file, 'r' );
    unless ($fh) { die "cannot open '$file': $!\n" }
    $source = $fh;
}
else {
    $source = '-';
}
my $formatter = Perl::Tidy::XmlWriter->new($file);
my $dest;

# start perltidy, which will start calling our write_line()
perltidy(
    'formatter'   => $formatter,    # callback object
    'source'      => $source,
    'destination' => \$dest,        # not really needed
    'argv'        => "-npro -se",   # dont need .perltidyrc
                                    # errors to STDOUT
);
$fh->close() if $fh;

#####################################################################
#
# The Perl::Tidy::XmlWriter class writes a copy of the input stream in xml
#
#####################################################################

package Perl::Tidy::XmlWriter;

# class variables
use vars qw{
  %token_short_names
  %short_to_long_names
  $rOpts
  $missing_html_entities
};

# replace unsafe characters with HTML entity representation if HTML::Entities
# is available
{ eval "use HTML::Entities"; $missing_html_entities = $@; }

sub new {

    my ( $class, $input_file ) = @_;
    my $self = bless { }, $class;

    $self->print( <<"HEADER");
<?xml version = "1.0"?>
HEADER

    unless ( !$input_file || $input_file eq '-' || ref($input_file) ) {

        $self->print( <<"COMMENT");
<!-- created by perltidy from file: $input_file -->
COMMENT
    }

    $self->print("<file>\n");
    return $self;
}

sub print {
    my ( $self, $line ) = @_;
    print $line;
}

sub write_line {

    # This routine will be called once perl line by perltidy
    my $self = shift;
    my ($line_of_tokens) = @_;
    my $line_type        = $line_of_tokens->{_line_type};
    my $input_line       = $line_of_tokens->{_line_text};
    my $line_number      = $line_of_tokens->{_line_number};
    chomp $input_line;
    $self->print(" <line type='$line_type'>\n");
    $self->print("  <text>\n");

    $input_line = my_encode_entities($input_line);
    $self->print("$input_line\n");
    $self->print("  </text>\n");

    # markup line of code..
    if ( $line_type eq 'CODE' ) {
        my $xml_line;
        my $rtoken_type = $line_of_tokens->{_rtoken_type};
        my $rtokens     = $line_of_tokens->{_rtokens};

        if ( $input_line =~ /(^\s*)/ ) {
            $xml_line = $1;
        }
        else {
            $xml_line = "";
        }
        my $rmarked_tokens = $self->markup_tokens( $rtokens, $rtoken_type );
        $xml_line .= join '', @$rmarked_tokens;

        $self->print("  <tokens>\n");
        $self->print("$xml_line\n");
        $self->print("  </tokens>\n");
    }

    $self->print(" </line>\n");
}

BEGIN {

    # This is the official list of tokens which may be identified by the
    # user.  Long names are used as getopt keys.  Short names are
    # convenient short abbreviations for specifying input.  Short names
    # somewhat resemble token type characters, but are often different
    # because they may only be alphanumeric, to allow command line
    # input.  Also, note that because of case insensitivity of xml,
    # this table must be in a single case only (I've chosen to use all
    # lower case).
    # When adding NEW_TOKENS: update this hash table
    # short names => long names
    %short_to_long_names = (
        'n'  => 'numeric',
        'p'  => 'paren',
        'q'  => 'quote',
        's'  => 'structure',
        'c'  => 'comment',
        'b'  => 'blank',
        'v'  => 'v-string',
        'cm' => 'comma',
        'w'  => 'bareword',
        'co' => 'colon',
        'pu' => 'punctuation',
        'i'  => 'identifier',
        'j'  => 'label',
        'h'  => 'here-doc-target',
        'hh' => 'here-doc-text',
        'k'  => 'keyword',
        'sc' => 'semicolon',
        'm'  => 'subroutine',
        'pd' => 'pod-text',
    );

    # Now we have to map actual token types into one of the above short
    # names; any token types not mapped will get 'punctuation'
    # properties.

    # The values of this hash table correspond to the keys of the
    # previous hash table.
    # The keys of this hash table are token types and can be seen
    # by running with --dump-token-types (-dtt).

    # When adding NEW_TOKENS: update this hash table
    # $type => $short_name
    %token_short_names = (
        '#'  => 'c',
        'n'  => 'n',
        'v'  => 'v',
        'b'  => 'b',
        'k'  => 'k',
        'F'  => 'k',
        'Q'  => 'q',
        'q'  => 'q',
        'J'  => 'j',
        'j'  => 'j',
        'h'  => 'h',
        'H'  => 'hh',
        'w'  => 'w',
        ','  => 'cm',
        '=>' => 'cm',
        ';'  => 'sc',
        ':'  => 'co',
        'f'  => 'sc',
        '('  => 'p',
        ')'  => 'p',
        'M'  => 'm',
        'P'  => 'pd',
    );

    # These token types will all be called identifiers for now
    # FIXME: need to separate user defined modules as separate type
    my @identifier = qw" i t U C Y Z G :: ";
    @token_short_names{@identifier} = ('i') x scalar(@identifier);

    # These token types will be called 'structure'
    my @structure = qw" { } ";
    @token_short_names{@structure} = ('s') x scalar(@structure);

}

sub markup_tokens {
    my $self = shift;
    my ( $rtokens, $rtoken_type ) = @_;
    my ( @marked_tokens, $j, $string, $type, $token );

    for ( $j = 0 ; $j < @$rtoken_type ; $j++ ) {
        $type  = $$rtoken_type[$j];
        $token = $$rtokens[$j];

        #-------------------------------------------------------
        # Patch : intercept a sub name here and split it
        # into keyword 'sub' and sub name
        if ( $type eq 'i' && $token =~ /^(sub\s+)(\w.*)$/ ) {
            $token = $self->markup_xml_element( $1, 'k' );
            push @marked_tokens, $token;
            $token = $2;
            $type  = 'M';
        }

        # Patch : intercept a package name here and split it
        # into keyword 'package' and name
        if ( $type eq 'i' && $token =~ /^(package\s+)(\w.*)$/ ) {
            $token = $self->markup_xml_element( $1, 'k' );
            push @marked_tokens, $token;
            $token = $2;
            $type  = 'i';
        }
        #-------------------------------------------------------

        $token = $self->markup_xml_element( $token, $type );
        push @marked_tokens, $token;
    }
    return \@marked_tokens;
}

sub my_encode_entities {
    my ($token) = @_;

    # escape any characters not allowed in XML content.
    # ??s/’/&apos;/;
    if ($missing_html_entities) {
        $token =~ s/\&/&amp;/g;
        $token =~ s/\</&lt;/g;
        $token =~ s/\>/&gt;/g;
        $token =~ s/\"/&quot;/g;
    }
    else {
        HTML::Entities::encode_entities($token);
    }
    return $token;
}

sub markup_xml_element {
    my $self = shift;
    my ( $token, $type ) = @_;
    if ($token) { $token = my_encode_entities($token) }

    # get the short abbreviation for this token type
    my $short_name = $token_short_names{$type};
    if ( !defined($short_name) ) {
        $short_name = "pu";    # punctuation is default
    }
    $token = qq(<$short_name>) . $token . qq(</$short_name>);
    return $token;
}

sub finish_formatting {

    # called after last line
    my $self = shift;
    $self->print("</file>\n");
    return;
}
