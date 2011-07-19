use Sub::Install qw(reinstall_sub);
use Test::More tests => 15;

use strict;
use warnings;

# These tests largely copied from Damian Conway's Sub::Installer tests.

{ # Install a sub in a package...

  my $sub_ref = reinstall_sub({ code => \&ok, as => 'ok1' });

  isa_ok($sub_ref, 'CODE', 'return value of first install_sub');

  is_deeply($sub_ref, \&Test::More::ok, 'it returned the right coderef');

  $sub_ref->(1, 'returned code ref runs');
  ok1(1, "reinstalled sub runs");
}

{
  my $to_avail = eval "use Test::Output; 1";
  SKIP: {
    skip "can't run this test without Test::Output", 1 unless $to_avail;
    Sub::Install::reinstall_sub({ code => \&ok, as => 'tmp_ok' });
    
    my $expected_warning = <<'END_WARNING';
Prototype mismatch: sub main::tmp_ok ($;$) vs ($$;$) at t/reinstall.t line 32
END_WARNING

    my $stderr = Test::Output::stderr_from(
      sub { Sub::Install::reinstall_sub({ code => \&is, as => 'tmp_ok' }) }
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
  my $proto = 0;

  local $SIG{__WARN__} = sub {
    return ($proto = 1) if $_[0] =~ m{Prototype mismatch.+t.reinstall\.t};
    die "unexpected warning: @_";
  };

  my $sub_ref = reinstall_sub({ code => \&is, as => 'ok1' });

  ok($proto, 'correct warning went to $SIG{__WARN__}');

  isa_ok($sub_ref, 'CODE', 'return value of second install_sub');

  is_deeply($sub_ref, \&Test::More::is, 'it returned the right coderef');

  $sub_ref->(1, 1, 'returned code ref runs');
  ok1(1,1, 'reinstalled sub reruns');
}

{ # Install in another package...
  my $new_code = sub { ok(1, "remotely installed sub runs") };

  my $sub_ref = reinstall_sub({
    code => $new_code,
    into => 'Other',
    as   => 'ok1',
  });

  isa_ok($sub_ref, 'CODE', 'return value of third install_sub');

  is_deeply($sub_ref, $new_code, 'it returned the right coderef');

  ok1(1,1, 'reinstalled sub reruns');

  package Other;
  ok1();
}

eval {
  my $arg = { code => sub {}, into => 'Other', as => 'ok1' };
  Sub::Install::_build_public_installer(\&Sub::Install::_install_fatal)->($arg);
};

like($@, qr/redefine/, "(experimental fatal installer should croak)");
