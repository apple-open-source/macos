use Sub::Install qw(install_sub);
use Test::More 'no_plan';

use strict;
use warnings;

{ # you have to install /something/!
  eval { install_sub({ into => "Doesn't::Matter" }); };

  like($@, qr/code.+not optional/, "you must supply something to install");
}

{ # you can't just make names up and expect Sub::Install to know what you mean
  eval { install_sub({ code => 'none_such', into => 'Whatever' }); };

  like($@, qr/couldn't find subroutine/, "error on unfound sub name");
}

{ # can't install anonymous subs without a name
  eval { install_sub({ code => sub { return 1; } }); };

  like($@, qr/couldn't determine name/, "anon subs need names to install");
}
