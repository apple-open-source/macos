use Sub::Install;
use Test::More tests => 17;

use strict;
use warnings;

# These tests largely copied from Damian Conway's Sub::Installer tests.

{ # Install a sub in a package...
  my $sub_ref = Sub::Install::install_sub({ code => \&ok, as => 'ok1' });

  isa_ok($sub_ref, 'CODE', 'return value of first install_sub');

  is_deeply($sub_ref, \&ok, 'it returns the correct code ref');

  ok1(1, 'installed sub runs');
}

{
  my $to_avail = eval "use Test::Output; 1";
  SKIP: {
    skip "can't run this test without Test::Output", 1 unless $to_avail;
    Sub::Install::install_sub({ code => \&ok, as => 'tmp_ok' });
    
    my $expected_warning = <<'END_WARNING';
Subroutine main::tmp_ok redefined at t/install.t line 31
Prototype mismatch: sub main::tmp_ok ($;$) vs ($$;$) at t/install.t line 31
END_WARNING

    my $stderr = Test::Output::stderr_from(
      sub { Sub::Install::install_sub({ code => \&is, as => 'tmp_ok' }) }
    );

    $stderr =~ s!\\!/!g;
    is(
      $stderr,
      $expected_warning,
      "got expected warning",
    );
  }
}

{ # Install the same sub in the same package...
  my $redef = 0;
  my $proto = 0;

  local $SIG{__WARN__} = sub {
    return ($redef = 1) if $_[0] =~ m{Subroutine \S+ redef.+t.install\.t};
    return ($proto = 1) if $_[0] =~ m{Prototype mismatch.+t.install\.t};
    # pass("warned as expected: $_[0]") if $_[0] =~ /redefined/;
    die "unexpected warning: @_";
  };

  my $sub_ref = Sub::Install::install_sub({ code => \&is, as => 'ok1' });

  ok($redef, 'correct redefinition warning went to $SIG{__WARN__}');
  ok($proto, 'correct prototype warning went to $SIG{__WARN__}');

  isa_ok($sub_ref, 'CODE', 'return value of second install_sub');

  is_deeply($sub_ref, \&is, 'install2 returns correct code ref');

  ok1(1,1, 'installed sub runs (with new arguments)');
}

{ # Install in another package...
  my $sub_ref = Sub::Install::install_sub({
    code => \&ok,
    into => 'Other',
    as   => 'ok1'
  });

  isa_ok($sub_ref, 'CODE', 'return value of third install_sub');

  is_deeply($sub_ref, \&ok, 'it returns the correct code ref');

  ok1(1,1, 'sub previously installed into main still runs properly');

  package Other;
  ok1(1,   'remotely installed sub runs properly');
}

{ # cross-package installation
  sub Other::Another::foo { return $_[0] }

  my $sub_ref = Sub::Install::install_sub({
    code => 'foo',
    from => 'Other::Another',
    into => 'Other::YetAnother',
    as   => 'return_lhs'
  });

  isa_ok($sub_ref, 'CODE', 'return value of fourth install_sub');

  is_deeply(
    $sub_ref,
    \&Other::Another::foo,
    'it returns the correct code ref'
  );

  is(
    Other::Another->foo,
    'Other::Another',
    'the original code does what we want',
  );

  is(
    Other::YetAnother->return_lhs,
    'Other::YetAnother',
    'and the installed code works, too',
  );
}
