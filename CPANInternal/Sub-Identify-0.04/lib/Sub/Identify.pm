package Sub::Identify;

use strict;
use Exporter;

BEGIN {
    our $VERSION = '0.04';
    our @ISA = ('Exporter');
    our %EXPORT_TAGS = (all => [ our @EXPORT_OK = qw(sub_name stash_name sub_fullname get_code_info) ]);

    my $loaded = 0;
    unless ($ENV{PERL_SUB_IDENTIFY_PP}) {
        local $@;
        eval {
            if ($] >= 5.006) {
                require XSLoader;
                XSLoader::load(__PACKAGE__, $VERSION);
            }
            else {
                require DynaLoader;
                push @ISA, 'DynaLoader';
                __PACKAGE__->bootstrap($VERSION);
            }
        };

        die $@ if $@ && $@ !~ /object version|loadable object/;

        $loaded = 1 unless $@;
    }

    our $IsPurePerl = !$loaded;

    if ($IsPurePerl) {
        require B;
        *get_code_info = sub ($) {
            my ($coderef) = @_;
            ref $coderef or return;
            my $cv = B::svref_2object($coderef);
            $cv->isa('B::CV') or return;
            # bail out if GV is undefined
            $cv->GV->isa('B::SPECIAL') and return;

            return ($cv->GV->STASH->NAME, $cv->GV->NAME);
        };
    }
}

sub stash_name   ($) { (get_code_info($_[0]))[0] }
sub sub_name     ($) { (get_code_info($_[0]))[1] }
sub sub_fullname ($) { join '::', get_code_info($_[0]) }

1;

__END__

=head1 NAME

Sub::Identify - Retrieve names of code references

=head1 SYNOPSIS

    use Sub::Identify ':all';
    my $subname = sub_name( $some_coderef );
    my $p = stash_name( $some_coderef );
    my $fully_qualified_name = sub_fullname( $some_coderef );
    defined $subname
	and print "this coderef points to sub $subname in package $p\n";

=head1 DESCRIPTION

C<Sub::Identify> allows you to retrieve the real name of code references. For
this, it uses perl's introspection mechanism, provided by the C<B> module.

It provides four functions : C<sub_name> returns the name of the
subroutine (or C<__ANON__> if it's an anonymous code reference),
C<stash_name> returns its package, and C<sub_fullname> returns the
concatenation of the two.

The fourth function, C<get_code_info>, returns a list of two elements,
the package and the subroutine name (in case of you want both and are worried
by the speed.)

In case of subroutine aliasing, those functions always return the
original name.

=head1 LICENSE

(c) Rafael Garcia-Suarez (rgarciasuarez at gmail dot com) 2005, 2008

This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself.

=cut
