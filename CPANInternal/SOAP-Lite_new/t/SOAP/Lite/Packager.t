use strict;
use warnings;
use Test::More tests => 4; #qw(no_plan);

use_ok qw(SOAP::Lite::Packager);

ok SOAP::Lite::Packager->new(), 'SOAP::Lite::Packager->new()';
ok my $mime = SOAP::Lite::Packager::MIME->new(), 'SOAP::Lite::Packager::MIME->new()';

my $entity = bless {}, 'MIME::Entity';
ok $mime->is_supported_part($entity);


