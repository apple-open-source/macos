package Text::WordDiff::ANSIColor;

use strict;
use Term::ANSIColor qw(:constants);
use vars qw($VERSION @ISA);

# Term::ANSIColor doesn't support STRIKETHROUGH, so we'll do it ourselves.
use constant STRIKETHROUGH => "\e[9m";

$VERSION = '0.08';
@ISA = qw(Text::WordDiff::Base);

sub same_items {
    shift;
    return join '', @_;
}

sub delete_items {
    shift;
    return join '', BOLD, RED, STRIKETHROUGH, @_, RESET;
}

sub insert_items {
    shift;
    return join '', BOLD, GREEN, UNDERLINE, @_, RESET;
}

1;
__END__

=begin comment

Fake-out Module::Build. Delete if it ever changes to support =head1 headers
other than all uppercase.

=head1 NAME

Text::WordDiff::ANSIColor - ANSI colored formatting for Text::WordDiff

=end comment

=head1 Name

Text::WordDiff::ANSIColor - ANSI colored formatting for Text::WordDiff

=head1 Synopsis

    use Text::WordDiff;

    my $diff = word_diff 'file1.txt', 'file2.txt';
    my $diff = word_diff \$string1,   \$string2,   { STYLE => 'ANSIColor' };
    my $diff = word_diff \*FH1,       \*FH2;       \%options;
    my $diff = word_diff \&reader1,   \&reader2;
    my $diff = word_diff \@records1,  \@records2;

    # May also mix input types:
    my $diff = word_diff \@records1,  'file_B.txt';

=head1 Description

This class subclasses Text::WordDiff::Base to provide a formatting class for
Text::WordDiff that uses ANSI-standard terminal escape sequences to highlight
deleted and inserted text. This formatting class is the default class used by
L<Text::WordDiff|Text::WordDiff>; see its documentation for details on its
interface. This class should never be used directly.

Text::WordDiff::ANSIColor formats word diffs for viewing in an ANSI-standard
terminal session. The diff content is highlighted as follows:

=over

=item Deletes

Deleted words will display in bold-faced red. The ANSI standard for
strikethrough is also used, but since it is not supported by most terminals,
likely will not show up.

=item Inserts

Inserted words will display in bold-faced, underlined green.

=back

All other content is simply returned.

=head1 See Also

=over

=item L<Text::WordDiff|Text::WordDiff>

=item L<Text::WordDiff::HTML|Text::WordDiff::HTML>

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
