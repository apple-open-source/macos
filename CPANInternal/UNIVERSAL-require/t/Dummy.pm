package Dummy;

require Exporter;

@ISA         = qw(Exporter);
@EXPORT      = qw(foo);
@EXPORT_OK   = qw(bar);
$VERSION = 0.5;

sub foo { 42 }

sub bar { 23 }

sub car { "yarblockos" }

return 23;
