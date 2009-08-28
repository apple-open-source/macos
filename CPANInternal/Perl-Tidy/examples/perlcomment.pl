#!/usr/bin/perl -w
#
# Walk through a perl script and reformat perl comments 
# using Text::Autoformat.
#
# usage:
# perlcomment -l72 myfile.pl >myfile.new
#
# where -l specifies the maximum comment line length.
#
# You will be given an opportunity to accept or reject each proposed
# change.
#
# This file demonstrates using Perl::Tidy to walk through a perl file
# and find all of its comments.  It offers to reformat each group of
# consecutive full-line comments with Text::Autoformat.  
#
# This may or may not be useful, depending on your coding style.
# Change it to suit your own purposes; see sub get_line().
#
# Uses: Text::Autoformat 
#       Perl::Tidy
#
# Steve Hancock, March 2003
# Based on a suggestion by Tim Maher
#
# TODO: (just ideas that probably won't get done)
# -Handle lines of stars, dashes, etc better
# -Need flag to limit changes to lines greater than some minimum length
# -reformat side and hanging side comments
use strict;
use Getopt::Std;
use Text::Autoformat;
$| = 1;
use vars qw($opt_l $opt_h);

my $usage = <<EOM;
   usage: perlcomment [ -ln ] filename >outfile
          where n=line length (default 72)
EOM

getopts('hl:') or die "$usage";
if ($opt_h) {die $usage}
if ( !defined $opt_l ) {
    $opt_l = 72;
}
else {
    $opt_l =~ /^\d+$/ or die "$usage";
}

unless ( @ARGV == 1 ) { die $usage }
my $file = $ARGV[0];
autoformat_file( $file, $opt_l );

sub autoformat_file {
    my ( $file, $line_length ) = @_;
    use Perl::Tidy;
    use IO::File;
    my $fh = IO::File->new( $file, 'r' );
    unless ($fh) { die "cannot open '$file': $!\n" }
    my $formatter = CommentFormatter->new($line_length);

    perltidy(
        'formatter' => $formatter,    # callback object
        'source'    => $fh,
        'argv'        => "-npro -se",   # dont need .perltidyrc
                                        # errors to STDOUT
    );
    $fh->close();
}

#####################################################################
#
# The CommentFormatter object has a write_line() method which receives
# tokenized lines from perltidy
#
#####################################################################

package CommentFormatter;

sub new {
    my ( $class, $line_length ) = @_;
    my $comment_block = "";
    bless {
        _rcomment_block          => \$comment_block,
        _maximum_comment_length  => 0,
        _line_length             => $line_length,
        _in_hanging_side_comment => 0,
      },
      $class;
}

sub write_line {

    # This is called from perltidy line-by-line
    # Comments will be treated specially (reformatted)
    # Other lines go to stdout immediately
    my $self           = shift;
    my $line_of_tokens = shift;
    my $line_type      = $line_of_tokens->{_line_type}; 
    ## my $input_line_number = $line_of_tokens->{_line_number}; 
    my $input_line  = $line_of_tokens->{_line_text};  # the orignal line
    my $rtoken_type = $line_of_tokens->{_rtoken_type}; # type of tokens
    my $rtokens     = $line_of_tokens->{_rtokens}; # text of tokens

    # Just print non-code, non-comment lines
    if (
        $line_type ne 'CODE'    # if it's not code,
        || !@$rtokens           # or is a blank line
        || $$rtoken_type[-1] ne '#'    # or the last token isn't a comment
      )
    {
        $self->print($input_line);
        $self->{_in_hanging_side_comment} = 0;
        return;
    }

    # Now we either have:
    # - a line with a side comment (@$rtokens >1), or
    # - a full line comment (@$rtokens==1)

    # Output a line with a side comment, but remember it
    if (@$rtokens > 1) {
        $self->print($input_line);
        $self->{_in_hanging_side_comment} = 1;
        return;
    }

    # A hanging side comment is a full-line comment immediately
    # following a side comment or another hanging side comment.
    # Output a hanging side comment directly
    if ($self->{_in_hanging_side_comment}) {
        $self->print($input_line);
        return;
    }

    # Now we know we have a full-line, non-hanging, comment
    # Decide what to do --

    # output comment without any words directly, since these don't get
    # handled well by autoformat yet.  For example, a box of stars.
    # TODO: we could truncate obvious separator lines to the desired
    # line length
    if ( $$rtokens[-1] !~ /\w/ ) {
        $self->print($input_line);
    }

    # otherwise, append this comment to the group we are collecting
    else {
        $self->append_comment($input_line);
    }
    return;
}

sub print {
    my ( $self, $input_line ) = @_;
    $self->flush_comments();
    print $input_line;
}

sub append_comment {
    my ( $self, $input_line ) = @_;
    my $rcomment_block = $self->{_rcomment_block};
    my $maximum_comment_length = $self->{_maximum_comment_length};
    $$rcomment_block .= $input_line;
    if (length($input_line) > $maximum_comment_length) {
        $self->{_maximum_comment_length}=length($input_line);
    }
}

{
    my ( $separator1, $separator2, $separator3 );

    BEGIN {
        $separator1 = '-' x 2 . ' Original ' . '-' x 60 . "\n";
        $separator2 = '-' x 2 . ' Modified ' . '-' x 60 . "\n";
        $separator3 = '-' x 72 . "\n";
    }

    sub flush_comments {

        my ($self)         = @_;
        my $rcomment_block = $self->{_rcomment_block};
        my $line_length    = $self->{_line_length};
        my $maximum_comment_length = $self->{_maximum_comment_length};
        if ($$rcomment_block) {
            my $comments           = $$rcomment_block;

            # we will just reformat lines longer than the desired length for now
            # TODO: this can be changed
            if ( $maximum_comment_length > $line_length ) {
                my $formatted_comments =
                  Text::Autoformat::autoformat( $comments,
                    { right => $line_length, all => 1 } );

                if ( $formatted_comments ne $comments ) {
                    print STDERR $separator1;
                    print STDERR $$rcomment_block;
                    print STDERR $separator2;
                    print STDERR $formatted_comments;
                    print STDERR $separator3;
                    if ( ifyes("Accept Changes? [Y/N]") ) {
                        $comments = $formatted_comments;
                    }
                }
            }
            print $comments;
            $$rcomment_block = "";
            $self->{_maximum_comment_length}=0;
        }
    }
}

sub query {
    my ($msg) = @_;
    print STDERR $msg;
    my $ans = <STDIN>;
    chomp $ans;
    return $ans;
}

sub queryu {
    return uc query(@_);
}

sub ifyes {
    my $count = 0;
  ASK:
    my $ans   = queryu(@_);
    if    ( $ans =~ /^Y/ ) { return 1 }
    elsif ( $ans =~ /^N/ ) { return 0 }
    else {
        $count++;
        if ( $count > 6 ) { die "error count exceeded in ifyes\n" }
        print STDERR "Please answer 'Y' or 'N'\n";
        goto ASK;
    }
}

# called once after the last line of a file
sub finish_formatting {
    my $self = shift;
    $self->flush_comments();
}
