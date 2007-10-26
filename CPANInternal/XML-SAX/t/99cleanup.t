use Test;
BEGIN { plan tests => 1 }
use File::Spec;
ok(unlink(
    File::Spec->catdir(qw(blib lib XML SAX ParserDetails.ini))),
    1,
    'delete ParserDetails.ini'
);
