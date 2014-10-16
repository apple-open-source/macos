package Template::Plugin::ProcBar;
use Template::Plugin::ProcFoo;
@ISA = qw(Template::Plugin::ProcFoo);

sub bar { "This is procbarbar" }
sub baz { "This is procbarbaz" }

1;
