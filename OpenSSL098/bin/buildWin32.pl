#!/usr/bin/perl
use strict;
use File::Basename;
use File::Copy;
use Cwd;

sub usage {
    my ($str) = (@_);
    if ($str) {
        print "$str\n";
    }
    print "$0 <x86> <x64> <debug> <release> <dll> <static> <legacy> <test>\n";
    print "Any combination of the above can be specified at once, i.e.\n";
    print "    $0 x86 x64 debug release static\n";
    exit 1;
}

sub runcmd($) {
    my ($cmd) = @_;
    my $dir = getcwd();
    print "[${dir}] $cmd\n";
    system($cmd);
}

if (! -d $ENV{"VSINSTALLDIR"}) {
    usage("VSINSTALLDIR not set\n");
}

if (!$ENV{"OBJROOT"}) {
    $ENV{"OBJROOT"} = "$ENV{TEMP}\\obj";
}

my (@archs, @cfgs, @types, $legacy, $test);

foreach my $arg (@ARGV) {
    if (($arg eq "x86") || ($arg eq "x64")) {
        push @archs, $arg;
    } elsif (($arg eq "debug") || ($arg eq "release")) {
        push @cfgs, $arg;
    } elsif (($arg eq "dll") || ($arg eq "static")) {
        push @types, $arg;
    } elsif ($arg eq "legacy") {
        $legacy = 1;
    } elsif ($arg eq "test") {
        $test = 1;
    } else {
        usage("Unknown argument $arg\n");
    }
}

my %pids;
my %wanted;
my @incs;

mkdir("$ENV{OBJROOT}");

foreach my $arch (@archs) {
    my ($cfgarg, $batch, $bits, $sehflags);
    
    if ($arch eq "x86") {
        ($cfgarg, $batch, $bits, $sehflags) = ("VC-WIN32",  "ms\\do_masm.bat",   32, "/SAFESEH");
    } elsif ($arch eq "x64") {
        ($cfgarg, $batch, $bits, $sehflags) = ("VC-WIN64A", "ms\\do_win64a.bat", 64, "");
    } else {
        usage("$arch is not a valid architecture\n");
    }
    my $sehflagslc = $sehflags;
    $sehflagslc =~ tr|A-Z|a-z|;
    
    foreach my $cfg (@cfgs) {
        
        my $cfgsfx;

        if ($cfg eq "debug") {
            $cfgsfx = "_debug";
        } elsif ($cfg eq "release") {
            $cfgsfx = "";
        } else {
            usage("$cfg is not a valid configuration\n");
        }

        foreach my $type (@types) {
            
            my ($libsfx, $makefile);

            if ($type eq "dll") {
                $libsfx = $cfgsfx;
                $makefile = "ms\\ntdll.mak";
            } elsif ($type eq "static") {
                $libsfx = "_static$cfgsfx";
                $makefile = "ms\\nt.mak";
            } else {
                usage("$type is not a valid library type\n");
            }

            my $buildType = "${arch}-${cfg}-${type}";
            
            mkdir("$ENV{OBJROOT}/$buildType");
            runcmd("xcopy /e/i/q/y src $ENV{OBJROOT}\\$buildType");
            runcmd("xcopy /e/i/q/y win32\\opensslconf_wrapper.h $ENV{OBJROOT}\\$buildType\\");

            my $makeArgs = join(" ",
            "SSL=ssleay32$libsfx",
            "CRYPTO=libeay32$libsfx",
            "PDB_PATH=build\\openssl$libsfx",
            "OUT_D=build",
            "EXTRA_LFLAGS=\"/NXCOMPAT /DYNAMICBASE /DEBUG $sehflags\"",    # <rdar://problem/7370791> Verification failed for YSopenssl and doesn't have debug libs.
            "EXTRA_MLFLAGS=\"/NXCOMPAT /DYNAMICBASE /DEBUG $sehflags\"",   # <rdar://problem/7370791> Verification failed for YSopenssl and doesn't have debug libs.
            "EXTRA_ASMFLAGS=\"$sehflagslc\"",                              # <rdar://problem/7370791> Verification failed for YSopenssl and doesn't have debug libs.
            );
            
            if (!$test) {
                # we don't need to build the test programs if we're not going to test them
                $makeArgs .= " T_EXE=";
            }
            
            open my $f, "> $ENV{OBJROOT}/$buildType/buildit.bat";
            print $f "echo on\r\n";
            if ($cfg eq "debug") {
                print $f "set DEBUG_BUILD=1\r\n";
            }
            print $f "call \"$ENV{VSINSTALLDIR}\\VC\\vcvarsall.bat\" $arch\r\n";
            print $f "echo on\r\n";
            print $f "perl Configure $cfgarg no-idea -Zi\r\n";
            print $f "call $batch\r\n";
            print $f "echo on\r\n";
            if ($test) {
                print $f "nmake -f $makefile $makeArgs\r\n";
                print $f "nmake -f $makefile test $makeArgs\r\n";
                print $f "if errorlevel 1 exit 1\r\n";
            } else {
                print $f "nmake -f $makefile all_noexe $makeArgs\r\n";
            }
            print $f "move inc32\\openssl\\opensslconf.h inc32\\openssl\\opensslconf${bits}.h\r\n";
            print $f "copy opensslconf_wrapper.h inc32\\openssl\\opensslconf.h\r\n";
            close $f;

            my $buildDir = "$ENV{OBJROOT}/${buildType}/build";
            push @incs, "$ENV{OBJROOT}\\${buildType}\\inc32";
            
            foreach my $lib ("ssleay32$libsfx", "libeay32$libsfx") {
                $wanted{"$buildDir/${lib}.lib"} = "lib$bits";
                if ($type eq "dll") {
                    $wanted{"$buildDir/${lib}.dll"} = "bin$bits";
                    $wanted{"$buildDir/${lib}.pdb"} = "bin$bits";
                }
            }
            
            if ($type eq "static") {
                $wanted{"$buildDir/openssl${libsfx}.pdb"} = "lib$bits";
            }

            my $pid = fork();
            if ($pid == 0) {
                chdir("$ENV{OBJROOT}/$buildType");
                exec("cmd", "/c", "call", "buildit.bat") || die "buildit: $!";
            }
            $pids{$pid} = $buildType;
        }
    }
}

