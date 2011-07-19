use strict;
use Test::More;

BEGIN
{
    if ($] < 5.009_005) {
        plan(skip_all => "This test is only applicable for perl >= 5.9.5");
    } elsif ( ! eval { require MRO::Compat } || $@) {
        plan(skip_all => "MRO::Compat not available");
    } elsif ( ! eval { require Class::C3 } || $@) {
        plan(skip_all => "Class::C3 not available");
    } else {
        plan(tests => 2);
    }
}

{
    # If the bug still exists, I should get a few warnings
    my @warnings;
    local $SIG{__WARN__} = sub {
        push @warnings, $_[0];
    };

    # Remove symbols from respective tables, and
    # remove from INC, so we force re-evaluation
    foreach my $class qw(Class::C3 MRO::Compat) {
        my $file = $class;
        $file =~ s/::/\//g;
        $file .= '.pm';

        delete $INC{$file};

        { # Don't do this at home, kids!
            no strict 'refs';
            foreach my $key (keys %{ "${class}::" }) {
                delete ${"${class}::"}{$key};
            }
        }
    }

    eval {
        require MRO::Compat;
        require Class::C3;
    };
    ok( ! $@, "Class::C3 loaded ok");
    if (! ok( ! @warnings, "loading Class::C3 did not generate warnings" )) {
        diag("Generated warnings are (expecting 'subroutine redefined...')");
        diag("   $_") for @warnings;
    }
}
