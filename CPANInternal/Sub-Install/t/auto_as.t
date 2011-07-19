use Sub::Install qw(install_sub);
use Test::More 'no_plan';

use strict;
use warnings;

sub source_method {
  my ($package) = @_;
  return $package;
}

{ # install named method and let the name be the same
  install_sub({ code => "source_method", into => "By::Name" });

  is(
    By::Name->source_method,
    'By::Name',
    "method installed by name"
  );
}

{ # install via a coderef and let name be looked up
  install_sub({ code => \&source_method, into => "By::Ref" });

  is(
    By::Ref->source_method,
    'By::Ref',
    "method installed by ref, without name"
  );
}
