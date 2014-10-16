package Template::Plugin::ProcFoo;
use Template::Plugin::Procedural;
@ISA = qw(Template::Plugin::Procedural);

sub foo { "This is procfoofoo" }
sub bar { "This is procfoobar" }

1;
