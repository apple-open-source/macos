package Importer;
use warnings;
use strict;
use Sub::Uplevel qw/:aggressive/;
sub import_for_me {
    my ($pkg, @p) = @_;
    my $level = 1;
    my $import = $pkg->can('import');
    if ($import) {
        uplevel $level, $import, ($pkg, @p);
    } else {
        warn "no import in $pkg\n";
    }
}
1;
