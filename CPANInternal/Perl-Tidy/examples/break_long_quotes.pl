#!/usr/bin/perl -w

# Break long quoted strings in perl code into smaller pieces
# This version only breaks at blanks.  See sub break_at_blanks to
# customize.
#
# usage:
# break_long_quotes.pl -ln myfile.pl >myfile.new
#
# where n specifies the maximum quote length.

# NOTES:
# 1. Use with caution - has not been extensively tested
#
# 2. The output is not beautified so that you can use diff to see what
# changed.  If all is ok, run the output through perltidy to clean it up.
#
# 3. This version only breaks single-line quotes contained within
# either single or double quotes.

# Steve Hancock, Sept 28, 2006
#
use strict;
use Getopt::Std;
$| = 1;
use vars qw($opt_l $opt_h);

my $usage = <<EOM;
   usage: break_long_quotes.pl [ -ln ] filename >outfile
          where n=line length (default 72)
EOM

getopts('hl:') or die "$usage";
if ($opt_h) { die $usage }
if ( !defined $opt_l ) {
    $opt_l = 70;
}
else {
    $opt_l =~ /^\d+$/ or die "$usage";
}

unless ( @ARGV == 1 ) { die $usage }
my $file = $ARGV[0];
scan_file( $file, $opt_l );

sub scan_file {
    my ( $file, $line_length ) = @_;
    use Perl::Tidy;
    use IO::File;
    my $fh = IO::File->new( $file, 'r' );
    unless ($fh) { die "cannot open '$file': $!\n" }
    my $formatter = MyWriter->new($line_length);

    perltidy(
        'formatter' => $formatter,     # callback object
        'source'    => $fh,
        'argv'      => "-npro -se",    # dont need .perltidyrc
                                       # errors to STDOUT
    );
    $fh->close();
} ## end sub scan_file

#####################################################################
#
# This is a class with a write_line() method which receives
# tokenized lines from perltidy
#
#####################################################################

package MyWriter;

sub new {
    my ( $class, $line_length ) = @_;
    my $comment_block = "";
    bless {
        _rcomment_block          => \$comment_block,
        _maximum_comment_length  => 0,
        _max_quote_length        => $line_length,
        _in_hanging_side_comment => 0,
    }, $class;
} ## end sub new

sub write_line {

    # This is called from perltidy line-by-line
    # We will look for quotes and fix them up if necessary
    my $self              = shift;
    my $line_of_tokens    = shift;
    my $line_type         = $line_of_tokens->{_line_type};
    my $input_line_number = $line_of_tokens->{_line_number};
    my $input_line        = $line_of_tokens->{_line_text};    # the orignal line
    my $rtoken_type       = $line_of_tokens->{_rtoken_type};  # type of tokens
    my $rtokens           = $line_of_tokens->{_rtokens};      # text of tokens
    my $starting_in_quote =
      $line_of_tokens->{_starting_in_quote};                  # text of tokens
    my $ending_in_quote  = $line_of_tokens->{_ending_in_quote}; # text of tokens
    my $max_quote_length = $self->{_max_quote_length};
    chomp $input_line;

    # look in lines of CODE (and not POD for example)
    if ( $line_type eq 'CODE' && @$rtoken_type ) {

        my $jmax = @$rtoken_type - 1;

        # find leading whitespace
        my $leading_whitespace = ( $input_line =~ /^(\s*)/ ) ? $1 : "";
        if ($starting_in_quote) {$leading_whitespace=""};
        my $new_line = $leading_whitespace;

        # loop over tokens looking for quotes (token type Q)
        for ( my $j = 0 ; $j <= $jmax ; $j++ ) {

            # pull out the actual token text
            my $token = $$rtokens[$j];

            # look for long quoted strings on a single line
            # (multiple line quotes not currently handled)
            if (   $$rtoken_type[$j] eq 'Q'
                && !( $j == 0     && $starting_in_quote )
                && !( $j == $jmax && $ending_in_quote )
                && ( length($token) > $max_quote_length ) )
            {
                my $quote_char = substr( $token, 0, 1 );
                if ( $quote_char eq '"' || $quote_char eq '\'' ) {

                    # safety check - shouldn't happen
                    my $check_char = substr( $token, -1, 1 );
                    if ( $check_char ne $quote_char ) {
                        die <<EOM;
programming error at line $input_line 
starting quote character is <<$quote_char>> but ending quote character is <<$check_char>>
quoted string is:
$token
EOM
                    } ## end if ( $check_char ne $quote_char)
                    $token =
                      break_at_blanks( $token, $quote_char, $max_quote_length );
                } ## end if ( $quote_char eq '"'...
            } ## end if ( $$rtoken_type[$j]...
            $new_line .= $token;
        } ## end for ( my $j = 0 ; $j <=...

        # substitude the modified line for the original line
        $input_line = $new_line;
    } ## end if ( $line_type eq 'CODE')

    # print the line
    $self->print($input_line."\n");
    return;
} ## end sub write_line

sub break_at_blanks {

    # break a string at one or more spaces so that the longest substring is
    # less than the desired length (if possible).
    my ( $str, $quote_char, $max_length ) = @_;
    my $blank     = ' ';
    my $prev_char = "";
    my @break_after_pos;
    my $quote_pos = -1;
    while ( ( $quote_pos = index( $str, $blank, 1 + $quote_pos ) ) >= 0 ) {

        # as a precaution, do not break if preceded by a backslash
        if ( $quote_pos > 0 ) {
            next if ( substr( $str, $quote_pos - 1, 1 ) eq '\\' );
        }
        push @break_after_pos, $quote_pos;
    } ## end while ( ( $quote_pos = index...
    push @break_after_pos, length($str);

    my $starting_pos = 0;
    my $new_str      = "";
    for ( my $i = 1 ; $i < @break_after_pos ; $i++ ) {
        my $pos    = $break_after_pos[$i];
        my $length = $pos - $starting_pos;
        if ( $length > $max_length - 1 ) {
            $pos = $break_after_pos[ $i - 1 ];
            $new_str .= substr( $str, $starting_pos, $pos - $starting_pos + 1 )
              . "$quote_char . $quote_char";
            $starting_pos = $pos + 1;
        } ## end if ( $length > $max_length...
    } ## end for ( my $i = 1 ; $i < ...
    my $pos = length($str);
    $new_str .= substr( $str, $starting_pos, $pos );
    return $new_str;
} ## end sub break_at_blanks

sub print {
    my ( $self, $input_line ) = @_;
    print $input_line;
}

# called once after the last line of a file
sub finish_formatting {
    my $self = shift;
    $self->flush_comments();
}
