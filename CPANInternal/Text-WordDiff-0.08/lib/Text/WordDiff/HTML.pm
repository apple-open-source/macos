package Text::WordDiff::HTML;

use strict;
use HTML::Entities qw(encode_entities);
use vars qw($VERSION @ISA);

$VERSION = '0.08';
@ISA = qw(Text::WordDiff::Base);

sub file_header {
    my $header = shift->SUPER::file_header(@_);
    return '<div class="file">' unless $header;
    return qq{<div class="file"><span class="fileheader">$header</span>};
}

sub hunk_header { return '<span class="hunk">' }
sub hunk_footer { return '</span>' }
sub file_footer { return '</div>' }

sub same_items {
    shift;
    return encode_entities( join '', @_ );
}

sub delete_items {
    shift;
    return '<del>' . encode_entities( join'', @_ ) . '</del>';
}

sub insert_items {
    shift;
    return '<ins>' . encode_entities( join'', @_ ) . '</ins>';
}

1;
__END__


=begin comment

Fake-out Module::Build. Delete if it ever changes to support =head1 headers
other than all uppercase.

=head1 NAME

Text::WordDiff::HTML - XHTML formatting for Text::WordDiff

=end comment

=head1 Name

Text::WordDiff::HTML - XHTML formatting for Text::WordDiff

=head1 Synopsis

    use Text::WordDiff;

    my $diff = word_diff 'file1.txt', 'file2.txt'; { STYLE => 'HTML' };
    my $diff = word_diff \$string1,   \$string2,    { STYLE => 'HTML' };
    my $diff = word_diff \*FH1,       \*FH2,        { STYLE => 'HTML' };
    my $diff = word_diff \&reader1,   \&reader2,    { STYLE => 'HTML' };
    my $diff = word_diff \@records1,  \@records2,   { STYLE => 'HTML' };

    # May also mix input types:
    my $diff = word_diff \@records1,  'file_B.txt', { STYLE => 'HTML' };

=head1 Description

This class subclasses Text::WordDiff::Base to provide a XHTML formatting for
Text::WordDiff. See L<Term::WordDiff|Term::WordDiff> for usage details. This
class should never be used directly.

Text::WordDiff::HTML formats word diffs for viewing in a Web browser. The diff
content is highlighted as follows:

=over

=item * C<< <div class="file"> >>

This element contains the entire contents of the diff "file" returned by
C<word_diff()>. All of the following elements are subsumed by this one.

=over

=item * C<< <span class="fileheader"> >>

The header section for the files being C<diff>ed, usually something like:

  --- in.txt	Thu Sep  1 12:51:03 2005
  +++ out.txt	Thu Sep  1 12:52:12 2005

This element immediately follows the opening "file" C<< <div> >> element, but
will not be present if Text::WordDif cannot deterimine the file names for both
files being compared.

=item * C<< <span class="hunk"> >>

This element contains a single diff "hunk". Each hunk may contain the
following elements:

=over

=item * C<< <ins> >>

Inserted content.

=item * C<< <del> >>

Deleted content.

=back

=back

=back

You may do whatever you like with these elements and classes; I highly
recommend that you style them using CSS. You'll find an example CSS file in
the F<eg> directory in the Text-WordDiff distribution.

=head1 See Also

=over

=item L<Text::WordDiff|Text::WordDiff>

=item L<Text::WordDiff::ANSIColor|Text::WordDiff::ANSIColor>

=back

=head1 Support

This module is stored in an open repository at the following address:

L<https://svn.kineticode.com/Text-WordDiff/trunk/>

Patches against Text::WordDiff are welcome. Please send bug reports to
<bug-text-worddiff@rt.cpan.org>.

=head1 Author

=begin comment

Fake-out Module::Build. Delete if it ever changes to support =head1 headers
other than all uppercase.

=head1 AUTHOR

=end comment

David Wheeler <david@kineticode.com>

=head1 Copyright and License

Copyright (c) 2005-2011 David E. Wheeler. Some Rights Reserved.

This module is free software; you can redistribute it and/or modify it under the
same terms as Perl itself.

=cut
