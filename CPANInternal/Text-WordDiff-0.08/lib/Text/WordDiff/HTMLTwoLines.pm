package Text::WordDiff::HTMLTwoLines;

use strict;
use HTML::Entities qw(encode_entities);
use vars qw($VERSION @ISA);

$VERSION = '0.08';
@ISA = qw(Text::WordDiff::Base);

sub file_header {
	my $self = shift;
	my $fn1 = $self->filename_a;
	my $fn2 = $self->filename_b;

	if (defined $fn1 && defined $fn2)
	{	my $p1 = $self->filename_prefix_a;
		my $t1 = $self->mtime_a;
		my $p2 = $self->filename_prefix_b;
		my $t2 = $self->mtime_b;

		$self->{__str1} = '<div class="file"><span class="fileheader">'
		. "$p1 $fn1" . (defined $t1 ? " " . localtime $t1 : '') . '</span>';

		$self->{__str2} = '<div class="file"><span class="fileheader">'
		. "$p2 $fn2" . (defined $t2 ? " " . localtime $t2 : '') . '</span>';
	}
	else
	{	$self->{__str1} = $self->{__str2} = '<div class="file">';
	}
	return '';
}

sub hunk_header {
	my $self = shift;
	$self->{__str1} .= '<span class="hunk">';
	$self->{__str2} .= '<span class="hunk">';
	return '';
}
sub hunk_footer {
	my $self = shift;
	$self->{__str1} .= '</span>';
	$self->{__str2} .= '</span>';
	return '';
}

sub file_footer {
	my $self = shift;
	return $self->{__str1} . "</div>\n" . $self->{__str2} . "</div>\n";
}

sub same_items {
	my $self = shift;
	$self->{__str1} .= encode_entities( join '', @_ );
	$self->{__str2} .= encode_entities( join '', @_ );
	return '';
}

sub delete_items {
	my $self = shift;
	$self->{__str1} .= '<del>' . encode_entities( join '', @_ ) . '</del>';
	return '';
}

sub insert_items {
	my $self = shift;
	$self->{__str2} .= '<ins>' . encode_entities( join '', @_ ) . '</ins>';
	return '';
}

1;

__END__

=head1 Name

Text::WordDiff::HTMLTwoLines - XHTML formatting for Text::WordDiff with content on two lines

=head1 Synopsis

    use Text::WordDiff;

    my $diff = word_diff 'file1.txt', 'file2.txt';  { STYLE => 'HTMLTwoLines' };
    my $diff = word_diff \$string1,   \$string2,    { STYLE => 'HTMLTwoLines' };
    my $diff = word_diff \*FH1,       \*FH2,        { STYLE => 'HTMLTwoLines' };
    my $diff = word_diff \&reader1,   \&reader2,    { STYLE => 'HTMLTwoLines' };
    my $diff = word_diff \@records1,  \@records2,   { STYLE => 'HTMLTwoLines' };

    # May also mix input types:
    my $diff = word_diff \@records1,  'file_B.txt', { STYLE => 'HTMLTwoLines' };

=head1 Description

This class subclasses Text::WordDiff::Base to provide a XHTML formatting for
Text::WordDiff. See L<Term::WordDiff|Term::WordDiff> for usage details. This
class should never be used directly.

Text::WordDiff::HTMLTwoLines formats word diffs for viewing in a Web browser.
The output is similar to that produced by
L<Term::WordDiff::HTML|Term::WordDiff::HTML> but the two lines (or files,
records, etc.) are shown separately, with deleted items highlighted in the
first line and inserted items highlighted in the second. HTMLTwoLines puts a
span tag around each word or set of words in the diff.

The diff content is highlighted as follows:

=over

=item * C<< <div class="file"> >>

The inputs to C<word_diff()> are each contained in a div element of class
"file". All the following results are subsumed by these elements.

=over

=item * C<< <span class="fileheader"> >>

The header section for the files being C<diff>ed, usually something like:

  --- in.txt	Thu Sep  1 12:51:03 2005

for the first file, and

  +++ out.txt	Thu Sep  1 12:52:12 2005

for the second.

This element immediately follows the opening "file" C<< <div> >> element, but
will not be present if Text::WordDiff cannot determine the file names for both
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

=item L<Text::WordDiff::ANSIColor|Text::WordDiff::HTML>

=item L<Text::WordDiff::ANSIColor|Text::WordDiff::ANSIColor>

=back

=head1 Author

Amelia Ireland <join(".", $firstname, $lastname) . "@gmail.com">

=head1 Copyright and License

Copyright (c) 2011 Amelia Ireland. Some Rights Reserved.

This module is free software; you can redistribute it and/or modify it under the
same terms as Perl itself.

=cut
