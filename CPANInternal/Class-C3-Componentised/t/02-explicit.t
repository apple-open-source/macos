use strict;
use warnings;

use FindBin;
use Test::More;
use Test::Exception;

use lib "$FindBin::Bin/lib";

plan tests => 7;

use_ok('MyModuleNoBase');
is(MyModuleNoBase->new->message, " MyModuleNoBase", "initial message matches");
lives_ok (
  sub { MyModuleNoBase->load_components('+MyModule::Plugin::Foo') },
  'explicit load_components does not throw',
);
is(MyModuleNoBase->new->message, "Foo MyModuleNoBase", "component works");

throws_ok (
  sub { MyModuleNoBase->load_components('ClassC3ComponentFooThatShouldntExist') },
  qr/Can't locate object method "component_base_class"/,
  'non-explicit component specification fails without component_base_class()',
);

throws_ok (
  sub { MyModuleNoBase->load_optional_components('ClassC3ComponentFooThatShouldntExist') },
  qr/Can't locate object method "component_base_class"/,
  'non-explicit component specification fails without component_base_class()',
);

lives_ok (
  sub { MyModuleNoBase->load_optional_components('+ClassC3ComponentFooThatShouldntExist') },
  'explicit optional component specification does not throw',
);
