# $Id: ShellQuote.pm,v 1.6 1997-12-07 12:08:20-05 roderick Exp $
#
# Copyright (c) 1997 Roderick Schertler.  All rights reserved.  This
# program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

=head1 NAME

String::ShellQuote - quote strings for passing through the shell

=head1 SYNOPSIS

    $string = shell_quote @list;
    $string = shell_comment_quote $string;

=head1 DESCRIPTION

This module contains some functions which are useful for quoting strings
which are going to pass through the shell or a shell-like object.

=over

=cut

package String::ShellQuote;

use strict;
use vars qw($VERSION @ISA @EXPORT);

require Exporter;

$VERSION	= '1.00';
@ISA		= qw(Exporter);
@EXPORT		= qw(shell_quote shell_comment_quote);

=item B<shell_quote> [I<string>]...

B<shell_quote> quotes strings so they can be passed through the shell.
Each I<string> is quoted so that the shell will pass it along as a
single argument and without further interpretation.  If no I<string>s
are given an empty string is returned.

=cut

sub shell_quote {
    my @in = @_;
    return '' unless @in;
    my $ret = '';
    foreach (@in) {
	if (!defined $_ or $_ eq '') {
	    $_ = "''";
	} elsif (/[^\w\d.\-\/]/) {
	    s/\'/\'\\\'\'/g;
	    $_ = "'$_'";
	    s/^''//;
	    s/''$//;
	}
	$ret .= "$_ ";
    }
    chop $ret;
    return $ret;
}

=item B<shell_comment_quote> [I<string>]

B<shell_comment_quote> quotes the I<string> so that it can safely be
included in a shell-style comment (the current algorithm is that a sharp
character is placed after any newlines in the string).

This routine might be changed to accept multiple I<string> arguments
in the future.  I haven't done this yet because I'm not sure if the
I<string>s should be joined with blanks ($") or nothing ($,).  Cast
your vote today!  Be sure to justify your answer.

=cut

sub shell_comment_quote {
    return '' unless @_;
    unless (@_ == 1) {
    	require Carp;
	Carp::croak("Too many arguments to shell_comment_quote "
	    	    . "(got " . @_ . " expected 1)");
    }
    local $_ = shift;
    s/\n/\n#/g;
    return $_;
}

1;

__END__

=back

=head1 EXAMPLES

    $cmd = 'fuser 2>/dev/null ' . shell_quote @files;
    @pids = split ' ', `$cmd`;

    print CFG "# Configured by: ",
		shell_comment_quote($ENV{LOGNAME}), "\n";

=head1 AUTHOR

Roderick Schertler <F<roderick@argon.org>>

=head1 SEE ALSO

perl(1).

=cut
