#!/usr/bin/perl

# Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# The contents of this file constitute Original Code as defined in and
# are subject to the Apple Public Source License Version 1.1 (the
# "License").  You may not use this file except in compliance with the
# License.  Please obtain a copy of the License at
# http://www.apple.com/publicsource and read it before using this file.
# 
# This Original Code and all software distributed under the License are
# distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License.
# 
# @APPLE_LICENSE_HEADER_END@
#
# Subtle combination of files and libraries make up the C++ runtime system for
# kernel modules.  We are dependant on the KernelModule kmod.make and
# CreateKModInfo.perl scripts to be exactly instep with both this library
# module and the libkmod module as well.
#
# If you do any maintenance on any of the following files make sure great
# care is taken to keep them in Sync.
#    KernelModule.bproj/kmod.make
#    KernelModule.bproj/CreateKModInfo.perl
#    KernelModule.bproj/kmodc++/pure.c
#    KernelModule.bproj/kmodc++/cplus_start.c
#    KernelModule.bproj/kmodc++/cplus_start.c
#    KernelModule.bproj/kmodc/c_start.c
#    KernelModule.bproj/kmodc/c_stop.c
#
# The trick is that the linkline links all of the developers modules.
# If any static constructors are used .constructors_used will be left as
# an undefined symbol.  This symbol is exported by the cplus_start.c routine
# which automatically brings in the appropriate C++ _start routine.  However
# the actual _start symbol is only required by the kmod_info structure that
# is created and initialized by the CreateKModInfo.perl script.  If no C++
# was used the _start will be an undefined symbol that is finally satisfied
# by the c_start module in the kmod library.
# 
# The linkline must look like this.
#    *.o -lkmodc++ kmod_info.o -lkmod
# 

use Getopt::Std;
use Cwd;

use File::Basename;
$cmdName = basename($0, '\.perl');
$inputName = 'CustomInfo.xml';

if ( ! -r $inputName ) {
    print STDERR "$cmdName: Can't find ", cwd(), "/$inputName";
    exit 1;
}

$ptypePath = "/Developer/ProjectTypes";
$xmldump = "$ptypePath/KernelExtension.projectType/Resources/xmldump";

$versionKey = "CFBundleVersion";
$modVersionKey = "Module/Version";
$nameKey = "Module/Name";
$initKey = "Module/Initialize";
$finalKey = "Module/Finalize";
$iokitKey1 = "Personality/IOProviderClass";
$iokitKey2 = "Personality/IOImports";

$defaultInit = "0";
$defaultFinal = "0";

getopts('dw');

sub lookupKey {
    my ($dict, $key, $default) = @_;
    my ($scaler);

    $scaler = `$xmldump $dict $key 2> /dev/null` || undef $scaler;
    if ( defined($scaler) )
        { chop $scaler; return $scaler; }
    elsif (defined($default)) {
        warn "$cmdName: Couldn't find $key in $inputName, using $default\n";
        return $default;
    }

    return undef;
}

$projectName = &lookupKey($inputName, $nameKey,
                                ${basename(cwd(), '.kmodproj')});
$subsystem = &lookupKey($inputName, $systemKey);

$iokitCheck = &lookupKey($inputName, $iokitKey1);
if ( !defined($iokitCheck) ) {
    $iokitCheck = &lookupKey($inputName, $iokitKey2);
}

if ( defined($iokitCheck) ) {
    $realInitFunc = $defaultInit;
    $realFinalFunc = $defaultFinal;
}
else {
    $realInitFunc = &lookupKey($inputName, $initKey, $defaultInit);
    $realFinalFunc = &lookupKey($inputName, $finalKey, $defaultFinal);
}

$version = &lookupKey($inputName, $modVersionKey);
if ( !defined($version) ) {
    if ( ! -r "../$inputName" ) {
        print STDERR "$cmdName: Can't find ", cwd(), "/../$inputName";
        exit 1;
    }

    $version = &lookupKey("../$inputName", $versionKey, "0.1a");
}

if ($realInitFunc ne $defaultInit) {
    $initExtern = "$realInitFunc(kmod_info_t *ki, void *data);\n";
    $initExtern = "__private_extern__ kern_return_t $initExtern";
}

if ($realFinalFunc ne $defaultFinal) {
    $finalExtern = "$realFinalFunc(kmod_info_t *ki, void *data);\n";
    $finalExtern = "__private_extern__ kern_return_t $finalExtern";
}

print "#include <mach/mach_types.h>

extern kern_return_t _start(kmod_info_t *ki, void *data);
extern kern_return_t _stop(kmod_info_t *ki, void *data);
$initExtern$finalExtern
KMOD_EXPLICIT_DECL($projectName, \"$version\", _start, _stop)
__private_extern__ kmod_start_func_t *_realmain = $realInitFunc;
__private_extern__ kmod_stop_func_t *_antimain = $realFinalFunc;

";


