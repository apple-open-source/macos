use ExtUtils::MakeMaker;
use Config qw(%Config);

#### Build information
my $statvfs_header = "sys/statvfs.h";
my $statfs_header = "sys/statfs.h";
my $statvfs_symbol = "d_statvfs";
my $statvfs_header_def = "i_sysstatvfs";
my $statfs_symbol = "d_statfs_s";  #### Really the struct def, we equate it to statfs being defined
my $statfs_header_def = "i_sysstatfs";
my $define = "";
my $statfs_use_mount = 0;
my $xs_file = 'Df.xs';


print "OS = $Config{osname}\n";

#### Windows
if($Config{osname} =~ /^MSWin/i) {
	print "This module does not support Windows.\n";
	die "You might try Filesys::DfPortable instead.\n";
}

#### Check for the existance of statvfs
if(check_statvfs()) {
	####$define .= "-DDF_STATVFS ";
	copy_xs("XS_statvfs", $xs_file);
	print "Building with statvfs ....\n";
}

#### Check for the existance of statfs
elsif(check_statfs()) {
	#### use_mount needed for headers
	($statfs_use_mount) &&
		($define .= "-DDF_STATFS_USE_MOUNT ") ||
		($define .= "-DDF_STATFS ");

	#### Needed for 4 arg statfs
	($Config{osname} =~ /^solaris$/i) &&
        ($define .= "-DDF_SOLARIS ");
	
	copy_xs("XS_statfs", $xs_file);
	print "Building with statfs ....\n";
}

#### OS/2, old Mac, etc
else {
	print "We could not find statvfs, or statfs.\n";
	die "You need at least one of these to build this module.\n";
}



sub check_statvfs {
	print "Checking for statvfs .....\n";
	if(exists $Config{$statvfs_symbol} && defined $Config{$statvfs_symbol}) { 
		print "$statvfs_symbol is defined.\n";
		if(exists $Config{$statvfs_header_def} && defined $Config{$statvfs_header_def}) {
			print "$statvfs_header_def is defined.\n";
			return(1);
		}

		else {
			print "Weird, $statvfs_header_def is not defined.\n";
			#### Have never seen a system with statvfs and no sys/statvfs.h header
			#### Lets see if we can find one
			if(look_for_header($statvfs_header)) {
				return(1);
			}

			else {
				#### no idea what header would be
				print "Cannot find a $statvfs_header file\n";
				print "We will not try to build with statvfs\n";
				return(0);
			}
		}
	}

	else {
		print "$statvfs_symbol is not defined\n";

		### OK if we find a header should we build with it?
		if(look_for_header($statvfs_header)) {
			return(1);
		}

		else {
			#### don't use statvfs
			print "Cannot find a $statvfs_header file\n";
			print "We will not try to build with statvfs\n";
			return(0);
		}
	}

	return(0);
}


sub check_statfs {
	print "Checking for statfs .....\n";
	if(exists $Config{$statfs_symbol} && defined $Config{$statfs_symbol}) {
		print "Good, $statfs_symbol is defined.\n";
		if(exists $Config{$statfs_header_def} && defined $Config{$statfs_header_def}) {
			print "$statfs_header_def is defined.\n";
			return(1);
		}

		else {
			print "$statfs_header_def not defined.\n";
			#### check for BSD and Darwin 
			if($Config{osname} =~ /^darwin|^bsd|bsd$/i) {
				print "You are running Darwin or BSD.\n";
				print "Will assume you need the mount.h and param.h headers.\n";
				$statfs_use_mount = 1;
				return(1);
			}

			elsif(look_for_header($statfs_header)) {
				return(1);
			}

			else {
				#### don't use statfs
				print "Cannot find a $statfs_header file\n";
				print "We will not try to build Statfs\n";
			}
		}
	}

	else {
		print "$statfs_symbol is not defined\n";

		if(look_for_header($statfs_header)) {
			return(1);
		}
	
		else {
			print "Cannot find a $statfs_header file\n";
			print "We will not try to build Statfs\n";
			return(0);
		}
	}

	return(0);
}


sub copy_xs {
my $source = shift;
my $dest = shift;

	open(SOURCE, "$source") or die "$! $source\n";
	open(DEST, ">$dest") or die "$! $dest\n";
	@contents = <SOURCE>;
	print DEST @contents;
	close(DEST);
	close(SOURCE);
}


sub look_for_header {
my $header = shift;

  	#my @header_inc = split(/\s+/, join(" ", $Config{usrinc}, $Config{locincpth}));
  	my @header_inc = split(/\s+/, join(" ", $Config{usrinc}));
	foreach $header_path (@header_inc) {
		if(-f $header_path . '/' . $header) {
			print "Header found:" , $header_path . '/' . $header, "\n";
			return(1);
		}
	}

	return(0);
}


WriteMakefile(
    'NAME'	=> 'Filesys::Df',
    'VERSION_FROM' => 'Df.pm', # finds $VERSION
    'LIBS'	=> [''],   # e.g., '-lm' 
    'DEFINE'	=> $define,     # e.g., '-DHAVE_SOMETHING' 
    'INC'	=> '',     # e.g., '-I/usr/include/other' 
    'clean'     => {FILES => 'Df.xs'},
    'XSPROTOARG' => '-prototypes' 
);
