package Mail::Sender;
use strict;
use Win32API::Registry qw(RegOpenKeyEx RegQueryValueEx HKEY_CLASSES_ROOT);

sub GuessCType {
	my $ext = shift;
	$ext =~ s/^.*\././;
	my ($key, $type, $data);
	RegOpenKeyEx( HKEY_CLASSES_ROOT, $ext, 0, KEY_READ, $key )
		or return 'application/octet-stream';
	RegQueryValueEx( $key, "Content Type", [], $type, $data, [] )
		or return 'application/octet-stream';
	return $data || 'application/octet-stream';
}

1;