my $status = 0;
my $errors = "";
foreach my $pid (keys(%pids)) {
    my $ret = waitpid($pid, 0);
    if (($ret != $pid) || ($? != 0)) {
        $errors .= "  error: $pids{$pid} build failed\n";
        $status = 1;
    }
}

mkdir("$ENV{DSTROOT}");
mkdir("$ENV{DSTROOT}/AppleInternal");

foreach my $wanted (sort(keys(%wanted))) {
    my $base = basename($wanted);
    my $dir = "$ENV{DSTROOT}/AppleInternal/$wanted{$wanted}";
    my $dst = "$dir/$base";
    mkdir($dir);
    if (copy($wanted, $dst)) {
        print "$wanted => $dst\n";
    } else {
        $errors .= "  error: Couldn't copy $wanted to $dst: $!\n";
        $status = 1;
    }
    if ($legacy && ($base =~ m|^(.*)_static.lib|)) {
        # make a copy of the static lib without the _static suffix to satisfy clients
        # of the legacy YSopenssl_static project
        $dst = "$dir/${1}.lib";
        if (copy($wanted, $dst)) {
            print "$wanted => $dst\n";
        } else {
            $errors .= "  error: Couldn't copy $wanted to $dst: $!\n";
            $status = 1;
        }
    }
}

foreach my $inc (@incs) {
    runcmd("xcopy /e/i/q/y $inc $ENV{DSTROOT}\\AppleInternal\\include");
}

# legacy dirs
if (-d "$ENV{DSTROOT}/AppleInternal/bin32") {
    runcmd("xcopy /e/i/q/y $ENV{DSTROOT}\\AppleInternal\\bin32 $ENV{DSTROOT}\\AppleInternal\\bin");
}

if (-d "$ENV{DSTROOT}/AppleInternal/lib32") {
    runcmd("xcopy /e/i/q/y $ENV{DSTROOT}\\AppleInternal\\lib32 $ENV{DSTROOT}\\AppleInternal\\lib");
}

if (length($errors)) { print "\n$errors"; }
exit $status;
