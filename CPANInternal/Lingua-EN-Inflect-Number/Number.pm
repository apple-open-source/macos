package Lingua::EN::Inflect::Number;
use 5.006;
use strict;
use warnings;

require Exporter;
our @ISA = qw(Exporter);
our $VERSION = '1.1';
our @EXPORT_OK = qw(to_PL to_S number);
use Lingua::EN::Inflect qw(PL PL_N_eq);

sub import {
    my ($self, @syms) = @_;
    # Grep out the ones we provide:
    my $provide = join "|", map quotemeta, @EXPORT_OK;
    my @new_syms;
    for my $sym (@syms) {
        if ($sym =~ /^\&?($provide)$/) {
            $self->export_to_level(1, $self, $sym);
        } else {
            push @new_syms, $sym;
        }
    }
    return unless @new_syms;

    # Pretend we don't exist
    @_ = ("Lingua::EN::Inflect", @new_syms);
    goto &Exporter::import;
}

sub to_PL {
    my $word = shift;
    my $num = number($word);
    return $word if $num eq "ambig" or $num eq "p";
    return PL($word);
}

sub to_S {
    my $word = shift;
    my $num = number($word);
    return $word if $num eq "ambig" or $num eq "s";
    return PL($word); # I don't know why this works, but it seems to.
}

sub number {
    my $word = shift;
    my $test = PL_N_eq($word, PL($word));
    $test =~ s/:.*//;
    $test = "ambig" if $test eq "eq";
    return $test;
}

1;
__END__
# Below is stub documentation for your module. You better edit it!

=head1 NAME

Lingua::EN::Inflect::Number - Force number of words to singular or plural

=head1 SYNOPSIS

  use Lingua::EN::Inflect::Number qw(
    number to_S to_PL # Or anything you want from Lingua::EN::Inflect
  );

  print number("goat");  # "s" - there's only one goat
  print number("goats"); # "p" - there's several goats
  print number("sheep"); # "ambig" - there could be one or many sheep

  print to_S("goats");   # "goat"
  print to_PL("goats");  # "goats" - it already is
  print to_S("goat");    # "goat" - it already is
  print to_S("sheep");   # "sheep"

=head1 DESCRIPTION

This module extends the functionality of Lingua::EN::Inflect with three
new functions available for export:

=head2 number

This takes a word, and determines its number. It returns C<s> for singular,
C<p> for plural, and C<ambig> for words that can be either singular or plural.

Based on that:

=head2 to_S / to_PL

These take a word and convert it forcefully either to singular or to
plural. C<Lingua::EN::Inflect> does funny things if you try to pluralise
an already-plural word, but this module does the right thing.

=head1 DISCLAIMER

The whole concept is based on several undocumented features and
idiosyncracies in the way Lingua::EN::Inflect works. Because of this,
the module only works reliably on nouns. It's also possible that these
idiosyncracies will be fixed at some point in the future and this module
will need to be rethought. But it works at the moment. Additionally,
any disclaimers on Lingua::EN::Inflect apply double here.

=head1 AUTHOR

Simon Cozens, C<simon@cpan.org>

=head1 SEE ALSO

L<Lingua::EN::Inflect>.

=cut
