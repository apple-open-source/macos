use strict;
use warnings;

use Test::More;

eval "use Test::Output";
if ($@) {
    plan skip_all => 'These tests require Test::Output.';
}

plan tests => 1;

{

    package DateTime::Locale::fake;

    use strict;
    use warnings;

    use DateTime::Locale;

    use base 'DateTime::Locale::root';

    sub cldr_version {'0.1'}

    sub _default_date_format_length {'medium'}

    sub _default_time_format_length {'medium'}

    DateTime::Locale->register(
        id          => 'fake',
        en_language => 'Fake',
    );
}

{
    stderr_like(
        sub { DateTime::Locale->load('fake') },
        qr/\Qfrom an older version (0.1)/,
        'loading timezone where olson version is older than current'
    );
}

