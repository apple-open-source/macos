use strict;
use warnings;

use inc::Module::Install;

name          ('Data-OptList');
author        ('Ricardo SIGNES <rjbs@cpan.org>');
license       ('perl');
version_from  ('lib/Data/OptList.pm');

requires('Sub::Install' => 0.921); # exporter, needed by Data::OptList
requires('List::Util'   => 0.00); # unknown minimum; used for "first"
requires('Params::Util' => 0.14); # _HASHLIKE, _ARRAYLIKE, _CODELIKE

repository('http://github.com/rjbs/data-optlist');
auto_manifest;

WriteAll();
