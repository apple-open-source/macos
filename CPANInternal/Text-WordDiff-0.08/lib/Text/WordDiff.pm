package Text::WordDiff;

use strict;
use vars qw(@ISA $VERSION);
use Algorithm::Diff ();
use IO::File;
use Carp;

$VERSION = '0.08';

# _Mastering Regular Expressions_, p. 132.
my $BEGIN_WORD = $] >= 5.006
    ? qr/(?:(?<!\p{IsWord})(?=\p{IsWord})|(?<!\p{IsPunct})(?=\p{IsPunct})|(?<!\p{IsCntrl})(?=\p{IsCntrl}))/msx
    : qr/(?:(?<!\w)(?=\w)|(?<![\]\[!"%&'()*,\.\/:;?\{}\-@])(?=[\]\[!"%&'()*,\.\/:;?\{}\-@])|(?<![\n\r\t])(?=[\n\r\t]))/msx;

my %styles = (
    ANSIColor    => undef,
    HTML         => undef,
    HTMLTwoLines => undef,
);

sub import {
    my $caller = caller;
    no strict 'refs';
    *{"$caller\::word_diff"} = \&word_diff;
}

sub word_diff ($$;$) {
    my @seqs = ( shift, shift );
    my $opts = $_[0] ? { %{ +shift } } : {};
    $opts->{FILENAME_PREFIX_A} ||= '---';
    $opts->{FILENAME_PREFIX_B} ||= '+++';
    my $AorB = 'A';

    for my $seq (@seqs) {
        my $type = ref $seq;

        while ( $type eq 'CODE' ) {
            $seq = $seq->( $opts );
            $type = ref $seq;
        }

        # Get a handle on options.
        my $filename = \$opts->{"FILENAME_$AorB"};
        my $mtime    = \$opts->{"MTIME_$AorB"};

        if ( $type eq 'ARRAY' ) {
            # The work has already been done for us.
        }

        elsif ( $type eq 'SCALAR' ) {
            # Parse the words from the string.
            $seq     = [ split $BEGIN_WORD, $$seq ];
        }

        elsif ( !$type ) {
            # Assume that it's a raw file name.
            $$filename = $seq           unless defined $$filename;
            $$mtime    = (stat $seq)[9] unless defined $$mtime;

            # Parse the words from the file.
            my $seq_fh = IO::File->new($seq, '<');
            $seq       = do { local $/; [ split $BEGIN_WORD, <$seq_fh> ] };
            $seq_fh->close;
        }

        elsif ( $type eq "GLOB" || UNIVERSAL::isa( $seq, "IO::Handle" ) ) {
            # Parse the words from the file.
            $seq       = do { local $/; [ split $BEGIN_WORD, <$seq> ] };
        }

        else {
            # Damn.
            confess "Can't handle input of type $type";
        }
        $AorB++;
    }

    # Set up the output handler.
    my $output;
    my $out_handler = delete $opts->{OUTPUT};
    my $type = ref $out_handler ;

    if ( ! defined $out_handler ) {
        # Default to concatenating a string.
        $output = '';
        $out_handler = sub { $output .= shift };
    }
    elsif ( $type eq 'CODE' ) {
        # We'll just use the handler.
    }
    elsif ( $type eq 'SCALAR' ) {
        # Append to the scalar reference.
        my $out_ref = $out_handler;
        $out_handler = sub { $$out_ref .= shift };
    }
    elsif ( $type eq 'ARRAY' ) {
        # Push each item onto the array.
        my $out_ref = $out_handler;
        $out_handler = sub { push @$out_ref, shift };
    }
    elsif ( $type eq 'GLOB' || UNIVERSAL::isa( $out_handler, 'IO::Handle' )) {
        # print to the file handle.
        my $output_handle = $out_handler;
        $out_handler = sub { print $output_handle shift };
    }
    else {
        # D'oh!
        croak "Unrecognized output type: $type";
    }

    # Instantiate the diff object, along with any options.
    my $diff = Algorithm::Diff->new(@seqs, delete $opts->{DIFF_OPTS});

    # Load the style class and instantiate an instance.
    my $style  = delete $opts->{STYLE} || 'ANSIColor';
    $style     = __PACKAGE__ . "::$style" if exists $styles{$style};
    eval "require $style" or die $@ unless $style->can('new');
    $style     = $style->new($opts) if !ref $style;

    # Run the diff.
    my $hunks = 0;
    $out_handler->($style->file_header());
    while ($diff->Next) {
        $hunks++;
        $out_handler->( $style->hunk_header() );

        # Output unchanged items.
        if (my @same = $diff->Same) {
            $out_handler->( $style->same_items(@same) );
        }

        # Output deleted and inserted items.
        else {
            if (my @del = $diff->Items(1)) {
                $out_handler->( $style->delete_items(@del) );
            }
            if (my @ins = $diff->Items(2)) {
                $out_handler->( $style->insert_items(@ins) );
            }
        }
        $out_handler->( $style->hunk_footer() );
    }
    $out_handler->( $style->file_footer() );

    return defined $output ? $output : $hunks;
}

package Text::WordDiff::Base;

sub new {
    my ($class, $opts) = @_;
    return bless { %{$opts} } => $class;
}


sub file_header  {
    my $self = shift;
    my $fn1 = $self->filename_a;
    my $fn2 = $self->filename_b;
    return '' unless defined $fn1 && defined $fn2;

    my $p1 = $self->filename_prefix_a;
    my $t1 = $self->mtime_a;
    my $p2 = $self->filename_prefix_b;
    my $t2 = $self->mtime_b;

    return "$p1 $fn1" . (defined $t1 ? "\t" . localtime $t1 : '') . "\n"
         . "$p2 $fn2" . (defined $t2 ? "\t" . localtime $t2 : '') . "\n"
         ;
}

sub hunk_header         { return '' }
sub same_items          { return '' }
sub insert_items        { return '' }
sub delete_items        { return '' }
sub hunk_footer         { return '' }
sub file_footer         { return '' }
sub filename_a          { return shift->{FILENAME_A} }
sub filename_b          { return shift->{FILENAME_B} }
sub mtime_a             { return shift->{MTIME_A}    }
sub mtime_b             { return shift->{MTIME_B}    }
sub filename_prefix_a   { return shift->{FILENAME_PREFIX_A} }
sub filename_prefix_b   { return shift->{FILENAME_PREFIX_B} }

1;
__END__

##############################################################################

=head1 Name

Text::WordDiff - Track changes between documents

=head1 Synopsis

    use Text::WordDiff;

    my $diff = word_diff 'file1.txt', 'file2.txt', { STYLE => 'HTML' };
    my $diff = word_diff \$string1,   \$string2,   { STYLE => 'ANSIColor' };
    my $diff = word_diff \*FH1,       \*FH2;       \%options;
    my $diff = word_diff \&reader1,   \&reader2;
    my $diff = word_diff \@records1,  \@records2;

    # May also mix input types:
    my $diff = word_diff \@records1,  'file_B.txt';

=head1 Description

This module is a variation on the lovely L<Text::Diff|Text::Diff> module.
Rather than generating traditional line-oriented diffs, however, it generates
word-oriented diffs. This can be useful for tracking changes in narrative
documents or documents with very long lines. To diff source code, one is still
best off using L<Text::Diff|Text::Diff>. But if you want to see how a short
story changed from one version to the next, this module will do the job very
nicely.

=head2 What is a Word?

I'm glad you asked! Well, sort of. It's a really hard question to answer. I
consulted a number of sources, but really just did my best to punt on the
question by reformulating it as, "How do I split text up into individual
words?" The short answer is to split on word boundaries. However, every word
has two boundaries, one at the beginning and one at the end. So splitting on
C</\b/> didn't work so well. What I really wanted to do was to split on the
I<beginning> of every word. Fortunately, _Mastering Regular Expressions_ has a
recipe for that: C<< /(?<!\w)(?=\w)/ >>. I've borrowed this regular expression
for use in Perls before 5.6.x, but go for the Unicode variant in 5.6.0 and
newer: C<< /(?<!\p{IsWord})(?=\p{IsWord})/ >>. Adding some additional controls
for punctuation and control characters, this sentence, for example, would be
split up into the following tokens:

  my @words = (
      "Adding ",
      "some ",
      "additional ",
      "controls",
      "\n",
      "for ",
      "punctuation ",
      "and ",
      "control ",
      "characters",
      ", ",
      "this ",
      "sentence",
      ", ",
      "for ",
      "example",
      ", ",
      "would ",
      "be",
      "\n",
      "split ",
      "up ",
      "into ",
      "the ",
      "following ",
      "tokens",
      ":",
  );

So it's not just comparing words, but word-like tokens and control/punctuation
tokens. This makes sense to me, at least, as the diff is between these tokens,
and thus leads to a nice word-and-space-and-punctuation type diff. It's not
unlike what a word processor might do (although a lot of them are
character-based, but that seemed a bit extreme--feel free to dupe this module
into Text::CharDiff!).

Now, I acknowledge that there are localization issues with this approach. In
particular, it will fail with Chinese, Japanese, and Korean text, as these
languages don't put non-word characters between words. Ideally, Test::WordDiff
would then split on every charaters (since a single character often equals a
word), but such is not the case when the C<utf8> flag is set on a string.
For example, This simple script:

=encoding utf8

  use strict;
  use utf8;
  use Data::Dumper;
  my $string = '뼈뼉뼘뼙뼛뼜뼝뽀뽁뽄뽈뽐뽑뽕뾔뾰뿅뿌뿍뿐뿔뿜뿟뿡쀼쁑쁘쁜쁠쁨쁩삐';
  my @tokens = split /(?<!\p{IsWord})(?=\p{IsWord})/msx, $string;
  print Dumper \@tokens;

Outputs:

  $VAR1 = [
            "\x{bf08}\x{bf09}\x{bf18}\x{bf19}\x{bf1b}\x{bf1c}\x{bf1d}\x{bf40}\x{bf41}\x{bf44}\x{bf48}\x{bf50}\x{bf51}\x{bf55}\x{bf94}\x{bfb0}\x{bfc5}\x{bfcc}\x{bfcd}\x{bfd0}\x{bfd4}\x{bfdc}\x{bfdf}\x{bfe1}\x{c03c}\x{c051}\x{c058}\x{c05c}\x{c060}\x{c068}\x{c069}\x{c090}"
          ];

Not so useful. It seems to be less of a problem if the C<use utf8;> line is
commented out, in which case we get:

  $VAR1 = [
            '뼈',
            '뼉',
            '뼘',
            '뼙',
            '뼛',
            '뼜',
            '뼝',
            '뽀',
            '뽁',
            '뽄',
            '뽈',
            '뽐',
            '뽑',
            '뽕',
            '뾔',
            '뾰',
            '뿅',
            '뿌',
            '뿍',
            '뿐',
            '뿔',
            '뿜',
            '뿟',
            '뿡',
            '?',
            '?쁑',
            '쁘',
            '쁜',
            '쁠',
            '쁨',
            '쁩',
            '삐'
          ];

Someone whose more familiar with non-space-using languages will have to
explain to me how I might be able to duplicate this pattern within the scope
of C<use utf8;>, seing as it may very well be important to have it on in order
to ensure proper character semantics.

However, if my word tokenization approach is just too naive, and you decide
that you need to take a different approach (maybe use
L<Lingua::ZH::Toke|Lingua::ZH::Toke> or similar module), you can still use
this module; you'll just have to tokenize your strings into words yourself,
and pass them to word_diff() as array references:

  word_diff \@my_words1, \@my_words2;

=head1 Options

word_diff() takes two arguments from which to draw input and an optional hash
reference of options to control its output. The first two arguments contain
the data to be diffed, and each may be in the form of any of the following
(that is, they can be in two different formats):

=over

=item * String

A bare scalar will be assumed to be a file name. The file will be opened and
split up into words. word_diff() will also C<stat> the file to get the last
modified time for use in the header, unless the relevant option (C<MTIME_A> or
C<MTIME_B>) has been specified explicitly.

=item * Scalar Reference

A scalar reference will be assumed to refer to a string. That string will be
split up into words.

=item * Array Reference

An array reference will be assumed to be a list of words.

=item * File Handle

A glob or IO::Handle-derived object will be read from and split up into
its constituent words.

=back

The optional hash reference may contain the following options. Additional
options may be specified by the formattting class; see the specific class for
details.

=over

=item * STYLE

"ANSIColor", "HTML" or an object or class name for a class providing
C<file_header()>, C<hunk_header()>, C<same_items()>, C<delete_items()>,
C<insert_items()>, C<hunk_footer()> and C<file_footer()> methods. Defaults to
"ANSIColor" for nice display of diffs in an ANSI Color-supporting terminal.

If the package indicated by the C<STYLE> has no C<new()> method,
C<word_diff()> will load it automatically (lazy loading). It will then
instantiate an object of that class, passing in the options hash reference
with which the formatting class can initialize the object.

Styles may be specified as class names (C<< STYLE => "My::Foo" >>), in which
case they will be instantiated by calling the C<new()> construcctor and
passing in the options hash reference, or as objects (C<< STYLE =>
My::Foo->new >>).

The simplest way to implement your own formatting style is to create a new
class that inherits from Text::WordDiff::Base, wherein the C<new()> method is
already provided, and the C<file_header()> returns a Unified diff-style
header. All of the other formatting methods simply return empty strings, and
are therefore ripe for overriding.

=item * FILENAME_A, MTIME_A, FILENAME_B, MTIME_B

The name of the file and the modification time "files" in epoch seconds.
Unless a defined value is specified for these options, they will be filled in
for each file when word_diff() is passed a filename. If a filename is not
passed in and C<FILENAME_A> and C<FILENAME_B> are not defined, the header will
not be printed by the base formatting base class.

=item * OUTPUT

The method by which diff output should be, well, I<output>. Examples and their
equivalent subroutines:

    OUTPUT => \*FOOHANDLE,   # like: sub { print FOOHANDLE shift() }
    OUTPUT => \$output,      # like: sub { $output .= shift }
    OUTPUT => \@output,      # like: sub { push @output, shift }
    OUTPUT => sub { $output .= shift },

If C<OUTPUT> is not defined, word_diff() will simply return the diff as a
string. If C<OUTPUT> is a code reference, it will be called once with the file
header, once for each hunk body, and once for each piece of content. If
C<OUTPUT> is an L<IO::Handle|IO::Handle>-derived object, output will be
sent to that handle.

=item * FILENAME_PREFIX_A, FILENAME_PREFIX_B

The string to print before the filename in the header. Defaults are C<"---">,
C<"+++">.

=item * DIFF_OPTS

A hash reference to be passed as the options to C<< Algorithm::Diff->new >>.
See L<Algorithm::Diff|Algorithm::Diff> for details on available options.

=back

=head1 Formatting Classes

Text::WordDiff comes with two formatting classes:

=over

=item L<Text::WordDiff::ANSIColor|Text::WordDiff::ANSIColor>

This is the default formatting class. It emits a header and then the diff
content, with deleted text in bodfaced red and inserted text in boldfaced
green.

=item L<Text::WordDiff::HTML|Text::WordDiff::HTML>

Specify C<< STYLE => 'HTML' >> to take advantage of this formatting class. It
outputs the diff content as XHTML, with deleted text in C<< <del> >> elements
and inserted text in C<< <ins> >> elements.

=back

To implement your own formatting class, simply inherit from
Text::WordDiff::Base and override its methods as necssary. By default,
only the C<file_header()> formatting method returns a value. All others
simply return empty strings, and are therefore ripe for overriding:

  package My::WordDiff::Format;
  use base 'Text::WordDiff::Base';

  sub file_footer { return "End of diff\n"; }

The methods supplied by the base class are:

=over

=item C<new()>

Constructs and returns a new formatting object. It takes a single hash
reference as its argument, and uses it to construct the object. The nice thing
about this is that if you want to support other options in your formatting
class, you can just use them in the formatting object constructed by the
Text::WordDiff::Base class and document that they can be passed as
part of the options hash refernce to word_diff().

=item C<file_header()>

Called once for a single call to C<word_diff()>, this method outputs the
header for the whole diff. This is the only formatting method in the base
class that returns anything other than an empty string. It collects the
filenames from C<filname_a()> and C<filename_b()> and, if they're defined,
uses the relevant prefixes and modification times to return a unified
diff-style header.

=item C<hunk_header()>

This method is called for each diff hunk. It should output any necessary
header for the hunk.

=item C<same_items()>

This method is called for items that have not changed between the two
sequnces being compared. The unchanged items will be passed as a
list to the method.

=item C<delete_items>

This method is called for items in the first sequence that are not present in
the second sequcne. The deleted items will be passed as a list to the method.

=item C<insert_items>

This method is called for items in the second sequence that are not present in
the first sequcne. The inserted items will be passed as a list to the method.

=item C<hunk_footer>

This method is called at the end of a hunk. It should output any necessary
content to close out the hunk.

=item C<file_footer()>

This method is called once when the whole diff has been procssed. It should
output any necessary content to close out the diff file.

=item C<filename_a>

This accessor returns the value specified for the C<FILENAME_A> option
to word_diff().

=item C<filename_b>

This accessor returns the value specified for the C<FILENAME_B> option
to word_diff().

=item C<mtime_a>

This accessor returns the value specified for the C<MTIME_A> option to
word_diff().

=item C<mtime_b>

This accessor returns the value specified for the C<MTIME_B> option to
word_diff().

=item C<filename_prefix_a>

This accessor returns the value specified for the C<FILENAME_PREFIX_A> option
to word_diff().

=item C<filename_prefix_b>

This accessor returns the value specified for the C<FILENAME_PREFIX_B> option
to word_diff().

=back

=head1 See Also

=over

=item L<Text::Diff|Text::Diff>

Inspired the interface and implementation of this module. Thanks Barry!

=item L<Text::ParagraphDiff|Text::ParagraphDiff>

A module that attempts to diff paragraphs and the words in them.

=item L<Algorithm::Diff|Algorithm::Diff>

The module that makes this all possible.

=back

=head1 Support

This module is stored in an open L<GitHub
repository|http://github.com/theory/text-worddiff/>. Feel free to fork and
contribute!

Please file bug reports via L<GitHub
Issues|http://github.com/theory/text-worddiff/issues/> or by sending mail to
L<bug-Text-WordDiff@rt.cpan.org|mailto:bug-Text-WordDiff@rt.cpan.org>.

=head1 Author

David E. Wheeler <david@justatheory.com>

=head1 Copyright and License

Copyright (c) 2005-2011 David E. Wheeler. Some Rights Reserved.

This module is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut
