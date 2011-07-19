use strict;
use warnings;

package B::Hooks::EndOfScope;

use 5.008000;
use Variable::Magic;

our $VERSION = '0.08';

use Sub::Exporter -setup => {
    exports => ['on_scope_end'],
    groups  => { default => ['on_scope_end'] },
};

=head1 NAME

B::Hooks::EndOfScope - Execute code after a scope finished compilation

=head1 SYNOPSIS

    on_scope_end { ... };

=head1 DESCRIPTION

This module allows you to execute code when perl finished compiling the
surrounding scope.

=head1 FUNCTIONS

=head2 on_scope_end

    on_scope_end { ... };

    on_scope_end $code;

Registers C<$code> to be executed after the surrounding scope has been
compiled.

This is exported by default. See L<Sub::Exporter> on how to customize it.

=cut

{
    my $wiz = Variable::Magic::wizard
        data => sub { [$_[1]] },
        free => sub { $_->() for @{ $_[1] }; () };

    sub on_scope_end (&) {
        my $cb = shift;

        $^H |= 0x020000;

        if (my $stack = Variable::Magic::getdata %^H, $wiz) {
            push @{ $stack }, $cb;
        }
        else {
            Variable::Magic::cast %^H, $wiz, $cb;
        }
    }
}

=head1 SEE ALSO

L<Sub::Exporter>

L<Variable::Magic>

=head1 AUTHOR

Florian Ragwitz E<lt>rafl@debian.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (c) 2008  Florian Ragwitz

This module is free software.

You may distribute this code under the same terms as Perl itself.

=cut

1;
